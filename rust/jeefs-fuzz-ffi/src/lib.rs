// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! C-ABI shim over `jeefs_header::fs`, so one libFuzzer target can apply the
//! same operation to the C core and the Rust port and compare what each did
//! to the image. Nothing here is published; it exists for `fuzz_fs_diff`.

// The shim links into a host fuzz binary, so it uses std; the library it
// wraps stays no_std (it is pulled in with default-features = false).
use core::slice;
use jeefs_header::fs::{add_file, delete_file, files, format, read_file, write_file, FsError};

/// Width of the name field in a file header — every name pointer crossing
/// this boundary points at a buffer of exactly this size.
const NAME_FIELD: usize = 16;

/// Mirrors the C error codes so the two sides can be compared directly.
fn code(e: FsError) -> i16 {
    match e {
        FsError::FileExists => -1,
        FsError::FileNameNotValid => -4,
        FsError::FileNotFound => -5,
        FsError::NotEnoughSpace => -6,
        FsError::BufferNotValid => -8,
        FsError::EepromCorrupted => -10,
        FsError::FsVersionNotSupported => -13,
    }
}

/// Borrow a caller name from a fixed 16-byte field, up to its NUL.
///
/// Sixteen is the on-wire width of `JEEFSFileHeaderv1::name`, and the whole
/// field is read regardless of where the terminator sits — searching for the
/// NUL byte by byte would read the same memory anyway.
///
/// `None` means the bytes are not a name the library accepts, and every
/// caller turns that into `FILENAMENOTVALID` — the same code the C core
/// returns for such a name. A name outside printable ASCII fails here when
/// it is not valid UTF-8 and inside `filename_valid` when it is; both roads
/// lead to the same answer, which is what the differential harness checks.
///
/// # Safety
/// `name` must be null or point to 16 bytes valid for reads for the duration
/// of the call. A shorter allocation is undefined behaviour even when the
/// NUL comes early.
unsafe fn name_str<'a>(name: *const u8) -> Option<&'a str> {
    if name.is_null() {
        return None;
    }
    let bytes = slice::from_raw_parts(name, NAME_FIELD);
    let end = bytes.iter().position(|&b| b == 0)?;
    core::str::from_utf8(&bytes[..end]).ok()
}

/// # Safety
/// `image` must be valid for reads and writes of `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_format(image: *mut u8, size: u16, version: u8) -> i16 {
    let img = slice::from_raw_parts_mut(image, size as usize);
    match format(img, version) {
        Ok(()) => 0,
        Err(e) => code(e),
    }
}

/// # Safety
/// `image` must be valid for `size` bytes; `name` NUL-terminated; `data` valid for `data_len`.
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_add(image: *mut u8, size: u16, name: *const u8, data: *const u8, data_len: u16) -> i16 {
    let img = slice::from_raw_parts_mut(image, size as usize);
    let Some(n) = name_str(name) else {
        return -4;
    };
    let payload = if data.is_null() {
        &[][..]
    } else {
        slice::from_raw_parts(data, data_len as usize)
    };
    match add_file(img, n, payload) {
        Ok(written) => written as i16,
        Err(FsError::FileExists) => 0, // the C core reports this as zero written
        Err(e) => code(e),
    }
}

/// # Safety
/// As for [`jeefs_rs_add`].
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_write(image: *mut u8, size: u16, name: *const u8, data: *const u8, data_len: u16) -> i16 {
    let img = slice::from_raw_parts_mut(image, size as usize);
    let Some(n) = name_str(name) else {
        return -4;
    };
    let payload = if data.is_null() {
        &[][..]
    } else {
        slice::from_raw_parts(data, data_len as usize)
    };
    match write_file(img, n, payload) {
        Ok(written) => written as i16,
        Err(e) => code(e),
    }
}

/// # Safety
/// `image` must be valid for `size` bytes; `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_delete(image: *mut u8, size: u16, name: *const u8) -> i16 {
    let img = slice::from_raw_parts_mut(image, size as usize);
    let Some(n) = name_str(name) else {
        return -4;
    };
    match delete_file(img, n) {
        Ok(()) => 1, // the C core reports a completed delete as 1
        Err(e) => code(e),
    }
}

/// # Safety
/// `image` valid for `size` bytes; `name` NUL-terminated; `out` valid for `out_len`.
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_read(image: *const u8, size: u16, name: *const u8, out: *mut u8, out_len: u16) -> i16 {
    let img = slice::from_raw_parts(image, size as usize);
    let Some(n) = name_str(name) else {
        return -4;
    };
    let buf = slice::from_raw_parts_mut(out, out_len as usize);
    match read_file(img, n, buf) {
        Ok(read) => read as i16,
        Err(e) => code(e),
    }
}

/// Write the names the walk reaches into `out` and return how many, or the
/// error that stopped the walk. The layout matches the C `EEPROM_ListFiles`
/// array — 16 bytes per entry, NUL-padded — so the caller can compare the
/// two side by side rather than just counting.
///
/// # Safety
/// `image` must be valid for reads of `size` bytes, and `out` for writes of
/// `max_files * 16` bytes.
#[no_mangle]
pub unsafe extern "C" fn jeefs_rs_list(image: *const u8, size: u16, out: *mut u8, max_files: u16) -> i16 {
    let img = slice::from_raw_parts(image, size as usize);
    let names = slice::from_raw_parts_mut(out, max_files as usize * NAME_FIELD);
    names.fill(0);

    let iter = match files(img) {
        Ok(it) => it,
        Err(e) => return code(e),
    };
    let mut count: i16 = 0;
    for entry in iter {
        match entry {
            Ok(e) => {
                if count as usize >= max_files as usize {
                    return count; // the C core stops at the caller's cap too
                }
                let name = e.name_bytes();
                let at = count as usize * NAME_FIELD;
                names[at..at + name.len()].copy_from_slice(name);
                count += 1;
            }
            Err(e) => return code(e),
        }
    }
    count
}

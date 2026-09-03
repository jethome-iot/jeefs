// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Filesystem CRUD over a caller-owned image buffer — the allocation-free
//! Rust port of the C core in `src/jeefs.c` (filesystem-v1.md).
//!
//! The library performs no I/O: the environment reads the EEPROM, hands
//! every operation the image bytes, and writes them back. Nothing here
//! allocates, so a `no_std` firmware without a heap can add, overwrite and
//! delete files in place; whole-image build/parse lives in [`crate::image`]
//! and needs an allocator.
//!
//! Every rule is the C core's rule — chain validation, the `device.id`
//! first-slot invariant, the headerless-image claim, atomic failure (an
//! error return never mutates the buffer). Cross-language conformance is
//! locked by the shared mutation vectors in `tests/cross-language`.

use crate::generated::{DEVICE_ID_FILENAME, EMPTYBYTE, FILE_NAME_LENGTH, FS_VERSION, FS_VERSION_OFFSET};
use crate::generated::{HEADER_VERSION, MAGIC, MAGIC_LENGTH};
use crate::header::{detect_version, header_size, initialize_header, update_crc, verify_crc};

/// File header size on the wire (`JEEFSFileHeaderv1`).
const FHDR: usize = 28;
/// Bytes of the file header covered by `headerCrc32`.
const FHDR_CRC_COVERAGE: usize = 24;
/// Payload ceiling: the C API reports sizes in an `int16_t`.
const MAX_DATA: usize = 32767;
/// Chain links are `uint16_t`, so nothing past 64 KiB is addressable.
const MAX_IMAGE: usize = 65535;

/// Failure modes of the filesystem operations — the `EEPROMError` codes of
/// the C core, one variant each.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FsError {
    /// No file with that name in the chain.
    FileNotFound,
    /// A file with that name is already present (the C core returns 0).
    FileExists,
    /// Empty name, or longer than 15 characters.
    FileNameNotValid,
    /// Empty payload, one above `INT16_MAX`, a caller buffer too small for
    /// the file, or an image past the 16-bit address space.
    BufferNotValid,
    /// The image cannot hold the header, the chain and the new file.
    NotEnoughSpace,
    /// No usable board header, or a chain that fails validation.
    EepromCorrupted,
    /// The `fs_version` byte names a layout this build does not know.
    FsVersionNotSupported,
}

impl core::fmt::Display for FsError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            FsError::FileNotFound => write!(f, "file not found"),
            FsError::FileExists => write!(f, "file already exists"),
            FsError::FileNameNotValid => write!(f, "invalid file name"),
            FsError::BufferNotValid => write!(f, "invalid buffer"),
            FsError::NotEnoughSpace => write!(f, "not enough space"),
            FsError::EepromCorrupted => write!(f, "eeprom corrupted"),
            FsError::FsVersionNotSupported => write!(f, "unsupported filesystem version"),
        }
    }
}

impl core::error::Error for FsError {}

fn crc32(data: &[u8]) -> u32 {
    crc32fast::hash(data)
}

/// A byte is empty in either domain: written zero or erased 0xFF.
fn byte_is_empty(b: u8) -> bool {
    b == 0x00 || b == 0xFF
}

/// One entry of the chain, as returned by [`files`].
#[derive(Debug, Clone, Copy)]
pub struct FileEntry {
    name: [u8; FILE_NAME_LENGTH + 1],
    addr: u16,
    data_size: u16,
}

impl FileEntry {
    /// The name as stored, up to the NUL terminator.
    pub fn name_bytes(&self) -> &[u8] {
        let end = self.name.iter().position(|&b| b == 0).unwrap_or(self.name.len());
        &self.name[..end]
    }

    /// The name as text. Names are bytes on the medium; a name that is not
    /// valid UTF-8 reads as empty here — compare [`name_bytes`](Self::name_bytes)
    /// instead when that matters.
    pub fn name_str(&self) -> &str {
        core::str::from_utf8(self.name_bytes()).unwrap_or("")
    }

    /// Payload size in bytes.
    pub fn size(&self) -> usize {
        self.data_size as usize
    }

    /// Offset of this file's header in the image.
    pub fn offset(&self) -> usize {
        self.addr as usize
    }

    /// Offset of this file's payload in the image.
    pub fn data_offset(&self) -> usize {
        self.addr as usize + FHDR
    }
}

/// In-memory form of a file header; the wire form is little-endian.
#[derive(Clone, Copy, Default)]
struct FileHeader {
    name: [u8; FILE_NAME_LENGTH + 1],
    data_size: u16,
    crc32: u32,
    next: u16,
}

fn hdr_from_bytes(raw: &[u8]) -> FileHeader {
    let mut h = FileHeader::default();
    h.name.copy_from_slice(&raw[..FILE_NAME_LENGTH + 1]);
    h.data_size = u16::from_le_bytes([raw[16], raw[17]]);
    h.crc32 = u32::from_le_bytes([raw[18], raw[19], raw[20], raw[21]]);
    h.next = u16::from_le_bytes([raw[22], raw[23]]);
    h
}

/// Every header write goes through here, so `headerCrc32` is resealed on
/// creation, link rewrites and data-CRC refreshes alike.
fn hdr_to_bytes(h: &FileHeader, raw: &mut [u8]) {
    raw[..FILE_NAME_LENGTH + 1].copy_from_slice(&h.name);
    raw[16..18].copy_from_slice(&h.data_size.to_le_bytes());
    raw[18..22].copy_from_slice(&h.crc32.to_le_bytes());
    raw[22..24].copy_from_slice(&h.next.to_le_bytes());
    let seal = crc32(&raw[..FHDR_CRC_COVERAGE]);
    raw[24..FHDR].copy_from_slice(&seal.to_le_bytes());
}

/// Size of the board header, or `None` when the image carries none.
fn header_size_of(image: &[u8]) -> Option<usize> {
    if image.len() > MAX_IMAGE {
        return None;
    }
    let version = detect_version(image)?;
    let size = header_size(version)?;
    if size > image.len() {
        return None;
    }
    Some(size)
}

fn filename_valid(name: &str) -> bool {
    let len = name.len();
    len > 0 && len <= FILE_NAME_LENGTH
}

/// Validated chain iterator. `files` hands out entries; the internal walk
/// keeps the predecessor address the mutating operations need.
struct Iter<'a> {
    image: &'a [u8],
    addr: u16,
    prev: u16,
    next_addr: u16,
    fs_start: u16,
    hdr: FileHeader,
}

fn iter_begin(image: &[u8]) -> Result<Iter<'_>, FsError> {
    if image.len() > MAX_IMAGE {
        return Err(FsError::BufferNotValid);
    }
    let header_size = header_size_of(image).ok_or(FsError::EepromCorrupted)?;

    // The filesystem is gated by the fs_version byte of the board header:
    // 0 = no filesystem (the file area is empty regardless of content, as on
    // every image written before the field existed), 1 = the current layout,
    // anything else = a layout this build does not know.
    let fs_version = image[FS_VERSION_OFFSET];
    if fs_version != 0 && fs_version as usize != FS_VERSION {
        return Err(FsError::FsVersionNotSupported);
    }

    Ok(Iter {
        image,
        addr: 0,
        prev: 0,
        next_addr: if fs_version as usize == FS_VERSION {
            header_size as u16
        } else {
            0
        },
        fs_start: header_size as u16,
        hdr: FileHeader::default(),
    })
}

impl Iter<'_> {
    /// Advance to the next file. `Ok(true)` when a validated header is
    /// loaded, `Ok(false)` at the end of the chain.
    fn step(&mut self) -> Result<bool, FsError> {
        if self.next_addr == 0 {
            return Ok(false); // previous file was terminal
        }
        let start = self.next_addr as usize;
        if start + FHDR > self.image.len() {
            return Ok(false); // no room for another header: clean end
        }
        let raw = &self.image[start..start + FHDR];
        let mut hdr = hdr_from_bytes(raw);

        if byte_is_empty(hdr.name[0]) {
            return Ok(false); // unwritten slot (0x00 or 0xFF): end of chain
        }

        // A written header must checksum before any of its fields is trusted.
        let stored = u32::from_le_bytes([raw[24], raw[25], raw[26], raw[27]]);
        if crc32(&raw[..FHDR_CRC_COVERAGE]) != stored {
            return Err(FsError::EepromCorrupted);
        }

        // Defense in depth behind the CRC: a terminated name and a sane size.
        if hdr.name[FILE_NAME_LENGTH] != 0 {
            return Err(FsError::EepromCorrupted);
        }
        if hdr.data_size == 0 || hdr.data_size == 0xFFFF {
            return Err(FsError::EepromCorrupted);
        }

        let end = start + FHDR + hdr.data_size as usize;
        if end > self.image.len() {
            return Err(FsError::EepromCorrupted);
        }

        // An erased link (0xFFFF) terminates the chain like 0: the file may
        // have been written onto erased media without a link update.
        if hdr.next == 0xFFFF {
            hdr.next = 0;
        }

        // Contiguity: the link either terminates or names exactly the next
        // slot, and a claimed successor must have room for its own header.
        // This also makes cycles impossible — addresses strictly increase.
        if hdr.next != 0 && (hdr.next as usize != end || end + FHDR > self.image.len()) {
            return Err(FsError::EepromCorrupted);
        }

        self.prev = self.addr;
        self.addr = start as u16;
        self.next_addr = hdr.next;
        self.hdr = hdr;
        Ok(true)
    }

    fn entry(&self) -> FileEntry {
        FileEntry {
            name: self.hdr.name,
            addr: self.addr,
            data_size: self.hdr.data_size,
        }
    }
}

/// Iterator over the files in an image, in chain order.
///
/// Validation failures surface as an `Err` item and end the walk; a clean
/// end of chain simply stops.
pub struct FileIter<'a> {
    it: Iter<'a>,
    done: bool,
}

impl Iterator for FileIter<'_> {
    type Item = Result<FileEntry, FsError>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.done {
            return None;
        }
        match self.it.step() {
            Ok(true) => Some(Ok(self.it.entry())),
            Ok(false) => {
                self.done = true;
                None
            }
            Err(e) => {
                self.done = true;
                Some(Err(e))
            }
        }
    }
}

/// Walk the chain of `image`.
///
/// The board header and the `fs_version` gate are checked up front, so a
/// broken image fails here rather than mid-iteration.
pub fn files(image: &[u8]) -> Result<FileIter<'_>, FsError> {
    Ok(FileIter {
        it: iter_begin(image)?,
        done: false,
    })
}

/// Result of a full chain walk: the match (if any) and the first free byte.
struct Walk {
    found: Option<(u16, u16, u16, u32)>, // addr, prev, data_size, data crc32
    chain_end: usize,
}

fn chain_walk(image: &[u8], filename: Option<&str>) -> Result<Walk, FsError> {
    let mut it = iter_begin(image)?;
    let mut end = it.fs_start as usize;
    let mut found = None;
    let mut guard = 0usize;
    let max_files = image.len() / FHDR + 1;

    while it.step()? {
        guard += 1;
        if guard > max_files {
            return Err(FsError::EepromCorrupted); // unreachable by invariant
        }
        end = it.addr as usize + FHDR + it.hdr.data_size as usize;
        if let Some(name) = filename {
            if it.entry().name_bytes() == name.as_bytes() {
                found = Some((it.addr, it.prev, it.hdr.data_size, it.hdr.crc32));
            }
        }
    }
    Ok(Walk { found, chain_end: end })
}

/// Stamp the current filesystem version into the board header and refresh
/// the header CRC, which covers the byte. A no-op when already stamped.
fn fs_stamp(image: &mut [u8]) -> Result<(), FsError> {
    if image[FS_VERSION_OFFSET] as usize == FS_VERSION {
        return Ok(());
    }
    let size = header_size_of(image).ok_or(FsError::EepromCorrupted)?;
    image[FS_VERSION_OFFSET] = FS_VERSION as u8;
    if update_crc(&mut image[..size]) {
        Ok(())
    } else {
        Err(FsError::EepromCorrupted)
    }
}

/// Whether the image carries a board header whose CRC checks out.
pub fn header_check_consistency(image: &[u8]) -> bool {
    match header_size_of(image) {
        Some(size) => verify_crc(&image[..size]),
        None => false,
    }
}

/// Write an initialized header of `version` and wipe the file area.
pub fn format(image: &mut [u8], version: u8) -> Result<(), FsError> {
    if image.len() > MAX_IMAGE {
        return Err(FsError::BufferNotValid);
    }
    let size = header_size(version).ok_or(FsError::EepromCorrupted)?;
    if size > image.len() {
        return Err(FsError::NotEnoughSpace);
    }
    if !initialize_header(&mut image[..size], version) {
        return Err(FsError::EepromCorrupted);
    }
    image[size..].fill(EMPTYBYTE);
    // A formatted image carries the (empty) current filesystem.
    fs_stamp(image)
}

/// Copy the contents of `filename` into `buffer`, returning its size.
///
/// The stored data CRC is verified on every read.
pub fn read_file(image: &[u8], filename: &str, buffer: &mut [u8]) -> Result<usize, FsError> {
    if !filename_valid(filename) {
        return Err(FsError::FileNameNotValid);
    }
    if buffer.is_empty() {
        return Err(FsError::BufferNotValid);
    }
    let walk = chain_walk(image, Some(filename))?;
    let (addr, _, data_size, stored_crc) = walk.found.ok_or(FsError::FileNotFound)?;

    let size = data_size as usize;
    if size > MAX_DATA {
        return Err(FsError::EepromCorrupted); // not representable as int16_t
    }
    if size > buffer.len() {
        return Err(FsError::BufferNotValid);
    }
    let start = addr as usize + FHDR;
    let data = &image[start..start + size];
    if crc32(data) != stored_crc {
        return Err(FsError::EepromCorrupted);
    }
    buffer[..size].copy_from_slice(data);
    Ok(size)
}

/// Append `data` as a new file, returning the number of bytes written.
///
/// An image with no header at all — blank, erased or garbage — is claimed
/// on first write: an empty current-version header reserves the slot and
/// the real identity arrives later. A matching magic with an unknown
/// version is never claimed: that may be a header from a newer format. The
/// claim is atomic with the write — when header and file cannot both fit,
/// nothing is touched. The reserved `device.id` name always takes the
/// first slot, shifting an existing chain up.
pub fn add_file(image: &mut [u8], filename: &str, data: &[u8]) -> Result<usize, FsError> {
    if image.len() > MAX_IMAGE {
        return Err(FsError::BufferNotValid);
    }
    if !filename_valid(filename) {
        return Err(FsError::FileNameNotValid);
    }
    if data.is_empty() || data.len() > MAX_DATA {
        return Err(FsError::BufferNotValid);
    }
    let data_size = data.len();

    if header_size_of(image).is_none() {
        if image.len() >= MAGIC_LENGTH && &image[..MAGIC_LENGTH] == MAGIC.as_slice() {
            return Err(FsError::EepromCorrupted);
        }
        let claim_hdr = header_size(HEADER_VERSION as u8).ok_or(FsError::EepromCorrupted)?;
        if claim_hdr + FHDR + data_size > image.len() {
            return Err(FsError::NotEnoughSpace);
        }
        format(image, HEADER_VERSION as u8)?;
    }

    let walk = chain_walk(image, Some(filename))?;
    if walk.found.is_some() {
        return Err(FsError::FileExists);
    }
    let chain_end = walk.chain_end;
    if chain_end + FHDR + data_size > image.len() {
        return Err(FsError::NotEnoughSpace);
    }
    let header_size = header_size_of(image).ok_or(FsError::EepromCorrupted)?;

    fs_stamp(image)?;

    let mut hdr = FileHeader {
        data_size: data_size as u16,
        crc32: crc32(data),
        next: 0,
        ..FileHeader::default()
    };
    hdr.name[..filename.len()].copy_from_slice(filename.as_bytes());

    // The reserved device-identity file always takes the FIRST slot, so a
    // boot environment can read the whole identity as a bounded prefix
    // (header + one file header + record) instead of buffering the image.
    let span = FHDR + data_size;
    if chain_end > header_size && filename == DEVICE_ID_FILENAME {
        image.copy_within(header_size..chain_end, header_size + span);

        // Relink the moved headers. Contiguity was validated before the
        // move, so the moved chain is walked arithmetically. The one legal
        // nonzero terminal — a link onto the (formerly) empty slot at the
        // old chain end — is normalized to 0: shifting it would point at
        // unvetted bytes past the moved region.
        let moved_end = chain_end + span;
        let mut addr = header_size + span;
        while addr + FHDR <= moved_end {
            let mut mh = hdr_from_bytes(&image[addr..addr + FHDR]);
            if mh.next == 0 || mh.next == 0xFFFF {
                break;
            }
            if mh.next as usize >= chain_end {
                mh.next = 0;
                hdr_to_bytes(&mh, &mut image[addr..addr + FHDR]);
                break;
            }
            mh.next += span as u16;
            hdr_to_bytes(&mh, &mut image[addr..addr + FHDR]);
            addr += FHDR + mh.data_size as usize;
        }

        hdr.next = (header_size + span) as u16;
        hdr_to_bytes(&hdr, &mut image[header_size..header_size + FHDR]);
        image[header_size + FHDR..header_size + FHDR + data_size].copy_from_slice(data);
        return Ok(data_size);
    }

    let new_addr = chain_end;
    hdr_to_bytes(&hdr, &mut image[new_addr..new_addr + FHDR]);
    image[new_addr + FHDR..new_addr + FHDR + data_size].copy_from_slice(data);

    // Link the predecessor (the last existing file), if any. The first file
    // needs no link: its position is implied by the header size.
    if new_addr != header_size {
        let mut tail: Option<(u16, FileHeader)> = None;
        {
            let mut it = iter_begin(image)?;
            while it.step()? {
                if it.addr as usize != new_addr {
                    tail = Some((it.addr, it.hdr));
                }
            }
        }
        if let Some((addr, mut th)) = tail {
            th.next = new_addr as u16;
            let at = addr as usize;
            hdr_to_bytes(&th, &mut image[at..at + FHDR]);
        }
    }

    Ok(data_size)
}

/// Remove `filename`, compacting the chain behind it.
pub fn delete_file(image: &mut [u8], filename: &str) -> Result<(), FsError> {
    if !filename_valid(filename) {
        return Err(FsError::FileNameNotValid);
    }
    let walk = chain_walk(image, Some(filename))?;
    let (addr, prev, data_size, _) = walk.found.ok_or(FsError::FileNotFound)?;
    let chain_end = walk.chain_end;

    let victim = addr as usize;
    let shift = FHDR + data_size as usize;
    let tail_start = victim + shift; // first byte after the victim
    let tail_len = chain_end - tail_start; // bytes of real files after it

    // Re-read the victim's link: the walk keeps only what the callers need.
    let victim_next = u16::from_le_bytes([image[victim + 22], image[victim + 23]]);

    // The victim is terminal when nothing real follows it — including the
    // legal case of a non-zero link into a valid-but-empty slot (the
    // iterator ends the chain on the empty name).
    if victim_next == 0 || victim_next == 0xFFFF || tail_len == 0 {
        if prev != 0 {
            let at = prev as usize;
            let mut prev_hdr = hdr_from_bytes(&image[at..at + FHDR]);
            prev_hdr.next = 0;
            hdr_to_bytes(&prev_hdr, &mut image[at..at + FHDR]);
        }
        image[victim..victim + shift].fill(EMPTYBYTE);
        return Ok(());
    }

    // Compact: move [tail_start, chain_end) down by `shift`. The successor
    // lands exactly at the victim's address, so the predecessor's link
    // (which already names that address) stays valid without a write.
    image.copy_within(tail_start..chain_end, victim);

    // Rewrite the moved headers' absolute links. Only headers inside the
    // moved region are trusted — they were validated before the move; the
    // last moved file may legally link one past the region, where the freed
    // span is wiped to empty below.
    let moved_end = victim + tail_len;
    let mut at = victim;
    while at != 0 && at + FHDR <= moved_end {
        let mut hdr = hdr_from_bytes(&image[at..at + FHDR]);
        if hdr.next == 0 || hdr.next == 0xFFFF {
            break; // terminal link (0 or erased) needs no rewrite
        }
        let next = hdr.next as usize - shift;
        if next <= at {
            break; // defense in depth: validated links strictly increase
        }
        hdr.next = next as u16;
        hdr_to_bytes(&hdr, &mut image[at..at + FHDR]);
        at = next;
    }

    // Wipe the freed span at the old end of the chain.
    image[chain_end - shift..chain_end].fill(EMPTYBYTE);
    Ok(())
}

/// Replace the contents of `filename`, returning the number of bytes written.
///
/// A same-size write lands in place; a different size relocates the file
/// through delete + add, but only after checking that the result fits — a
/// failed write never loses the file.
pub fn write_file(image: &mut [u8], filename: &str, data: &[u8]) -> Result<usize, FsError> {
    if !filename_valid(filename) {
        return Err(FsError::FileNameNotValid);
    }
    if data.is_empty() || data.len() > MAX_DATA {
        return Err(FsError::BufferNotValid);
    }
    let data_size = data.len();

    let walk = chain_walk(image, Some(filename))?;
    let (addr, _, old_size, _) = walk.found.ok_or(FsError::FileNotFound)?;
    let chain_end = walk.chain_end;

    if old_size as usize == data_size {
        // Same size: overwrite in place, then refresh the stored CRC.
        let at = addr as usize;
        image[at + FHDR..at + FHDR + data_size].copy_from_slice(data);
        let mut hdr = hdr_from_bytes(&image[at..at + FHDR]);
        hdr.crc32 = crc32(data);
        hdr_to_bytes(&hdr, &mut image[at..at + FHDR]);
        return Ok(data_size);
    }

    // Different size: ensure the delete + add cannot run out of space
    // BEFORE destroying the old content.
    let old_span = FHDR + old_size as usize;
    let needed = chain_end - old_span + FHDR + data_size;
    if needed > image.len() {
        return Err(FsError::NotEnoughSpace);
    }

    delete_file(image, filename)?;
    add_file(image, filename, data)
}

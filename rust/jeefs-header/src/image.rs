// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Whole-image build and parse for the JEEFS filesystem (filesystem-v1.md).
//!
//! The library performs no I/O: [`build_image`] returns the bytes the
//! environment writes to the medium, [`parse_image`] consumes the bytes the
//! environment read. The rules mirror the C reader in `src/jeefs.c` and the
//! Python port; cross-language conformance is locked by the byte-identity
//! of the committed goldens (ctest `image_build_rs_matches_golden*`).

use crate::generated::{DEVICE_ID_FILENAME, FILE_NAME_LENGTH, FS_VERSION, FS_VERSION_OFFSET};
use crate::header::{detect_version, header_size};
use std::string::String;
use std::vec::Vec;

const FHDR: usize = 28; // sizeof(JEEFSFileHeaderv1)
const MAX_DATA: usize = 32767; // INT16_MAX: the C API's int16_t byte counts
const MAX_IMAGE: usize = 65535; // uint16_t addressing

/// One file of the chain: a name (<= 15 byte-range chars) and its payload.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImageFile {
    pub name: String,
    pub data: Vec<u8>,
}

/// The [`parse_image`] result. `files` is empty when `fs_version` is 0 —
/// the file area of such an image is empty regardless of content.
/// `unreadable` mirrors the C API's split between listing and reading:
/// names whose chain entry is valid but whose payload `EEPROM_ReadFile`
/// would refuse (data-CRC mismatch or dataSize above INT16_MAX); such
/// files still appear in `files` with the payload as stored.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedImage {
    pub version: u8,
    pub header_bytes: Vec<u8>,
    pub fs_version: u8,
    pub files: Vec<ImageFile>,
    pub unreadable: Vec<String>,
}

/// Errors of the image surface. Chain corruption carries the offset of
/// the offending file header.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageError {
    /// No valid board header (magic/version mismatch or short buffer).
    NoHeader,
    /// The board header fails its own CRC — a failed CRC is a failed
    /// header: no field is interpreted (checked on raw bytes before the
    /// typed model, ahead of every other rule).
    HeaderCrc,
    /// The fs_version byte names a layout this build does not know.
    FsVersion(u8),
    /// A file header fails validation at the given offset.
    Chain(usize),
    /// A file name is empty, too long, or carries NUL / non-byte chars.
    BadName,
    /// A payload is empty or above INT16_MAX bytes.
    BadData,
    /// Two files share a name.
    DuplicateName,
    /// image_size is outside 1..=65535 or cannot hold header + chain.
    Capacity,
}

impl core::fmt::Display for ImageError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            ImageError::NoHeader => write!(f, "no valid board header"),
            ImageError::HeaderCrc => write!(f, "board header CRC mismatch"),
            ImageError::FsVersion(v) => write!(f, "unsupported fs_version {v}"),
            ImageError::Chain(off) => write!(f, "corrupted file chain at offset {off}"),
            ImageError::BadName => write!(f, "invalid file name"),
            ImageError::BadData => write!(f, "invalid file payload size"),
            ImageError::DuplicateName => write!(f, "duplicate file names"),
            ImageError::Capacity => write!(f, "image size cannot hold header and chain"),
        }
    }
}

impl std::error::Error for ImageError {}

fn crc32(data: &[u8]) -> u32 {
    crc32fast::hash(data)
}

fn seal_file_header(name: &str, data: &[u8], next_addr: u16) -> [u8; FHDR] {
    let mut raw = [0u8; FHDR];
    for (i, ch) in name.chars().enumerate() {
        raw[i] = ch as u8; // validated byte-range chars only
    }
    raw[16..18].copy_from_slice(&(data.len() as u16).to_le_bytes());
    raw[18..22].copy_from_slice(&crc32(data).to_le_bytes());
    raw[22..24].copy_from_slice(&next_addr.to_le_bytes());
    let hc = crc32(&raw[..24]);
    raw[24..28].copy_from_slice(&hc.to_le_bytes());
    raw
}

/// Build a complete EEPROM image: board header + JEEFS file chain.
///
/// `header` is a prepared board header (its CRC is refreshed here after
/// `fs_version = 1` is stamped into the copy — the image carries a,
/// possibly empty, filesystem; the caller's buffer is untouched). Files
/// are laid out contiguously in the given order with one exception: the
/// reserved `device.id` file always takes the first slot
/// (filesystem-v1.md). The free space is zero-filled.
pub fn build_image(
    header: &[u8],
    files: &[(&str, &[u8])],
    image_size: usize,
) -> Result<Vec<u8>, ImageError> {
    if image_size == 0 || image_size > MAX_IMAGE {
        return Err(ImageError::Capacity);
    }
    let version = detect_version(header).ok_or(ImageError::NoHeader)?;
    let hdr_size = header_size(version).ok_or(ImageError::NoHeader)?;
    if header.len() < hdr_size {
        return Err(ImageError::NoHeader);
    }

    for (name, data) in files {
        let n = name.chars().count();
        if n == 0 || n > FILE_NAME_LENGTH || name.chars().any(|c| c == '\0' || c as u32 > 0xFF) {
            return Err(ImageError::BadName);
        }
        if data.is_empty() || data.len() > MAX_DATA {
            return Err(ImageError::BadData);
        }
    }
    let mut seen = std::collections::HashSet::new();
    for (name, _) in files {
        if !seen.insert(*name) {
            return Err(ImageError::DuplicateName);
        }
    }

    // The reserved device-identity file always occupies the first slot;
    // the sort is stable, so the rest keep their order.
    let mut ordered: Vec<&(&str, &[u8])> = files.iter().collect();
    ordered.sort_by_key(|(name, _)| *name != DEVICE_ID_FILENAME);

    let mut image = Vec::with_capacity(image_size);
    image.extend_from_slice(&header[..hdr_size]);
    image[FS_VERSION_OFFSET] = FS_VERSION as u8;
    let coverage = hdr_size - 4;
    let hc = crc32(&image[..coverage]).to_le_bytes();
    image[coverage..hdr_size].copy_from_slice(&hc);

    let mut offset = hdr_size;
    for (i, (name, data)) in ordered.iter().enumerate() {
        let end = offset + FHDR + data.len();
        if end > image_size {
            return Err(ImageError::Capacity);
        }
        let next = if i == ordered.len() - 1 {
            0
        } else {
            end as u16
        };
        image.extend_from_slice(&seal_file_header(name, data, next));
        image.extend_from_slice(data);
        offset = end;
    }
    if image.len() > image_size {
        return Err(ImageError::Capacity);
    }
    image.resize(image_size, 0);
    Ok(image)
}

/// Parse a complete EEPROM image back into header bytes and files.
///
/// The reader rules of filesystem-v1.md, exactly as the C iterator
/// applies them — with one stricter gate: this API returns header field
/// bytes, so the board-header CRC is verified on the raw bytes before
/// anything else is interpreted (a failed CRC is a failed header).
pub fn parse_image(data: &[u8]) -> Result<ParsedImage, ImageError> {
    let version = detect_version(data).ok_or(ImageError::NoHeader)?;
    let hdr_size = header_size(version).ok_or(ImageError::NoHeader)?;
    if data.len() < hdr_size {
        return Err(ImageError::NoHeader);
    }

    let coverage = hdr_size - 4;
    let stored = u32::from_le_bytes(data[coverage..hdr_size].try_into().unwrap());
    if crc32(&data[..coverage]) != stored {
        return Err(ImageError::HeaderCrc);
    }

    let fs_version = data[FS_VERSION_OFFSET];
    let mut parsed = ParsedImage {
        version,
        header_bytes: data[..hdr_size].to_vec(),
        fs_version,
        files: Vec::new(),
        unreadable: Vec::new(),
    };
    if fs_version == 0 {
        return Ok(parsed); // no filesystem: the file area is empty by contract
    }
    if fs_version != FS_VERSION as u8 {
        return Err(ImageError::FsVersion(fs_version));
    }

    let mut offset = hdr_size;
    while offset != 0 && offset + FHDR <= data.len() {
        let raw = &data[offset..offset + FHDR];
        if raw[0] == 0x00 || raw[0] == 0xFF {
            break; // unwritten slot: end of chain
        }
        let stored_hcrc = u32::from_le_bytes(raw[24..28].try_into().unwrap());
        if crc32(&raw[..24]) != stored_hcrc {
            return Err(ImageError::Chain(offset));
        }
        if raw[FILE_NAME_LENGTH] != 0 {
            return Err(ImageError::Chain(offset));
        }
        let data_size = u16::from_le_bytes(raw[16..18].try_into().unwrap()) as usize;
        let stored_crc = u32::from_le_bytes(raw[18..22].try_into().unwrap());
        let mut next = u16::from_le_bytes(raw[22..24].try_into().unwrap()) as usize;
        if data_size == 0 || data_size == 0xFFFF {
            return Err(ImageError::Chain(offset));
        }
        let end = offset + FHDR + data_size;
        if end > data.len() {
            return Err(ImageError::Chain(offset));
        }
        if next == 0xFFFF {
            next = 0; // erased link terminates like 0 (RFC #14)
        }
        if next != 0 && (next != end || end + FHDR > data.len()) {
            return Err(ImageError::Chain(offset));
        }

        let name: String = raw[..16]
            .iter()
            .take_while(|&&b| b != 0)
            .map(|&b| b as char)
            .collect();
        let payload = data[offset + FHDR..end].to_vec();
        if data_size > MAX_DATA || crc32(&payload) != stored_crc {
            parsed.unreadable.push(name.clone());
        }
        parsed.files.push(ImageFile {
            name,
            data: payload,
        });
        offset = next;
    }

    Ok(parsed)
}

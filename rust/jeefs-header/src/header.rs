// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Header parsing, CRC verification, and field access utilities.

use crate::generated::*;

/// Detect header version from raw bytes. Returns `Some(1..=3)` or `None`.
pub fn detect_version(data: &[u8]) -> Option<u8> {
    if data.len() < core::mem::size_of::<JeepromHeaderVersion>() {
        return None;
    }
    if &data[0..8] != MAGIC {
        return None;
    }
    let ver = data[8];
    if (1..=3).contains(&ver) {
        Some(ver)
    } else {
        None
    }
}

/// Return the expected header size (in bytes) for a given version.
pub fn header_size(version: u8) -> Option<usize> {
    match version {
        1 => Some(core::mem::size_of::<JeepromHeaderV1>()),
        2 => Some(core::mem::size_of::<JeepromHeaderV2>()),
        3 => Some(core::mem::size_of::<JeepromHeaderV3>()),
        _ => None,
    }
}

/// CRC32 coverage size for a given version (bytes before the crc32 field).
fn crc_coverage(version: u8) -> Option<usize> {
    match version {
        1 => Some(508), // 512 - 4
        2 => Some(252), // 256 - 4
        3 => Some(252), // 256 - 4
        _ => None,
    }
}

/// Verify the CRC32 of a header buffer. Returns `true` if valid.
pub fn verify_crc(data: &[u8]) -> bool {
    let ver = match detect_version(data) {
        Some(v) => v,
        None => return false,
    };
    let hdr_size = match header_size(ver) {
        Some(s) => s,
        None => return false,
    };
    if data.len() < hdr_size {
        return false;
    }
    let coverage = match crc_coverage(ver) {
        Some(c) => c,
        None => return false,
    };

    let calc = crc32fast::hash(&data[..coverage]);
    let stored = u32::from_le_bytes([
        data[coverage],
        data[coverage + 1],
        data[coverage + 2],
        data[coverage + 3],
    ]);
    calc == stored
}

/// Initialize a header in a caller-provided buffer.
/// Zeros the buffer, sets magic + version byte, computes CRC.
/// Buffer must be at least `header_size(version)` bytes.
/// Returns `true` on success, `false` if version is unknown or buffer is too small.
pub fn initialize_header(buf: &mut [u8], version: u8) -> bool {
    let size = match header_size(version) {
        Some(s) => s,
        None => return false,
    };
    if buf.len() < size {
        return false;
    }
    for b in buf[..size].iter_mut() {
        *b = 0;
    }
    buf[0..8].copy_from_slice(MAGIC);
    buf[8] = version;
    update_crc(&mut buf[..size])
}

/// Update the CRC32 field in a mutable header buffer. Returns `true` on success.
pub fn update_crc(data: &mut [u8]) -> bool {
    let ver = match detect_version(data) {
        Some(v) => v,
        None => return false,
    };
    let hdr_size = match header_size(ver) {
        Some(s) => s,
        None => return false,
    };
    if data.len() < hdr_size {
        return false;
    }
    let coverage = match crc_coverage(ver) {
        Some(c) => c,
        None => return false,
    };

    let calc = crc32fast::hash(&data[..coverage]);
    let bytes = calc.to_le_bytes();
    data[coverage..coverage + 4].copy_from_slice(&bytes);
    true
}

/// Extract a null-terminated string from a byte slice.
fn str_from_bytes(bytes: &[u8]) -> &str {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    core::str::from_utf8(&bytes[..end]).unwrap_or("")
}

// --- Accessor traits / impls for common header fields ---

impl JeepromHeaderV1 {
    /// Interpret raw bytes as a V1 header reference (zero-copy).
    ///
    /// # Safety
    /// Caller must ensure `data` is at least 512 bytes and properly aligned
    /// for a packed struct (which has alignment 1, so any alignment works).
    pub fn from_bytes(data: &[u8]) -> Option<&Self> {
        if data.len() < core::mem::size_of::<Self>() {
            return None;
        }
        // Safety: repr(C, packed) has alignment 1, so any pointer is valid.
        Some(unsafe { &*(data.as_ptr() as *const Self) })
    }

    pub fn boardname_str(&self) -> &str {
        str_from_bytes(&self.boardname)
    }

    pub fn boardversion_str(&self) -> &str {
        str_from_bytes(&self.boardversion)
    }

    pub fn serial_str(&self) -> &str {
        str_from_bytes(&self.serial)
    }

    pub fn usid_str(&self) -> &str {
        str_from_bytes(&self.usid)
    }

    pub fn cpuid_str(&self) -> &str {
        str_from_bytes(&self.cpuid)
    }
}

impl JeepromHeaderV2 {
    pub fn from_bytes(data: &[u8]) -> Option<&Self> {
        if data.len() < core::mem::size_of::<Self>() {
            return None;
        }
        Some(unsafe { &*(data.as_ptr() as *const Self) })
    }

    pub fn boardname_str(&self) -> &str {
        str_from_bytes(&self.boardname)
    }

    pub fn boardversion_str(&self) -> &str {
        str_from_bytes(&self.boardversion)
    }

    pub fn serial_str(&self) -> &str {
        str_from_bytes(&self.serial)
    }

    pub fn usid_str(&self) -> &str {
        str_from_bytes(&self.usid)
    }

    pub fn cpuid_str(&self) -> &str {
        str_from_bytes(&self.cpuid)
    }
}

impl JeepromHeaderV3 {
    pub fn from_bytes(data: &[u8]) -> Option<&Self> {
        if data.len() < core::mem::size_of::<Self>() {
            return None;
        }
        Some(unsafe { &*(data.as_ptr() as *const Self) })
    }

    pub fn boardname_str(&self) -> &str {
        str_from_bytes(&self.boardname)
    }

    pub fn boardversion_str(&self) -> &str {
        str_from_bytes(&self.boardversion)
    }

    pub fn serial_str(&self) -> &str {
        str_from_bytes(&self.serial)
    }

    pub fn usid_str(&self) -> &str {
        str_from_bytes(&self.usid)
    }

    pub fn cpuid_str(&self) -> &str {
        str_from_bytes(&self.cpuid)
    }

    pub fn signature_algorithm(&self) -> Result<SignatureAlgorithm, u8> {
        SignatureAlgorithm::from_u8(self.signature_version)
    }
}

impl JeefsFileHeaderV1 {
    pub fn from_bytes(data: &[u8]) -> Option<&Self> {
        if data.len() < core::mem::size_of::<Self>() {
            return None;
        }
        Some(unsafe { &*(data.as_ptr() as *const Self) })
    }

    pub fn name_str(&self) -> &str {
        str_from_bytes(&self.name)
    }
}

#[cfg(test)]
extern crate alloc;

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::vec;
    use alloc::vec::Vec;

    fn make_v3_header() -> Vec<u8> {
        let mut buf = vec![0u8; 256];
        // Magic
        buf[0..8].copy_from_slice(MAGIC);
        // Version = 3
        buf[8] = 3;
        // signature_version = 0
        buf[9] = 0;
        // boardname at offset 12
        let name = b"TestBoard";
        buf[12..12 + name.len()].copy_from_slice(name);
        // boardversion at offset 44
        let ver = b"1.0";
        buf[44..44 + ver.len()].copy_from_slice(ver);
        // serial at offset 76
        let serial = b"SN-001";
        buf[76..76 + serial.len()].copy_from_slice(serial);
        // MAC at offset 172
        buf[172..178].copy_from_slice(&[0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);
        // Calculate and set CRC
        let crc = crc32fast::hash(&buf[..252]);
        buf[252..256].copy_from_slice(&crc.to_le_bytes());
        buf
    }

    #[test]
    fn test_detect_version() {
        let buf = make_v3_header();
        assert_eq!(detect_version(&buf), Some(3));
    }

    #[test]
    fn test_detect_version_too_short() {
        assert_eq!(detect_version(&[0; 4]), None);
    }

    #[test]
    fn test_detect_version_bad_magic() {
        let mut buf = make_v3_header();
        buf[0] = b'X';
        assert_eq!(detect_version(&buf), None);
    }

    #[test]
    fn test_header_size() {
        assert_eq!(header_size(1), Some(512));
        assert_eq!(header_size(2), Some(256));
        assert_eq!(header_size(3), Some(256));
        assert_eq!(header_size(4), None);
    }

    #[test]
    fn test_verify_crc() {
        let buf = make_v3_header();
        assert!(verify_crc(&buf));
    }

    #[test]
    fn test_verify_crc_corrupted() {
        let mut buf = make_v3_header();
        buf[20] ^= 0xFF; // corrupt a byte
        assert!(!verify_crc(&buf));
    }

    #[test]
    fn test_update_crc() {
        let mut buf = make_v3_header();
        buf[20] = 0x42; // modify data
        assert!(!verify_crc(&buf));
        assert!(update_crc(&mut buf));
        assert!(verify_crc(&buf));
    }

    #[test]
    fn test_v3_field_access() {
        let buf = make_v3_header();
        let hdr = JeepromHeaderV3::from_bytes(&buf).unwrap();
        assert_eq!(hdr.boardname_str(), "TestBoard");
        assert_eq!(hdr.boardversion_str(), "1.0");
        assert_eq!(hdr.serial_str(), "SN-001");
        assert_eq!(hdr.mac, [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);
        assert_eq!(hdr.signature_algorithm(), Ok(SignatureAlgorithm::NONE));
    }

    #[test]
    fn test_struct_sizes() {
        assert_eq!(core::mem::size_of::<JeepromHeaderVersion>(), 12);
        assert_eq!(core::mem::size_of::<JeepromHeaderV1>(), 512);
        assert_eq!(core::mem::size_of::<JeepromHeaderV2>(), 256);
        assert_eq!(core::mem::size_of::<JeepromHeaderV3>(), 256);
        assert_eq!(core::mem::size_of::<JeefsFileHeaderV1>(), 24);
    }

    #[test]
    fn test_initialize_header() {
        for ver in [1u8, 2, 3] {
            let size = header_size(ver).unwrap();
            let mut buf = vec![0xFFu8; size];
            assert!(initialize_header(&mut buf, ver));
            assert_eq!(detect_version(&buf), Some(ver));
            assert!(verify_crc(&buf));
        }
        let mut buf = [0u8; 512];
        assert!(!initialize_header(&mut buf, 0));
        assert!(!initialize_header(&mut buf, 4));
        // Buffer too small
        let mut small = [0u8; 100];
        assert!(!initialize_header(&mut small, 3));
    }

    #[test]
    fn test_enum_from_u8() {
        assert_eq!(SignatureAlgorithm::from_u8(0), Ok(SignatureAlgorithm::NONE));
        assert_eq!(SignatureAlgorithm::from_u8(2), Ok(SignatureAlgorithm::SECP256R1));
        assert_eq!(SignatureAlgorithm::from_u8(99), Err(99));
    }

    #[test]
    fn test_signature_size() {
        assert_eq!(SignatureAlgorithm::NONE.signature_size(), 0);
        assert_eq!(SignatureAlgorithm::SECP192R1.signature_size(), 48);
        assert_eq!(SignatureAlgorithm::SECP256R1.signature_size(), 64);
    }
}

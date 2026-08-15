// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
// Copyright (c) 2026 JetHome. All rights reserved.
//! DeviceIdentityV1 record operations (RFC #26).
//!
//! A self-contained 256-byte signed blob stored as the JEEFS file
//! `device.id`. The header entry points gate on the JETHOME magic, so
//! the record has its own parallel module.

use crate::generated::*;
use crate::header::str_from_bytes;

pub const DEVID_RECORD_VERSION: u8 = 1;
const DEVID_SIZE: usize = core::mem::size_of::<DeviceIdentityV1>();
const DEVID_CRC_OFFSET: usize = DEVID_SIZE - 4;

/// Detect a DeviceIdentityV1 record. Returns the record_version or `None`
/// when the magic, the record_version or the signature_version byte is
/// unknown. An erased (all-0x00/0xFF) buffer fails the magic check —
/// "no record", not "corrupt".
pub fn devid_detect(data: &[u8]) -> Option<u8> {
    if data.len() < DEVID_SIZE {
        return None;
    }
    if &data[0..8] != DEVID_MAGIC {
        return None;
    }
    if data[8] != DEVID_RECORD_VERSION {
        return None;
    }
    if SignatureAlgorithm::from_u8(data[9]).is_err() {
        return None;
    }
    Some(data[8])
}

/// Verify the record CRC32 (bytes 0-251, IEEE 802.3).
pub fn devid_verify_crc(data: &[u8]) -> bool {
    if devid_detect(data).is_none() {
        return false;
    }
    let stored = u32::from_le_bytes([
        data[DEVID_CRC_OFFSET],
        data[DEVID_CRC_OFFSET + 1],
        data[DEVID_CRC_OFFSET + 2],
        data[DEVID_CRC_OFFSET + 3],
    ]);
    stored == crc32fast::hash(&data[..DEVID_CRC_OFFSET])
}

/// Recalculate and store the record CRC32.
pub fn devid_update_crc(data: &mut [u8]) -> bool {
    if devid_detect(data).is_none() {
        return false;
    }
    let crc = crc32fast::hash(&data[..DEVID_CRC_OFFSET]);
    data[DEVID_CRC_OFFSET..DEVID_CRC_OFFSET + 4].copy_from_slice(&crc.to_le_bytes());
    true
}

/// Initialize an empty record: magic, record_version = 1, zeros, CRC.
pub fn devid_init(data: &mut [u8]) -> bool {
    if data.len() < DEVID_SIZE {
        return false;
    }
    data[..DEVID_SIZE].fill(0);
    data[0..8].copy_from_slice(DEVID_MAGIC);
    data[8] = DEVID_RECORD_VERSION;
    devid_update_crc(data)
}

impl DeviceIdentityV1 {
    /// Interpret raw bytes as a record reference (zero-copy, align 1).
    pub fn from_bytes(data: &[u8]) -> Option<&Self> {
        if data.len() < DEVID_SIZE {
            return None;
        }
        Some(unsafe { &*(data.as_ptr() as *const Self) })
    }

    pub fn device_model_str(&self) -> &str {
        str_from_bytes(&self.device_model)
    }

    pub fn device_serial_str(&self) -> &str {
        str_from_bytes(&self.device_serial)
    }

    pub fn hw_revision_str(&self) -> &str {
        str_from_bytes(&self.hw_revision)
    }

    pub fn signature_algorithm(&self) -> Result<SignatureAlgorithm, u8> {
        SignatureAlgorithm::from_u8(self.signature_version)
    }
}

#[cfg(test)]
mod tests {
    extern crate alloc;

    use super::*;
    use alloc::vec;

    #[test]
    fn init_detect_verify_round_trip() {
        let mut buf = vec![0xFFu8; 256];
        assert!(devid_init(&mut buf));
        assert_eq!(devid_detect(&buf), Some(1));
        assert!(devid_verify_crc(&buf));
        assert_eq!(&buf[0..8], b"JHDEVID\0");
    }

    #[test]
    fn erased_buffers_are_no_record() {
        assert_eq!(devid_detect(&[0u8; 256]), None);
        assert_eq!(devid_detect(&[0xFFu8; 256]), None);
    }

    #[test]
    fn unknown_versions_rejected() {
        let mut buf = vec![0u8; 256];
        assert!(devid_init(&mut buf));
        buf[8] = 2;
        assert_eq!(devid_detect(&buf), None);
        buf[8] = 1;
        buf[9] = 7;
        assert_eq!(devid_detect(&buf), None);
    }

    #[test]
    fn committed_vector_parses() {
        // Expectations mirror test-vectors/vectors/devid_record_v1.json
        let raw: &[u8] = include_bytes!("../../../test-vectors/vectors/devid_record_v1.bin");
        assert_eq!(devid_detect(raw), Some(1));
        assert!(devid_verify_crc(raw));
        let rec = DeviceIdentityV1::from_bytes(raw).unwrap();
        assert_eq!(rec.device_model_str(), "JetHub-D2");
        assert_eq!(rec.device_serial_str(), "DSN-2026-000042");
        assert_eq!(rec.hw_revision_str(), "1.2a");
    }

    #[test]
    fn crc_gates_content() {
        let mut buf = vec![0u8; 256];
        assert!(devid_init(&mut buf));
        buf[12] = b'X';
        assert!(!devid_verify_crc(&buf));
        assert!(devid_update_crc(&mut buf));
        assert!(devid_verify_crc(&buf));
        let rec = DeviceIdentityV1::from_bytes(&buf).unwrap();
        assert_eq!(rec.device_model_str(), "X");
    }
}

// GENERATED FILE — DO NOT EDIT BY HAND.
// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//
// Source of truth: docs/format/*.md (see docs/CODEGEN.md for the policy).
// Manual edits are rejected: the codegen-check CI job and the local prek
// hook diff this file against the specs on every change.
//
// Regenerate with:
//   python -m jeefs_codegen --specs docs/format/*.md --rs-output rust/jeefs-header/src/generated.rs

#![allow(non_camel_case_types, dead_code)]

// --- Named constants ---

pub const BOARDNAME_LENGTH: usize = 31;
pub const BOARDVERSION_LENGTH: usize = 31;
pub const CPUID_LENGTH: usize = 32;
pub const DEVICE_ID_FILENAME: &str = "device.id";
pub const DEVID_MAGIC: &[u8; 8] = b"JHDEVID\0";
pub const EMPTYBYTE: u8 = 0x00;
pub const ERASEDBYTE: u8 = 0xFF;
pub const FILE_NAME_LENGTH: usize = 15;
pub const HEADER_VERSION: usize = 4;
pub const MAC_LENGTH: usize = 6;
pub const MAGIC: &[u8; 8] = b"JETHOME\0";
pub const MAGIC_LENGTH: usize = 8;
pub const PARTITION_SIZE: usize = 4096;
pub const SERIAL_LENGTH: usize = 32;
pub const SIGNATURE_FIELD_SIZE: usize = 64;
pub const USID_LENGTH: usize = 32;

/// JEEFSSignatureAlgorithm
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SignatureAlgorithm {
    NONE = 0,  // No signature
    SECP192R1 = 1,  // ECDSA secp192r1/NIST P-192, r‖s
    SECP256R1 = 2,  // ECDSA secp256r1/NIST P-256, r‖s
}

impl SignatureAlgorithm {
    pub fn from_u8(v: u8) -> Result<Self, u8> {
        match v {
            0 => Ok(SignatureAlgorithm::NONE),
            1 => Ok(SignatureAlgorithm::SECP192R1),
            2 => Ok(SignatureAlgorithm::SECP256R1),
            _ => Err(v),
        }
    }

    pub fn signature_size(self) -> usize {
        match self {
            SignatureAlgorithm::NONE => 0,
            SignatureAlgorithm::SECP192R1 => 48,
            SignatureAlgorithm::SECP256R1 => 64,
        }
    }
}

/// JEEPROMHeaderversion (12 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeepromHeaderVersion {
    pub magic: [u8; 8],  // Magic string "JETHOME\0"
    pub version: u8,  // Header version number
    pub reserved1: [u8; 3],  // Alignment / reserved
}

const _: () = assert!(core::mem::size_of::<JeepromHeaderVersion>() == 12);

/// JEEPROMHeaderv1 (512 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeepromHeaderV1 {
    pub magic: [u8; 8],  // "JETHOME\0"
    pub version: u8,  // Header version = 1
    pub reserved1: [u8; 3],  // Reserved (zeros)
    pub boardname: [u8; 32],  // Board name, null-terminated
    pub boardversion: [u8; 32],  // Board version, null-term.
    pub serial: [u8; 32],  // Board serial (bounded str.)
    pub usid: [u8; 32],  // CPU eFuse USID (bounded)
    pub cpuid: [u8; 32],  // CPU ID (bounded string)
    pub mac: [u8; 6],  // MAC address (6 raw bytes)
    pub reserved2: [u8; 2],  // Reserved for extended MAC
    pub modules: [u16; 16],  // 16 module IDs
    pub reserved3: [u8; 296],  // Reserved for future use
    pub crc32: u32,  // CRC32 of bytes 0-507
}

const _: () = assert!(core::mem::size_of::<JeepromHeaderV1>() == 512);

impl core::fmt::Debug for JeepromHeaderV1 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("JeepromHeaderV1")
            .field("magic", &self.magic)
            .field("version", &{ self.version })
            .field("reserved1", &self.reserved1)
            .field("boardname", &self.boardname)
            .field("boardversion", &self.boardversion)
            .field("serial", &self.serial)
            .field("usid", &self.usid)
            .field("cpuid", &self.cpuid)
            .field("mac", &self.mac)
            .field("reserved2", &self.reserved2)
            .field("modules", &self.modules())
            .field("reserved3", &&self.reserved3[..])
            .field("crc32", &self.crc32())
            .finish()
    }
}

impl JeepromHeaderV1 {
    /// `modules` decoded from the little-endian wire representation.
    pub fn modules(&self) -> [u16; 16] {
        let mut a = self.modules;
        for v in &mut a { *v = u16::from_le(*v); }
        a
    }
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
}

/// JEEPROMHeaderv2 (256 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeepromHeaderV2 {
    pub magic: [u8; 8],  // "JETHOME\0"
    pub version: u8,  // Header version = 2
    pub reserved1: [u8; 3],  // Reserved (zeros)
    pub boardname: [u8; 32],  // Board name, null-terminated
    pub boardversion: [u8; 32],  // Board version, null-term.
    pub serial: [u8; 32],  // Board serial (bounded str.)
    pub usid: [u8; 32],  // CPU eFuse USID (bounded)
    pub cpuid: [u8; 32],  // CPU ID (bounded string)
    pub mac: [u8; 6],  // MAC address (6 raw bytes)
    pub reserved2: [u8; 2],  // Reserved for extended MAC
    pub reserved3: [u8; 72],  // Reserved for future use
    pub crc32: u32,  // CRC32 of bytes 0-251
}

const _: () = assert!(core::mem::size_of::<JeepromHeaderV2>() == 256);

impl core::fmt::Debug for JeepromHeaderV2 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("JeepromHeaderV2")
            .field("magic", &self.magic)
            .field("version", &{ self.version })
            .field("reserved1", &self.reserved1)
            .field("boardname", &self.boardname)
            .field("boardversion", &self.boardversion)
            .field("serial", &self.serial)
            .field("usid", &self.usid)
            .field("cpuid", &self.cpuid)
            .field("mac", &self.mac)
            .field("reserved2", &self.reserved2)
            .field("reserved3", &&self.reserved3[..])
            .field("crc32", &self.crc32())
            .finish()
    }
}

impl JeepromHeaderV2 {
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
}

/// JEEPROMHeaderv3 (256 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeepromHeaderV3 {
    pub magic: [u8; 8],  // "JETHOME\0" (null-terminated string)
    pub version: u8,  // Header version = 3
    pub signature_version: u8,  // Signature algorithm (see enums)
    pub header_reserved: [u8; 2],  // Reserved (zeros)
    pub boardname: [u8; 32],  // Board name, null-terminated
    pub boardversion: [u8; 32],  // Board version, null-terminated
    pub serial: [u8; 32],  // Board serial number (bounded string)
    pub usid: [u8; 32],  // CPU eFuse USID (bounded string)
    pub cpuid: [u8; 32],  // CPU ID / factory MAC (bounded string)
    pub mac: [u8; 6],  // MAC address (6 raw bytes)
    pub reserved2: [u8; 2],  // Reserved for extended MAC
    pub signature: [u8; 64],  // ECDSA signature (r‖s, zero-padded)
    pub timestamp: i64,  // Unix timestamp (seconds)
    pub crc32: u32,  // CRC32 of bytes 0-251
}

const _: () = assert!(core::mem::size_of::<JeepromHeaderV3>() == 256);

impl core::fmt::Debug for JeepromHeaderV3 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("JeepromHeaderV3")
            .field("magic", &self.magic)
            .field("version", &{ self.version })
            .field("signature_version", &{ self.signature_version })
            .field("header_reserved", &self.header_reserved)
            .field("boardname", &self.boardname)
            .field("boardversion", &self.boardversion)
            .field("serial", &self.serial)
            .field("usid", &self.usid)
            .field("cpuid", &self.cpuid)
            .field("mac", &self.mac)
            .field("reserved2", &self.reserved2)
            .field("signature", &&self.signature[..])
            .field("timestamp", &self.timestamp())
            .field("crc32", &self.crc32())
            .finish()
    }
}

impl JeepromHeaderV3 {
    /// `timestamp` decoded from the little-endian wire representation.
    pub fn timestamp(&self) -> i64 {
        i64::from_le(self.timestamp)
    }
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
}

/// JEEPROMHeaderv4 (256 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeepromHeaderV4 {
    pub magic: [u8; 8],  // "JETHOME\0" (null-terminated string)
    pub version: u8,  // Header version = 4
    pub signature_version: u8,  // Signature algorithm (see enums)
    pub header_reserved: [u8; 2],  // Reserved (zeros)
    pub boardname: [u8; 32],  // Board name, null-terminated
    pub boardversion: [u8; 32],  // Board version, null-terminated
    pub board_serial: [u8; 32],  // Board serial number (bounded string)
    pub usid: [u8; 32],  // Board CPU/eFuse USID, if available
    pub cpuid: [u8; 32],  // Board CPU ID, if available
    pub mac: [u8; 6],  // MAC address (6 raw bytes)
    pub reserved2: [u8; 2],  // Reserved (alignment)
    pub signature: [u8; 64],  // ECDSA signature (r‖s, zero-padded)
    pub timestamp: i64,  // Header creation/signing time (Unix s)
    pub crc32: u32,  // CRC32 of bytes 0-251
}

const _: () = assert!(core::mem::size_of::<JeepromHeaderV4>() == 256);

impl core::fmt::Debug for JeepromHeaderV4 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("JeepromHeaderV4")
            .field("magic", &self.magic)
            .field("version", &{ self.version })
            .field("signature_version", &{ self.signature_version })
            .field("header_reserved", &self.header_reserved)
            .field("boardname", &self.boardname)
            .field("boardversion", &self.boardversion)
            .field("board_serial", &self.board_serial)
            .field("usid", &self.usid)
            .field("cpuid", &self.cpuid)
            .field("mac", &self.mac)
            .field("reserved2", &self.reserved2)
            .field("signature", &&self.signature[..])
            .field("timestamp", &self.timestamp())
            .field("crc32", &self.crc32())
            .finish()
    }
}

impl JeepromHeaderV4 {
    /// `timestamp` decoded from the little-endian wire representation.
    pub fn timestamp(&self) -> i64 {
        i64::from_le(self.timestamp)
    }
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
}

/// DeviceIdentityV1 (256 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct DeviceIdentityV1 {
    pub magic: [u8; 8],  // "JHDEVID\0" (null-terminated string)
    pub record_version: u8,  // Record version = 1
    pub signature_version: u8,  // Signature algorithm (same enum as header)
    pub reserved1: [u8; 2],  // Reserved (zeros)
    pub device_model: [u8; 32],  // Device model name (bounded string)
    pub device_serial: [u8; 32],  // Device serial number (bounded string)
    pub hw_revision: [u8; 16],  // Device hardware revision (bounded string)
    pub flags: u16,  // Flags: all bits reserved (see below)
    pub reserved2: [u8; 86],  // Reserved for future use (zeros)
    pub signature: [u8; 64],  // ECDSA signature (r‖s, zero-padded)
    pub timestamp: i64,  // Record creation/signing time (Unix s)
    pub crc32: u32,  // CRC32 of bytes 0-251
}

const _: () = assert!(core::mem::size_of::<DeviceIdentityV1>() == 256);

impl core::fmt::Debug for DeviceIdentityV1 {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("DeviceIdentityV1")
            .field("magic", &self.magic)
            .field("record_version", &{ self.record_version })
            .field("signature_version", &{ self.signature_version })
            .field("reserved1", &self.reserved1)
            .field("device_model", &self.device_model)
            .field("device_serial", &self.device_serial)
            .field("hw_revision", &self.hw_revision)
            .field("flags", &self.flags())
            .field("reserved2", &&self.reserved2[..])
            .field("signature", &&self.signature[..])
            .field("timestamp", &self.timestamp())
            .field("crc32", &self.crc32())
            .finish()
    }
}

impl DeviceIdentityV1 {
    /// `flags` decoded from the little-endian wire representation.
    pub fn flags(&self) -> u16 {
        u16::from_le(self.flags)
    }
    /// `timestamp` decoded from the little-endian wire representation.
    pub fn timestamp(&self) -> i64 {
        i64::from_le(self.timestamp)
    }
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
}

/// JEEFSFileHeaderv1 (24 bytes)
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct JeefsFileHeaderV1 {
    pub name: [u8; 16],  // Filename, null-terminated (max 15 ch.)
    pub data_size: u16,  // File data size in bytes
    pub crc32: u32,  // CRC32 of file data only (not header)
    pub next_file_address: u16,  // Absolute offset of next file, 0 = end
}

const _: () = assert!(core::mem::size_of::<JeefsFileHeaderV1>() == 24);

impl JeefsFileHeaderV1 {
    /// `data_size` decoded from the little-endian wire representation.
    pub fn data_size(&self) -> u16 {
        u16::from_le(self.data_size)
    }
    /// `crc32` decoded from the little-endian wire representation.
    pub fn crc32(&self) -> u32 {
        u32::from_le(self.crc32)
    }
    /// `next_file_address` decoded from the little-endian wire representation.
    pub fn next_file_address(&self) -> u16 {
        u16::from_le(self.next_file_address)
    }
}

// --- C name to Rust name mapping ---
// JEEPROMHeaderversion -> JeepromHeaderVersion
// JEEPROMHeaderv1 -> JeepromHeaderV1
// JEEPROMHeaderv2 -> JeepromHeaderV2
// JEEPROMHeaderv3 -> JeepromHeaderV3
// JEEPROMHeaderv4 -> JeepromHeaderV4
// JEEFSFileHeaderv1 -> JeefsFileHeaderV1

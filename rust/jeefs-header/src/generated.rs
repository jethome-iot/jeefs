// DO NOT EDIT — auto-generated from docs/format/*.md by tools/jeefs_codegen
// SPDX-License-Identifier: (GPL-2.0+ or MIT)
//
// Regenerate with:
//   python -m jeefs_codegen --specs docs/format/*.md --rs-output rust/jeefs-header/src/generated.rs

#![allow(non_camel_case_types, dead_code)]

// --- Named constants ---

pub const MAGIC: &[u8; 8] = b"JETHOME\0";
pub const MAGIC_LENGTH: usize = 8;
pub const HEADER_VERSION: usize = 3;
pub const SIGNATURE_FIELD_SIZE: usize = 64;
pub const FILE_NAME_LENGTH: usize = 15;
pub const MAC_LENGTH: usize = 6;
pub const SERIAL_LENGTH: usize = 32;
pub const USID_LENGTH: usize = 32;
pub const CPUID_LENGTH: usize = 32;
pub const BOARDNAME_LENGTH: usize = 31;
pub const BOARDVERSION_LENGTH: usize = 31;
pub const EEPROM_EMPTYBYTE: u8 = 0x00;
pub const EEPROM_PARTITION_SIZE: usize = 4096;

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
    pub serial: [u8; 32],  // Device serial number
    pub usid: [u8; 32],  // CPU eFuse USID
    pub cpuid: [u8; 32],  // CPU ID
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
            .field("modules", &{ self.modules })
            .field("reserved3", &&self.reserved3[..])
            .field("crc32", &{ self.crc32 })
            .finish()
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
    pub serial: [u8; 32],  // Device serial number
    pub usid: [u8; 32],  // CPU eFuse USID
    pub cpuid: [u8; 32],  // CPU ID
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
            .field("crc32", &{ self.crc32 })
            .finish()
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
    pub serial: [u8; 32],  // Device serial number
    pub usid: [u8; 32],  // CPU eFuse USID
    pub cpuid: [u8; 32],  // CPU ID / factory MAC
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
            .field("timestamp", &{ self.timestamp })
            .field("crc32", &{ self.crc32 })
            .finish()
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

// --- C name to Rust name mapping ---
// JEEPROMHeaderversion -> JeepromHeaderVersion
// JEEPROMHeaderv1 -> JeepromHeaderV1
// JEEPROMHeaderv2 -> JeepromHeaderV2
// JEEPROMHeaderv3 -> JeepromHeaderV3
// JEEFSFileHeaderv1 -> JeefsFileHeaderV1

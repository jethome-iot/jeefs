// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Golden reference EEPROM binary verification (Rust).
//!
//! Reads the 8192-byte reference image, verifies:
//! 1. Header v3 fields match expected values
//! 2. CRC32 is valid
//! 3. File system linked list contains 3 files: "config", "wifi.conf", "serial"
//! 4. File data CRC32 matches
//!
//! Usage: verify_golden_rs <eeprom_full.bin>

use jeefs_header::*;
use std::fs;
use std::process;

const EEPROM_SIZE: usize = 8192;

static mut FAILURES: i32 = 0;

fn check_str(name: &str, actual: &str, expected: &str) {
    if actual != expected {
        eprintln!("  FAIL: {} = \"{}\" (expected \"{}\")", name, actual, expected);
        unsafe { FAILURES += 1 };
    } else {
        println!("  OK: {} = \"{}\"", name, actual);
    }
}

fn check_int(name: &str, actual: i64, expected: i64) {
    if actual != expected {
        eprintln!("  FAIL: {} = {} (expected {})", name, actual, expected);
        unsafe { FAILURES += 1 };
    } else {
        println!("  OK: {} = {}", name, actual);
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <eeprom_full.bin>", args[0]);
        process::exit(2);
    }

    let eeprom = fs::read(&args[1]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[1], e);
        process::exit(2);
    });

    if eeprom.len() != EEPROM_SIZE {
        eprintln!(
            "FAIL: file size = {} (expected {})",
            eeprom.len(),
            EEPROM_SIZE
        );
        process::exit(1);
    }

    println!("=== Header verification (Rust) ===");

    // Version detection
    let ver = detect_version(&eeprom);
    check_int("header_version", ver.map(|v| v as i64).unwrap_or(-1), 3);

    // CRC
    if !verify_crc(&eeprom) {
        eprintln!("  FAIL: header CRC32 invalid");
        unsafe { FAILURES += 1 };
    } else {
        println!("  OK: header CRC32 valid");
    }

    // Header fields via Rust API
    let hdr = JeepromHeaderV3::from_bytes(&eeprom).unwrap();
    check_str("boardname", hdr.boardname_str(), "JetHub-D1p");
    check_str("boardversion", hdr.boardversion_str(), "2.0");
    check_str("serial", hdr.serial_str(), "SN-GOLDEN-001");
    check_int("signature_version", hdr.signature_version as i64, 0);

    println!("\n=== Filesystem verification (Rust) ===");

    // Walk the linked list
    let expected_names = ["config", "wifi.conf", "serial"];
    let mut file_count = 0usize;
    let mut offset = 256u16; // after v3 header

    while offset != 0 && (offset as usize) < EEPROM_SIZE {
        let fh = match JeefsFileHeaderV1::from_bytes(&eeprom[offset as usize..]) {
            Some(f) => f,
            None => break,
        };

        if fh.name[0] == 0 {
            break;
        }

        if file_count < 3 {
            check_str("filename", fh.name_str(), expected_names[file_count]);
        }

        let data_size = { fh.data_size } as usize;
        let next = { fh.next_file_address };
        let stored_crc = { fh.crc32 };

        println!(
            "  File {}: \"{}\" size={} next={}",
            file_count,
            fh.name_str(),
            data_size,
            next,
        );

        // Verify file data CRC
        let data_start = offset as usize + core::mem::size_of::<JeefsFileHeaderV1>();
        let file_data = &eeprom[data_start..data_start + data_size];
        let calc_crc = crc32fast::hash(file_data);
        if calc_crc != stored_crc {
            eprintln!(
                "  FAIL: file '{}' CRC mismatch: stored=0x{:08x} calculated=0x{:08x}",
                fh.name_str(),
                stored_crc,
                calc_crc
            );
            unsafe { FAILURES += 1 };
        } else {
            println!(
                "  OK: file '{}' CRC32 = 0x{:08x}",
                fh.name_str(),
                calc_crc
            );
        }

        file_count += 1;
        offset = next;
    }

    check_int("file_count", file_count as i64, 3);

    let failures = unsafe { FAILURES };
    println!("\nResult: {} failure(s)", failures);
    process::exit(if failures > 0 { 1 } else { 0 });
}

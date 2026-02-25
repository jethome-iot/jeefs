// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Example: Read EEPROM header, print version and MAC address.
//!
//! Usage: cargo run --example read_header -- <eeprom.bin>

use jeefs_header::header::{detect_version, header_size, verify_crc};
use jeefs_header::generated::{JeepromHeaderV1, JeepromHeaderV2, JeepromHeaderV3};
use std::env;
use std::fs;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <eeprom.bin>", args[0]);
        process::exit(1);
    }

    // Read file
    let data = fs::read(&args[1]).unwrap_or_else(|e| {
        eprintln!("Error reading {}: {}", args[1], e);
        process::exit(1);
    });

    // Detect version
    let version = match detect_version(&data) {
        Some(v) => v,
        None => {
            eprintln!("Error: invalid EEPROM header (bad magic or too short)");
            process::exit(1);
        }
    };
    println!("Header version: {}", version);

    // Check buffer is large enough for this version's struct
    let expected = header_size(version).unwrap();
    if data.len() < expected {
        eprintln!(
            "Error: file too short for v{} header ({} < {} bytes)",
            version,
            data.len(),
            expected
        );
        process::exit(1);
    }

    // Verify CRC
    if verify_crc(&data) {
        println!("CRC32: OK");
    } else {
        eprintln!("Warning: CRC32 mismatch");
    }

    // Access fields based on version
    match version {
        1 => {
            let hdr = JeepromHeaderV1::from_bytes(&data).unwrap();
            println!("Board name: {}", hdr.boardname_str());
            print_mac(&hdr.mac);
        }
        2 => {
            let hdr = JeepromHeaderV2::from_bytes(&data).unwrap();
            println!("Board name: {}", hdr.boardname_str());
            print_mac(&hdr.mac);
        }
        3 => {
            let hdr = JeepromHeaderV3::from_bytes(&data).unwrap();
            println!("Board name: {}", hdr.boardname_str());
            print_mac(&hdr.mac);
            let sig = hdr.signature_algorithm()
                .map(|a| format!("{:?}", a))
                .unwrap_or_else(|v| format!("Unknown({})", v));
            println!("Signature algorithm: {}", sig);
        }
        _ => unreachable!(),
    }
}

fn print_mac(mac: &[u8; 6]) {
    println!(
        "MAC address: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}

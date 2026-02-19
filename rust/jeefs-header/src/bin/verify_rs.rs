// SPDX-License-Identifier: (GPL-2.0+ or MIT)
//! Cross-language verification using Rust API.
//! Usage: verify_rs <bin_file> <json_file>

use jeefs_header::*;
use std::fs;
use std::process;

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

fn check_mac(name: &str, actual: &[u8; 6], expected_str: &str) {
    let parts: Vec<u8> = expected_str
        .split(':')
        .filter_map(|s| u8::from_str_radix(s, 16).ok())
        .collect();
    if parts.len() != 6 {
        eprintln!("  FAIL: cannot parse expected MAC: {}", expected_str);
        unsafe { FAILURES += 1 };
        return;
    }
    let expected: [u8; 6] = [parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]];
    if *actual != expected {
        eprintln!("  FAIL: {} mismatch (expected {})", name, expected_str);
        unsafe { FAILURES += 1 };
    } else {
        println!(
            "  OK: {} = {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            name, actual[0], actual[1], actual[2], actual[3], actual[4], actual[5]
        );
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <bin_file> <json_file>", args[0]);
        process::exit(2);
    }

    let bin_data = fs::read(&args[1]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[1], e);
        process::exit(2);
    });

    let json_str = fs::read_to_string(&args[2]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[2], e);
        process::exit(2);
    });

    let json: serde_json::Value = serde_json::from_str(&json_str).unwrap_or_else(|e| {
        eprintln!("JSON parse error: {}", e);
        process::exit(2);
    });

    let expected_version = json["version"].as_i64().unwrap_or(0);
    println!(
        "Verifying (Rust): {} (version {}, {} bytes)",
        args[1],
        expected_version,
        bin_data.len()
    );

    // Version detection
    let ver = detect_version(&bin_data);
    check_int(
        "detected_version",
        ver.map(|v| v as i64).unwrap_or(-1),
        expected_version,
    );

    // CRC
    if !verify_crc(&bin_data) {
        eprintln!("  FAIL: CRC verification failed");
        unsafe { FAILURES += 1 };
    } else {
        println!("  OK: CRC32 valid");
    }

    // Header size
    if let Some(v) = ver {
        if let Some(expected_size) = json["header_size"].as_i64() {
            check_int(
                "header_size",
                header_size(v).unwrap_or(0) as i64,
                expected_size,
            );
        }
    }

    // Fields are nested under "fields" in the JSON
    let fields = &json["fields"];

    // Common fields: use version-appropriate struct
    let ver_num = ver.unwrap_or(0);
    match ver_num {
        1 => {
            if let Some(hdr) = JeepromHeaderV1::from_bytes(&bin_data) {
                if let Some(s) = fields["boardname"].as_str() {
                    check_str("boardname", hdr.boardname_str(), s);
                }
                if let Some(s) = fields["boardversion"].as_str() {
                    check_str("boardversion", hdr.boardversion_str(), s);
                }
                if let Some(s) = fields["serial"].as_str() {
                    check_str("serial", hdr.serial_str(), s);
                }
                if let Some(s) = fields["usid"].as_str() {
                    check_str("usid", hdr.usid_str(), s);
                }
                if let Some(s) = fields["cpuid"].as_str() {
                    check_str("cpuid", hdr.cpuid_str(), s);
                }
                if let Some(s) = fields["mac"].as_str() {
                    check_mac("mac", &hdr.mac, s);
                }
            }
        }
        2 => {
            if let Some(hdr) = JeepromHeaderV2::from_bytes(&bin_data) {
                if let Some(s) = fields["boardname"].as_str() {
                    check_str("boardname", hdr.boardname_str(), s);
                }
                if let Some(s) = fields["boardversion"].as_str() {
                    check_str("boardversion", hdr.boardversion_str(), s);
                }
                if let Some(s) = fields["serial"].as_str() {
                    check_str("serial", hdr.serial_str(), s);
                }
                if let Some(s) = fields["usid"].as_str() {
                    check_str("usid", hdr.usid_str(), s);
                }
                if let Some(s) = fields["cpuid"].as_str() {
                    check_str("cpuid", hdr.cpuid_str(), s);
                }
                if let Some(s) = fields["mac"].as_str() {
                    check_mac("mac", &hdr.mac, s);
                }
            }
        }
        3 => {
            if let Some(hdr) = JeepromHeaderV3::from_bytes(&bin_data) {
                if let Some(s) = fields["boardname"].as_str() {
                    check_str("boardname", hdr.boardname_str(), s);
                }
                if let Some(s) = fields["boardversion"].as_str() {
                    check_str("boardversion", hdr.boardversion_str(), s);
                }
                if let Some(s) = fields["serial"].as_str() {
                    check_str("serial", hdr.serial_str(), s);
                }
                if let Some(s) = fields["usid"].as_str() {
                    check_str("usid", hdr.usid_str(), s);
                }
                if let Some(s) = fields["cpuid"].as_str() {
                    check_str("cpuid", hdr.cpuid_str(), s);
                }
                if let Some(s) = fields["mac"].as_str() {
                    check_mac("mac", &hdr.mac, s);
                }
                // V3-specific fields
                if let Some(sig_ver) = fields["signature_version"].as_i64() {
                    check_int(
                        "signature_version",
                        hdr.signature_version as i64,
                        sig_ver,
                    );
                }
            }
        }
        _ => {
            eprintln!("  FAIL: unsupported version {}", ver_num);
            unsafe { FAILURES += 1 };
        }
    }

    let failures = unsafe { FAILURES };
    println!("\nResult: {} failure(s)", failures);
    process::exit(if failures > 0 { 1 } else { 0 });
}

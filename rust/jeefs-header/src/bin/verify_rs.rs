// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
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

/// The signature field is 64 bytes whatever the algorithm puts in it: a
/// shorter signature is zero-padded to the end, and "no signature" means the
/// whole field is zero. Checking only the populated prefix would let a
/// generator leave anything behind it.
fn check_signature(bin_data: &[u8], fields: &serde_json::Value) {
    if bin_data.len() < 244 {
        // A file shorter than the header it claims to be has no signature to
        // read — say so instead of panicking on the slice.
        eprintln!("  FAIL: file is {} bytes, too short for a signature", bin_data.len());
        unsafe { FAILURES += 1 };
        return;
    }

    // A present-but-not-a-string value is malformed metadata, not an absent
    // signature: say so instead of quietly expecting an empty field.
    let raw = &fields["signature_hex"];
    if !raw.is_null() && !raw.is_string() {
        eprintln!("  FAIL: signature_hex is not a string");
        unsafe { FAILURES += 1 };
        return;
    }

    let expected: Vec<u8> = match raw.as_str() {
        Some(hex) if !hex.is_empty() => {
            // Slicing by byte pairs below is only valid on ASCII: a
            // multi-byte character has an even byte length but no char
            // boundary at index 2, which panics rather than failing.
            if !hex.is_ascii() {
                eprintln!("  FAIL: signature_hex is not hex");
                unsafe { FAILURES += 1 };
                return;
            }
            if !hex.len().is_multiple_of(2) || hex.len() / 2 > 64 {
                eprintln!("  FAIL: signature_hex length {}", hex.len());
                unsafe { FAILURES += 1 };
                return;
            }
            match (0..hex.len())
                .step_by(2)
                .map(|i| u8::from_str_radix(&hex[i..i + 2], 16).ok())
                .collect::<Option<Vec<u8>>>()
            {
                Some(bytes) => bytes,
                None => {
                    eprintln!("  FAIL: signature_hex not hex");
                    unsafe { FAILURES += 1 };
                    return;
                }
            }
        }
        _ => Vec::new(),
    };

    // The declared algorithm fixes the length; a vector that supplies a
    // different one is malformed however well the bytes match.
    let wire_sig_ver = bin_data[9];
    let want_len = match SignatureAlgorithm::from_u8(wire_sig_ver) {
        Ok(algo) => algo.signature_size(),
        Err(v) => {
            eprintln!("  FAIL: unknown signature_version {v}");
            unsafe { FAILURES += 1 };
            return;
        }
    };
    if want_len != expected.len() {
        eprintln!(
            "  FAIL: signature_version {wire_sig_ver} wants {want_len} bytes, vector supplies {}",
            expected.len()
        );
        unsafe { FAILURES += 1 };
        return;
    }

    let field = &bin_data[180..244];
    if field[..expected.len()] != expected[..] {
        eprintln!("  FAIL: signature mismatch");
        unsafe { FAILURES += 1 };
    } else if field[expected.len()..].iter().any(|&b| b != 0) {
        eprintln!(
            "  FAIL: signature tail not zero-padded past {} bytes",
            expected.len()
        );
        unsafe { FAILURES += 1 };
    } else {
        println!("  OK: signature ({} bytes, zero-padded to 64)", expected.len());
    }
}

/// Kept separate from the signature check: a broken signature expectation
/// must not decide whether the timestamp gets validated, or this port would
/// quietly verify less than C, C++ and Python do for the same vector.
fn check_timestamp(bin_data: &[u8], fields: &serde_json::Value) {
    // as_i64() says None for both an absent key and a present non-integer;
    // only the first is an omission, the second is malformed metadata.
    let raw = &fields["timestamp"];
    let expected_ts = match raw.as_i64() {
        Some(v) => v,
        None if raw.is_null() => return,
        None => {
            eprintln!("  FAIL: timestamp is present but not an integer");
            unsafe { FAILURES += 1 };
            return;
        }
    };
    if bin_data.len() < 252 {
        eprintln!("  FAIL: file is {} bytes, too short for a timestamp", bin_data.len());
        unsafe { FAILURES += 1 };
        return;
    }
    let actual_ts = i64::from_le_bytes(bin_data[244..252].try_into().unwrap());
    check_int("timestamp", actual_ts, expected_ts);
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
                check_signature(&bin_data, fields);
                check_timestamp(&bin_data, fields);
            }
        }
        4 => {
            if let Some(hdr) = JeepromHeaderV4::from_bytes(&bin_data) {
                if let Some(s) = fields["boardname"].as_str() {
                    check_str("boardname", hdr.boardname_str(), s);
                }
                if let Some(s) = fields["boardversion"].as_str() {
                    check_str("boardversion", hdr.boardversion_str(), s);
                }
                if let Some(s) = fields["board_serial"].as_str() {
                    check_str("board_serial", hdr.board_serial_str(), s);
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
                if let Some(sig_ver) = fields["signature_version"].as_i64() {
                    check_int(
                        "signature_version",
                        hdr.signature_version as i64,
                        sig_ver,
                    );
                }
                check_signature(&bin_data, fields);
                check_timestamp(&bin_data, fields);
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

// SPDX-License-Identifier: (GPL-2.0+ or MIT)
//! Cross-language header generator (Rust): create a .bin header from .json spec.
//! Usage: generate_rs <json_file> <output_bin>

use jeefs_header::*;
use std::fs;
use std::process;

fn pack_string(buf: &mut [u8], offset: usize, size: usize, value: &str) {
    let bytes = value.as_bytes();
    let len = bytes.len().min(size - 1);
    buf[offset..offset + len].copy_from_slice(&bytes[..len]);
    // Rest is already zero from initialization
}

fn parse_mac(mac_str: &str) -> Option<[u8; 6]> {
    let parts: Vec<u8> = mac_str
        .split(':')
        .filter_map(|s| u8::from_str_radix(s, 16).ok())
        .collect();
    if parts.len() != 6 {
        return None;
    }
    Some([parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]])
}

fn hex_to_bytes(hex: &str) -> Vec<u8> {
    (0..hex.len())
        .step_by(2)
        .filter_map(|i| u8::from_str_radix(&hex[i..i + 2], 16).ok())
        .collect()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <json_file> <output_bin>", args[0]);
        process::exit(2);
    }

    let json_str = fs::read_to_string(&args[1]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[1], e);
        process::exit(2);
    });

    let json: serde_json::Value = serde_json::from_str(&json_str).unwrap_or_else(|e| {
        eprintln!("JSON parse error: {}", e);
        process::exit(2);
    });

    let version = json["version"].as_u64().unwrap_or(0) as u8;

    let mut buf = initialize_header(version).unwrap_or_else(|| {
        eprintln!("Unsupported version: {}", version);
        process::exit(1);
    });

    let fields = &json["fields"];

    // Common string fields (same offsets for all versions)
    if let Some(s) = fields["boardname"].as_str() {
        pack_string(&mut buf, 12, 32, s);
    }
    if let Some(s) = fields["boardversion"].as_str() {
        pack_string(&mut buf, 44, 32, s);
    }
    if let Some(s) = fields["serial"].as_str() {
        pack_string(&mut buf, 76, 32, s);
    }
    if let Some(s) = fields["usid"].as_str() {
        pack_string(&mut buf, 108, 32, s);
    }
    if let Some(s) = fields["cpuid"].as_str() {
        pack_string(&mut buf, 140, 32, s);
    }

    if let Some(s) = fields["mac"].as_str() {
        if let Some(mac) = parse_mac(s) {
            buf[172..178].copy_from_slice(&mac);
        }
    }

    // V3-specific fields
    if version == 3 {
        if let Some(v) = fields["signature_version"].as_u64() {
            buf[9] = v as u8;
        }

        if let Some(ts) = fields["timestamp"].as_i64() {
            buf[244..252].copy_from_slice(&ts.to_le_bytes());
        }

        if let Some(hex) = fields["signature_hex"].as_str() {
            let sig_bytes = hex_to_bytes(hex);
            let len = sig_bytes.len().min(64);
            buf[180..180 + len].copy_from_slice(&sig_bytes[..len]);
        }
    }

    update_crc(&mut buf);

    fs::write(&args[2], &buf).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[2], e);
        process::exit(2);
    });

    println!(
        "Generated (Rust): {} ({} bytes, version {})",
        args[2],
        buf.len(),
        version
    );
}

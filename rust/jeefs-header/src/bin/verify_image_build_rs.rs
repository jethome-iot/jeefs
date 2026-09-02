// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Image-API golden lock (Rust): build_image must reproduce the committed
//! golden byte-for-byte from its JSON description, and parse_image must
//! read it back to the same files. Mirrors verify_image_build_py.py.
//!
//! Usage: verify_image_build_rs <golden.bin> <golden.json>

use jeefs_header::image::{build_image, parse_image};
use jeefs_header::{initialize_header, update_crc};
use std::fs;
use std::process;

fn pack_str(buf: &mut [u8], off: usize, len: usize, value: &str) {
    let bytes = value.as_bytes();
    let n = bytes.len().min(len);
    buf[off..off + n].copy_from_slice(&bytes[..n]);
}

fn parse_mac(s: &str) -> [u8; 6] {
    let hex: String = s.chars().filter(|c| c.is_ascii_hexdigit()).collect();
    let mut mac = [0u8; 6];
    for (i, byte) in mac.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&hex[i * 2..i * 2 + 2], 16).unwrap();
    }
    mac
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <bin_file> <json_file>", args[0]);
        process::exit(2);
    }
    let golden = fs::read(&args[1]).expect("golden bin");
    let spec: serde_json::Value =
        serde_json::from_str(&fs::read_to_string(&args[2]).expect("json")).expect("json");

    let h = &spec["header"];
    let version = h["version"].as_u64().unwrap() as u8;
    let serial_key = if version == 4 {
        "board_serial"
    } else {
        "serial"
    };

    let mut header = vec![0u8; if version == 1 { 512 } else { 256 }];
    assert!(initialize_header(&mut header, version));
    header[9] = h["signature_version"].as_u64().unwrap_or(0) as u8;
    pack_str(&mut header, 12, 31, h["boardname"].as_str().unwrap());
    pack_str(&mut header, 44, 31, h["boardversion"].as_str().unwrap());
    pack_str(&mut header, 76, 32, h[serial_key].as_str().unwrap());
    pack_str(&mut header, 108, 32, h["usid"].as_str().unwrap());
    pack_str(&mut header, 140, 32, h["cpuid"].as_str().unwrap());
    header[172..178].copy_from_slice(&parse_mac(h["mac"].as_str().unwrap()));
    let ts = h["timestamp"].as_i64().unwrap_or(0);
    header[244..252].copy_from_slice(&ts.to_le_bytes());
    assert!(update_crc(&mut header));

    let files: Vec<(String, Vec<u8>)> = spec["files"]
        .as_array()
        .unwrap()
        .iter()
        .map(|f| {
            let hex = f["data_hex"].as_str().unwrap();
            let data = (0..hex.len())
                .step_by(2)
                .map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap())
                .collect();
            (f["name"].as_str().unwrap().to_string(), data)
        })
        .collect();
    let file_refs: Vec<(&str, &[u8])> = files
        .iter()
        .map(|(n, d)| (n.as_str(), d.as_slice()))
        .collect();

    let image_size = spec["eeprom_size"].as_u64().unwrap() as usize;
    let built = build_image(&header, &file_refs, image_size).expect("build_image");

    if built != golden {
        let diff = built
            .iter()
            .zip(golden.iter())
            .position(|(a, b)| a != b)
            .unwrap_or(built.len().min(golden.len()));
        eprintln!(
            "FAIL: built image differs from golden at byte {diff} (sizes {}/{})",
            built.len(),
            golden.len()
        );
        process::exit(1);
    }
    println!("OK: build_image reproduces {} byte-for-byte", args[1]);

    let parsed = parse_image(&golden).expect("parse_image");
    let got: Vec<(&str, &[u8])> = parsed
        .files
        .iter()
        .map(|f| (f.name.as_str(), f.data.as_slice()))
        .collect();
    if got != file_refs {
        eprintln!("FAIL: parse_image files mismatch");
        process::exit(1);
    }
    if !parsed.unreadable.is_empty() {
        eprintln!(
            "FAIL: golden files reported unreadable: {:?}",
            parsed.unreadable
        );
        process::exit(1);
    }
    println!("OK: parse_image reads {} files back", got.len());
}

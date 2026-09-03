// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Mutation-vector runner (Rust): apply a shared .ops scenario to an image
//! and report what happened, then dump the resulting bytes. The C runner
//! (apply_ops_c) speaks the same script and the same journal, so the two
//! ports are compared on both — see verify_fs_mutation.py.
//!
//! Usage: apply_ops_rs <scenario.ops> <out.bin>

use jeefs_header::fs::{add_file, delete_file, files, format, header_check_consistency, read_file, write_file, FsError};
use std::fs;
use std::process;

/// MAX_FILES of the C runner: its ListFiles buffer holds this many names.
const C_LIST_CAP: usize = 512;
/// Ceiling for anything a scenario can ask the runner to allocate — the C
/// runner's static buffers are this size, and a typo in a vector must fail
/// loudly instead of exhausting memory.
const MAX_BUF: usize = 65535;

fn err_class(e: FsError) -> &'static str {
    match e {
        FsError::FileNotFound => "not_found",
        FsError::FileExists => "exists",
        FsError::FileNameNotValid => "name_invalid",
        FsError::BufferNotValid => "buffer_invalid",
        FsError::NotEnoughSpace => "no_space",
        FsError::EepromCorrupted => "corrupted",
        FsError::FsVersionNotSupported => "fs_version",
    }
}

/// "fill:<byte>:<count>" or a hex string.
fn parse_payload(spec: &str) -> Option<Vec<u8>> {
    if let Some(rest) = spec.strip_prefix("fill:") {
        let (b, n) = rest.split_once(':')?;
        let count = n.parse::<usize>().ok()?;
        if count > MAX_BUF {
            return None;
        }
        return Some(vec![b.parse::<u8>().ok()?; count]);
    }
    if spec.len() / 2 > MAX_BUF {
        return None;
    }
    if !spec.len().is_multiple_of(2) || !spec.is_ascii() {
        return None;
    }
    (0..spec.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&spec[i..i + 2], 16).ok())
        .collect()
}

fn init_image(kind: &str, size: usize) -> Vec<u8> {
    match kind {
        "erased" => vec![0xFFu8; size],
        // deterministic, carries no magic
        "garbage" => (0..size).map(|i| (i.wrapping_mul(37).wrapping_add(11)) as u8).collect(),
        _ => vec![0u8; size],
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <scenario.ops> <out.bin>", args[0]);
        process::exit(2);
    }
    let script = fs::read_to_string(&args[1]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[1], e);
        process::exit(2);
    });

    let mut image = init_image("zeros", 8192);

    for (idx, line) in script
        .lines()
        .map(str::trim_end)
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .enumerate()
    {
        let mut it = line.split_whitespace();
        let op = it.next().unwrap_or("");
        let arg1 = it.next().unwrap_or("");
        let arg2 = it.next().unwrap_or("");

        match op {
            "init" => {
                let size: usize = arg2.parse().unwrap_or(0);
                if size > MAX_BUF {
                    eprintln!("image size out of range: {arg2}");
                    process::exit(2);
                }
                image = init_image(arg1, size);
                println!("{idx} init ok {}", image.len());
            }
            "format" => match format(&mut image, arg1.parse().unwrap_or(0)) {
                Ok(()) => println!("{idx} format ok 0"),
                Err(e) => println!("{idx} format err {}", err_class(e)),
            },
            "add" | "write" => {
                let data = parse_payload(arg2).unwrap_or_else(|| {
                    eprintln!("bad payload: {arg2}");
                    process::exit(2);
                });
                let r = if op == "add" {
                    add_file(&mut image, arg1, &data)
                } else {
                    write_file(&mut image, arg1, &data)
                };
                match r {
                    Ok(n) => println!("{idx} {op} ok {n}"),
                    Err(e) => println!("{idx} {op} err {}", err_class(e)),
                }
            }
            "delete" => match delete_file(&mut image, arg1) {
                // The C core reports a completed delete as 1.
                Ok(()) => println!("{idx} delete ok 1"),
                Err(e) => println!("{idx} delete err {}", err_class(e)),
            },
            "read" => {
                let cap: usize = arg2.parse().unwrap_or(0).min(MAX_BUF);
                let mut buf = vec![0u8; cap];
                match read_file(&image, arg1, &mut buf) {
                    Ok(n) => println!("{idx} read ok {n} {:08x}", crc32fast::hash(&buf[..n])),
                    Err(e) => println!("{idx} read err {}", err_class(e)),
                }
            }
            "list" => match files(&image) {
                Err(e) => println!("{idx} list err {}", err_class(e)),
                Ok(iter) => {
                    let mut out = String::new();
                    let mut count = 0usize;
                    let mut failure = None;
                    for entry in iter {
                        // The C ListFiles stops at its caller-supplied cap
                        // and still reports success; mirror that here so the
                        // journals compare the ports, not the API shapes.
                        if count >= C_LIST_CAP {
                            break;
                        }
                        match entry {
                            Ok(e) => {
                                out.push(' ');
                                out.push_str(e.name_str());
                                count += 1;
                            }
                            Err(e) => {
                                failure = Some(e);
                                break;
                            }
                        }
                    }
                    // The C ListFiles reports a mid-chain failure instead of
                    // the names it had already collected.
                    match failure {
                        Some(e) => println!("{idx} list err {}", err_class(e)),
                        None => println!("{idx} list ok {count}{out}"),
                    }
                }
            },
            "poke" => {
                let off = parse_uint(arg1);
                let val = u8::from_str_radix(arg2.trim_start_matches("0x"), 16).unwrap_or(0);
                if off < image.len() {
                    image[off] = val;
                }
                println!("{idx} poke ok {off}");
            }
            "consistency" => println!(
                "{idx} consistency ok {}",
                if header_check_consistency(&image) { 1 } else { 0 }
            ),
            other => {
                eprintln!("unknown op: {other}");
                process::exit(2);
            }
        }
    }

    fs::write(&args[2], &image).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[2], e);
        process::exit(2);
    });
}

/// Accept both decimal and 0x-prefixed offsets, like C's strtoul(.., 0).
fn parse_uint(s: &str) -> usize {
    match s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        Some(hex) => usize::from_str_radix(hex, 16).unwrap_or(usize::MAX),
        None => s.parse().unwrap_or(usize::MAX),
    }
}

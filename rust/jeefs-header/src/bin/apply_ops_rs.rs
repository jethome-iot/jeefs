// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Mutation-vector runner (Rust): execute a compiled ops program against an
//! image and report what happened, then dump the resulting bytes. The C
//! runner (apply_ops_c) executes the same program and prints the same
//! journal, so the two ports are compared on both — see
//! verify_fs_mutation.py.
//!
//! The program is produced by tests/cross-language/ops_program.py, which is
//! the only reader of .ops text in the harness: every runner used to parse
//! the text itself and they disagreed on malformed input, so a divergence
//! between the *parsers* was reported as a divergence between the *ports*
//! (#119). Here there is nothing left to interpret — fixed-width integers
//! and length-prefixed blobs.
//!
//! Usage: apply_ops_rs <program.jops> <out.bin>

use jeefs_header::fs::{add_file, delete_file, files, format, header_check_consistency, read_file, write_file, FsError};
use jeefs_header::walk::{DataVerifier, Step, Walk};
use std::fs;
use std::io::{self, Write};
use std::process;
use std::str;

/// MAX_FILES of the C runner: its ListFiles buffer holds this many names.
const C_LIST_CAP: usize = 512;
/// Ceiling for anything a scenario can ask the runner to allocate — the C
/// runner's static buffers are this size, and a typo in a vector must fail
/// loudly instead of exhausting memory.
const MAX_BUF: usize = 65535;

const MAGIC: &[u8] = b"JOPS";
const PROGRAM_VERSION: u8 = 1;

const OP_INIT: u8 = 0;
const OP_FORMAT: u8 = 1;
const OP_ADD: u8 = 2;
const OP_WRITE: u8 = 3;
const OP_DELETE: u8 = 4;
const OP_READ: u8 = 5;
const OP_LIST: u8 = 6;
const OP_WALK: u8 = 7;
const OP_POKE: u8 = 8;
const OP_RESEAL: u8 = 9;
const OP_CONSISTENCY: u8 = 10;

const KIND_ZEROS: u8 = 0;
const KIND_ERASED: u8 = 1;
const KIND_GARBAGE: u8 = 2;

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

/// A program that cannot be read is a broken build artifact, not a test
/// outcome: it must never reach the journal, where the comparison would
/// read it as something the ports disagree about.
fn broken_program(what: &str) -> ! {
    eprintln!("{what}");
    process::exit(2);
}

/// Cursor over the program bytes. Every field is taken through `take()`,
/// so the end of the buffer is checked in exactly one place and a
/// truncated program ends the run with a diagnostic rather than a panic.
struct Program<'a> {
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Program<'a> {
    fn new(bytes: &'a [u8]) -> Self {
        Program { bytes, pos: 0 }
    }

    fn at_end(&self) -> bool {
        self.pos >= self.bytes.len()
    }

    fn take(&mut self, n: usize) -> &'a [u8] {
        // checked_add, not pos + n: a blob length is attacker-width and
        // would otherwise wrap the bound it is being checked against.
        let end = match self.pos.checked_add(n) {
            Some(end) if end <= self.bytes.len() => end,
            _ => broken_program(&format!("truncated program: want {n} bytes at offset {}", self.pos)),
        };
        let field = &self.bytes[self.pos..end];
        self.pos = end;
        field
    }

    fn u8(&mut self) -> u8 {
        self.take(1)[0]
    }

    fn u32(&mut self) -> u32 {
        let b = self.take(4);
        u32::from_le_bytes([b[0], b[1], b[2], b[3]])
    }

    fn blob(&mut self) -> &'a [u8] {
        let len = self.u32() as usize;
        self.take(len)
    }
}

fn init_image(kind: u8, size: usize) -> Vec<u8> {
    match kind {
        KIND_ZEROS => vec![0u8; size],
        KIND_ERASED => vec![0xFFu8; size],
        // deterministic, carries no magic
        KIND_GARBAGE => (0..size).map(|i| (i.wrapping_mul(37).wrapping_add(11)) as u8).collect(),
        // Three kinds exist; a fourth would leave the two runners applying
        // the scenario to different media, which is the divergence this
        // whole harness is built to detect.
        _ => broken_program(&format!("unknown image kind: {kind}")),
    }
}

/// A name field is bytes, because the format's name domain is printable
/// ASCII (#116) and a scenario names something outside it to watch every
/// port refuse it. `fs` and `walk` take `&str`, so a non-UTF-8 name cannot
/// be passed through — but it must not be skipped either: any byte outside
/// printable ASCII is outside the domain, so the library would answer
/// FileNameNotValid whatever the encoding. The caller prints that answer,
/// which is the line the C runner prints for the same program.
fn name_of(blob: &[u8]) -> Option<&str> {
    str::from_utf8(blob).ok()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <program.jops> <out.bin>", args[0]);
        process::exit(2);
    }
    let bytes = fs::read(&args[1]).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[1], e);
        process::exit(2);
    });

    let mut prog = Program::new(&bytes);
    if prog.take(MAGIC.len()) != MAGIC {
        broken_program(&format!("{}: not an ops program", args[1]));
    }
    let version = prog.u8();
    if version != PROGRAM_VERSION {
        broken_program(&format!("{}: program version {version}, expected {PROGRAM_VERSION}", args[1]));
    }

    let mut image = init_image(KIND_ZEROS, 8192);
    let mut idx = 0usize;

    while !prog.at_end() {
        let op = prog.u8();
        match op {
            OP_INIT => {
                let kind = prog.u8();
                let size = prog.u32() as usize;
                if size == 0 || size > MAX_BUF {
                    broken_program(&format!("image size out of range: {size}"));
                }
                image = init_image(kind, size);
                println!("{idx} init ok {}", image.len());
            }
            OP_FORMAT => {
                // The field is a u32 so a scenario can ask to be formatted
                // with a version that does not exist. The library takes a
                // byte; a wider request falls back to 0, which is not a
                // header version either, so it is refused as corrupted —
                // the class the C runner reports for the same request.
                // Truncating instead would turn 260 into a valid 4.
                let version = u8::try_from(prog.u32()).unwrap_or(0);
                match format(&mut image, version) {
                    Ok(()) => println!("{idx} format ok 0"),
                    Err(e) => println!("{idx} format err {}", err_class(e)),
                }
            }
            OP_ADD | OP_WRITE => {
                let name = prog.blob();
                let data = prog.blob();
                let op_name = if op == OP_ADD { "add" } else { "write" };
                if data.len() > MAX_BUF {
                    broken_program(&format!("payload too large: {}", data.len()));
                }
                match name_of(name) {
                    None => println!("{idx} {op_name} err name_invalid"),
                    Some(name) => {
                        let r = if op == OP_ADD {
                            add_file(&mut image, name, data)
                        } else {
                            write_file(&mut image, name, data)
                        };
                        match r {
                            Ok(n) => println!("{idx} {op_name} ok {n}"),
                            Err(e) => println!("{idx} {op_name} err {}", err_class(e)),
                        }
                    }
                }
            }
            OP_DELETE => match name_of(prog.blob()) {
                None => println!("{idx} delete err name_invalid"),
                Some(name) => match delete_file(&mut image, name) {
                    // The C core reports a completed delete as 1.
                    Ok(()) => println!("{idx} delete ok 1"),
                    Err(e) => println!("{idx} delete err {}", err_class(e)),
                },
            },
            OP_READ => {
                let name = prog.blob();
                let cap = (prog.u32() as usize).min(MAX_BUF);
                match name_of(name) {
                    None => println!("{idx} read err name_invalid"),
                    Some(name) => {
                        let mut buf = vec![0u8; cap];
                        match read_file(&image, name, &mut buf) {
                            Ok(n) => println!("{idx} read ok {n} {:08x}", crc32fast::hash(&buf[..n])),
                            Err(e) => println!("{idx} read err {}", err_class(e)),
                        }
                    }
                }
            }
            OP_LIST => match files(&image) {
                Err(e) => println!("{idx} list err {}", err_class(e)),
                Ok(iter) => {
                    // Names go out as the bytes the medium holds, not as
                    // text: C prints the stored field, and a name outside
                    // the printable-ASCII domain can sit on a medium this
                    // library did not write. Rendering it through a `&str`
                    // turned such a name into nothing at all (#119).
                    let mut out: Vec<u8> = Vec::new();
                    let mut count = 0usize;
                    let mut failure = None;
                    for entry in iter {
                        match entry {
                            Ok(e) => {
                                // The cap is applied after the entry is
                                // validated, as in the C ListFiles: its
                                // iterator step runs first, so a corrupt
                                // header just past the cap is still
                                // reported rather than hidden by it.
                                if count >= C_LIST_CAP {
                                    break;
                                }
                                out.push(b' ');
                                out.extend_from_slice(e.name_bytes());
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
                        None => {
                            let mut line = format!("{idx} list ok {count}").into_bytes();
                            line.extend_from_slice(&out);
                            line.push(b'\n');
                            if io::stdout().write_all(&line).is_err() {
                                // Everything else in this runner exits 2 on a
                                // failure; a closed pipe should not be the one
                                // case that panics instead.
                                eprintln!("cannot write the journal");
                                process::exit(2);
                            }
                        }
                    }
                }
            },
            OP_RESEAL => {
                // reseal <offset>: recompute a file header's headerCrc32
                // after a poke, so a scenario can present a header that was
                // legally WRITTEN with unusual content rather than merely
                // corrupted.
                // Checked: a scenario naming a huge offset would otherwise
                // wrap the bound and slice out of range, and out of range
                // prints no number so the journal cannot carry a value the
                // two runners hold in different widths.
                let off = prog.u32() as usize;
                if off.checked_add(28).is_some_and(|end| end <= image.len()) {
                    let c = crc32fast::hash(&image[off..off + 24]);
                    image[off + 24..off + 28].copy_from_slice(&c.to_le_bytes());
                    println!("{idx} reseal ok {off}");
                } else {
                    println!("{idx} reseal skip");
                }
            }
            OP_WALK => {
                // Locate the file the way a bounded-RAM environment does:
                // the pull-model walker plus a running CRC over the
                // payload. The journal records the read count too, so a
                // port that reaches the same terminal by a different
                // number of hops diverges.
                let name = prog.blob();
                let prefix_len = image.len().min(256);
                let mut hops = 0usize;
                // A name walk_begin would refuse costs no hop, so the
                // refused-name line carries the same 0 the C runner prints.
                match name_of(name) {
                    None => println!("{idx} walk err name_invalid {hops}"),
                    Some(name) => match Walk::begin(&image[..prefix_len], image.len() as u16, name) {
                        Err(e) => println!("{idx} walk err {} {hops}", err_class(e)),
                        Ok(mut w) => {
                            let outcome = loop {
                                match w.step() {
                                    Step::Want { offset, len } => {
                                        let at = offset as usize;
                                        hops += 1;
                                        if let Err(e) = w.feed(&image[at..at + len as usize]) {
                                            break Err(e);
                                        }
                                    }
                                    Step::Failed(e) => break Err(e),
                                    terminal => break Ok(terminal),
                                }
                            };
                            match outcome {
                                Err(e) => println!("{idx} walk err {} {hops}", err_class(e)),
                                Ok(Step::Found(f)) => {
                                    let at = f.offset() as usize;
                                    let mut v = DataVerifier::new(&f);
                                    for chunk in image[at..at + f.size() as usize].chunks(64) {
                                        v.update(chunk);
                                    }
                                    println!(
                                        "{idx} walk ok found {hops} {} {} {:08x} {}",
                                        f.offset(),
                                        f.size(),
                                        f.crc32(),
                                        if v.finish() { 1 } else { 0 }
                                    );
                                }
                                Ok(_) => println!("{idx} walk ok notfound {hops}"),
                            }
                        }
                    },
                }
            }
            OP_POKE => {
                let off = prog.u32() as usize;
                let value = prog.u32();
                // The compiler's grammar makes this field a byte; a wider
                // value is a program nothing in the harness emits, and
                // narrowing it silently would write a byte no scenario asked
                // for.
                let Ok(value) = u8::try_from(value) else {
                    broken_program(&format!("poke value out of range: {value}"));
                };
                if off < image.len() {
                    image[off] = value;
                    println!("{idx} poke ok {off}");
                } else {
                    println!("{idx} poke skip");
                }
            }
            OP_CONSISTENCY => println!(
                "{idx} consistency ok {}",
                if header_check_consistency(&image) { 1 } else { 0 }
            ),
            other => broken_program(&format!("unknown opcode: {other}")),
        }
        idx += 1;
    }

    fs::write(&args[2], &image).unwrap_or_else(|e| {
        eprintln!("{}: {}", args[2], e);
        process::exit(2);
    });
}

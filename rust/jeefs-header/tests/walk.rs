// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Pull-model walker — the Rust port of src/jeefs_walk.c (#109). The
//! scenarios mirror tests/test_09_walk/test_09.c one for one: the two
//! walkers must reach the same terminal state, after the same number of
//! header reads, on the same image.

use jeefs_header::fs::{add_file, format, read_file, FsError};
use jeefs_header::walk::{DataVerifier, Step, Walk};
use jeefs_header::HEADER_VERSION;

const IMG: usize = 2048;
const HDR: usize = 256;
const FHDR: usize = 28;

fn fresh_with(entries: &[(&str, u16)]) -> [u8; IMG] {
    let mut img = [0u8; IMG];
    format(&mut img, HEADER_VERSION as u8).expect("format");
    for (i, (name, size)) in entries.iter().enumerate() {
        let mut buf = [0u8; 512];
        let n = *size as usize;
        for (j, b) in buf[..n].iter_mut().enumerate() {
            *b = (i as u8 + 1).wrapping_add(j as u8);
        }
        assert_eq!(add_file(&mut img, name, &buf[..n]).unwrap(), n);
    }
    img
}

/// Drive the walker to a terminal state from an in-memory image, counting
/// the header reads the environment had to perform.
fn drive(w: &mut Walk, img: &[u8]) -> (Step, usize) {
    let mut hops = 0;
    loop {
        match w.step() {
            Step::Want { offset, len } => {
                assert_eq!(len as usize, FHDR);
                let at = offset as usize;
                assert!(at + len as usize <= img.len());
                hops += 1;
                if w.feed(&img[at..at + len as usize]).is_err() {
                    return (w.step(), hops);
                }
            }
            terminal => return (terminal, hops),
        }
    }
}

#[test]
fn found_matches_read_file() {
    let img = fresh_with(&[("alpha", 40), ("beta", 200), ("gamma", 64)]);

    let mut w = Walk::begin(&img[..HDR], IMG as u16, "beta").unwrap();
    let (state, hops) = drive(&mut w, &img);
    let found = match state {
        Step::Found(f) => f,
        other => panic!("expected Found, got {other:?}"),
    };
    assert_eq!(hops, 2, "alpha, then beta");

    let mut expect = [0u8; 512];
    let n = read_file(&img, "beta", &mut expect).unwrap();
    assert_eq!(n, 200);
    assert_eq!(found.size, 200);
    let at = found.offset as usize;
    assert_eq!(&img[at..at + 200], &expect[..200]);

    // Stream verification in windows, the running-CRC form a bounded-RAM
    // target uses instead of holding the payload.
    let mut v = DataVerifier::new(&found);
    for chunk in img[at..at + found.size as usize].chunks(64) {
        v.update(chunk);
    }
    assert!(v.finish());
}

#[test]
fn missing_file_ends_the_chain_cleanly() {
    let img = fresh_with(&[("alpha", 10), ("beta", 20)]);

    let mut w = Walk::begin(&img[..HDR], IMG as u16, "nosuch").unwrap();
    let (state, hops) = drive(&mut w, &img);
    assert_eq!(state, Step::NotFound);
    assert_eq!(hops, 2, "a zero link ends the walk without another read");
}

#[test]
fn device_id_is_one_hop() {
    let mut img = fresh_with(&[("payload", 40), ("second", 10)]);
    add_file(&mut img, jeefs_header::DEVICE_ID_FILENAME, &[9u8; 256]).unwrap();

    let mut w = Walk::begin(&img[..HDR], IMG as u16, jeefs_header::DEVICE_ID_FILENAME).unwrap();
    let (state, hops) = drive(&mut w, &img);
    assert!(matches!(state, Step::Found(_)));
    assert_eq!(
        hops, 1,
        "the identity record always occupies the first slot"
    );
}

#[test]
fn fs_version_gates_the_walk() {
    let mut img = fresh_with(&[("alpha", 10)]);

    // 0 = no filesystem: a clean NotFound without asking for a single byte.
    img[jeefs_header::FS_VERSION_OFFSET] = 0;
    reseal_header(&mut img);
    let mut w = Walk::begin(&img[..HDR], IMG as u16, "alpha").unwrap();
    assert_eq!(w.step(), Step::NotFound);

    // An unknown layout is an error, not a guess.
    img[jeefs_header::FS_VERSION_OFFSET] = 2;
    reseal_header(&mut img);
    assert!(matches!(
        Walk::begin(&img[..HDR], IMG as u16, "alpha"),
        Err(FsError::FsVersionNotSupported)
    ));
}

#[test]
fn corruption_is_detected() {
    let mut img = fresh_with(&[("alpha", 10), ("beta", 20)]);
    img[HDR + 1] ^= 0x40; // name byte, header CRC now stale

    let mut w = Walk::begin(&img[..HDR], IMG as u16, "beta").unwrap();
    let at = match w.step() {
        Step::Want { offset, .. } => offset as usize,
        other => panic!("expected Want, got {other:?}"),
    };
    assert!(matches!(
        w.feed(&img[at..at + FHDR]),
        Err(FsError::EepromCorrupted)
    ));
}

#[test]
fn a_failed_walk_stays_failed() {
    // The C walker stores the error where it stores its terminals, so
    // jeefs_walk_want stops asking. A step-driven loop here would
    // otherwise ask for the window that just failed, forever.
    let mut img = fresh_with(&[("alpha", 10), ("beta", 20)]);
    img[HDR + 1] ^= 0x40; // name byte, header CRC now stale

    let mut w = Walk::begin(&img[..HDR], IMG as u16, "beta").unwrap();
    let at = match w.step() {
        Step::Want { offset, .. } => offset as usize,
        other => panic!("expected Want, got {other:?}"),
    };
    assert!(matches!(
        w.feed(&img[at..at + FHDR]),
        Err(FsError::EepromCorrupted)
    ));

    assert_eq!(w.step(), Step::Failed(FsError::EepromCorrupted));
    // Feeding again reports the same failure rather than resuming.
    assert!(matches!(
        w.feed(&img[at..at + FHDR]),
        Err(FsError::EepromCorrupted)
    ));
    assert_eq!(w.step(), Step::Failed(FsError::EepromCorrupted));

    // A wrong-length window is a failure of the same kind.
    let mut w2 = Walk::begin(&img[..HDR], IMG as u16, "beta").unwrap();
    assert!(matches!(w2.feed(&img[..FHDR - 1]), Err(FsError::BufferNotValid)));
    assert_eq!(w2.step(), Step::Failed(FsError::BufferNotValid));
}

#[test]
fn a_terminal_walk_ignores_further_feeding() {
    let img = fresh_with(&[("alpha", 10)]);
    let mut w = Walk::begin(&img[..HDR], IMG as u16, "alpha").unwrap();
    let found = drive(&mut w, &img).0;
    assert!(matches!(found, Step::Found(_)));
    // Found and NotFound are not failures: feeding is a no-op, as in C.
    assert!(w.feed(&img[HDR..HDR + FHDR]).is_ok());
    assert_eq!(w.step(), found);
}

#[test]
fn found_stops_before_a_corrupt_tail() {
    // The walker certifies the chain prefix up to the match and stops
    // there — unlike read_file, which validates the rest of the chain.
    let mut img = fresh_with(&[("alpha", 10), ("beta", 20)]);
    let second = HDR + FHDR + 10;
    img[second + 1] ^= 0x40; // corrupt "beta"'s header, after the match

    let mut w = Walk::begin(&img[..HDR], IMG as u16, "alpha").unwrap();
    let (state, hops) = drive(&mut w, &img);
    assert!(matches!(state, Step::Found(_)));
    assert_eq!(hops, 1);
    // read_file walks the whole chain and therefore does see the damage
    assert!(matches!(
        read_file(&img, "alpha", &mut [0u8; 64]),
        Err(FsError::EepromCorrupted)
    ));
}

#[test]
fn begin_validates_its_input() {
    let img = fresh_with(&[("alpha", 10)]);

    assert!(matches!(
        Walk::begin(&img[..8], IMG as u16, "alpha"),
        Err(FsError::BufferNotValid)
    ));
    assert!(matches!(
        Walk::begin(&img[..HDR], IMG as u16, ""),
        Err(FsError::FileNameNotValid)
    ));
    assert!(matches!(
        Walk::begin(&img[..HDR], IMG as u16, "name-way-too-long"),
        Err(FsError::FileNameNotValid)
    ));
    // The target is caller-supplied, so it faces the printable-ASCII
    // domain the FS operations apply (#116).
    assert!(matches!(
        Walk::begin(&img[..HDR], IMG as u16, "a\u{ff}"),
        Err(FsError::FileNameNotValid)
    ));
    assert!(matches!(
        Walk::begin(&[0xABu8; 16], IMG as u16, "alpha"),
        Err(FsError::EepromCorrupted)
    ));
}

#[test]
fn feed_rejects_a_wrong_length_window() {
    let img = fresh_with(&[("alpha", 10)]);
    let mut w = Walk::begin(&img[..HDR], IMG as u16, "alpha").unwrap();
    assert!(matches!(
        w.feed(&img[HDR..HDR + FHDR - 1]),
        Err(FsError::BufferNotValid)
    ));
}

#[test]
fn an_image_with_no_room_for_a_file_header_ends_clean() {
    let mut small = [0u8; HDR + 8];
    format(&mut small, HEADER_VERSION as u8).unwrap();
    let w = Walk::begin(&small[..HDR], small.len() as u16, "alpha").unwrap();
    assert_eq!(w.step(), Step::NotFound);
}

#[test]
fn the_verifier_rejects_a_wrong_payload() {
    let img = fresh_with(&[("alpha", 40)]);
    let mut w = Walk::begin(&img[..HDR], IMG as u16, "alpha").unwrap();
    let found = match drive(&mut w, &img).0 {
        Step::Found(f) => f,
        other => panic!("expected Found, got {other:?}"),
    };

    let at = found.offset as usize;
    let mut payload = [0u8; 40];
    payload.copy_from_slice(&img[at..at + 40]);
    payload[0] ^= 0xFF;

    let mut v = DataVerifier::new(&found);
    v.update(&payload);
    assert!(!v.finish());
}

fn reseal_header(img: &mut [u8]) {
    assert!(jeefs_header::update_crc(img));
}

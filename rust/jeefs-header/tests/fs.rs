// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Allocation-free filesystem CRUD over a caller-owned image buffer —
//! the Rust port of src/jeefs.c (#99). Every rule here is the C core's
//! rule; byte-level conformance is locked separately by the shared
//! mutation vectors in tests/cross-language.

use jeefs_header::fs::{
    add_file, delete_file, files, format, header_check_consistency, read_file, write_file, FsError,
};
use jeefs_header::{detect_version, header_is_empty, DEVICE_ID_FILENAME, FS_VERSION_OFFSET, HEADER_VERSION};

const IMG: usize = 8192;
const FHDR: usize = 28;
const HDR: usize = 256;

fn fresh() -> [u8; IMG] {
    let mut img = [0u8; IMG];
    format(&mut img, HEADER_VERSION as u8).expect("format");
    img
}

fn names(image: &[u8]) -> Vec<String> {
    files(image)
        .expect("iter")
        .map(|e| e.expect("entry").name_str().to_string())
        .collect()
}

fn read_owned(image: &[u8], name: &str) -> Vec<u8> {
    let mut buf = [0u8; IMG];
    let n = read_file(image, name, &mut buf).expect("read");
    buf[..n].to_vec()
}

#[test]
fn add_then_read_roundtrips() {
    let mut img = fresh();
    assert_eq!(add_file(&mut img, "alpha", b"hello").unwrap(), 5);
    assert_eq!(add_file(&mut img, "beta", &[7u8; 100]).unwrap(), 100);

    assert_eq!(names(&img), vec!["alpha", "beta"]);
    assert_eq!(read_owned(&img, "alpha"), b"hello");
    assert_eq!(read_owned(&img, "beta"), vec![7u8; 100]);
}

#[test]
fn chain_is_contiguous_and_terminated() {
    let mut img = fresh();
    add_file(&mut img, "one", b"12345").unwrap();
    add_file(&mut img, "two", b"XY").unwrap();

    // one: header at HDR, data 5 bytes -> two starts at HDR+28+5
    let second = HDR + FHDR + 5;
    assert_eq!(u16::from_le_bytes([img[HDR + 22], img[HDR + 23]]) as usize, second);
    // the last file terminates the chain
    assert_eq!(u16::from_le_bytes([img[second + 22], img[second + 23]]), 0);
}

#[test]
fn device_id_takes_the_first_slot() {
    let mut img = fresh();
    add_file(&mut img, "payload", &[1u8; 40]).unwrap();
    add_file(&mut img, "second", &[2u8; 10]).unwrap();
    add_file(&mut img, DEVICE_ID_FILENAME, &[9u8; 256]).unwrap();

    assert_eq!(names(&img), vec![DEVICE_ID_FILENAME, "payload", "second"]);
    assert_eq!(read_owned(&img, DEVICE_ID_FILENAME), vec![9u8; 256]);
    assert_eq!(read_owned(&img, "payload"), vec![1u8; 40]);
    assert_eq!(read_owned(&img, "second"), vec![2u8; 10]);
}

#[test]
fn delete_compacts_the_chain() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 30]).unwrap();
    add_file(&mut img, "b", &[2u8; 50]).unwrap();
    add_file(&mut img, "c", &[3u8; 20]).unwrap();

    delete_file(&mut img, "b").unwrap();

    assert_eq!(names(&img), vec!["a", "c"]);
    assert_eq!(read_owned(&img, "c"), vec![3u8; 20]);
    // "c" moved down into b's slot; the freed span at the end is wiped
    let end = HDR + (FHDR + 30) + (FHDR + 20);
    assert!(img[end..end + FHDR + 50].iter().all(|&b| b == 0));
}

#[test]
fn delete_last_file_terminates_predecessor() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 30]).unwrap();
    add_file(&mut img, "b", &[2u8; 50]).unwrap();

    delete_file(&mut img, "b").unwrap();

    assert_eq!(names(&img), vec!["a"]);
    assert_eq!(u16::from_le_bytes([img[HDR + 22], img[HDR + 23]]), 0);
    let freed = HDR + FHDR + 30;
    assert!(img[freed..freed + FHDR + 50].iter().all(|&b| b == 0));
}

#[test]
fn delete_missing_file_reports_not_found() {
    let mut img = fresh();
    add_file(&mut img, "a", b"x").unwrap();
    assert!(matches!(delete_file(&mut img, "ghost"), Err(FsError::FileNotFound)));
}

#[test]
fn write_same_size_updates_in_place() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 20]).unwrap();
    add_file(&mut img, "b", &[2u8; 20]).unwrap();

    assert_eq!(write_file(&mut img, "a", &[8u8; 20]).unwrap(), 20);

    assert_eq!(names(&img), vec!["a", "b"]);
    assert_eq!(read_owned(&img, "a"), vec![8u8; 20]);
    assert_eq!(read_owned(&img, "b"), vec![2u8; 20]);
}

#[test]
fn write_different_size_relocates_the_file() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 20]).unwrap();
    add_file(&mut img, "b", &[2u8; 20]).unwrap();

    assert_eq!(write_file(&mut img, "a", &[8u8; 60]).unwrap(), 60);

    assert_eq!(names(&img), vec!["b", "a"]);
    assert_eq!(read_owned(&img, "a"), vec![8u8; 60]);
    assert_eq!(read_owned(&img, "b"), vec![2u8; 20]);
}

#[test]
fn write_keeps_the_file_when_space_runs_out() {
    let mut small = [0u8; HDR + FHDR + 100];
    format(&mut small, HEADER_VERSION as u8).unwrap();
    add_file(&mut small, "a", &[1u8; 50]).unwrap();
    let before = small;

    // 256 header + 28 file header + 101 payload = 385 > 384 bytes
    assert!(matches!(
        write_file(&mut small, "a", &[9u8; 101]),
        Err(FsError::NotEnoughSpace)
    ));
    assert_eq!(small, before, "a failed write must not touch the buffer");
    assert_eq!(read_owned(&small, "a"), vec![1u8; 50]);
}

#[test]
fn add_claims_a_headerless_image() {
    let mut img = [0xAAu8; IMG]; // garbage, no magic
    assert_eq!(add_file(&mut img, "first", b"data").unwrap(), 4);

    assert_eq!(detect_version(&img), Some(HEADER_VERSION as u8));
    assert_eq!(header_is_empty(&img), Some(true));
    assert!(header_check_consistency(&img));
    assert_eq!(names(&img), vec!["first"]);
    assert_eq!(read_owned(&img, "first"), b"data");
}

#[test]
fn add_never_claims_a_matching_magic_with_unknown_version() {
    let mut img = [0u8; IMG];
    img[..8].copy_from_slice(b"JETHOME\0");
    img[8] = 99; // header of some future format
    let before = img;

    assert!(matches!(add_file(&mut img, "x", b"y"), Err(FsError::EepromCorrupted)));
    assert_eq!(img, before);
}

#[test]
fn add_out_of_space_leaves_the_buffer_untouched() {
    let mut small = [0u8; HDR + FHDR + 10];
    format(&mut small, HEADER_VERSION as u8).unwrap();
    let before = small;

    assert!(matches!(add_file(&mut small, "a", &[1u8; 40]), Err(FsError::NotEnoughSpace)));
    assert_eq!(small, before);
}

#[test]
fn add_existing_name_is_refused() {
    let mut img = fresh();
    add_file(&mut img, "a", b"first").unwrap();
    let before = img;

    assert!(matches!(add_file(&mut img, "a", b"second"), Err(FsError::FileExists)));
    assert_eq!(img, before);
    assert_eq!(read_owned(&img, "a"), b"first");
}

#[test]
fn names_are_validated() {
    let mut img = fresh();
    assert!(matches!(add_file(&mut img, "", b"x"), Err(FsError::FileNameNotValid)));
    assert!(matches!(
        add_file(&mut img, "0123456789abcdef", b"x"),
        Err(FsError::FileNameNotValid)
    ));
    assert!(matches!(read_file(&img, "", &mut [0u8; 4]), Err(FsError::FileNameNotValid)));
    // exactly 15 characters is the limit and must be accepted
    assert_eq!(add_file(&mut img, "0123456789abcde", b"x").unwrap(), 1);
}

#[test]
fn payload_sizes_are_validated() {
    let mut img = fresh();
    assert!(matches!(add_file(&mut img, "a", b""), Err(FsError::BufferNotValid)));
    assert!(matches!(
        add_file(&mut img, "a", &[0u8; 32768]),
        Err(FsError::BufferNotValid)
    ));
}

#[test]
fn read_into_a_short_buffer_is_refused() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 40]).unwrap();
    assert!(matches!(
        read_file(&img, "a", &mut [0u8; 39]),
        Err(FsError::BufferNotValid)
    ));
}

#[test]
fn read_detects_a_data_crc_mismatch() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 40]).unwrap();
    img[HDR + FHDR] ^= 0xFF; // flip a payload byte

    assert!(matches!(read_file(&img, "a", &mut [0u8; 64]), Err(FsError::EepromCorrupted)));
    // listing still walks: the header itself is intact
    assert_eq!(names(&img), vec!["a"]);
}

#[test]
fn a_corrupt_file_header_stops_the_walk() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 40]).unwrap();
    img[HDR + 2] ^= 0xFF; // flip a name byte, headerCrc32 no longer matches

    assert!(matches!(files(&img).unwrap().next(), Some(Err(FsError::EepromCorrupted))));
    assert!(matches!(read_file(&img, "a", &mut [0u8; 64]), Err(FsError::EepromCorrupted)));
}

#[test]
fn fs_version_gates_the_file_area() {
    let mut img = fresh();
    add_file(&mut img, "a", b"data").unwrap();

    // 0 = no filesystem: the area reads empty regardless of content
    img[FS_VERSION_OFFSET] = 0;
    assert!(names(&img).is_empty());
    assert!(matches!(read_file(&img, "a", &mut [0u8; 8]), Err(FsError::FileNotFound)));

    // anything else = a layout this build does not know
    img[FS_VERSION_OFFSET] = 7;
    assert!(matches!(files(&img), Err(FsError::FsVersionNotSupported)));
    assert!(matches!(
        read_file(&img, "a", &mut [0u8; 8]),
        Err(FsError::FsVersionNotSupported)
    ));
}

#[test]
fn add_stamps_the_fs_version_and_reseals_the_header() {
    let mut img = fresh();
    img[FS_VERSION_OFFSET] = 0;
    jeefs_header::update_crc(&mut img[..HDR]);

    add_file(&mut img, "a", b"data").unwrap();

    assert_eq!(img[FS_VERSION_OFFSET], 1);
    assert!(header_check_consistency(&img));
}

#[test]
fn format_wipes_the_file_area_and_stamps_the_version() {
    let mut img = [0xEEu8; IMG];
    format(&mut img, HEADER_VERSION as u8).unwrap();

    assert_eq!(detect_version(&img), Some(HEADER_VERSION as u8));
    assert_eq!(img[FS_VERSION_OFFSET], 1);
    assert!(header_check_consistency(&img));
    assert!(img[HDR..].iter().all(|&b| b == 0));
    assert!(names(&img).is_empty());
}

#[test]
fn operations_on_a_short_buffer_are_refused() {
    let tiny = [0u8; 4];
    assert!(matches!(files(&tiny), Err(FsError::EepromCorrupted)));
    assert!(matches!(
        read_file(&tiny, "a", &mut [0u8; 4]),
        Err(FsError::EepromCorrupted)
    ));
    let mut tiny_mut = [0u8; 4];
    assert!(matches!(
        add_file(&mut tiny_mut, "a", b"x"),
        Err(FsError::NotEnoughSpace)
    ));
}

#[test]
fn an_erased_link_ends_the_chain() {
    let mut img = fresh();
    add_file(&mut img, "a", &[1u8; 16]).unwrap();
    add_file(&mut img, "b", &[2u8; 16]).unwrap();

    // erase the first file's link the way an unlinked write on erased media would
    img[HDR + 22] = 0xFF;
    img[HDR + 23] = 0xFF;
    let crc = crc32fast::hash(&img[HDR..HDR + 24]);
    img[HDR + 24..HDR + 28].copy_from_slice(&crc.to_le_bytes());

    assert_eq!(names(&img), vec!["a"]);
}

#[test]
fn an_image_past_the_16_bit_address_space_is_refused() {
    // The chain links are uint16_t: anything beyond 64 KiB is unaddressable.
    let mut huge = vec![0u8; 65536 + 1];
    assert!(matches!(
        format(&mut huge, HEADER_VERSION as u8),
        Err(FsError::BufferNotValid)
    ));
    assert!(matches!(files(&huge), Err(FsError::BufferNotValid)));
    assert!(matches!(add_file(&mut huge, "a", b"x"), Err(FsError::BufferNotValid)));
}

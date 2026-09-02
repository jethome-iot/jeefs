// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//! Whole-image build/parse conformance tests. The rules mirror the C
//! reader (src/jeefs.c) and the Python port (python/jeefs/image.py);
//! byte-identity with the committed goldens is locked by the ctest
//! bin verify_image_build_rs.

use jeefs_header::image::{build_image, parse_image, ImageError};
use jeefs_header::{header_size, initialize_header, update_crc, DEVICE_ID_FILENAME};

const HDR: usize = 256;
const FHDR: usize = 28;

fn v4_header() -> Vec<u8> {
    let mut h = vec![0u8; HDR];
    assert!(initialize_header(&mut h, 4));
    h[12..17].copy_from_slice(b"board");
    assert!(update_crc(&mut h));
    h
}

fn crc(data: &[u8]) -> u32 {
    crc32fast::hash(data)
}

#[test]
fn roundtrip_with_device_id_first() {
    let files: Vec<(&str, &[u8])> = vec![
        ("config", b"abc"),
        (DEVICE_ID_FILENAME, &[1u8; 256]),
        ("wifi", b"12345"),
    ];
    let img = build_image(&v4_header(), &files, 2048).unwrap();
    assert_eq!(img.len(), 2048);

    let parsed = parse_image(&img).unwrap();
    assert_eq!(parsed.version, 4);
    assert_eq!(parsed.fs_version, 1);
    let names: Vec<&str> = parsed.files.iter().map(|f| f.name.as_str()).collect();
    assert_eq!(names, [DEVICE_ID_FILENAME, "config", "wifi"]);
    assert_eq!(parsed.files[0].data, vec![1u8; 256]);
    assert_eq!(parsed.files[1].data, b"abc");
    assert!(parsed.unreadable.is_empty());
}

#[test]
fn fs_version_stamped_and_header_crc_sealed() {
    let hdr = v4_header();
    assert_eq!(hdr[10], 0); // builder stamps the copy, not the caller's buffer
    let img = build_image(&hdr, &[("a", b"x")], 1024).unwrap();
    assert_eq!(img[10], 1);
    assert_eq!(hdr[10], 0);
    let stored = u32::from_le_bytes(img[252..256].try_into().unwrap());
    assert_eq!(stored, crc(&img[..252]));
}

#[test]
fn chain_layout_contiguous_le() {
    let img = build_image(&v4_header(), &[("a", b"xy"), ("b", b"z")], 1024).unwrap();
    assert_eq!(
        u16::from_le_bytes(img[HDR + 16..HDR + 18].try_into().unwrap()),
        2
    );
    assert_eq!(
        u16::from_le_bytes(img[HDR + 22..HDR + 24].try_into().unwrap()),
        (HDR + FHDR + 2) as u16
    );
    assert_eq!(
        u32::from_le_bytes(img[HDR + 24..HDR + 28].try_into().unwrap()),
        crc(&img[HDR..HDR + 24])
    );
}

#[test]
fn build_rejections() {
    let h = v4_header();
    let long = "abcdefghijklmnop";
    assert!(matches!(
        build_image(&h, &[("", b"x")], 512),
        Err(ImageError::BadName)
    ));
    assert!(matches!(
        build_image(&h, &[(long, b"x")], 512),
        Err(ImageError::BadName)
    ));
    assert!(matches!(
        build_image(&h, &[("a\0b", b"x")], 512),
        Err(ImageError::BadName)
    ));
    assert!(matches!(
        build_image(&h, &[("a", b"")], 512),
        Err(ImageError::BadData)
    ));
    let big = vec![0u8; 32768];
    assert!(matches!(
        build_image(&h, &[("a", &big)], 65535),
        Err(ImageError::BadData)
    ));
    assert!(matches!(
        build_image(&h, &[("a", b"x"), ("a", b"y")], 512),
        Err(ImageError::DuplicateName)
    ));
    let d300 = vec![0u8; 300];
    assert!(matches!(
        build_image(&h, &[("a", &d300)], 512),
        Err(ImageError::Capacity)
    ));
    assert!(matches!(
        build_image(&h, &[], 255),
        Err(ImageError::Capacity)
    ));
    assert!(matches!(
        build_image(&h, &[], 65536),
        Err(ImageError::Capacity)
    ));
    assert!(matches!(
        build_image(&[0u8; 256], &[], 512),
        Err(ImageError::NoHeader)
    ));
}

#[test]
fn header_crc_gate_runs_before_anything() {
    let mut img = build_image(&v4_header(), &[("a", b"x")], 512).unwrap();
    img[9] = 7; // invalid signature_version AND stale CRC
    assert!(matches!(parse_image(&img), Err(ImageError::HeaderCrc)));
    let mut img2 = build_image(&v4_header(), &[("a", b"x")], 512).unwrap();
    img2[100] ^= 1; // identity byte
    assert!(matches!(parse_image(&img2), Err(ImageError::HeaderCrc)));
}

#[test]
fn fs_version_gate() {
    let mut img = build_image(&v4_header(), &[("a", b"x")], 512).unwrap();
    img[10] = 0;
    let c = crc(&img[..252]).to_le_bytes();
    img[252..256].copy_from_slice(&c);
    assert!(parse_image(&img).unwrap().files.is_empty());
    img[10] = 2;
    let c = crc(&img[..252]).to_le_bytes();
    img[252..256].copy_from_slice(&c);
    assert!(matches!(parse_image(&img), Err(ImageError::FsVersion(2))));
}

#[test]
fn no_header_rejected() {
    assert!(matches!(
        parse_image(&[0u8; 512]),
        Err(ImageError::NoHeader)
    ));
    assert!(matches!(
        parse_image(&[0xFFu8; 512]),
        Err(ImageError::NoHeader)
    ));
    assert!(matches!(parse_image(b"short"), Err(ImageError::NoHeader)));
}

#[test]
fn corrupted_file_header_rejected() {
    let mut img = build_image(&v4_header(), &[("a", b"x")], 512).unwrap();
    img[HDR + 1] ^= 0x40; // name byte, file headerCrc32 stale
    assert!(matches!(parse_image(&img), Err(ImageError::Chain(_))));
}

#[test]
fn bad_payloads_reported_not_fatal() {
    // data-CRC mismatch: the walk continues, exactly like the C iterator
    let mut img = build_image(&v4_header(), &[("a", b"xyz"), ("b", b"ok")], 512).unwrap();
    img[HDR + FHDR] ^= 0xFF;
    let parsed = parse_image(&img).unwrap();
    let names: Vec<&str> = parsed.files.iter().map(|f| f.name.as_str()).collect();
    assert_eq!(names, ["a", "b"]);
    assert_eq!(parsed.unreadable, ["a"]);

    // dataSize above INT16_MAX walks fine but is unreadable per ReadFile
    let mut img = build_image(&v4_header(), &[("big", &[7u8; 100])], 40_384).unwrap();
    img[HDR + 16..HDR + 18].copy_from_slice(&40_000u16.to_le_bytes());
    img[HDR + 22..HDR + 24].copy_from_slice(&0u16.to_le_bytes());
    let hc = crc(&img[HDR..HDR + 24]).to_le_bytes();
    img[HDR + 24..HDR + 28].copy_from_slice(&hc);
    assert_eq!(parse_image(&img).unwrap().unreadable, ["big"]);
}

#[test]
fn byte_transparent_names() {
    // the wire constrains only length and terminator — a 0xE9 name byte
    // must round out as the latin-1 char, matching the Python port
    let mut img = build_image(&v4_header(), &[("a", b"x")], 512).unwrap();
    img[HDR] = 0xE9;
    let hc = crc(&img[HDR..HDR + 24]).to_le_bytes();
    img[HDR + 24..HDR + 28].copy_from_slice(&hc);
    let parsed = parse_image(&img).unwrap();
    assert_eq!(parsed.files[0].name, "\u{e9}");
}

#[test]
fn erased_tail_is_clean_end() {
    let mut img = build_image(&v4_header(), &[("a", b"x")], 1024).unwrap();
    let tail = HDR + FHDR + 1;
    for b in img[tail..].iter_mut() {
        *b = 0xFF;
    }
    let parsed = parse_image(&img).unwrap();
    assert_eq!(parsed.files.len(), 1);
}

#[test]
fn v1_header_supported_for_parse_offsets() {
    // build accepts any detected header version; v1 shifts the chain to 512
    let mut h1 = vec![0u8; 512];
    assert!(initialize_header(&mut h1, 1));
    let img = build_image(&h1, &[("f", b"d")], 1024).unwrap();
    assert_eq!(header_size(1).unwrap(), 512);
    let parsed = parse_image(&img).unwrap();
    assert_eq!(parsed.version, 1);
    assert_eq!(parsed.files[0].name, "f");
}

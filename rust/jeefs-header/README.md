# jeefs-header

Rust implementation of the JEEFS EEPROM format used across JetHome
devices: board identity headers, the signed `device.id` record, the
filesystem, and whole-image build/parse.

The crate performs no I/O and, in its default `no_std` form, no
allocation: the environment reads the EEPROM and hands every operation a
caller-owned byte slice.

```bash
cargo add jeefs-header                          # host tooling (std)
cargo add jeefs-header --no-default-features    # firmware (bare no_std)
```

## What is in it

| Module | Contents | Needs |
|--------|----------|-------|
| `header` | detect version, verify/update CRC, initialize, `header_is_empty` | nothing |
| `generated` | packed structs for headers v1-v4 and `DeviceIdentityV1`, with little-endian accessors | nothing |
| `devid` | the `device.id` record: parse, build, verify | nothing |
| `fs` | filesystem over a caller buffer: `format`, `files`, `read_file`, `add_file`, `write_file`, `delete_file` | nothing |
| `image` | whole-image `build_image` / `parse_image` | `alloc` |

Features: `std` (default, implies `alloc`), `alloc`, `bins` (the
cross-language test executables).

## Reading a header

```rust
use jeefs_header::{detect_version, verify_crc, JeepromHeaderV4};

let version = detect_version(&image).expect("no JEEFS header");
assert!(verify_crc(&image[..256]));
let hdr = JeepromHeaderV4::from_bytes(&image).unwrap();
println!("{} / {}", hdr.boardname_str(), hdr.board_serial_str());
```

## Editing files in place, without a heap

```rust
use jeefs_header::fs::{add_file, files, read_file};

add_file(&mut image, "wifi.conf", b"ssid=jethome")?;
for entry in files(&image)? {
    let entry = entry?;
    println!("{} ({} bytes)", entry.name_str(), entry.size());
}
let mut buf = [0u8; 256];
let n = read_file(&image, "device.id", &mut buf)?;
# Ok::<(), jeefs_header::fs::FsError>(())
```

The reserved `device.id` always takes the first slot, so a bootloader
reads the whole device identity from a bounded 540-byte prefix. An image
with no header is claimed on first write; a failed operation leaves the
buffer untouched.

## Conformance

The format lives in [`docs/format/`](https://github.com/jethome-iot/jeefs/tree/master/docs/format);
these structures are generated from it. The port is held to the C
implementation by a cross-language test matrix, golden images, and
shared filesystem mutation vectors — the same scenarios run through both
and must produce identical bytes.

Embedding notes for firmware: [`docs/PORTING.md`](https://github.com/jethome-iot/jeefs/blob/master/docs/PORTING.md).

Dual-licensed GPL-2.0-or-later / Apache-2.0.

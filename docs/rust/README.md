# JEEFS Header — Rust Crate

Rust library for parsing JEEFS EEPROM headers (v1/v2/v3). `no_std`-compatible core with `std` file I/O in examples.

## Integration

### Cargo.toml

```toml
[dependencies]
jeefs-header = { path = "path/to/jeefs/rust/jeefs-header" }
```

Or when published:

```toml
[dependencies]
jeefs-header = "0.1"
```

## Dependencies

- `crc32fast` — CRC32 calculation
- Rust 2021 edition

## Building

```bash
cd rust/jeefs-header
cargo build
cargo test        # 12 unit tests
cargo run --example read_header -- path/to/eeprom.bin
```

## API

### Functions (`jeefs_header::header`)

```rust
use jeefs_header::header::{detect_version, header_size, verify_crc, update_crc, initialize_header};
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `detect_version` | `fn(data: &[u8]) -> Option<u8>` | Detect version (1/2/3) from magic + version byte |
| `header_size` | `fn(version: u8) -> Option<usize>` | Header size: 512 (v1), 256 (v2/v3) |
| `verify_crc` | `fn(data: &[u8]) -> bool` | Verify stored CRC32 |
| `update_crc` | `fn(data: &mut [u8]) -> bool` | Recalculate and write CRC32 |
| `initialize_header` | `fn(version: u8) -> Option<Vec<u8>>` | Create zeroed header with magic + CRC |

### Structs (`jeefs_header::generated`)

```rust
use jeefs_header::generated::{JeepromHeaderV1, JeepromHeaderV2, JeepromHeaderV3};
```

All structs are `#[repr(C, packed)]` with zero-copy `from_bytes()`:

```rust
let hdr = JeepromHeaderV3::from_bytes(&data).unwrap();
println!("Board: {}", hdr.boardname_str());
println!("MAC: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
    hdr.mac[0], hdr.mac[1], hdr.mac[2],
    hdr.mac[3], hdr.mac[4], hdr.mac[5]);
```

#### String accessors

Each header struct provides `_str()` methods that return `&str`:

- `boardname_str()`
- `boardversion_str()`
- `serial_str()`
- `usid_str()`
- `cpuid_str()`

#### V3-specific

```rust
let algo = hdr.signature_algorithm();  // Result<SignatureAlgorithm, u8>
```

### Enum (`SignatureAlgorithm`)

```rust
use jeefs_header::generated::SignatureAlgorithm;

SignatureAlgorithm::NONE       // 0
SignatureAlgorithm::SECP192R1  // 1, signature_size() = 48
SignatureAlgorithm::SECP256R1  // 2, signature_size() = 64

SignatureAlgorithm::from_u8(byte)  // -> Result<Self, u8>
```

### Example: create and modify a header

```rust
use jeefs_header::header::{initialize_header, update_crc};
use jeefs_header::generated::JeepromHeaderV3;

let mut buf = initialize_header(3).unwrap();

// Modify fields via raw bytes (boardname at offset 12)
let name = b"MyBoard\0";
buf[12..12 + name.len()].copy_from_slice(name);

// Set MAC (offset 172)
buf[172..178].copy_from_slice(&[0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);

update_crc(&mut buf);
assert!(jeefs_header::header::verify_crc(&buf));
```

## Usage example

See [examples/rust/read_header.rs](../../examples/rust/read_header.rs) — reads an EEPROM binary, prints version, CRC status, board name, MAC address, and signature algorithm.

## Testing

```bash
cd rust/jeefs-header
cargo test    # 12 unit tests
```

## Format specification

- [Header common properties](../format/header-common.md)
- [Header v1](../format/header-v1.md) / [v2](../format/header-v2.md) / [v3](../format/header-v3.md)
- [Full format spec](../../EEPROM_FORMAT.md)

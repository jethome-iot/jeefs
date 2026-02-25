# JEEFS Header — Python Package

Pure Python library for parsing and generating JEEFS EEPROM headers. The main API class `EEPROMHeaderV3` handles v3 headers; constants and field offsets for all versions (v1/v2/v3) are available via `jeefs.constants`. No native dependencies.

## Installation

```bash
pip install jeefs
```

## Dependencies

- Python >= 3.10
- No external runtime dependencies (uses stdlib `struct`, `binascii`)

## Building / Development

```bash
cd python
uv venv .venv && uv pip install -e ".[test]"
.venv/bin/python -m pytest tests/ -v
```

## API

### `EEPROMHeaderV3` — main dataclass

```python
from jeefs.header import EEPROMHeaderV3
```

#### Creating a header

```python
header = EEPROMHeaderV3(
    boardname="JetHub-D1p",
    boardversion="2.0",
    serial="SN-2024-001",
    mac="F0:57:8D:01:02:03",
)
binary = header.to_bytes()  # 256-byte binary
```

#### Parsing from binary

```python
header = EEPROMHeaderV3.from_bytes(raw_data)
print(header.boardname)   # "JetHub-D1p"
print(header.mac)         # "F0:57:8D:01:02:03"
print(header.serial)      # "SN-2024-001"
```

#### CRC verification

```python
# Instance method
header = EEPROMHeaderV3.from_bytes(data)
is_valid = header.verify_crc(data)

# Static method (no instance needed)
is_valid = EEPROMHeaderV3.verify_crc_static(data)
```

#### Validation

```python
errors = header.validate()
if errors:
    print("Invalid:", errors)
```

#### Partition image

```python
image = header.to_partition_image()  # 4096-byte flash image
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `boardname` | `str` | Board name (max 31 chars) |
| `boardversion` | `str` | Board version (max 31 chars) |
| `serial` | `str` | Serial number (max 31 chars) |
| `usid` | `str` | CPU eFuse USID |
| `cpuid` | `str` | CPU ID |
| `mac` | `str` | MAC address ("AA:BB:CC:DD:EE:FF") |
| `signature` | `bytes` | ECDSA signature (raw r\|\|s) |
| `signature_algorithm` | `SignatureAlgorithm` | NONE / SECP192R1 / SECP256R1 |
| `timestamp` | `int \| None` | Unix timestamp (auto-set if None/0) |

### `SignatureAlgorithm` enum

```python
from jeefs.constants import SignatureAlgorithm

SignatureAlgorithm.NONE       # 0
SignatureAlgorithm.SECP192R1  # 1 (48-byte signature)
SignatureAlgorithm.SECP256R1  # 2 (64-byte signature)
```

### Constants

```python
from jeefs.constants import (
    EEPROM_MAGIC,          # b"JETHOME\x00"
    EEPROM_HEADER_SIZE,    # 256
    EEPROM_FIELDS,         # dict of {field: (offset, size)}
    EEPROM_CRC_COVERAGE,   # 252
    EEPROM_PARTITION_SIZE,  # 4096
)
```

## Usage example

See [examples/python/read_header.py](https://github.com/jethome-iot/jeefs/blob/master/examples/python/read_header.py) — reads an EEPROM binary, prints version, CRC status, board name, MAC address, and full v3 fields.

## Testing

```bash
cd python
python -m pytest tests/ -v   # 58 tests
```

## Format specification

- [Header common properties](https://github.com/jethome-iot/jeefs/blob/master/docs/format/header-common.md)
- [Header v3](https://github.com/jethome-iot/jeefs/blob/master/docs/format/header-v3.md)
- [Full format spec](https://github.com/jethome-iot/jeefs/blob/master/EEPROM_FORMAT.md)

# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Unit tests for the spec parser (issue #12).

The parser is load-bearing: a silently dropped definition removes a
struct from the generated code of three languages at once. Malformed
input must raise, never skip.
"""

from pathlib import Path

import pytest

from jeefs_codegen.parser import parse_file, parse_files

REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC_FILES = [
    REPO_ROOT / "docs/format" / name
    for name in (
        "header-common.md",
        "header-v1.md",
        "header-v2.md",
        "header-v3.md",
        "filesystem-v1.md",
    )
]

STRUCT_TABLE = """\
| Offset | Size | Field | Type | Endianness | Description |
|--------|------|-------|------|------------|-------------|
| 0 | 8 | magic | char[8] | - | Magic |
| 8 | 1 | version | uint8_t | - | Version |
| 9-11 | 3 | reserved1 | uint8_t[3] | - | Reserved |
| 12-15 | 4 | crc32 | uint32_t | LE | CRC |
"""


def write_spec(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "spec.md"
    p.write_text(body, encoding="utf-8")
    return p


def test_well_formed_struct(tmp_path):
    spec = parse_file(
        write_spec(
            tmp_path,
            "<!-- STRUCT: Demo -->\n"
            "<!-- SIZE: 16 -->\n"
            "<!-- CRC_FIELD: crc32 -->\n"
            "<!-- CRC_COVERAGE: 0-11 -->\n" + STRUCT_TABLE,
        )
    )
    assert len(spec.structs) == 1
    s = spec.structs[0]
    assert s.name == "Demo"
    assert s.total_size == 16
    assert s.crc_coverage == (0, 11)
    assert [f.name for f in s.fields] == ["magic", "version", "reserved1", "crc32"]
    assert s.fields[3].offset == 12


def test_blank_line_inside_metadata_block(tmp_path):
    # Regression: a blank line between metadata comments silently dropped
    # the whole definition.
    spec = parse_file(
        write_spec(
            tmp_path,
            "<!-- STRUCT: Demo -->\n"
            "\n"
            "<!-- SIZE: 16 -->\n" + STRUCT_TABLE,
        )
    )
    assert len(spec.structs) == 1
    assert spec.structs[0].total_size == 16


def test_metadata_without_table_raises(tmp_path):
    with pytest.raises(ValueError, match="no table"):
        parse_file(
            write_spec(tmp_path, "<!-- STRUCT: Demo -->\n<!-- SIZE: 16 -->\n\nProse.\n")
        )


def test_unknown_key_adjacent_to_table_raises(tmp_path):
    with pytest.raises(ValueError, match="[Uu]nknown metadata"):
        parse_file(
            write_spec(
                tmp_path,
                "<!-- STRUCT: Demo -->\n<!-- SIZE: 16 -->\n<!-- FOOBAR: x -->\n"
                + STRUCT_TABLE,
            )
        )


def test_prose_comment_without_table_is_ignored(tmp_path):
    spec = parse_file(
        write_spec(tmp_path, "Intro.\n\n<!-- TODO revisit -->\n\nMore prose.\n")
    )
    assert not spec.structs and not spec.enums and not spec.constants


def test_missing_size_raises(tmp_path):
    with pytest.raises(ValueError, match="SIZE"):
        parse_file(write_spec(tmp_path, "<!-- STRUCT: Demo -->\n" + STRUCT_TABLE))


def test_offset_range_end_mismatch_raises(tmp_path):
    bad = STRUCT_TABLE.replace("| 9-11 | 3 |", "| 9-12 | 3 |")
    with pytest.raises(ValueError, match="range"):
        parse_file(
            write_spec(tmp_path, "<!-- STRUCT: Demo -->\n<!-- SIZE: 16 -->\n" + bad)
        )


def test_bad_offset_raises(tmp_path):
    bad = STRUCT_TABLE.replace("| 0 | 8 |", "| x | 8 |")
    with pytest.raises(ValueError, match="offset"):
        parse_file(
            write_spec(tmp_path, "<!-- STRUCT: Demo -->\n<!-- SIZE: 16 -->\n" + bad)
        )


def test_enum_hex_values(tmp_path):
    spec = parse_file(
        write_spec(
            tmp_path,
            "<!-- ENUM: Flags -->\n"
            "| Value | Name | Description |\n"
            "|-------|------|-------------|\n"
            "| 0x00 | NONE | none |\n"
            "| 0x10 | HIGH | high |\n",
        )
    )
    assert [m.value for m in spec.enums[0].members] == [0, 16]


def test_real_specs_parse():
    spec = parse_files(SPEC_FILES)
    struct_names = {s.name for s in spec.structs}
    assert {"JEEPROMHeaderv1", "JEEPROMHeaderv2", "JEEPROMHeaderv3"} <= struct_names
    assert spec.constants, "constants tables must survive parsing"

# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Parse markdown format specification files into structured data models."""

from __future__ import annotations

import re
from pathlib import Path

from .models import (
    C_TYPE_SIZES,
    ConstantDef,
    EnumDef,
    EnumMember,
    FieldDef,
    FormatSpec,
    StructDef,
    UnionDef,
    UnionMember,
)

# Regex for HTML comment metadata: <!-- KEY: VALUE --> or bare <!-- KEY -->
_META_KV_RE = re.compile(r"<!--\s*(\w+)\s*:\s*(.+?)\s*-->")
_META_BARE_RE = re.compile(r"<!--\s*(\w+)\s*-->")

# Regex for C type with optional array: uint8_t[32], char[8], int64_t
_TYPE_RE = re.compile(r"^(\w+?)(\[(\d+)\])?$")

# Regex for offset range: "10-11" or single "8"
_OFFSET_RE = re.compile(r"^(\d+)(?:-(\d+))?$")


def _parse_table_rows(lines: list[str]) -> tuple[list[str], list[list[str]]]:
    """Extract header and data rows from markdown table lines.

    Returns (column_names, rows) where each row is a list of stripped cell values.
    """
    columns: list[str] = []
    rows: list[list[str]] = []

    for line in lines:
        line = line.strip()
        if not line.startswith("|"):
            continue

        cells = [c.strip() for c in line.split("|")]
        # Remove empty first/last from leading/trailing pipes
        if cells and cells[0] == "":
            cells = cells[1:]
        if cells and cells[-1] == "":
            cells = cells[:-1]

        if not cells:
            continue

        # Skip separator row (---|---|---)
        if all(re.match(r"^[-:]+$", c) for c in cells):
            continue

        if not columns:
            columns = [c.lower() for c in cells]
        else:
            rows.append(cells)

    return columns, rows


def _parse_struct_table(
    metadata: dict[str, str], columns: list[str], rows: list[list[str]]
) -> StructDef:
    """Parse a struct definition from table data."""
    name = metadata["STRUCT"]
    total_size = int(metadata["SIZE"])
    version = int(metadata["VERSION"]) if "VERSION" in metadata else None
    crc_field = metadata.get("CRC_FIELD")
    crc_coverage = None
    if "CRC_COVERAGE" in metadata:
        parts = metadata["CRC_COVERAGE"].split("-")
        crc_coverage = (int(parts[0]), int(parts[1]))

    # Find column indices
    col_idx = {c: i for i, c in enumerate(columns)}
    required = {"offset", "size", "field", "type"}
    missing = required - set(col_idx.keys())
    if missing:
        raise ValueError(f"Struct table '{name}' missing columns: {missing}")

    fields: list[FieldDef] = []
    for row in rows:
        if len(row) < len(columns):
            row.extend([""] * (len(columns) - len(row)))

        offset_str = row[col_idx["offset"]]
        size = int(row[col_idx["size"]])
        field_name = row[col_idx["field"]]
        type_str = row[col_idx["type"]]
        endianness = row[col_idx.get("endianness", -1)] if "endianness" in col_idx else "-"
        description = row[col_idx.get("description", -1)] if "description" in col_idx else ""

        # Parse offset
        m = _OFFSET_RE.match(offset_str)
        if not m:
            raise ValueError(f"Invalid offset '{offset_str}' in struct '{name}'")
        offset = int(m.group(1))

        # Parse type
        tm = _TYPE_RE.match(type_str)
        if not tm:
            raise ValueError(f"Invalid type '{type_str}' for field '{field_name}' in '{name}'")
        c_type = tm.group(1)
        array_size = int(tm.group(3)) if tm.group(3) else 0

        if c_type not in C_TYPE_SIZES:
            raise ValueError(f"Unknown C type '{c_type}' for field '{field_name}' in '{name}'")

        fields.append(
            FieldDef(
                name=field_name,
                offset=offset,
                size=size,
                c_type=c_type,
                array_size=array_size,
                endianness=endianness,
                description=description,
            )
        )

    return StructDef(
        name=name,
        total_size=total_size,
        version=version,
        crc_field=crc_field,
        crc_coverage=crc_coverage,
        fields=fields,
    )


def _parse_enum_table(
    metadata: dict[str, str], columns: list[str], rows: list[list[str]]
) -> EnumDef:
    """Parse an enum definition from table data."""
    name = metadata["ENUM"]
    c_prefix = metadata.get("C_PREFIX", name.upper())
    py_class = metadata.get("PY_CLASS", name)

    col_idx = {c: i for i, c in enumerate(columns)}
    if "value" not in col_idx or "name" not in col_idx:
        raise ValueError(f"Enum table '{name}' missing 'Value' or 'Name' columns")

    members: list[EnumMember] = []
    for row in rows:
        if len(row) < len(columns):
            row.extend([""] * (len(columns) - len(row)))

        value = int(row[col_idx["value"]])
        member_name = row[col_idx["name"]]
        description = row[col_idx.get("description", -1)] if "description" in col_idx else ""

        # Collect extra columns as metadata
        extra: dict[str, str] = {}
        for col_name, idx in col_idx.items():
            if col_name not in ("value", "name", "description"):
                extra[col_name] = row[idx]

        members.append(
            EnumMember(value=value, name=member_name, description=description, extra=extra)
        )

    return EnumDef(name=name, c_prefix=c_prefix, py_class=py_class, members=members)


def _parse_constants_table(columns: list[str], rows: list[list[str]]) -> list[ConstantDef]:
    """Parse a constants table."""
    col_idx = {c: i for i, c in enumerate(columns)}
    if "name" not in col_idx or "value" not in col_idx:
        raise ValueError("Constants table missing 'Name' or 'Value' columns")

    constants: list[ConstantDef] = []
    for row in rows:
        if len(row) < len(columns):
            row.extend([""] * (len(columns) - len(row)))

        name = row[col_idx["name"]]
        value = row[col_idx["value"]]
        type_ = row[col_idx.get("type", -1)] if "type" in col_idx else "int"
        desc = row[col_idx.get("description", -1)] if "description" in col_idx else ""

        constants.append(ConstantDef(name=name, value=value, type=type_, description=desc))

    return constants


def _parse_union_table(
    metadata: dict[str, str], columns: list[str], rows: list[list[str]]
) -> UnionDef:
    """Parse a union definition from table data."""
    name = metadata["UNION"]
    col_idx = {c: i for i, c in enumerate(columns)}

    members: list[UnionMember] = []
    for row in rows:
        if len(row) < len(columns):
            row.extend([""] * (len(columns) - len(row)))

        member_name = row[col_idx.get("member", 0)]
        type_name = row[col_idx.get("type", 1)]
        description = row[col_idx.get("description", 2)] if "description" in col_idx else ""

        members.append(UnionMember(name=member_name, type_name=type_name, description=description))

    return UnionDef(name=name, members=members)


def parse_file(path: Path) -> FormatSpec:
    """Parse a single markdown file and extract all definitions.

    A file can contain multiple structs, enums, constants tables, and unions.
    Each definition is preceded by HTML comment metadata.
    """
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    spec = FormatSpec()

    # Split file into sections: each section starts with metadata comments
    # followed by a markdown table
    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Collect metadata comments
        metadata: dict[str, str] = {}
        while i < len(lines):
            stripped_line = lines[i].strip()
            m = _META_KV_RE.match(stripped_line)
            m_bare = _META_BARE_RE.match(stripped_line) if not m else None
            if m:
                metadata[m.group(1)] = m.group(2)
                i += 1
            elif m_bare:
                # Bare metadata like <!-- CONSTANTS -->
                metadata[m_bare.group(1)] = ""
                i += 1
            elif metadata:
                # We have metadata, now look for the table
                break
            else:
                i += 1
                break

        if not metadata:
            continue

        # Collect table lines (consecutive lines starting with |)
        table_lines: list[str] = []
        while i < len(lines):
            stripped = lines[i].strip()
            if stripped.startswith("|"):
                table_lines.append(stripped)
                i += 1
            elif stripped == "" and not table_lines:
                # Skip blank lines between metadata and table
                i += 1
            elif stripped == "" and table_lines:
                # Blank line after table = end of table
                break
            elif not stripped.startswith("|") and table_lines:
                break
            else:
                i += 1
                break

        if not table_lines:
            continue

        columns, rows = _parse_table_rows(table_lines)

        # Dispatch based on metadata type
        if "STRUCT" in metadata:
            spec.structs.append(_parse_struct_table(metadata, columns, rows))
        elif "ENUM" in metadata:
            spec.enums.append(_parse_enum_table(metadata, columns, rows))
        elif "CONSTANTS" in metadata:
            spec.constants.extend(_parse_constants_table(columns, rows))
        elif "UNION" in metadata:
            spec.unions.append(_parse_union_table(metadata, columns, rows))

    return spec


def parse_files(paths: list[Path]) -> FormatSpec:
    """Parse multiple markdown files and merge into a single FormatSpec."""
    merged = FormatSpec()
    for path in paths:
        spec = parse_file(path)
        merged.structs.extend(spec.structs)
        merged.enums.extend(spec.enums)
        merged.constants.extend(spec.constants)
        merged.unions.extend(spec.unions)
    return merged

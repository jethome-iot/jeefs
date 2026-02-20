"""Validate parsed format specifications for consistency."""

from __future__ import annotations

from .models import C_TYPE_SIZES, FormatSpec, StructDef


def validate_struct(s: StructDef) -> list[str]:
    """Validate a single struct definition. Returns list of error messages."""
    errors: list[str] = []

    if not s.fields:
        errors.append(f"{s.name}: no fields defined")
        return errors

    # Check field ordering and coverage
    covered = [False] * s.total_size
    for f in s.fields:
        # Validate type size
        base_size = C_TYPE_SIZES.get(f.c_type)
        if base_size is None:
            errors.append(f"{s.name}.{f.name}: unknown C type '{f.c_type}'")
            continue

        expected_size = base_size * f.array_size if f.array_size > 0 else base_size
        if f.size != expected_size:
            errors.append(
                f"{s.name}.{f.name}: size mismatch: "
                f"declared {f.size}B but type {f.c_type}"
                f"{'[' + str(f.array_size) + ']' if f.array_size else ''} = {expected_size}B"
            )

        # Check offset bounds
        if f.offset < 0 or f.offset + f.size > s.total_size:
            errors.append(
                f"{s.name}.{f.name}: offset {f.offset}+{f.size} "
                f"exceeds total size {s.total_size}"
            )
            continue

        # Check for overlaps
        for byte_idx in range(f.offset, f.offset + f.size):
            if covered[byte_idx]:
                errors.append(
                    f"{s.name}.{f.name}: byte {byte_idx} already covered by another field"
                )
                break
            covered[byte_idx] = True

        # Check endianness for multi-byte integer types
        if base_size > 1 and f.c_type != "char" and f.endianness == "-":
            errors.append(
                f"{s.name}.{f.name}: multi-byte type {f.c_type} "
                f"must have explicit endianness (not '-')"
            )

    # Check for gaps
    for byte_idx, is_covered in enumerate(covered):
        if not is_covered:
            errors.append(f"{s.name}: byte {byte_idx} not covered by any field (gap)")

    # Check CRC field exists
    if s.crc_field:
        field_names = {f.name for f in s.fields}
        if s.crc_field not in field_names:
            errors.append(f"{s.name}: CRC field '{s.crc_field}' not found in fields")

    # Check CRC coverage
    if s.crc_coverage and s.crc_field:
        crc_fields = [f for f in s.fields if f.name == s.crc_field]
        if crc_fields:
            crc_f = crc_fields[0]
            expected_end = crc_f.offset - 1
            if s.crc_coverage[1] != expected_end:
                errors.append(
                    f"{s.name}: CRC coverage end {s.crc_coverage[1]} "
                    f"!= CRC field offset - 1 ({expected_end})"
                )

    return errors


def validate_spec(spec: FormatSpec) -> list[str]:
    """Validate the complete format specification. Returns list of error messages."""
    errors: list[str] = []

    for s in spec.structs:
        errors.extend(validate_struct(s))

    # Validate enum members have unique values
    for e in spec.enums:
        seen_values: dict[int, str] = {}
        for m in e.members:
            if m.value in seen_values:
                errors.append(
                    f"Enum {e.name}: duplicate value {m.value} "
                    f"({seen_values[m.value]} and {m.name})"
                )
            seen_values[m.value] = m.name

    # Validate constant names are unique
    seen_names: set[str] = set()
    for c in spec.constants:
        if c.name in seen_names:
            errors.append(f"Duplicate constant name: {c.name}")
        seen_names.add(c.name)

    return errors

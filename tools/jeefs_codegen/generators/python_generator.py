# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Generate Python constants module from parsed format specification."""

from __future__ import annotations

from ..models import ConstantDef, EnumDef, FormatSpec, StructDef


def _generate_banner() -> str:
    return '''\
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""EEPROM format constants — auto-generated from docs/format/*.md.

DO NOT EDIT — regenerate with:
    python -m jeefs_codegen --specs docs/format/*.md --py-output python/jeefs/constants_generated.py
"""

from __future__ import annotations

from enum import IntEnum
'''


def _generate_enum(enum: EnumDef) -> str:
    lines: list[str] = []
    lines.append("")
    lines.append(f"class {enum.py_class}(IntEnum):")
    doclines = [f'    """{enum.name} values.']
    doclines.append("")
    doclines.append("    Values:")
    for m in enum.members:
        doclines.append(f"        {m.name}: {m.description} ({m.value})")
    doclines.append('    """')
    lines.extend(doclines)
    lines.append("")
    for m in enum.members:
        lines.append(f"    {m.name} = {m.value}")

    # Add from_int classmethod
    lines.append("")
    lines.append("    @classmethod")
    lines.append(f'    def from_int(cls, value: int) -> "{enum.py_class}":')
    lines.append('        """Convert integer to enum, raising ValueError for unknown."""')
    lines.append("        try:")
    lines.append("            return cls(value)")
    lines.append("        except ValueError:")
    lines.append('            valid = ", ".join(f"{m.value}={m.name}" for m in cls)')
    lines.append("            raise ValueError(")
    lines.append(f'                f"Unknown value={{value}}. Valid values: {{valid}}"')
    lines.append("            ) from None")

    return "\n".join(lines)


def _generate_signature_sizes(enum: EnumDef) -> str:
    """Generate SIGNATURE_SIZES dict from enum extra data."""
    lines: list[str] = []
    lines.append("")
    lines.append(f"SIGNATURE_SIZES: dict[{enum.py_class}, int] = {{")
    for m in enum.members:
        sig_size = m.extra.get("signature size", "0")
        lines.append(f"    {enum.py_class}.{m.name}: {sig_size},")
    lines.append("}")
    lines.append('"""Signature byte size per algorithm (raw r||s format)."""')
    return "\n".join(lines)


def _generate_constants(constants: list[ConstantDef]) -> str:
    lines: list[str] = []
    lines.append("")
    lines.append("# --- Named constants ---")
    lines.append("")
    for c in constants:
        # Strip surrounding quotes from values if present
        val = c.value.strip('"').strip("'")
        # Avoid double-prefix: if name already starts with EEPROM_, don't add again
        prefix = "" if c.name.startswith("EEPROM_") else "EEPROM_"
        py_name = f"{prefix}{c.name}"
        if c.type == "string":
            lines.append(f'{py_name} = b"{val}\\x00"')
        elif c.type == "byte":
            lines.append(f"{py_name} = {val}")
        else:
            lines.append(f"{py_name} = {val}")

    return "\n".join(lines)


def _generate_struct_fields(s: StructDef) -> str:
    """Generate EEPROM_FIELDS dict for a struct."""
    lines: list[str] = []

    # Determine dict name based on struct
    if s.version is not None:
        if s.version == 3:
            # Current version gets the canonical name
            dict_name = "EEPROM_FIELDS"
            lines.append("")
            lines.append(f'# Field offsets and sizes for {s.name}')
            lines.append(f"{dict_name}: dict[str, tuple[int, int]] = {{")
        else:
            dict_name = f"EEPROM_FIELDS_V{s.version}"
            lines.append("")
            lines.append(f'# Field offsets and sizes for {s.name}')
            lines.append(f"{dict_name}: dict[str, tuple[int, int]] = {{")
    else:
        dict_name = f"EEPROM_FIELDS_{s.name.upper()}"
        lines.append("")
        lines.append(f'# Field offsets and sizes for {s.name}')
        lines.append(f"{dict_name}: dict[str, tuple[int, int]] = {{")

    for f in s.fields:
        lines.append(f'    "{f.name}": ({f.offset}, {f.size}),')

    lines.append("}")

    # Size and CRC constants
    if s.version is not None:
        if s.version == 3:
            prefix = "EEPROM"
        else:
            prefix = f"EEPROM_V{s.version}"
        lines.append(f"{prefix}_HEADER_SIZE = {s.total_size}")
        if s.crc_coverage:
            lines.append(f"{prefix}_CRC_COVERAGE = {s.crc_coverage[1] + 1}")

    return "\n".join(lines)


def generate_python_constants(spec: FormatSpec) -> str:
    """Generate the complete Python constants module content."""
    sections: list[str] = []

    # Banner
    sections.append(_generate_banner())

    # Enums
    for enum in spec.enums:
        sections.append(_generate_enum(enum))
        # If this is the signature algorithm enum, generate SIGNATURE_SIZES
        if any("signature size" in m.extra for m in enum.members):
            sections.append(_generate_signature_sizes(enum))

    # Constants
    if spec.constants:
        sections.append(_generate_constants(spec.constants))

    # Struct field dicts (only for versioned header structs, sorted by version desc)
    header_structs = [s for s in spec.structs if s.version is not None]
    header_structs.sort(key=lambda s: -(s.version or 0))

    for s in header_structs:
        sections.append(_generate_struct_fields(s))

    # Non-versioned structs (filesystem)
    fs_structs = [s for s in spec.structs if s.version is None]
    for s in fs_structs:
        sections.append(_generate_struct_fields(s))

    sections.append("")
    return "\n".join(sections)

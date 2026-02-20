"""Generate Rust module from parsed format specification."""

from __future__ import annotations

import re

from ..models import ConstantDef, EnumDef, FormatSpec, StructDef, UnionDef

# Map C types to Rust types
C_TO_RUST_TYPE: dict[str, str] = {
    "char": "u8",
    "uint8_t": "u8",
    "int8_t": "i8",
    "uint16_t": "u16",
    "int16_t": "i16",
    "uint32_t": "u32",
    "int32_t": "i32",
    "uint64_t": "u64",
    "int64_t": "i64",
}


def _to_snake_case(name: str) -> str:
    """Convert camelCase field name to snake_case.

    dataSize -> data_size
    nextFileAddress -> next_file_address
    crc32 -> crc32
    boardname -> boardname
    """
    # Insert underscore before uppercase letters
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    return s.lower()


def _c_name_to_rust(name: str) -> str:
    """Convert C struct name to Rust CamelCase.

    JEEPROMHeaderv3  -> JeepromHeaderV3
    JEEPROMHeaderv1  -> JeepromHeaderV1
    JEEPROMHeaderversion -> JeepromHeaderVersion
    JEEFSFileHeaderv1 -> JeefsFileHeaderV1
    """
    # Split on known boundaries
    # Pattern: uppercase sequences followed by CamelCase word or version suffix
    # Strategy: manually handle the known prefixes
    result = name

    # Handle JEEPROM -> Jeeprom
    result = re.sub(r"^JEEPROM", "Jeeprom", result)
    # Handle JEEFS -> Jeefs
    result = re.sub(r"^JEEFS", "Jeefs", result)

    # Uppercase the 'v' before version numbers: v1 -> V1, v2 -> V2, v3 -> V3
    result = re.sub(r"v(\d+)$", r"V\1", result)

    # Handle 'version' suffix (e.g. Headerversion -> HeaderVersion)
    result = re.sub(r"version$", "Version", result)

    return result


def _generate_banner() -> str:
    return """\
// DO NOT EDIT — auto-generated from docs/format/*.md by tools/jeefs_codegen
// SPDX-License-Identifier: (GPL-2.0+ or MIT)
//
// Regenerate with:
//   python -m jeefs_codegen --specs docs/format/*.md --rs-output rust/jeefs-header/src/generated.rs

#![allow(non_camel_case_types, dead_code)]
"""


def _generate_constants(constants: list[ConstantDef]) -> str:
    lines: list[str] = []
    lines.append("// --- Named constants ---")
    lines.append("")

    for c in constants:
        val = c.value.strip('"').strip("'")
        if c.type == "string":
            # Magic as byte string with explicit null terminator
            if c.name == "MAGIC":
                lines.append(f'pub const {c.name}: &[u8; {len(val) + 1}] = b"{val}\\0";')
            else:
                lines.append(f'pub const {c.name}: &str = "{val}";')
        elif c.type == "byte":
            lines.append(f"pub const {c.name}: u8 = {val};")
        else:
            lines.append(f"pub const {c.name}: usize = {val};")

    return "\n".join(lines)


def _generate_enum(enum: EnumDef) -> str:
    lines: list[str] = []

    # Determine Rust enum name: strip prefix (JEEFS prefix is redundant inside the crate)
    rust_name = enum.py_class  # Reuse Python class name (e.g. "SignatureAlgorithm")

    lines.append(f"/// {enum.name}")
    lines.append("#[repr(u8)]")
    lines.append("#[derive(Debug, Clone, Copy, PartialEq, Eq)]")
    lines.append(f"pub enum {rust_name} {{")

    for m in enum.members:
        comment = f"  // {m.description}" if m.description else ""
        lines.append(f"    {m.name} = {m.value},{comment}")

    lines.append("}")
    lines.append("")

    # from_u8 method
    lines.append(f"impl {rust_name} {{")
    lines.append(f"    pub fn from_u8(v: u8) -> Result<Self, u8> {{")
    lines.append("        match v {")
    for m in enum.members:
        lines.append(f"            {m.value} => Ok({rust_name}::{m.name}),")
    lines.append("            _ => Err(v),")
    lines.append("        }")
    lines.append("    }")

    # Generate signature_size() if enum has extra data
    if any("signature size" in m.extra for m in enum.members):
        lines.append("")
        lines.append("    pub fn signature_size(self) -> usize {")
        lines.append("        match self {")
        for m in enum.members:
            sig_size = m.extra.get("signature size", "0")
            lines.append(f"            {rust_name}::{m.name} => {sig_size},")
        lines.append("        }")
        lines.append("    }")

    lines.append("}")

    return "\n".join(lines)


def _generate_struct(s: StructDef) -> str:
    rust_name = _c_name_to_rust(s.name)
    lines: list[str] = []

    lines.append(f"/// {s.name} ({s.total_size} bytes)")
    lines.append("#[repr(C, packed)]")
    lines.append("#[derive(Clone, Copy)]")
    lines.append(f"pub struct {rust_name} {{")

    for f in s.fields:
        rust_type = C_TO_RUST_TYPE.get(f.c_type, f.c_type)
        rust_field = _to_snake_case(f.name)
        comment = f"  // {f.description}" if f.description else ""

        if f.array_size > 0:
            lines.append(f"    pub {rust_field}: [{rust_type}; {f.array_size}],{comment}")
        else:
            lines.append(f"    pub {rust_field}: {rust_type},{comment}")

    lines.append("}")
    lines.append("")

    # Compile-time size assertion
    lines.append(
        f"const _: () = assert!(core::mem::size_of::<{rust_name}>() == {s.total_size});"
    )

    return "\n".join(lines)


def _generate_struct_debug_impl(s: StructDef) -> str:
    """Generate a manual Debug impl since #[derive(Debug)] doesn't work with packed structs containing arrays > 32."""
    rust_name = _c_name_to_rust(s.name)
    lines: list[str] = []
    lines.append(f"impl core::fmt::Debug for {rust_name} {{")
    lines.append(f'    fn fmt(&self, f: &mut core::fmt::Formatter<\'_>) -> core::fmt::Result {{')
    lines.append(f'        f.debug_struct("{rust_name}")')

    for field in s.fields:
        rust_field = _to_snake_case(field.name)
        rust_type = C_TO_RUST_TYPE.get(field.c_type, field.c_type)
        # In packed structs, references to fields with alignment > 1 are UB.
        # For u8 arrays we can reference directly; everything else must be copied.
        if field.array_size > 0 and rust_type == "u8":
            if field.array_size > 32:
                lines.append(f'            .field("{rust_field}", &&self.{rust_field}[..])')
            else:
                lines.append(f'            .field("{rust_field}", &self.{rust_field})')
        elif field.array_size > 0:
            # Non-u8 array in packed struct: copy to local to avoid misaligned reference
            lines.append(f'            .field("{rust_field}", &{{ self.{rust_field} }})')
        else:
            # Scalar in packed struct: copy to avoid unaligned reference
            lines.append(f'            .field("{rust_field}", &{{ self.{rust_field} }})')

    lines.append("            .finish()")
    lines.append("    }")
    lines.append("}")
    return "\n".join(lines)


def generate_rust_module(spec: FormatSpec) -> str:
    """Generate the complete Rust module content."""
    sections: list[str] = []

    # Banner
    sections.append(_generate_banner())

    # Constants
    if spec.constants:
        sections.append(_generate_constants(spec.constants))

    # Enums
    for enum in spec.enums:
        sections.append("")
        sections.append(_generate_enum(enum))

    # Sort structs same as C generator
    def struct_sort_key(s: StructDef) -> tuple[int, int, str]:
        if "version" in s.name.lower() and s.total_size <= 12:
            return (0, 0, s.name)
        if s.version is not None:
            return (1, s.version, s.name)
        return (2, 0, s.name)

    sorted_structs = sorted(spec.structs, key=struct_sort_key)

    for s in sorted_structs:
        sections.append("")
        sections.append(_generate_struct(s))
        # Add manual Debug impl for structs with large arrays
        has_large_array = any(f.array_size > 32 for f in s.fields)
        if has_large_array:
            sections.append("")
            sections.append(_generate_struct_debug_impl(s))

    # Name mapping comment for cross-referencing with C
    sections.append("")
    sections.append("// --- C name to Rust name mapping ---")
    for s in sorted_structs:
        rust_name = _c_name_to_rust(s.name)
        if rust_name != s.name:
            sections.append(f"// {s.name} -> {rust_name}")

    sections.append("")
    return "\n".join(sections)

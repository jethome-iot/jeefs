"""Generate C header file from parsed format specification."""

from __future__ import annotations

from ..models import ConstantDef, EnumDef, FormatSpec, StructDef, UnionDef


def _generate_banner() -> str:
    return """\
// DO NOT EDIT — auto-generated from docs/format/*.md by tools/jeefs_codegen
// SPDX-License-Identifier: (GPL-2.0+ or MIT)
//
// Regenerate with:
//   python -m jeefs_codegen --specs docs/format/*.md --c-output include/jeefs_generated.h
"""


def _generate_constants(constants: list[ConstantDef]) -> str:
    lines: list[str] = []
    lines.append("// --- Named constants ---")
    lines.append("")

    for c in constants:
        # Strip surrounding quotes from values if present
        val = c.value.strip('"').strip("'")
        if c.type == "string":
            lines.append(f'#define {c.name} "{val}"')
        elif c.type == "byte":
            # Convert 0xNN to \xNN for C char literal
            if val.startswith("0x") or val.startswith("0X"):
                lines.append(f"#define {c.name} '\\x{val[2:]}'")
            else:
                lines.append(f"#define {c.name} '{val}'")
        else:
            lines.append(f"#define {c.name} {val}")

    return "\n".join(lines)


def _generate_enum(enum: EnumDef) -> str:
    lines: list[str] = []
    lines.append(f"// {enum.name}")
    lines.append(f"enum {enum.name} {{")

    for m in enum.members:
        comment = f"  // {m.description}" if m.description else ""
        lines.append(f"    {enum.c_prefix}_{m.name} = {m.value},{comment}")

    lines.append("};")
    return "\n".join(lines)


def _generate_struct(s: StructDef) -> str:
    lines: list[str] = []
    lines.append(f"// {s.name} ({s.total_size} bytes)")
    lines.append("typedef struct {")

    for f in s.fields:
        comment_parts: list[str] = []
        comment_parts.append(f"{f.size}B")
        comment_parts.append(f"offset {f.offset}")
        if f.description:
            comment_parts.append(f.description)

        comment = "  // " + ", ".join(comment_parts)

        if f.array_size > 0:
            lines.append(f"    {f.c_type} {f.name}[{f.array_size}];{comment}")
        else:
            lines.append(f"    {f.c_type} {f.name};{comment}")

    lines.append(f"}} {s.name};")
    return "\n".join(lines)


def _generate_union(union: UnionDef) -> str:
    lines: list[str] = []
    lines.append(f"// {union.name}")
    lines.append(f"union {union.name} {{")

    for m in union.members:
        comment = f"  // {m.description}" if m.description else ""
        lines.append(f"    {m.type_name} {m.name};{comment}")

    lines.append("};")
    return "\n".join(lines)


def _generate_static_asserts(structs: list[StructDef]) -> str:
    lines: list[str] = []
    lines.append("// --- Size assertions ---")
    lines.append("#ifdef __cplusplus")
    lines.append("#define JEEFS_STATIC_ASSERT static_assert")
    lines.append("#else")
    lines.append("#define JEEFS_STATIC_ASSERT _Static_assert")
    lines.append("#endif")
    for s in structs:
        lines.append(
            f'JEEFS_STATIC_ASSERT(sizeof({s.name}) == {s.total_size}, '
            f'"sizeof({s.name}) must be {s.total_size}");'
        )
    lines.append("#undef JEEFS_STATIC_ASSERT")
    return "\n".join(lines)


def generate_c_header(spec: FormatSpec) -> str:
    """Generate the complete C header file content."""
    sections: list[str] = []

    # Banner
    sections.append(_generate_banner())

    # Header guard
    sections.append("#ifndef JEEFS_GENERATED_H")
    sections.append("#define JEEFS_GENERATED_H")
    sections.append("")
    sections.append("#include <stdint.h>")
    sections.append("")
    sections.append('#ifdef __cplusplus')
    sections.append('extern "C" {')
    sections.append("#endif")

    # Constants
    if spec.constants:
        sections.append("")
        sections.append(_generate_constants(spec.constants))

    # Enums
    for enum in spec.enums:
        sections.append("")
        sections.append(_generate_enum(enum))

    # Packed structs
    sections.append("")
    sections.append("#pragma pack(push, 1)")

    # Sort structs: version-detect struct first, then by version (ascending), then non-versioned
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

    # Unions
    for union in spec.unions:
        sections.append("")
        sections.append(_generate_union(union))

    sections.append("")
    sections.append("#pragma pack(pop)")

    # Static asserts
    sections.append("")
    sections.append(_generate_static_asserts(sorted_structs))

    # Close guards
    sections.append("")
    sections.append("#ifdef __cplusplus")
    sections.append("}")
    sections.append("#endif")
    sections.append("")
    sections.append("#endif // JEEFS_GENERATED_H")
    sections.append("")

    return "\n".join(sections)

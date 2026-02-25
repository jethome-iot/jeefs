# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Data models for parsed format specifications."""

from __future__ import annotations

from dataclasses import dataclass, field


# Map C type names to their byte sizes (scalar)
C_TYPE_SIZES: dict[str, int] = {
    "char": 1,
    "uint8_t": 1,
    "int8_t": 1,
    "uint16_t": 2,
    "int16_t": 2,
    "uint32_t": 4,
    "int32_t": 4,
    "uint64_t": 8,
    "int64_t": 8,
}


@dataclass
class FieldDef:
    """A single field within a struct."""

    name: str
    offset: int
    size: int
    c_type: str  # Base C type, e.g. "uint8_t", "char", "int64_t"
    array_size: int  # 0 = scalar, >0 = array of this many elements
    endianness: str  # "little-endian", "big-endian", or "-"
    description: str

    @property
    def c_decl(self) -> str:
        """C struct member declaration (without trailing semicolon)."""
        if self.array_size > 0:
            return f"{self.c_type} {self.name}[{self.array_size}]"
        return f"{self.c_type} {self.name}"


@dataclass
class StructDef:
    """A packed binary struct parsed from a markdown spec."""

    name: str  # C typedef name, e.g. "JEEPROMHeaderv3"
    total_size: int
    version: int | None = None
    crc_field: str | None = None
    crc_coverage: tuple[int, int] | None = None  # (start, end) inclusive
    fields: list[FieldDef] = field(default_factory=list)


@dataclass
class EnumMember:
    """A single enum value."""

    value: int
    name: str
    description: str
    extra: dict[str, str] = field(default_factory=dict)  # e.g. {"Signature Size": "48"}


@dataclass
class EnumDef:
    """An enumeration parsed from a markdown spec."""

    name: str  # C enum name, e.g. "JEEFSSignatureAlgorithm"
    c_prefix: str  # Prefix for C values, e.g. "JEEFS_SIG"
    py_class: str  # Python IntEnum class name, e.g. "SignatureAlgorithm"
    members: list[EnumMember] = field(default_factory=list)


@dataclass
class ConstantDef:
    """A named constant."""

    name: str
    value: str
    type: str  # "string", "int", "byte"
    description: str


@dataclass
class UnionMember:
    """A member of a union type."""

    name: str  # Field name in the union
    type_name: str  # C type name
    description: str


@dataclass
class UnionDef:
    """A union type parsed from a markdown spec."""

    name: str  # C union name
    members: list[UnionMember] = field(default_factory=list)


@dataclass
class FormatSpec:
    """Complete parsed format specification from one or more markdown files."""

    structs: list[StructDef] = field(default_factory=list)
    enums: list[EnumDef] = field(default_factory=list)
    constants: list[ConstantDef] = field(default_factory=list)
    unions: list[UnionDef] = field(default_factory=list)

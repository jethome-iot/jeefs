# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Generate byte-backed Go wire types and private layout metadata."""
from __future__ import annotations
import json
import subprocess
from ..models import FormatSpec


def _name(name: str) -> str:
    if name.startswith('JEEPROMHeaderv'):
        return 'HeaderV' + name.removeprefix('JEEPROMHeaderv')
    return name


def generate_go_module(spec: FormatSpec) -> str:
    lines = [
        '// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)',
        '// Code generated from docs/format/*.md; DO NOT EDIT.',
        '// See docs/CODEGEN.md. Wire types are byte arrays, never native structs.',
        'package jeefs', '',
        'type fieldLayout struct { offset, size int; terminated bool }',
        'type wireLayout struct { size, crc int; fields map[string]fieldLayout }', '',
        '// SignatureAlgorithm identifies stored signature bytes, without cryptographic verification.',
        'type SignatureAlgorithm byte',
        'const (',
    ]
    for enum in spec.enums:
        for member in enum.members:
            lines.append(f'Signature{member.name} SignatureAlgorithm = {member.value}')
    lines += [')', '', 'const (']
    for c in spec.constants:
        value = json.dumps(c.value.strip('"')) if c.type == 'string' else c.value
        lines.append(f'Wire{c.name} = {value}')
    lines += [')', '', 'func signatureSize(a SignatureAlgorithm) (int, bool) {', 'switch a {']
    for enum in spec.enums:
        for m in enum.members:
            lines.append(f'case Signature{m.name}: return {int(m.extra["signature size"])}, true')
    lines += ['}; return 0, false', '}', '']
    for s in spec.structs:
        name = _name(s.name)
        lines += [f'// {name}Wire is the packed {s.total_size}-byte wire representation.',
                  f'type {name}Wire [{s.total_size}]byte', '']
        if s.crc_field is None:
            continue
        crc = next(f.offset for f in s.fields if f.name == s.crc_field)
        lines += [f'var layout{name} = wireLayout{{{s.total_size}, {crc}, map[string]fieldLayout{{']
        for f in s.fields:
            terminated = 'true' if 'null-term' in f.description.lower() and f.name != 'magic' else 'false'
            lines.append(f'{json.dumps(f.name)}: {{{f.offset}, {f.size}, {terminated}}},')
        lines += ['}}', '']
    lines += ['func headerLayout(version byte) (wireLayout, bool) {', 'switch version {']
    for s in spec.structs:
        if s.version is not None and s.name.startswith('JEEPROMHeader'):
            lines.append(f'case {s.version}: return layout{_name(s.name)}, true')
    lines += ['}; return wireLayout{}, false', '}', '']
    return subprocess.run(['gofmt'], input='\n'.join(lines), text=True, capture_output=True, check=True).stdout

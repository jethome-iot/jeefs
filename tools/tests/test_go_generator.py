# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
from pathlib import Path
from jeefs_codegen.parser import parse_files
from jeefs_codegen.generators.go_generator import generate_go_module


def test_generated_wire_layouts():
    root = Path(__file__).resolve().parents[2]
    names = ['header-common', 'header-v1', 'header-v2', 'header-v3', 'header-v4', 'device-identity-v1', 'filesystem-v1']
    spec = parse_files([root / 'docs/format' / (name + '.md') for name in names])
    text = generate_go_module(spec)
    assert text == generate_go_module(spec)
    assert 'type HeaderV1Wire [512]byte' in text
    assert 'type DeviceIdentityV1Wire [256]byte' in text
    assert '"timestamp":' in text and '{244, 8, false}' in text
    assert 'SignatureSECP192R1' in text and 'SignatureAlgorithm = 1' in text
    assert '"board_serial":' in text and '{76, 32, false}' in text

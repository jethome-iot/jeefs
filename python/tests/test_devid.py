# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""DeviceIdentityV1 record tests (issue #60)."""

import pytest

from jeefs import DeviceIdentityV1, SignatureAlgorithm
from jeefs.constants_generated import EEPROM_FIELDS_DEVICEIDENTITYV1 as F


def mk(**kw):
    defaults = dict(device_model="JetHub-D2", device_serial="DSN-0001",
                    hw_revision="1.2a")
    defaults.update(kw)
    return DeviceIdentityV1(**defaults)


class TestRoundTrip:
    def test_to_bytes_layout(self):
        raw = mk().to_bytes()
        assert len(raw) == 256
        assert raw[0:8] == b"JHDEVID\x00"
        assert raw[8] == 1  # record_version
        off, sz = F["device_model"]
        assert raw[off : off + sz].split(b"\x00")[0] == b"JetHub-D2"

    def test_round_trip(self):
        rec = mk(device_serial="S" * 32, timestamp=1755200000)
        back = DeviceIdentityV1.from_bytes(rec.to_bytes())
        assert back.device_model == "JetHub-D2"
        assert back.device_serial == "S" * 32  # bounded: all 32 bytes usable
        assert back.hw_revision == "1.2a"
        assert back.timestamp == 1755200000
        assert back.verify_crc(rec.to_bytes())

    def test_flags_ignored_on_read(self):
        raw = bytearray(mk().to_bytes())
        raw[92] = 0x01  # reserved flags bit
        raw[94] = 0xAA  # reserved2
        rec = DeviceIdentityV1(device_model="x")
        crc = rec.crc32_of(bytes(raw))
        raw[252:256] = crc.to_bytes(4, "little")
        back = DeviceIdentityV1.from_bytes(bytes(raw))
        assert back.device_model == "JetHub-D2"


class TestStrictGates:
    def test_bad_magic(self):
        raw = bytearray(mk().to_bytes())
        raw[0] = ord("X")
        with pytest.raises(ValueError, match="magic"):
            DeviceIdentityV1.from_bytes(bytes(raw))

    def test_unknown_record_version(self):
        raw = bytearray(mk().to_bytes())
        raw[8] = 2
        with pytest.raises(ValueError, match="record_version"):
            DeviceIdentityV1.from_bytes(bytes(raw))

    def test_unknown_signature_version(self):
        raw = bytearray(mk().to_bytes())
        raw[9] = 7
        with pytest.raises(ValueError, match="signature"):
            DeviceIdentityV1.from_bytes(bytes(raw))

    def test_erased_buffers(self):
        for fill in (0x00, 0xFF):
            with pytest.raises(ValueError, match="magic"):
                DeviceIdentityV1.from_bytes(bytes([fill]) * 256)

    def test_validate_lengths(self):
        assert mk(device_serial="S" * 32).validate() == []
        assert any("device_serial" in e for e in mk(device_serial="S" * 33).validate())
        assert any("hw_revision" in e for e in mk(hw_revision="1" * 17).validate())


class TestCommittedVector:
    def test_parse_committed_record(self):
        import json
        from pathlib import Path

        vectors = Path(__file__).resolve().parents[2] / "test-vectors" / "vectors"
        raw = (vectors / "devid_record_v1.bin").read_bytes()
        spec = json.loads((vectors / "devid_record_v1.json").read_text())["fields"]

        rec = DeviceIdentityV1.from_bytes(raw)
        assert rec.device_model == spec["device_model"]
        assert rec.device_serial == spec["device_serial"]
        assert rec.hw_revision == spec["hw_revision"]
        assert rec.timestamp == spec["timestamp"]
        assert rec.verify_crc(raw)

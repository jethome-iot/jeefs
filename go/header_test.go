// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

import (
	"bytes"
	"encoding/binary"
	"testing"
)

func TestHeaderContract(t *testing.T) {
	for v := byte(1); v <= 4; v++ {
		b := bytes.Repeat([]byte{0xff}, HeaderSize(v)+8)
		if err := InitHeader(b, v); err != nil {
			t.Fatal(err)
		}
		if DetectVersion(b[:12]) != v || !VerifyCRC(b) {
			t.Fatal("init/detect/crc", v)
		}
		if state, e := HeaderIsEmpty(b); e != nil || state != Placeholder {
			t.Fatal(state, e)
		}
		if !bytes.Equal(b[HeaderSize(v):], bytes.Repeat([]byte{0xff}, 8)) {
			t.Fatal("trailing bytes changed")
		}
		h, e := NewHeader(b)
		if e != nil {
			t.Fatal(e)
		}
		serial := "serial"
		if v == 4 {
			serial = "board_serial"
		}
		for _, name := range []string{serial, "usid", "cpuid"} {
			full := string(bytes.Repeat([]byte{'A'}, 32))
			if e := h.SetString(name, full); e != nil {
				t.Fatal(e)
			}
			if got, e := h.String(name); e != nil || got != full {
				t.Fatal(got, e)
			}
		}
		if e := h.SetString("boardname", string(bytes.Repeat([]byte{'b'}, 31))); e != nil {
			t.Fatal(e)
		}
		before := bytes.Clone(b)
		if h.SetString("boardname", string(bytes.Repeat([]byte{'b'}, 32))) == nil || !bytes.Equal(b, before) {
			t.Fatal("overflow changed bytes")
		}
		if h.SetString("usid", "a\x00b") == nil {
			t.Fatal("embedded NUL")
		}
		if VerifyCRC(b) {
			t.Fatal("mutation did not invalidate crc")
		}
		if e := h.UpdateCRC(); e != nil || !VerifyCRC(b) {
			t.Fatal(e)
		}
		if s, _ := HeaderIsEmpty(b); s != Populated {
			t.Fatal(s)
		}
		if v >= 3 {
			if e := h.SetTimestamp(-123456789); e != nil {
				t.Fatal(e)
			}
			if got, e := h.Timestamp(); got != -123456789 || e != nil {
				t.Fatal(got, e)
			}
			if binary.LittleEndian.Uint64(b[244:252]) != uint64(0xfffffffff8a432eb) {
				t.Fatal("not LE")
			}
			if e := h.SetSignature(SignatureSECP192R1, bytes.Repeat([]byte{7}, 48)); e != nil {
				t.Fatal(e)
			}
			if !bytes.Equal(b[228:244], make([]byte, 16)) {
				t.Fatal("signature padding")
			}
			before = bytes.Clone(b)
			if h.SetSignature(SignatureSECP192R1, []byte{1}) == nil || !bytes.Equal(b, before) {
				t.Fatal("invalid signature mutated")
			}
			b[9] = 255
			_ = h.UpdateCRC()
			if DetectVersion(b) != v || !VerifyCRC(b) {
				t.Fatal("signature gated detect/crc")
			}
			if _, e := h.SignatureAlgorithm(); e == nil {
				t.Fatal("unknown signature accepted")
			}
		}
		b[8] = 255
		if _, e := h.String("usid"); e == nil {
			t.Fatal("view trusted stale version")
		}
	}
}

func TestShortBuffersAndUnknownVersions(t *testing.T) {
	for n := 0; n < 512; n++ {
		for _, fill := range []byte{0, 255} {
			b := bytes.Repeat([]byte{fill}, n)
			if DetectVersion(b) != 0 || VerifyCRC(b) {
				t.Fatal("erased recognised")
			}
			if state, e := HeaderIsEmpty(b); e != nil || state != NoHeader {
				t.Fatal(state, e)
			}
			for v := byte(1); v <= 4; v++ {
				before := bytes.Clone(b)
				err := InitHeader(b, v)
				if n < HeaderSize(v) && (err == nil || !bytes.Equal(b, before)) {
					t.Fatal("short init", n, v)
				}
				copy(b, before)
			}
		}
	}
	b := make([]byte, 512)
	before := bytes.Clone(b)
	if InitHeader(b, 255) == nil || !bytes.Equal(b, before) {
		t.Fatal("unknown init mutated")
	}
	_ = InitHeader(b, 1)
	if _, e := HeaderIsEmpty(b[:12]); e == nil {
		t.Fatal("short detected header")
	}
	if VerifyCRC(b[:511]) || UpdateCRC(b[:511]) == nil {
		t.Fatal("short crc")
	}
	b[20] = 1
	if s, e := HeaderIsEmpty(b); s != Populated || e != nil {
		t.Fatal(s, e)
	}
	var zero Header
	if _, e := zero.Bytes(); e == nil {
		t.Fatal("zero view")
	}
}

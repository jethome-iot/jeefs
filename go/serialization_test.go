// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

import (
	"bytes"
	"testing"
)

func TestSerializationNormalizesSignaturePadding(t *testing.T) {
	for _, record := range []bool{false, true} {
		for _, a := range []SignatureAlgorithm{SignatureNONE, SignatureSECP192R1, SignatureSECP256R1} {
			b := make([]byte, 256)
			var v view
			if record {
				if e := InitRecord(b); e != nil {
					t.Fatal(e)
				}
				r, _ := NewRecord(b)
				v = r.view
			} else {
				if e := InitHeader(b, 4); e != nil {
					t.Fatal(e)
				}
				h, _ := NewHeader(b)
				v = h.view
			}
			n, _ := signatureSize(a)
			active := bytes.Repeat([]byte{7}, n)
			if e := v.SetSignature(a, active); e != nil {
				t.Fatal(e)
			}
			copy(b[180+n:244], bytes.Repeat([]byte{0xcc}, 64-n))
			if e := v.UpdateCRC(); e != nil {
				t.Fatal(e)
			}
			before := bytes.Clone(b)
			if !v.VerifyCRC() {
				t.Fatal("read path rejected valid CRC")
			}
			out, e := v.Bytes()
			if e != nil {
				t.Fatal(e)
			}
			if !bytes.Equal(out[180:180+n], active) {
				t.Fatal("active signature changed")
			}
			if !bytes.Equal(out[180+n:244], make([]byte, 64-n)) {
				t.Errorf("record=%v algorithm=%v: stale signature padding", record, a)
			}
			if !bytes.Equal(b, before) {
				t.Fatal("serialization changed caller memory")
			}
			var round view
			if record {
				r, e := NewRecord(out)
				if e != nil {
					t.Fatal(e)
				}
				round = r.view
			} else {
				h, e := NewHeader(out)
				if e != nil {
					t.Fatal(e)
				}
				round = h.view
			}
			if !round.VerifyCRC() {
				t.Fatal("serialization CRC")
			}
		}
	}
}

func TestSerializationPreservesUninterpretedHeaderBytes(t *testing.T) {
	for _, version := range []byte{1, 2, 3, 4} {
		b := make([]byte, HeaderSize(version))
		if e := InitHeader(b, version); e != nil {
			t.Fatal(e)
		}
		b[9] = 255
		copy(b[180:244], bytes.Repeat([]byte{0xcc}, 64))
		if e := UpdateCRC(b); e != nil {
			t.Fatal(e)
		}
		h, e := NewHeader(b)
		if e != nil {
			t.Fatal(e)
		}
		out, e := h.Bytes()
		if e != nil || !bytes.Equal(out, b) {
			t.Fatal("uninterpreted legacy/unknown signature bytes changed", version, e)
		}
	}
}

// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

import (
	"bytes"
	"testing"
)

func TestViewMutationAndCopies(t *testing.T) {
	b := make([]byte, 512)
	_ = InitHeader(b, 3)
	h, _ := NewHeader(b)
	mac := [6]byte{255, 255, 255, 255, 255, 255}
	if e := h.SetMAC(mac); e != nil {
		t.Fatal(e)
	}
	got, e := h.MAC()
	if e != nil || got != mac {
		t.Fatal(got, e)
	}
	if e := h.SetFSVersion(231); e != nil {
		t.Fatal(e)
	}
	if got, e := h.FSVersion(); e != nil || got != 231 {
		t.Fatal(got, e)
	}
	if e := h.SetTimestamp(0); e != nil {
		t.Fatal(e)
	}
	if got, e := h.Timestamp(); e != nil || got != 0 {
		t.Fatal(got, e)
	}
	invalidUTF8 := string([]byte{255, 254})
	if e := h.SetString("usid", invalidUTF8); e != nil {
		t.Fatal(e)
	}
	if got, e := h.String("usid"); e != nil || got != invalidUTF8 {
		t.Fatal(got, e)
	}
	before := bytes.Clone(b)
	for _, name := range []string{"magic", "version", "timestamp", "unknown", "device_model"} {
		if h.SetString(name, "x") == nil || !bytes.Equal(b, before) {
			t.Fatal(name)
		}
	}
	for _, name := range []string{"magic", "device_model"} {
		if _, e := h.String(name); e == nil {
			t.Fatal(name)
		}
	}
	for _, a := range []SignatureAlgorithm{SignatureNONE, SignatureSECP192R1, SignatureSECP256R1} {
		n, _ := signatureSize(a)
		data := bytes.Repeat([]byte{5}, n)
		if e := h.SetSignature(a, data); e != nil {
			t.Fatal(e)
		}
		signature, e := h.Signature()
		if e != nil || !bytes.Equal(signature, data) {
			t.Fatal(e)
		}
		if len(signature) > 0 {
			signature[0] = 99
			if b[180] == 99 {
				t.Fatal("signature aliases input")
			}
		}
	}
	// Input can alias the signature field itself.
	copy(b[180:244], bytes.Repeat([]byte{6}, 64))
	if e := h.SetSignature(SignatureSECP192R1, b[180:228]); e != nil || b[180] != 6 || b[228] != 0 {
		t.Fatal(e)
	}
	out, e := h.Bytes()
	if e != nil || !VerifyCRC(out) {
		t.Fatal(e)
	}
	out[20] = 99
	if b[20] == 99 {
		t.Fatal("bytes aliases input")
	}
	b[8] = 4 // Same layout, different semantics: the old view must not reinterpret it.
	if h.UpdateCRC() == nil || h.SetMAC(mac) == nil || h.SetFSVersion(1) == nil || h.SetTimestamp(1) == nil {
		t.Fatal("stale view mutated")
	}
	if _, e := h.Bytes(); e == nil {
		t.Fatal("stale serialized")
	}
	_ = InitHeader(b, 2)
	h, _ = NewHeader(b)
	if _, e := h.Timestamp(); e == nil {
		t.Fatal("legacy timestamp")
	}
	if _, e := h.Signature(); e == nil {
		t.Fatal("legacy signature")
	}
	if h.SetTimestamp(2) == nil || h.SetSignature(SignatureNONE, nil) == nil {
		t.Fatal("legacy tail write")
	}
}

func FuzzPublicAPIs(f *testing.F) {
	f.Add([]byte{})
	f.Add(bytes.Repeat([]byte{255}, 256))
	for v := byte(1); v <= 4; v++ {
		b := make([]byte, HeaderSize(v))
		_ = InitHeader(b, v)
		f.Add(b)
	}
	r := make([]byte, 256)
	_ = InitRecord(r)
	f.Add(r)
	f.Fuzz(func(t *testing.T, b []byte) {
		_ = DetectVersion(b)
		_ = VerifyCRC(b)
		_, _ = HeaderIsEmpty(b)
		_ = UpdateCRC(b)
		_ = DetectRecord(b)
		if h, e := NewHeader(b); e == nil {
			_, _ = h.String("usid")
			_, _ = h.MAC()
			_, _ = h.Timestamp()
			_, _ = h.Signature()
			_, _ = h.Bytes()
			_ = h.SetString("usid", "x")
			_ = h.SetSignature(SignatureNONE, nil)
			_ = h.SetTimestamp(0)
			_ = h.SetMAC([6]byte{})
			_ = h.SetFSVersion(0)
			_ = h.UpdateCRC()
		}
		if r, e := NewRecord(b); e == nil {
			_, _ = r.String("hw_revision")
			_, _ = r.Signature()
			_, _ = r.Bytes()
			_ = r.SetString("device_model", "x")
			_ = r.SetTimestamp(-1)
			_ = r.UpdateCRC()
		}
		// None of these constructors may panic, whatever the input length/content.
		_ = InitHeader(b, 4)
		_ = InitRecord(b)
	})
}

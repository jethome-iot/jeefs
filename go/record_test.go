// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

import (
	"bytes"
	"testing"
)

func TestRecordContract(t *testing.T) {
	b := make([]byte, 264)
	b[256] = 99
	if e := InitRecord(b); e != nil {
		t.Fatal(e)
	}
	r, e := NewRecord(b)
	if e != nil || !DetectRecord(b) || !r.VerifyCRC() {
		t.Fatal(e)
	}
	if b[256] != 99 {
		t.Fatal("trailing changed")
	}
	for name, n := range map[string]int{"device_model": 32, "device_serial": 32, "hw_revision": 16} {
		s := string(bytes.Repeat([]byte{'X'}, n))
		if e := r.SetString(name, s); e != nil {
			t.Fatal(e)
		}
		if got, e := r.String(name); e != nil || got != s {
			t.Fatal(got, e)
		}
	}
	b[10] = 255
	b[92] = 255
	b[94] = 255
	_ = r.UpdateCRC()
	if !r.VerifyCRC() || !DetectRecord(b) {
		t.Fatal("reserved read")
	}
	encoded, e := r.Bytes()
	if e != nil {
		t.Fatal(e)
	}
	if encoded[10] != 0 || encoded[92] != 0 || encoded[94] != 0 || encoded[12] != 'X' {
		t.Fatal("reserved serialization")
	}
	out, e := NewRecord(encoded)
	if e != nil || !out.VerifyCRC() {
		t.Fatal(e)
	}
	if b[10] != 255 {
		t.Fatal("serialization changed caller")
	}
	for _, off := range []int{8, 9} {
		old := b[off]
		b[off] = 255
		if DetectRecord(b) {
			t.Fatal("unknown version")
		}
		if _, e := r.String("device_model"); e == nil {
			t.Fatal("stale view")
		}
		b[off] = old
	}
	b[20] ^= 1
	if r.VerifyCRC() {
		t.Fatal("corruption")
	}
}
func TestShortRecords(t *testing.T) {
	for n := 0; n < 256; n++ {
		b := make([]byte, n)
		before := bytes.Clone(b)
		if InitRecord(b) == nil || !bytes.Equal(b, before) || DetectRecord(b) {
			t.Fatal(n)
		}
		if _, e := NewRecord(b); e == nil {
			t.Fatal(n)
		}
	}
	var r Record
	if r.VerifyCRC() {
		t.Fatal("zero view")
	}
	if _, e := r.Bytes(); e == nil {
		t.Fatal("zero serialization")
	}
}

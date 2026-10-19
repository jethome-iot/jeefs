// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

import (
	"bytes"
	"encoding/binary"
)

type view struct {
	b       []byte
	version byte
	record  bool
}

func (v view) layout() (wireLayout, error) {
	if v.record {
		if !DetectRecord(v.b) {
			return wireLayout{}, ErrRecord
		}
		return layoutDeviceIdentityV1, nil
	}
	l, e := headerBytes(v.b)
	if e != nil {
		return wireLayout{}, e
	}
	if v.b[8] != v.version {
		return wireLayout{}, ErrHeader
	}
	return l, nil
}
func (v view) field(name string) ([]byte, error) {
	l, e := v.layout()
	if e != nil {
		return nil, e
	}
	f, ok := l.fields[name]
	if !ok {
		return nil, ErrField
	}
	return v.b[f.offset : f.offset+f.size], nil
}
func identityString(name string) bool {
	switch name {
	case "boardname", "boardversion", "serial", "board_serial", "usid", "cpuid", "device_model", "device_serial", "hw_revision":
		return true
	}
	return false
}

// String returns identity bytes up to the first NUL; invalid UTF-8 is preserved.
// Names are the canonical spec field names; v4 uses board_serial, not serial.
func (v view) String(name string) (string, error) {
	if !identityString(name) {
		return "", ErrField
	}
	b, e := v.field(name)
	if e != nil {
		return "", e
	}
	if n := bytes.IndexByte(b, 0); n >= 0 {
		b = b[:n]
	}
	return string(b), nil
}

// SetString zero-pads an identity field. It refuses embedded NUL or overflow
// without modifying the buffer. Limits are measured in bytes, not runes.
func (v view) SetString(name, value string) error {
	if !identityString(name) {
		return ErrField
	}
	l, e := v.layout()
	if e != nil {
		return e
	}
	f, ok := l.fields[name]
	if !ok {
		return ErrField
	}
	limit := f.size
	if f.terminated {
		limit--
	}
	if len(value) > limit || bytes.IndexByte([]byte(value), 0) >= 0 {
		return ErrValue
	}
	dst := v.b[f.offset : f.offset+f.size]
	clear(dst)
	copy(dst, value)
	return nil
}

// SignatureAlgorithm interprets the stored algorithm; unknown bytes are errors.
func (v view) SignatureAlgorithm() (SignatureAlgorithm, error) {
	f, e := v.field("signature_version")
	if e != nil {
		return 0, e
	}
	a := SignatureAlgorithm(f[0])
	if _, ok := signatureSize(a); !ok {
		return 0, ErrValue
	}
	return a, nil
}

// Signature returns a copy of the algorithm's active signature bytes.
func (v view) Signature() ([]byte, error) {
	a, e := v.SignatureAlgorithm()
	if e != nil {
		return nil, e
	}
	n, _ := signatureSize(a)
	f, e := v.field("signature")
	if e != nil {
		return nil, e
	}
	return bytes.Clone(f[:n]), nil
}

// SetSignature stores exactly the algorithm's signature length and zero-pads its tail.
func (v view) SetSignature(a SignatureAlgorithm, signature []byte) error {
	n, ok := signatureSize(a)
	if !ok || len(signature) != n {
		return ErrValue
	}
	f, e := v.field("signature")
	if e != nil {
		return e
	}
	alg, e := v.field("signature_version")
	if e != nil {
		return e
	}
	// Clone before clearing: signature may alias caller memory in the same field.
	data := bytes.Clone(signature)
	clear(f)
	copy(f, data)
	alg[0] = byte(a)
	return nil
}
func (v view) Timestamp() (int64, error) {
	f, e := v.field("timestamp")
	if e != nil {
		return 0, e
	}
	return int64(binary.LittleEndian.Uint64(f)), nil
}

// SetTimestamp stores the signing time verbatim, including zero and negative values.
func (v view) SetTimestamp(t int64) error {
	f, e := v.field("timestamp")
	if e != nil {
		return e
	}
	binary.LittleEndian.PutUint64(f, uint64(t))
	return nil
}
func (v view) VerifyCRC() bool { l, e := v.layout(); return e == nil && verify(v.b, l) }

// UpdateCRC reseals the view's original bytes without normalizing reserved content.
func (v view) UpdateCRC() error {
	l, e := v.layout()
	if e != nil {
		return e
	}
	seal(v.b, l)
	return nil
}

// Bytes returns an owned, CRC-sealed serialization. Device-record reserved fields
// and flags are zeroed in the copy; caller memory remains unchanged.
func (v view) Bytes() ([]byte, error) {
	l, e := v.layout()
	if e != nil {
		return nil, e
	}
	out := bytes.Clone(v.b[:l.size])
	if v.record {
		for _, name := range []string{"reserved1", "reserved2", "flags"} {
			f := l.fields[name]
			clear(out[f.offset : f.offset+f.size])
		}
	}
	seal(out, l)
	return out, nil
}

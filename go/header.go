// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
// Package jeefs manipulates JEEFS identity headers and device records over
// caller-owned byte buffers. It performs no I/O or cryptographic verification.
package jeefs

import (
	"bytes"
	"encoding/binary"
	"errors"
	"hash/crc32"
)

var (
	ErrHeader      = errors.New("jeefs: no supported header")
	ErrRecord      = errors.New("jeefs: no supported device record")
	ErrShortBuffer = errors.New("jeefs: buffer too short")
	ErrField       = errors.New("jeefs: field unavailable")
	ErrValue       = errors.New("jeefs: invalid field value")
)

// EmptyState distinguishes absent, placeholder and populated identity headers.
type EmptyState byte

const (
	NoHeader EmptyState = iota
	Placeholder
	Populated
)

// HeaderSize returns zero for an unsupported header version.
func HeaderSize(version byte) int {
	l, ok := headerLayout(version)
	if !ok {
		return 0
	}
	return l.size
}

// DetectVersion inspects only the 12-byte probe; zero means no known header.
// Signature algorithm and CRC are deliberately not interpreted here.
func DetectVersion(b []byte) byte {
	if len(b) < 12 || string(b[:8]) != WireMAGIC+"\x00" {
		return 0
	}
	if HeaderSize(b[8]) == 0 {
		return 0
	}
	return b[8]
}

func headerBytes(b []byte) (wireLayout, error) {
	v := DetectVersion(b)
	if v == 0 {
		return wireLayout{}, ErrHeader
	}
	l, _ := headerLayout(v)
	if len(b) < l.size {
		return wireLayout{}, ErrShortBuffer
	}
	return l, nil
}

func verify(b []byte, l wireLayout) bool {
	return binary.LittleEndian.Uint32(b[l.crc:l.crc+4]) == crc32.ChecksumIEEE(b[:l.crc])
}
func seal(b []byte, l wireLayout) {
	binary.LittleEndian.PutUint32(b[l.crc:l.crc+4], crc32.ChecksumIEEE(b[:l.crc]))
}

// VerifyCRC accepts a full header or a larger image, checking just the header.
func VerifyCRC(b []byte) bool { l, e := headerBytes(b); return e == nil && verify(b, l) }

// UpdateCRC refreshes only the header CRC; it does not interpret its signature.
func UpdateCRC(b []byte) error {
	l, e := headerBytes(b)
	if e != nil {
		return e
	}
	seal(b, l)
	return nil
}

// InitHeader initializes and seals a header, preserving bytes after its end.
// Versions 1 and 2 are supported for conformance, but obsolete for production.
func InitHeader(b []byte, version byte) error {
	l, ok := headerLayout(version)
	if !ok {
		return ErrHeader
	}
	if len(b) < l.size {
		return ErrShortBuffer
	}
	clear(b[:l.size])
	copy(b, WireMAGIC)
	b[8] = version
	seal(b, l)
	return nil
}

// HeaderIsEmpty ignores CRC validity. A recognizable but truncated header is an error.
func HeaderIsEmpty(b []byte) (EmptyState, error) {
	if DetectVersion(b) == 0 {
		return NoHeader, nil
	}
	l, e := headerBytes(b)
	if e != nil {
		return NoHeader, e
	}
	if bytes.Equal(b[12:l.crc], make([]byte, l.crc-12)) {
		return Placeholder, nil
	}
	return Populated, nil
}

// Header aliases caller memory. Do not access it concurrently with mutation.
// Every operation revalidates magic, size and the original version. Setters
// invalidate the stored CRC; call UpdateCRC or Bytes before persistence.
type Header struct{ view }

// NewHeader checks bounds and version, but leaves CRC/signature checks explicit.
func NewHeader(b []byte) (Header, error) {
	l, e := headerBytes(b)
	if e != nil {
		return Header{}, e
	}
	return Header{view{b: b[:l.size], version: b[8]}}, nil
}

// MAC returns a copy of the six raw MAC bytes, preserving 00 and FF values.
func (h Header) MAC() ([6]byte, error) {
	var out [6]byte
	f, e := h.field("mac")
	if e != nil {
		return out, e
	}
	copy(out[:], f)
	return out, nil
}
func (h Header) SetMAC(mac [6]byte) error {
	f, e := h.field("mac")
	if e != nil {
		return e
	}
	copy(f, mac[:])
	return nil
}

// FSVersion returns the independent filesystem version byte, including unknown values.
func (h Header) FSVersion() (byte, error) {
	_, e := h.layout()
	if e != nil {
		return 0, e
	}
	return h.b[WireFS_VERSION_OFFSET], nil
}
func (h Header) SetFSVersion(version byte) error {
	_, e := h.layout()
	if e != nil {
		return e
	}
	h.b[WireFS_VERSION_OFFSET] = version
	return nil
}

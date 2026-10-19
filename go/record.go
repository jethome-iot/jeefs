// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package jeefs

// DetectRecord requires a complete record and known record/signature versions.
// Reserved bits and CRC do not participate in detection.
func DetectRecord(b []byte) bool {
	if len(b) < layoutDeviceIdentityV1.size || string(b[:8]) != WireDEVID_MAGIC+"\x00" || b[8] != 1 {
		return false
	}
	_, ok := signatureSize(SignatureAlgorithm(b[9]))
	return ok
}

// InitRecord zeroes and seals one DeviceIdentityV1, preserving trailing bytes.
func InitRecord(b []byte) error {
	l := layoutDeviceIdentityV1
	if len(b) < l.size {
		return ErrShortBuffer
	}
	clear(b[:l.size])
	copy(b, WireDEVID_MAGIC)
	b[8] = 1
	seal(b, l)
	return nil
}

// Record aliases a caller-owned DeviceIdentityV1 buffer; mutations invalidate CRC.
// Use Bytes to serialize with all reserved fields cleared and CRC refreshed.
type Record struct{ view }

// NewRecord checks size and both versions. VerifyCRC remains an explicit step.
func NewRecord(b []byte) (Record, error) {
	if len(b) < layoutDeviceIdentityV1.size {
		return Record{}, ErrShortBuffer
	}
	if !DetectRecord(b) {
		return Record{}, ErrRecord
	}
	return Record{view{b: b[:layoutDeviceIdentityV1.size], record: true}}, nil
}

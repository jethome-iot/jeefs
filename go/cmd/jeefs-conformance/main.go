// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
// jeefs-conformance is test tooling, not a public image/filesystem API.
package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"hash/crc32"
	"os"
	"strings"

	jeefs "github.com/jethome-iot/jeefs/go"
)

type object map[string]json.RawMessage

func parse(raw []byte) (object, error) {
	var m object
	if e := json.Unmarshal(raw, &m); e != nil {
		return nil, e
	}
	if m == nil {
		return nil, fmt.Errorf("expected object")
	}
	return m, nil
}
func get[T any](m object, key string, def T) (T, error) {
	raw, ok := m[key]
	if !ok {
		return def, nil
	}
	var out T
	if bytes.Equal(bytes.TrimSpace(raw), []byte("null")) {
		return out, fmt.Errorf("%s is null", key)
	}
	if e := json.Unmarshal(raw, &out); e != nil {
		return out, fmt.Errorf("%s: %w", key, e)
	}
	return out, nil
}
func requiredInt(m object, key string) (int, error) {
	if _, ok := m[key]; !ok {
		return 0, fmt.Errorf("missing %s", key)
	}
	return get(m, key, 0)
}
func unhex(m object, key string) ([]byte, error) {
	s, e := get(m, key, "")
	if e != nil {
		return nil, e
	}
	return hex.DecodeString(s)
}
func byteValue(m object, key string) (byte, error) {
	n, e := get(m, key, 0)
	if e != nil {
		return 0, e
	}
	if n < 0 || n > 255 {
		return 0, fmt.Errorf("%s outside byte range", key)
	}
	return byte(n), nil
}

func generate(raw []byte) ([]byte, error) {
	m, e := parse(raw)
	if e != nil {
		return nil, e
	}
	version, e := requiredInt(m, "version")
	if e != nil {
		return nil, e
	}
	if version < 1 || version > 4 {
		return nil, fmt.Errorf("unknown version")
	}
	size, e := requiredInt(m, "header_size")
	if e != nil {
		return nil, e
	}
	if size != jeefs.HeaderSize(byte(version)) {
		return nil, fmt.Errorf("wrong size")
	}
	coverage, e := get(m, "crc_coverage", size-4)
	if e != nil || coverage != size-4 {
		return nil, fmt.Errorf("wrong CRC coverage")
	}
	fields, e := parse(m["fields"])
	if e != nil {
		return nil, e
	}
	b := make([]byte, size)
	if e := jeefs.InitHeader(b, byte(version)); e != nil {
		return nil, e
	}
	h, e := jeefs.NewHeader(b)
	if e != nil {
		return nil, e
	}
	names := []string{"boardname", "boardversion", "serial", "usid", "cpuid"}
	if version == 4 {
		names[2] = "board_serial"
	}
	for _, name := range names {
		s, e := get(fields, name, "")
		if e != nil {
			return nil, e
		}
		if e := h.SetString(name, s); e != nil {
			return nil, e
		}
	}
	mac, e := get(fields, "mac", "")
	if e != nil {
		return nil, e
	}
	if mac != "" {
		parts := strings.Split(mac, ":")
		if len(parts) != 6 {
			return nil, fmt.Errorf("bad MAC")
		}
		var value [6]byte
		for i, p := range parts {
			if len(p) != 2 {
				return nil, fmt.Errorf("bad MAC")
			}
			v, e := hex.DecodeString(p)
			if e != nil {
				return nil, e
			}
			value[i] = v[0]
		}
		if e := h.SetMAC(value); e != nil {
			return nil, e
		}
	}
	fs, e := byteValue(fields, "fs_version")
	if e != nil {
		return nil, e
	}
	if e := h.SetFSVersion(fs); e != nil {
		return nil, e
	}
	if version >= 3 {
		alg, e := byteValue(fields, "signature_version")
		if e != nil {
			return nil, e
		}
		sig, e := unhex(fields, "signature_hex")
		if e != nil {
			return nil, e
		}
		if e := h.SetSignature(jeefs.SignatureAlgorithm(alg), sig); e != nil {
			return nil, e
		}
		timestamp, e := get(fields, "timestamp", int64(0))
		if e != nil {
			return nil, e
		}
		if e := h.SetTimestamp(timestamp); e != nil {
			return nil, e
		}
	}
	return h.Bytes()
}

func verifyHeader(b, raw []byte) error {
	expected, e := generate(raw)
	if e != nil {
		return e
	}
	if !jeefs.VerifyCRC(b) {
		return fmt.Errorf("invalid header CRC/magic/size")
	}
	if !bytes.Equal(b, expected) {
		return fmt.Errorf("header differs from vector")
	}
	return nil
}

func verifyRecord(b, raw []byte) error {
	m, e := parse(raw)
	if e != nil {
		return e
	}
	version, e := requiredInt(m, "record_version")
	if e != nil || version != 1 {
		return fmt.Errorf("wrong record version")
	}
	size, e := requiredInt(m, "record_size")
	if e != nil || size != 256 || len(b) != size {
		return fmt.Errorf("wrong record size")
	}
	coverage, e := requiredInt(m, "crc_coverage")
	if e != nil || coverage != 252 {
		return fmt.Errorf("wrong record coverage")
	}
	r, e := jeefs.NewRecord(b)
	if e != nil {
		return e
	}
	if !r.VerifyCRC() {
		return fmt.Errorf("record CRC")
	}
	fields, e := parse(m["fields"])
	if e != nil {
		return e
	}
	for _, name := range []string{"device_model", "device_serial", "hw_revision"} {
		want, e := get(fields, name, "")
		if e != nil {
			return e
		}
		actual, e := r.String(name)
		if e != nil || actual != want {
			return fmt.Errorf("record %s mismatch", name)
		}
	}
	alg, e := byteValue(fields, "signature_version")
	if e != nil {
		return e
	}
	actual, e := r.SignatureAlgorithm()
	if e != nil || byte(actual) != alg {
		return fmt.Errorf("record algorithm mismatch")
	}
	timestamp, e := get(fields, "timestamp", int64(0))
	if e != nil {
		return e
	}
	got, e := r.Timestamp()
	if e != nil || got != timestamp {
		return fmt.Errorf("record timestamp mismatch")
	}
	signature, e := unhex(fields, "signature_hex")
	if e != nil {
		return e
	}
	sig, e := r.Signature()
	if e != nil || !bytes.Equal(sig, signature) {
		return fmt.Errorf("record signature mismatch")
	}
	if !bytes.Equal(b[180+len(signature):244], make([]byte, 64-len(signature))) {
		return fmt.Errorf("record signature padding")
	}
	return nil
}

func verifyGolden(b, raw []byte) error {
	m, e := parse(raw)
	if e != nil {
		return e
	}
	size, e := requiredInt(m, "eeprom_size")
	if e != nil || len(b) != size {
		return fmt.Errorf("image size")
	}
	header, e := parse(m["header"])
	if e != nil {
		return e
	}
	version, e := requiredInt(header, "version")
	if e != nil || version < 1 || version > 4 {
		return fmt.Errorf("header version")
	}
	hs, e := requiredInt(header, "header_size")
	if e != nil || hs != jeefs.HeaderSize(byte(version)) || hs > len(b) {
		return fmt.Errorf("header size")
	}
	// Golden metadata flattens the header fields; reuse the vector verifier.
	vector := object{"version": header["version"], "header_size": header["header_size"]}
	vector["fields"], e = json.Marshal(header)
	if e != nil {
		return e
	}
	meta, e := json.Marshal(vector)
	if e != nil {
		return e
	}
	if e := verifyHeader(b[:hs], meta); e != nil {
		return e
	}
	if b[10] != 1 {
		return fmt.Errorf("filesystem version")
	}
	var files []object
	fileList, present := m["files"]
	if !present || bytes.Equal(bytes.TrimSpace(fileList), []byte("null")) {
		return fmt.Errorf("files must be a non-null array")
	}
	if e := json.Unmarshal(fileList, &files); e != nil {
		return e
	}
	off := hs
	for i, file := range files {
		if off < hs || off > len(b)-28 {
			return fmt.Errorf("file header bounds")
		}
		entry := b[off : off+28]
		if crc32.ChecksumIEEE(entry[:24]) != binary.LittleEndian.Uint32(entry[24:28]) {
			return fmt.Errorf("file header CRC")
		}
		nul := bytes.IndexByte(entry[:16], 0)
		if nul < 1 {
			return fmt.Errorf("file name termination")
		}
		name, e := get(file, "name", "")
		if e != nil || string(entry[:nul]) != name {
			return fmt.Errorf("file name")
		}
		data, e := unhex(file, "data_hex")
		if e != nil {
			return e
		}
		n := int(binary.LittleEndian.Uint16(entry[16:18]))
		end := off + 28 + n
		if end > len(b) || n != len(data) || !bytes.Equal(b[off+28:end], data) {
			return fmt.Errorf("file payload")
		}
		if crc32.ChecksumIEEE(data) != binary.LittleEndian.Uint32(entry[18:22]) {
			return fmt.Errorf("file data CRC")
		}
		if name == jeefs.WireDEVICE_ID_FILENAME {
			r, e := jeefs.NewRecord(data)
			if e != nil || !r.VerifyCRC() || i != 0 {
				return fmt.Errorf("device identity file")
			}
		}
		next := int(binary.LittleEndian.Uint16(entry[22:24]))
		if i == len(files)-1 {
			if next != 0 && next != 65535 {
				return fmt.Errorf("last link")
			}
		} else if next != end {
			return fmt.Errorf("noncontiguous link")
		}
		off = next
	}
	if len(files) == 0 && hs <= len(b)-28 {
		for _, v := range b[hs : hs+28] {
			if v != 0 && v != 255 {
				return fmt.Errorf("unexpected file")
			}
		}
	}
	return nil
}

// mode is fixed by the CMake-built wrapper's linker flag, or supplied as a subcommand.
var mode string

func main() {
	args := os.Args[1:]
	op := mode
	if op == "" {
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "expected generate|verify|golden|record")
			os.Exit(2)
		}
		op = args[0]
		args = args[1:]
	}
	if len(args) != 2 {
		fmt.Fprintln(os.Stderr, "expected two paths")
		os.Exit(2)
	}
	var e error
	if op == "generate" {
		var raw, b []byte
		raw, e = os.ReadFile(args[0])
		if e == nil {
			b, e = generate(raw)
		}
		if e == nil {
			e = os.WriteFile(args[1], b, 0644)
		}
	} else {
		var b, raw []byte
		b, e = os.ReadFile(args[0])
		if e == nil {
			raw, e = os.ReadFile(args[1])
		}
		if e == nil {
			switch op {
			case "verify":
				e = verifyHeader(b, raw)
			case "golden":
				e = verifyGolden(b, raw)
			case "record":
				e = verifyRecord(b, raw)
			default:
				e = fmt.Errorf("unknown command")
			}
		}
	}
	if e != nil {
		fmt.Fprintln(os.Stderr, e)
		os.Exit(1)
	}
	fmt.Println("OK (Go)")
}

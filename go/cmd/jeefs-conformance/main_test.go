// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
package main

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

func TestCommittedVectors(t *testing.T) {
	files, e := filepath.Glob("../../../test-vectors/vectors/v*.json")
	if e != nil || len(files) == 0 {
		t.Fatal(e)
	}
	for _, p := range files {
		raw, e := os.ReadFile(p)
		if e != nil {
			t.Fatal(e)
		}
		data, e := generate(raw)
		if e != nil {
			t.Fatal(p, e)
		}
		gold, e := os.ReadFile(p[:len(p)-5] + ".bin")
		if e != nil || !bytes.Equal(data, gold) {
			t.Fatal(p, e)
		}
		if e := verifyHeader(gold, raw); e != nil {
			t.Fatal(p, e)
		}
		data[20] ^= 1
		if verifyHeader(data, raw) == nil {
			t.Fatal("accepted corrupt header")
		}
	}
}
func TestRejectMalformedMetadata(t *testing.T) {
	raw, e := os.ReadFile("../../../test-vectors/vectors/v3_header_secp192r1.json")
	if e != nil {
		t.Fatal(e)
	}
	for _, bad := range []string{"null", "true", "1.0", "9223372036854775808", "\"oops\""} {
		mutated := bytes.Replace(raw, []byte("1755300000"), []byte(bad), 1)
		if _, e := generate(mutated); e == nil {
			t.Fatal(bad)
		}
	}
}
func TestGoldenAndRecord(t *testing.T) {
	for _, name := range []string{"eeprom_full", "eeprom_full_v4"} {
		b, e := os.ReadFile("../../../test-vectors/reference/" + name + ".bin")
		if e != nil {
			t.Fatal(e)
		}
		raw, e := os.ReadFile("../../../test-vectors/reference/" + name + ".json")
		if e != nil {
			t.Fatal(e)
		}
		if e := verifyGolden(b, raw); e != nil {
			t.Fatal(e)
		}
		b[280] ^= 1
		if verifyGolden(b, raw) == nil {
			t.Fatal("accepted corrupt file header")
		}
	}
	b, e := os.ReadFile("../../../test-vectors/vectors/devid_record_v1.bin")
	if e != nil {
		t.Fatal(e)
	}
	raw, e := os.ReadFile("../../../test-vectors/vectors/devid_record_v1.json")
	if e != nil {
		t.Fatal(e)
	}
	if e := verifyRecord(b, raw); e != nil {
		t.Fatal(e)
	}
}

func TestGoldenRejectMalformedFileList(t *testing.T) {
	b, e := os.ReadFile("../../../test-vectors/reference/eeprom_full_v4.bin")
	if e != nil {
		t.Fatal(e)
	}
	raw, e := os.ReadFile("../../../test-vectors/reference/eeprom_full_v4.json")
	if e != nil {
		t.Fatal(e)
	}
	clear(b[256:]) // A valid empty filesystem prevents payload checks hiding metadata bugs.
	m, e := parse(raw)
	if e != nil {
		t.Fatal(e)
	}
	m["files"] = json.RawMessage("[]")
	valid, e := json.Marshal(m)
	if e != nil {
		t.Fatal(e)
	}
	if e := verifyGolden(b, valid); e != nil {
		t.Fatal("valid empty list", e)
	}
	for _, bad := range []string{"null", "{}", "0", "\"files\"", "missing"} {
		if bad == "missing" {
			delete(m, "files")
		} else {
			m["files"] = json.RawMessage(bad)
		}
		malformed, e := json.Marshal(m)
		if e != nil {
			t.Fatal(e)
		}
		if verifyGolden(b, malformed) == nil {
			t.Errorf("accepted %s", bad)
		}
	}
}

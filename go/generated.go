// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
// Code generated from docs/format/*.md; DO NOT EDIT.
// See docs/CODEGEN.md. Wire types are byte arrays, never native structs.
package jeefs

type fieldLayout struct {
	offset, size int
	terminated   bool
}
type wireLayout struct {
	size, crc int
	fields    map[string]fieldLayout
}

// SignatureAlgorithm identifies stored signature bytes, without cryptographic verification.
type SignatureAlgorithm byte

const (
	SignatureNONE      SignatureAlgorithm = 0
	SignatureSECP192R1 SignatureAlgorithm = 1
	SignatureSECP256R1 SignatureAlgorithm = 2
)

const (
	WireBOARDNAME_LENGTH     = 31
	WireBOARDVERSION_LENGTH  = 31
	WireCPUID_LENGTH         = 32
	WireDEVICE_ID_FILENAME   = "device.id"
	WireDEVID_MAGIC          = "JHDEVID"
	WireEMPTYBYTE            = 0x00
	WireERASEDBYTE           = 0xFF
	WireFILE_NAME_LENGTH     = 15
	WireFS_VERSION           = 1
	WireFS_VERSION_OFFSET    = 10
	WireHEADER_VERSION       = 4
	WireMAC_LENGTH           = 6
	WireMAGIC                = "JETHOME"
	WireMAGIC_LENGTH         = 8
	WirePARTITION_SIZE       = 4096
	WireSERIAL_LENGTH        = 32
	WireSIGNATURE_FIELD_SIZE = 64
	WireUSID_LENGTH          = 32
)

func signatureSize(a SignatureAlgorithm) (int, bool) {
	switch a {
	case SignatureNONE:
		return 0, true
	case SignatureSECP192R1:
		return 48, true
	case SignatureSECP256R1:
		return 64, true
	}
	return 0, false
}

// HeaderV4Wire is the packed 256-byte wire representation.
type HeaderV4Wire [256]byte

var layoutHeaderV4 = wireLayout{256, 252, map[string]fieldLayout{
	"magic":             {0, 8, false},
	"version":           {8, 1, false},
	"signature_version": {9, 1, false},
	"fs_version":        {10, 1, false},
	"header_reserved":   {11, 1, false},
	"boardname":         {12, 32, true},
	"boardversion":      {44, 32, true},
	"board_serial":      {76, 32, false},
	"usid":              {108, 32, false},
	"cpuid":             {140, 32, false},
	"mac":               {172, 6, false},
	"reserved2":         {178, 2, false},
	"signature":         {180, 64, false},
	"timestamp":         {244, 8, false},
	"crc32":             {252, 4, false},
}}

// HeaderV3Wire is the packed 256-byte wire representation.
type HeaderV3Wire [256]byte

var layoutHeaderV3 = wireLayout{256, 252, map[string]fieldLayout{
	"magic":             {0, 8, false},
	"version":           {8, 1, false},
	"signature_version": {9, 1, false},
	"fs_version":        {10, 1, false},
	"header_reserved":   {11, 1, false},
	"boardname":         {12, 32, true},
	"boardversion":      {44, 32, true},
	"serial":            {76, 32, false},
	"usid":              {108, 32, false},
	"cpuid":             {140, 32, false},
	"mac":               {172, 6, false},
	"reserved2":         {178, 2, false},
	"signature":         {180, 64, false},
	"timestamp":         {244, 8, false},
	"crc32":             {252, 4, false},
}}

// HeaderV2Wire is the packed 256-byte wire representation.
type HeaderV2Wire [256]byte

var layoutHeaderV2 = wireLayout{256, 252, map[string]fieldLayout{
	"magic":        {0, 8, false},
	"version":      {8, 1, false},
	"reserved1":    {9, 3, false},
	"boardname":    {12, 32, true},
	"boardversion": {44, 32, true},
	"serial":       {76, 32, false},
	"usid":         {108, 32, false},
	"cpuid":        {140, 32, false},
	"mac":          {172, 6, false},
	"reserved2":    {178, 2, false},
	"reserved3":    {180, 72, false},
	"crc32":        {252, 4, false},
}}

// HeaderV1Wire is the packed 512-byte wire representation.
type HeaderV1Wire [512]byte

var layoutHeaderV1 = wireLayout{512, 508, map[string]fieldLayout{
	"magic":        {0, 8, false},
	"version":      {8, 1, false},
	"reserved1":    {9, 3, false},
	"boardname":    {12, 32, true},
	"boardversion": {44, 32, true},
	"serial":       {76, 32, false},
	"usid":         {108, 32, false},
	"cpuid":        {140, 32, false},
	"mac":          {172, 6, false},
	"reserved2":    {178, 2, false},
	"modules":      {180, 32, false},
	"reserved3":    {212, 296, false},
	"crc32":        {508, 4, false},
}}

// DeviceIdentityV1Wire is the packed 256-byte wire representation.
type DeviceIdentityV1Wire [256]byte

var layoutDeviceIdentityV1 = wireLayout{256, 252, map[string]fieldLayout{
	"magic":             {0, 8, false},
	"record_version":    {8, 1, false},
	"signature_version": {9, 1, false},
	"reserved1":         {10, 2, false},
	"device_model":      {12, 32, false},
	"device_serial":     {44, 32, false},
	"hw_revision":       {76, 16, false},
	"flags":             {92, 2, false},
	"reserved2":         {94, 86, false},
	"signature":         {180, 64, false},
	"timestamp":         {244, 8, false},
	"crc32":             {252, 4, false},
}}

// JEEFSFileHeaderv1Wire is the packed 28-byte wire representation.
type JEEFSFileHeaderv1Wire [28]byte

// HeaderVersionWire is the packed 12-byte wire representation.
type HeaderVersionWire [12]byte

func headerLayout(version byte) (wireLayout, bool) {
	switch version {
	case 4:
		return layoutHeaderV4, true
	case 3:
		return layoutHeaderV3, true
	case 2:
		return layoutHeaderV2, true
	case 1:
		return layoutHeaderV1, true
	}
	return wireLayout{}, false
}

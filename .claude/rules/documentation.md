# Documentation rules

Apply to `docs/**`, `EEPROM_FORMAT.md`, `README.md` and the Outline mirror
(<https://docs.iot.jethome.com/doc/format-eeprom-aktualnaya-specifikaciya-zerkalo-jeefsdocs-aKnx9Jdfy1>).

## Write for humans

- A format page answers three questions: **what it is, how to write it,
  how to read it**. A field table alone is not documentation — put the
  step-by-step write and read procedures next to it.
- No tracker noise: no issue/RFC status lines ("accepted 2026-08-14",
  "closed"), no milestone references, no decision dates inside format
  descriptions. Rationale belongs in the text as a sentence explaining
  *why*; link an RFC only when the discussion itself is the reference
  material.
- Reserved names, magic values, offsets and procedures are stated
  concretely — never "see the code".

## Outline mirror

- Structure: the main page carries the current state only — common
  properties, the current header version in full, the filesystem, a short
  device-identity intro. Subpages, one topic each: the `device.id` record
  (full write/read procedures) and one page per historical header version.
- Sync is one-way: `docs/format/*.md` is the source of truth; the mirror
  never introduces format facts of its own. Update the mirror as part of
  every release (see `docs/RELEASING.md`).
- The parent Outline document is a historical snapshot — never edit it.

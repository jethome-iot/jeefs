# Documentation rules

Apply to `docs/**`, `EEPROM_FORMAT.md`, `README.md` and the Outline mirror
(<https://docs.iot.jethome.com/doc/format-eeprom-aktualnaya-specifikaciya-zerkalo-jeefsdocs-aKnx9Jdfy1>).

## Write for humans

- A format page answers three questions: **what it is, how to write it,
  how to read it**. A field table alone is not documentation — put the
  step-by-step write and read procedures next to it.
- No tracker noise in format descriptions (`docs/format/*.md`,
  `EEPROM_FORMAT.md`, the mirror): no issue/RFC status lines
  ("accepted 2026-08-14", "closed"), no milestone references, no decision
  dates. Rationale belongs in the text as a sentence explaining *why*;
  link an RFC only when the discussion itself is the reference material.
  Planning documents (`docs/ROADMAP.md`, `docs/TODO.md`) are the opposite
  genre — issue and milestone references are their content.
- Reserved names, magic values, offsets and procedures are stated
  concretely — never "see the code".

## Outline mirror

The mirror is the document linked above. It is nested under the original
2023 «Формат eeprom» page — a separate, historical document.

- Structure: the mirror's main page carries the current state only — common
  properties, the current header version in full, the filesystem, a short
  device-identity intro. Subpages, one topic each: the `device.id` record
  (full write/read procedures) and one page per historical header version.
- Sync is one-way: `docs/format/*.md` is the source of truth; the mirror
  never introduces format facts of its own. Update the mirror as part of
  every release (see `docs/RELEASING.md`).
- The historical parent page is never edited.

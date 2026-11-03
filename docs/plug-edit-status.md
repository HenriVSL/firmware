# plug-edit: status and plan

A work note for this branch. Drop it before the branch is offered upstream.

## Where it stands

Branch `feat/plug-editor`, on the open UI stack (settings store, menu,
home screen, microphone gain, frequency offset); not tagged and not
merged into `oh6hl-main` yet. Not run on the radio in this form.

- Menu > Tools > Codeplug edits the codeplug on the radio: books, then
  records, then the named fields, then every byte of the record (`Bytes`
  page), so any value can be changed even without a named field.
- The screens use the menu's helpers: lists and the record's fields as
  the menu's lists with the panel, values as its buttons or spinner. The
  keypad screens for digits and text and the save summary are the
  editor's own (`app/src/plug_screens.cpp`).
- Saving and the settings store: the store writes what it keeps before a
  record is opened and before it is saved, and the save writes only the
  bits changed in the editor over the record as the codeplug holds it
  then. Both run on the ui task and share the board's writer.
- The console has `plug peek <addr> [count]` and `plug poke <addr> <bytes>`
  (hex). Writes go through the board's writer, fenced to the codeplug's
  own ranges; the MCU's internal flash is never written.
- `prog watch on|off` logs, run by run, the bytes each write from the PC
  changes (old and new), with a total at the end of the write.
- The earlier line (0.20.3-6HL-12 to 0.24.0-6HL-6) was checked on the
  radio: peek, a poke on a test channel and a read-back in QDMR showing
  only that change.

## Field names

About 200 fields are named: the channel record (basic, DMR, FM and APRS
pages), general and extended settings (radio, display, audio, DMR, boot,
keys, roaming), APRS (sending, receive filter, fixed position, all eight
frequencies), contacts, radio IDs and roaming repeaters. Offsets come
from QDMR's D878UVII encoding, checked by encoding a codeplug with one
setting changed and diffing the images (`dmrconf encode`, no radio
needed). `prog watch on` still shows what a real write from the PC
changes.

Still unnamed: melodies, auto-repeater limits, VFO scan limits, the
channel extension record (+0x2000), zone member lists.

## Later

- Add, copy and delete records.
- Zone channel membership.
- Contact numbers: needs the sorted ID table rewritten too.
- Repeaters and roam zones: the radio takes a saved one at the next
  start; reloading the roaming zone at once would start a search.
- Run `docs/hw-tests/plug-editor.md` on the radio, and check the ui
  task's stack.

## Branch housekeeping

- After the hardware tests: add to the fork's branch list for
  `oh6hl-main`, bump the fork release and tag.
- For upstream, offer the editor in parts from a branch on upstream's
  base, without this note, the codeplug addresses in docs or the fork's
  frequency offset.

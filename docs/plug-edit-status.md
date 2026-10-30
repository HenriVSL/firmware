# plug-edit: status and plan

A work note for this branch. Drop it before the branch is offered upstream.

## Where it stands

Version 0.20.3-6HL-12, not tagged and not merged into `oh6hl-main`.

- Menu > Codeplug edits the codeplug on the radio: books, then records,
  then the named fields, then every byte of the record (`Bytes` page), so
  any value can be changed even without a named field.
- The console has `plug peek <addr> [count]` and `plug poke <addr> <bytes>`
  (hex). Writes go through the board's writer, fenced to the codeplug's
  own ranges; the MCU's internal flash is never written.
- `prog watch on|off` logs, run by run, the bytes each write from the PC
  changes (old and new), with a total at the end of the write.
- Checked on the radio: peek, a poke on test channel #16 (index 15,
  address 0x008003C0) and a read-back in QDMR showing only that change.
  Channel #16 was restored; the codeplug matches the last backup.
- The radio runs an earlier 6HL-12 build, without `prog watch`. The image
  from de58f18 is built but not flashed yet.

## Field names

About 200 fields are named: the channel record (basic, DMR, FM and APRS
pages), general and extended settings (radio, display, audio, DMR, boot,
keys, roaming), APRS (sending, receive filter, fixed position, all eight
frequencies), contacts, radio IDs and roaming repeaters. Offsets come
from a field reference written from QDMR's D878UVII encoding
(`pr-drafts/qdmr-d878uv2-codeplug-fields.md` in the workspace) and are
checked by encoding a codeplug with one setting changed and diffing the
images (`dmrconf encode`, no radio needed). `prog watch on` still shows
what a real write from the PC changes.

Still unnamed: melodies, auto-repeater limits, VFO scan limits, the
channel extension record (+0x2000), zone member lists.

## Later

- Add, copy and delete records.
- Zone channel membership.
- Contact numbers: needs the sorted ID table at 0x04340000 rewritten too.
- Run `docs/hw-tests/plug-editor.md` and `docs/hw-tests/menu.md` on the
  radio, and check the UI task's stack.

## Branch housekeeping

- Reword db6284f when rebasing: its message tells how the fields were
  found.
- After the hardware tests: merge into `oh6hl-main` and tag.
- `oh6hl-main` does not have upstream 0.21.0 yet (flasher merged there);
  merging it would make the next release 0.21.0-6HL-1.
- For upstream, offer the editor in parts from a branch on upstream's
  base, without this note, the codeplug addresses in docs or the fork's
  frequency offset.

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

## Next: name the fields with the factory CPS

The editor can change every byte, but most fields have no name yet. The
plan is to change one setting at a time in the factory CPS and see which
bytes the write changes.

Setup:

1. Flash `DeltaMM_0.20.3-6HL-12.CDD` from
   `build/fw-d878uv2-release/boards/anytone_d878uv2/package/`.
2. Start the workspace's `scripts/watchlog.py codeplugs/cps-watch.log` in
   the background. It keeps the log port open across restarts and sends
   `prog watch on` on every connect.

Each round:

1. Read the radio in the CPS (under Wine, its COM port pointed at the
   DeltaMM data port).
2. Change one setting; note the CPS's name for it and its old and new
   value.
3. Write from the CPS. The radio restarts after the write.
4. Read the changed bytes from the log, add the field to the tables in
   `services/src/d878uv2_plug_editor.cpp` under the CPS's name, and add
   a case to `tests/services/test_d878uv2_plug_editor.cpp`.

Start each round from a fresh CPS read, so the CPS writes back what the
radio has. Good first targets: the rest of the channel record, then the
general settings.

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

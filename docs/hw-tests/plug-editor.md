# Codeplug editor on the radio

Menu > Codeplug lists the codeplug's books: Channels, Zones, Contacts, RX
groups, Repeaters, Roam zones, Radio IDs, Settings, More settings, APRS and
APRS text. A book lists its records by name; a record lists its named
fields on pages (P1 and P2 turn them) and then each of its bytes. The
panel under the fields says what the chosen one takes, or what it was
before a change. Changes stay on the radio until saved: leaving a record
with changes lists them with their old values, Menu saves, `#` drops them
and Exit goes back to editing. Saving goes through the codeplug writer, as
a write from the PC does: never while the PC writes, and only to the
codeplug's own blocks. A saved channel or zone is used at once; settings,
more settings and APRS restart the radio.

The console's `plug peek <address> [count]` logs codeplug bytes and
`plug poke <address> <byte>...` writes them (hexadecimal, up to 32 bytes),
through the same writer.

Before starting, read the codeplug with the PC (QDMR or the CPS) and keep
the file: it puts everything back.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Menu > Codeplug > Channels | The channels by name with their number, `n/N` in the title band |
| 2 | Menu on a channel | Its name, frequencies, mode, power, bandwidth, RX only; P2 turns to DMR (colour code, slot, contact, radio ID, RX group, roaming), FM (tones, squelch), then Bytes |
| 3 | Menu on Power, choose another, Menu | Back in the list with the new power in amber; the title ends with `*` |
| 4 | Menu on RX, type a frequency, Menu | The digits typed bright, the rest of the old frequency dim; the TX frequency stays as it was |
| 5 | Menu on Name, type with the keypad: a key, the same key again, `#`, `*`, the knob | Letters as on a phone keypad, capitals with `#`, `*` deletes, the knob steps the letter under the cursor |
| 6 | Menu on Contact | The contacts listed; Menu picks one, `#` sets none |
| 7 | Exit | `Save <name>?` with each change, its old value in the panel |
| 8 | Menu | `Saved`; back in the list; on the working screen the channel shows the changes |
| 9 | Read the codeplug with the PC | The changed channel as edited, every other channel and setting as before |
| 10 | Edit a channel again, Exit, `#` | `Changes dropped`; the channel as it was |
| 11 | Menu > Codeplug > Settings, change Light time, save | `Saved: restarting`; the radio restarts with the new light time |
| 12 | Start a write from the PC, then save an edit on the radio | `The PC is writing`; nothing saved |
| 13 | Console: `plug peek 2500000 10` | Sixteen bytes of the settings logged |
| 14 | Console: `plug poke 2F00000 00` | `refused or failed`: outside the codeplug's own blocks |
| 15 | Read the latest uptime line after using the editor | Every task above 50 unused stack words |

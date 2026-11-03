# Codeplug editor on the radio

Menu > Tools > Codeplug lists the codeplug's books: Channels, Zones,
Contacts, RX groups, Repeaters, Roam zones, Radio IDs, Settings, More
settings, APRS and APRS text. A book lists its records by name and number;
a record lists its named fields on pages (P1 and P2 turn them) and then
each of its bytes. The panel under the fields says what the chosen one
takes, or what it was before a change; a changed value is amber. Values
are chosen on the menu's value screens (up to four as buttons, more as a
large value), frequencies and large numbers typed on the keypad, names
tapped letter by letter. Changes stay in the editor until saved: leaving
a record with changes lists them with their old values; Menu saves, `#`
drops them and Exit goes back to editing.

Saving goes through the codeplug writer, as a write from the PC does:
never while the PC writes, and only to the codeplug's own blocks. The
settings store writes what it keeps first, and the save writes only the
bits changed in the editor over the record as the codeplug holds it then.
A saved channel, zone, contact, RX group or radio ID is read again at
once; Settings and More settings restart the radio.

The console's `plug peek <address> [count]` logs codeplug bytes and
`plug poke <address> <byte>...` writes them (hexadecimal, up to 0x100 and
32 bytes), through the same writer; a `plug` command typed before the one
before it has run gets `no such hardware, or it is busy`. `prog watch on|off` logs each run of
bytes a write from the PC changes, old and new, and the total.

On this branch Tools starts with Codeplug: in `menu.md` step 2 the panel
reads `Codeplug editor, log, key test and version`, and step 17 needs Down
three times.

Before starting, read the codeplug with the PC (QDMR or the CPS) and keep
the file: it puts everything back. Keep roaming off during the steps.

## Console steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | `plug peek 2500000 10`; `plug peek 2500000 16` | One line of sixteen bytes from `02500000:`; then 0x16 bytes, on two lines |
| 2 | `plug peek 2500000 101`, then `plug peek 2500000 zz` | The usage line each time: `peek <address> [count up to 100], in hex` |
| 3 | `plug poke 2F00000 00`; `plug peek 2F00000 1` | `poke 02f00000, 1 bytes: refused or failed`; the byte as before |
| 4 | `settings`; note `battery save <n>` | The codeplug's battery save |
| 5 | `save 2`, then at once `plug poke 250000B 00`; `plug peek 2500000 10`; `settings` | `settings kept in the codeplug` before `poke 0250000b, 1 bytes: written`; byte 0x0B reads `00`; `nothing pending` |
| 6 | `save <n>` from step 4; wait 3 s; `settings` | `battery save <n>` again |
| 7 | `prog watch on`; write the codeplug from the PC unchanged | `watching writes from the PC on`; at the end `the PC changed <n> bytes`, few or none; the radio restarts |
| 8 | `prog watch on`; change one setting in the PC tool and write | One or a few `changed <address>: <old> -> <new>` lines and the total; the radio restarts |

## Key steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Menu, Up, Menu; look | `Menu > Tools`: Codeplug first; the panel `Codeplug editor, log, key` / `test and version` |
| 2 | Menu; look | `Tools > Codeplug`, `1/11`: Channels, Zones, Contacts, RX groups, Repeaters and a scroll bar, each with `>` |
| 3 | Menu; look | `Channels`: the channels by name with `#n`, `1/N` in the title band; hint `Menu: open   Exit: back` |
| 4 | Down to a test channel; Menu; look | Its name in the title, `1/M`; Name, RX, TX, Mode, Power; the panel `Basic` / `Up to 16 letters`; hint `Menu: edit P2: page Exit: done` |
| 5 | P2, P2, P2, P1; look each time | DMR (colour code, slot, contact, radio ID, RX group, roaming), FM (tones, squelch), APRS, then back on FM |
| 6 | P1 until Basic; Down four times to Power; Menu; Down, Menu | Four buttons, the power in use starred; back on Power, the new power amber, the title ending in `*` |
| 7 | Up three times to RX; Menu; type `1453`; look; Menu | The digits typed bright, the rest of the old frequency dim, `MHz`; RX shows `145.3...`, TX as it was |
| 8 | Up to Name; Menu; press 2, 2 within a second, then 3; `#`, 3; `*`; knob 1 on; look; Menu | `B`, then `D` after it; `#` turns to lower case, `d`; `*` deletes it; the knob steps the character under the cursor; Name shows the text |
| 9 | P2 to DMR; Down twice to Contact; Menu; look; Exit; Menu, `#` | The contacts listed, titled `Contact`, hint `Menu: pick  #: none  Exit: back`; Exit keeps the contact; `#` sets `None` |
| 10 | Exit | `Save <name>?`, one row per change with its new value amber, the chosen one's old value in the panel (`Was ...`) and `<n> changes` |
| 11 | Menu | `Saved`; back in the channel list on that channel; `ui: codeplug: saved Channels <n>` |
| 12 | Exit four times; step to the channel | The working screen shows the new name and frequency |
| 13 | Read the codeplug with the PC | The channel as edited, every other channel and setting as before |
| 14 | Menu > Tools > Codeplug > Channels, the same channel; change Power; Exit; `#` | `Changes dropped`; the channel list; the power as before |
| 15 | Menu > Channel > Power: another power, Menu; within 3 s Menu > Tools > Codeplug > Channels, the channel in use; change Bandwidth; Exit, Menu | `Saved`; `plug peek` of the channel and the PC both show the new power and the new bandwidth |
| 16 | Menu > Tools > Codeplug > Settings; look; Exit | The settings record at once, no list; Exit goes back to the Codeplug group on Settings |
| 17 | Menu on Settings; Down to Light time; Menu; another time, Menu; Exit, Menu | `Saved: restarting`; `codeplug edited on the radio: restarting`; the radio starts with the new light time |
| 18 | Start a write from the PC; while it runs, save an edit on the radio | `The PC is writing`; the save screen stays; nothing saved |
| 19 | Menu > Tools > Codeplug > Radio IDs, an ID; Menu on ID; look; Exit | The ID on the keypad screen, as digits; Exit leaves it |
| 20 | After using the editor, wait for the `system: uptime ...` line | `unused stack words: ui` above 50 |

## Undo

- Write back the codeplug read before the steps (QDMR or the CPS); the
  radio restarts with it.

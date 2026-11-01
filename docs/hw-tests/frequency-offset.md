# Frequency offset

Checks the frequency offset (`docs/frequency-offset.md`) on a D878UVII:
the menu row, the console command, the value kept across restarts, and
that receive, transmit and DMR follow it.

Needs: the radio with the USB log port open in a terminal, a dummy load
or a short antenna, and a way to read the transmit frequency: a frequency
counter, a service monitor, or a receiver whose own frequency error is
known. For section 6, a DMR repeater in range and a talkgroup it carries.

Use an FM channel the radio may transmit on, at low power, without a
tone, on a free simplex frequency.

## Running it

The steps are key presses on the radio and what its screen then shows;
log lines named under Expect come on the log port. Keys are named as on
the key test (Menu, Exit, Up, Down, PTT). "Knob 1 on" turns the channel
knob one click clockwise, "knob 2 back" two clicks anticlockwise. Each
step starts where the one before ended.

## 1. The radio's own error

| # | Do | Expect |
| --- | --- | --- |
| 1 | Type `fm offset 0`, restart the radio, type `fm status` | The line ends `offset 0 Hz` |
| 2 | Hold PTT for about 3 s; read the counter | Note the difference from the channel's frequency (measured minus nominal): the radio's own error, `E` |

## 2. The menu

| # | Do | Expect |
| --- | --- | --- |
| 1 | On the standby screen press Menu, Down four times, Menu | `Menu > Settings`, `1/6`: Home screen, Audio, Radio, Keys & battery, Clock, and a scroll bar for GPS |
| 2 | Press Down twice; look | Radio at `3/6`, with `>`; the panel reads `Frequency offset for a` / `drifted reference` |
| 3 | Press Menu; look | `Settings > Radio`: the row Freq offset with `0 Hz`; the panel `Shifts every frequency;` / `Up and Down 500 Hz` |
| 4 | Press Menu; look | Title `Freq offset`; `0 Hz` large between arrows; a marker in the middle of a track; `In use: 0 Hz` under it |
| 5 | Knob 1 on; look; knob 2 back; look | `+62.5 Hz`, then `-62.5 Hz`; the marker moves a little each click |
| 6 | Press Up four times; look | `+1937.5 Hz` |
| 7 | Press Exit; look | The list shows `0 Hz`; no `frequency offset` line in the log |
| 8 | Press Menu, then Up four times; look | `+2000 Hz` |
| 9 | Press Menu; look | `fm: frequency offset +2000 Hz`; the list shows `+2000 Hz`; `settings kept` about 3 s later |
| 10 | Hold PTT for about 3 s; read the counter | 2000 Hz above the reading of 1.2, within the counter's accuracy |
| 11 | Press Menu, then Up eight times; look; knob 1 on; look | `+5000 Hz` after the sixth press, and it stays there: neither Up nor the knob goes round to the other end |
| 12 | Press Down twenty times; look; knob 1 back; look | `-5000 Hz`, and it stays there |
| 13 | Press Exit | The list still shows `+2000 Hz` |

## 3. Kept across a restart

| # | Do | Expect |
| --- | --- | --- |
| 1 | Restart the radio; type `fm status` | During start `fm: frequency offset +2000 Hz`; the line ends `offset +2000 Hz` |
| 2 | Type `settings` | The `preferences record:` line ends `frequency offset +2000 Hz` |
| 3 | Open Menu > Settings > Radio as in 2.1 to 2.3 | The row shows `+2000 Hz` |
| 4 | Press Exit until the standby screen is back; hold PTT for about 3 s | The counter reads as in 2.10 |

## 4. The console

| # | Do | Expect |
| --- | --- | --- |
| 1 | Type `fm offset -2000`; hold PTT for about 3 s | `fm: frequency offset -2000 Hz`; the counter reads 2000 Hz below the reading of 1.2 |
| 2 | Type `fm offset 1950` | `fm: frequency offset +1937.5 Hz`, the nearest 62.5 Hz step |
| 3 | Type `fm offset 62`, then `fm offset -31`, then `fm offset -32` | `+62.5 Hz`, `0 Hz` and `-62.5 Hz` |
| 4 | Type `fm offset 6000`, then `fm offset 12x` | Both `unknown command, try help`; `fm status` still ends `offset -62.5 Hz` |
| 5 | Open Menu > Settings > Radio as in 2.1 to 2.3 | The row shows `-62.5 Hz`: the console's value, kept as the menu's is |

## 5. Correcting the radio

| # | Do | Expect |
| --- | --- | --- |
| 1 | Type `fm offset <-E>`, the error from 1.2 with its sign turned round; hold PTT for about 3 s | The counter reads the channel's frequency, within 62.5 Hz |
| 2 | Receive a carrier on a known exact frequency (a signal generator, or a station known to be accurate) on FM; type `fm status` while it is on | The carrier is heard clearly; the line shows the nominal frequency and the offset |
| 3 | While it is on, type `fm offset 0`, then the value of 5.1 again | Each change logged; the receiver tunes again at once, without a channel change |

## 6. DMR

| # | Do | Expect |
| --- | --- | --- |
| 1 | Type `fm offset 0`; select a DMR channel on the repeater; call the talkgroup with PTT and listen for other calls | Note whether calls decode and the repeater answers |
| 2 | Type `fm offset <-E>` as in 5.1 and repeat | Calls decode and the repeater answers; on a radio with a large error, only with the offset |

## Undo

Set Menu > Settings > Radio > Freq offset to `0 Hz`, or type
`fm offset 0`; either is kept.

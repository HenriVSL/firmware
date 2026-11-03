# APRS mode

APRS mode holds the radio's one receiver on the codeplug's APRS frequency
and decodes the audio on the MCU until it is turned off
(`docs/dedicated-modes.md`). Everything below runs from the console
(`key <name> [long]`, `knob <steps>`, `screen shot`, `aprs ...`, `fm ...`)
except where a step says a person is needed.

Logs and screen shots hold other stations' callsigns: keep them on the
PC, out of the repository; report counts only.

## Before you start

- The log port open in a terminal that logs to a file, and the screen
  shot tool ready (`screen shot` streams the screen through the log).
- The codeplug has an APRS callsign and frequency (`aprs` shows
  `..., 144.800 MHz wide, ...`). GPS on, with a fix (`gps`), for the
  distance and beacon steps.
- The radio on an ordinary analog channel that is not the APRS frequency,
  APRS mode off (`aprs` shows `APRS mode off`), beacons off
  (`aprs every 0`), battery save on (`save 1`) to see that the mode
  overrides it. Note the channel: `screen shot` and `fm status`.

## Entering and the home screen

| # | Do | Expect |
| --- | --- | --- |
| 1 | `key menu`, `key down` three times, `key menu` | Menu > APRS open on "APRS mode  Off"; the panel says `One receiver: APRS mode` / `listens only on 144.800` (`screen shot`) |
| 2 | `key menu`, `key down`, `key menu` | `ui: APRS mode on`, `radio: receiver held at 144.800 MHz, 25 kHz, speaker quiet`, `aprs: APRS mode on at 144.800 MHz`, after about a second `aprs: MCU decoding on at 144.800 MHz`; the home screen comes up at once |
| 3 | `screen shot` | Top line `APRS 144.800` in blue with the clock; large `Listening` (or the last station's callsign); the panel `Nothing heard yet` or the last station; `STN` and `PKT` counts; bottom `Beacons off`; hint bar `Menu  P2 Log  PTT Beacon` |
| 4 | `fm status` | `144.800 MHz`, the speaker closed unless a signal is on |
| 5 | Wait for traffic (or a person sends a beacon from a second radio) | `aprs: heard (MCU) ...` lines; within a second the home screen's large name is that callsign, the panel its distance and bearing with GPS (else its comment) and `0 s`; `STN`/`PKT` count up (`screen shot`) |
| 6 | `screen shot` twice, 10 s apart, with no new packet | The age on the panel and the beacon countdown (once beacons are on) moved on; nothing else changed |
| 7 | `save` | `battery save ...; awake` and no `receiver asleep` while the mode is on, however long the radio is idle |
| 8 | `aprs rx` | `APRS mode on; board receive ...; MCU decoding in APRS mode, running, repair off`; `0 samples lost`; `reads: longest gap` well under 400 ms |

## Keys and PTT

| # | Do | Expect |
| --- | --- | --- |
| 9 | `key p1`, `key #`, `key 1` | Each: notice `In APRS mode`; `fm status` still 144.800 MHz |
| 10 | `knob 1` | The heard list (`APRS heard`); `knob 1` again moves the choice; `key menu` opens the station; `key exit` twice back to the APRS home screen |
| 11 | `key up` | The heard list again; `key exit` back home |
| 12 | `key p2` | The activity pages as usual; `key p2` back home |
| 13 | `fm rx 145500` (console tune) | `radio: refused: a dedicated mode holds the receiver`; still 144.800 MHz |
| 14 | `key ptt` (needs a GPS fix) | `ui: PTT: beacon`, notice `Beacon queued`, then `aprs: beacon ...`, `fm: transmitting 144.800 MHz, power ..., packet`, no `packet: listening on` (no retune), `aprs: MCU decoding off` during it and `on` a second after; no voice transmission |
| 15 | `ptt 500` (the radio's own console PTT) | `PTT: a dedicated mode holds the receiver`, notice `Mode is on`; nothing transmitted |
| 16 | `aprs every 60` | Bottom row `Next beacon 1:00` counting down; a beacon goes out on 144.800 within a minute without a retune. Then `aprs every 0` |

## The speaker and the squelch

| # | Do | Expect |
| --- | --- | --- |
| 17 | With a packet coming in (traffic or a second radio), listen | No packet sound from the speaker; the status strip's rule turns green during the packet; the backlight does not come on for it |
| 18 | Menu, Settings, APRS, Hear packets: On (`key menu`, `key exit`, `key down`, `key menu`, `key down` five times, `key menu`, `key down` twice, `key menu`, `key down`, `key menu`) | `radio: receiver held at 144.800 MHz, 25 kHz` without `speaker quiet`; the next packet is heard on the speaker (a person listens). Set it Off again the same way |
| 19 | `fm sq 5` with no signal on the frequency, then `audio probe 1000` | The probe's AC RMS and tone levels show receiver noise, not a flat line: the ADC input carries audio with the squelch closed. Then `fm sq 3` |
| 20 | `fm sq 5`, wait for a packet, `aprs rx` | Packets still decoded with the squelch closed for noise (strong packets open it anyway). Then `fm sq 3` |

## Leaving and the restart

| # | Do | Expect |
| --- | --- | --- |
| 21 | `key menu` on the home screen | Menu > APRS on the "APRS mode  On" row |
| 22 | `key menu`, `key up`, `key menu` | `ui: dedicated mode off`, `radio: receiver back to the channel`, `fm: receiving <the channel's frequency> ...`, `aprs: MCU decoding off`; the channel's home screen exactly as before step 1 (`screen shot`, `fm status`: same frequency, squelch, tones) |
| 23 | Repeat on a DMR roaming channel: note `roam`, enter and leave the mode | `roam` after leaving shows the same repeater in use as before |
| 24 | `aprs mode on`, then `restart` (or power off and on) | After the start screen the APRS home screen, `radio: receiver held at 144.800 MHz` before any `fm: receiving` on the channel |
| 25 | `aprs mode off`, `restart` | The radio starts on its channel |
| 26 | The uptime line (`system: uptime ...`) after an hour in the mode | Unused stack words: input at least 150, aprs at least 100 |

## An overnight run

Leave the radio in APRS mode overnight beside a second APRS radio
(a person compares the two lists in the morning). Then `aprs rx`: the
packets decoded, `samples lost` 0 (or each loss line's time stamp
matched to what the log shows at that moment), the longest gap between
reads, and the per-slicer counts. Report the counts, not the calls.

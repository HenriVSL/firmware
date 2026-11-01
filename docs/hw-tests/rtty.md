# Text mode: RTTY receive

The text mode holds the radio's one receiver on a frequency the user
enters and decodes RTTY (two-tone FSK, ITA2) on the MCU until it is
turned off (`docs/dedicated-modes.md`, "The text mode"). It only
receives: PTT is refused.

Every step runs from the console (`key <name> [long]`, `knob <steps>`,
`screen shot`, `text ...`, `aprs ...`, `fm ...`) except those marked
**human**: the radio hears only real RF, so a person must send RTTY
from a second radio (fldigi or a phone app playing into its microphone,
or a radio with a sound card cable), at low power, on the agreed
frequency. The examples use 144.600 MHz (the IARU Region 1 RTTY centre of
activity); use the frequency agreed with the sender.

Logs and screen shots of real traffic may hold other stations'
callsigns: keep them on the PC, out of the repository; report counts.

## Before you start

- The log port open in a terminal that logs to a file; `screen shot`
  ready.
- The radio on an ordinary analog channel, no dedicated mode on
  (`aprs` shows `APRS mode off`, `text` shows `decoder not running`).
  Note the channel: `screen shot` and `fm status`.
- **human**: a second radio with an RTTY source set to 45.45 Bd, 170 Hz
  shift, mark 2125 Hz / space 2295 Hz (fldigi: RTTY 45, audio centre
  2210 Hz, or "AFSK" with the tones set), narrow FM, low power, a few
  metres away. Its text names the sender's call after "DE".

## Settings and the frequency

| # | Do | Expect |
| --- | --- | --- |
| 1 | `key menu`, `key down` four times, `key menu` | Menu > Text open on "Text mode  Off"; "Frequency  Not set" on a radio that never had one; the panel `One receiver: text mode` / `needs a frequency (below)` (`screen shot`) |
| 2 | `key menu`, `key down`, `key menu` (Text mode: On) | Refused: notice `Set a frequency first`, `ui: Text mode refused: no frequency set`; the group again |
| 3 | `key down`, `key menu` (Frequency) | The digit entry `Text > Frequency`, `000.00000 MHz` (or the last frequency) |
| 4 | `key 1`, `key 5`, `key 0`, `key menu` | Refused: notice `144-146, 430-440 only`; the entry stays |
| 5 | `key *` three times, `key 1`, `key 4`, `key 4`, `key 6`, `key 0`, `key 0`, `key 0`, `key 0`, `key menu` | `ui: text mode frequency 144.600 MHz`; back in the group, "Frequency  144.600"; the Text mode row's panel `listens only on 144.600` |
| 6 | `knob 1` in the entry, then `key exit` | The value moves by 12.5 kHz; Exit leaves it unchanged |
| 7 | `text freq 144.6` (console) | The same as step 5 from the console; `text` shows `RTTY 144.600 MHz, 2125/2295, 45.45 Bd, unshift on space on, speaker off` |

## Entering and the home screen

| # | Do | Expect |
| --- | --- | --- |
| 8 | On "Text mode": `key menu`, `key down`, `key menu` (or `text mode on`) | `ui: Text mode on`, `radio: receiver held at 144.600 MHz, 12.5 kHz, speaker quiet`, `aprs: Text mode on at 144.600 MHz`, after about a second `aprs: MCU decoding RTTY on at 144.600 MHz`; the home screen comes up at once |
| 9 | `screen shot` | Top line `RTTY 144.600` with the clock; large `Listening`; an empty panel; `TON 2125/2295`, `SIG` a few per cent; bottom `45.45 Bd  0 chr  0 err`; hint bar `Menu  Up/Down Text  P2 Log` |
| 10 | `text` | `decoder running, signal off, quality` under 10 %; `0 characters`; `s of audio` counting up |
| 11 | `fm status`, `save` | 144.600 MHz narrow; battery save does not put the receiver to sleep while the mode is on |
| 12 | `text log on` | `text: log on` |
| 13 | Wait 10 minutes with nothing sent (`fm sq 0` for half of it: open squelch noise) | No `text: rx` lines; `text`: `0 characters`, a growing count of framing errors and characters dropped without a signal is fine (noise) |

## Receiving

| # | Do | Expect |
| --- | --- | --- |
| 14 | **human**: send `RYRYRYRY CQ CQ DE <call> <call> K` and a line with figures, `TEST 0123456789 ?/-.,` | `text: rx "RYRYRYRY CQ CQ DE <call> <call> K"` and the figures line exactly; the home screen's large name becomes `<call>`, the panel the two newest lines, `SIG` 60-100 % while sending, `chr` counting (`screen shot` during and after) |
| 15 | `text` right after | `signal off` again within about a second of the end; framing errors 0 or a few |
| 16 | **human**: send a longer text (the quick brown fox several times, 2-3 minutes) | Every line logged as sent; count the wrong characters |
| 17 | `knob -1` (or `key up`) | The received text view `RTTY 144.600`: the lines received, newest at the bottom; `knob -3` scrolls back (note `-3` on the right), `knob 3` forward; `key exit` home |
| 18 | **human**: send with the high tones swapped (fldigi "Rev"); `text reverse on` | Garbage or nothing before; after `text reverse on` the text is right and `TON` shows `2295/2125`. Then `text reverse off` |
| 19 | **human**: send at 1275/1445 Hz; `text tones low` | Nothing on the high pair; after `text tones low` the text comes, `TON 1275/1445`. Then `text tones high` |
| 20 | **human**: send at 50 Bd, then 75 Bd; `text baud 50`, `text baud 75` | Each decodes after its setting. Then `text baud 45` |
| 21 | **human**: send `1 2 3` | `1 2 3` with unshift on space on (the default) and with `text usos off`: the sender keys figures again after each space. Then `text usos on` |
| 22 | **human**: lower the sender's power or walk away until the signal is noisy | The text gets errors and then stops: no stream of garbage on noise; `SIG` falls under about 11 % as it stops |
| 23 | Menu > Text > Hear audio: On (`text hear on`) | `radio: receiver held at 144.600 MHz, 12.5 kHz` without `speaker quiet`; **human**: the tones are heard on the speaker. Then `text hear off` |
| 24 | `text`, and the uptime line after an hour in the mode | `longest decoding pass` a few ms at most; unused stack words: aprs at least 100 |

## Keys and PTT

| # | Do | Expect |
| --- | --- | --- |
| 25 | `key p1`, `key #`, `key 5` | Each: notice `In text mode`; `fm status` still 144.600 MHz |
| 26 | `key ptt` | `ui: PTT refused: the text mode only receives`, notice `Receive only`; nothing transmitted |
| 27 | `fm rx 145500` | `radio: refused: a dedicated mode holds the receiver` |
| 28 | `aprs every 60`, wait two minutes, `aprs every 0` | `aprs: automatic beacons paused in the text mode`, no beacon and no retune |
| 29 | `key menu` on the home screen | Menu > Text on "Text mode  On"; `key down`, `key menu` on Frequency gives the notice `Turn the mode off` |

## Leaving and the restart

| # | Do | Expect |
| --- | --- | --- |
| 30 | With the mode on, `restart` | After the start screen the text mode's home screen, `radio: receiver held at 144.600 MHz` before any `fm: receiving` on the channel; Menu > Text shows the frequency and options as set |
| 31 | On "Text mode": `key menu`, `key up`, `key menu` (or `text mode off`) | `ui: dedicated mode off`, `radio: receiver back to the channel`, `aprs: MCU decoding off`; the channel's home screen exactly as before (`screen shot`, `fm status`) |
| 32 | `aprs mode on`, then `text mode on` | APRS mode turns off, the text mode on (one mode at a time); `text mode off` back to the channel |

## On the PC: a recording through text-decode

The radio's own decoder also runs on the PC, on an `audio probe dump` or
a WAV file (`build/host/tools/text_decode/text-decode`).

| # | Do | Expect |
| --- | --- | --- |
| 33 | `text-decode --encode "RYRYRY CQ DE N0CALL K" ryry.wav`, then `text-decode ryry.wav` | The WAV is 45.45 Bd RTTY at 2125/2295 Hz; decoding it gives `RYRYRY CQ DE N0CALL K`, `0 framing errors` |
| 34 | **human**: the sender sends a short line; on the radio in the text mode (or on a channel at the frequency): `audio probe dump 5000` while it comes, logged to a file | `d` lines without gaps; the text mode pauses during the dump and starts again after it |
| 35 | `text-decode <log file>` | The line as sent, `[signal on ...]` around it, the quality in per cent; `--tones low`, `--reverse`, `--baud 50` as the sender used |
| 36 | **human** (optional): play `ryry.wav` from step 33 into the second radio's microphone, or into fldigi | fldigi prints the text: our tones and timing are standard |

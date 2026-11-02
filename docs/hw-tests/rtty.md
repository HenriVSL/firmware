# Text mode: RTTY

The text mode holds the radio's one receiver on a frequency the user
enters, decodes RTTY (two-tone FSK, ITA2) on the MCU, and sends text
typed on the keypad or a macro on the same frequency
(`docs/dedicated-modes.md`, "The text mode").

The steps run from the console (`key <name> [long]`, `knob <steps>`,
`screen shot`, `text ...`, `aprs ...`, `fm ...`; key names as the log
writes them: `0`-`9`, `*`, `#`, `Menu`, `Exit`, `Up`, `Down`, `Front1`,
`Front2`, `Side1`-`Side3`, `PTT`). Part A never keys the transmitter:
the decoder is fed the radio's own transmit audio through the loopback
(`text selftest`, `text loopback`, `text dry on`). Part B needs a second
station or keys the radio: every step marked **TX** keys the transmitter
and runs only with the licence holder's OK at the time, at low power,
into a dummy load or with the agreed station listening; steps marked
**human** need a person.

The examples use 144.600 MHz (the IARU Region 1 RTTY centre of
activity); use the frequency agreed for the test. Logs and screen shots
of real traffic may hold other stations' callsigns: keep them on the PC,
out of the repository; report counts.

The menu: Menu > Text is the fifth group (`key menu`, `key down` four
times, `key menu`). Its rows in order: Text mode, Frequency, Received
text, Macros, Send power, Add callsign, Tones, Baud, Reverse, Unshift,
Nordic letters, Hear audio. Each setting has a console twin; `text`
prints them all. Console lines are at most 80 characters of plain ASCII.

## Before you start

- The log port open in a terminal that logs to a file; `screen shot`
  ready.
- The radio on an ordinary analog channel, no dedicated mode on (`aprs`
  shows `APRS mode off`, `text` shows `decoder not running`). Note the
  channel: `screen shot` and `fm status`.
- The codeplug has an APRS callsign (the text mode's `$MYCALL`): `text`
  shows `callsign <call> added`.

## Part A: from the console, no transmitting

### A1. Frequency, settings and turning the mode on

| # | Do | Expect |
| --- | --- | --- |
| 1 | Menu > Text (`key menu`, `key down` ×4, `key menu`) | "Text mode  Off"; "Frequency  Not set" on a radio that never had one; the panel `One receiver: text mode` / `needs a frequency (below)` (`screen shot`) |
| 2 | On "Text mode": `key menu`, `key down`, `key menu` | Refused without a frequency: notice `Set a frequency first`, `ui: Text mode refused: no frequency set` |
| 3 | `text freq 150` | Refused: `ui: text mode frequency refused: outside 144-146 and 430-440 MHz`, notice `144-146, 430-440 only` |
| 4 | `text freq 144.6` | `ui: text mode frequency 144.600 MHz`; Menu > Text shows "Frequency  144.600" |
| 5 | `text power low`, `text addcall on`, `text nordic plain`, `text tones high`, `text baud 45`, `text reverse off`, `text usos on`, `text hear off` | `text` prints `RTTY 144.600 MHz, 2125/2295, 45.45 Bd, unshift on space on, speaker off` and `sending: power low, level 5 %, callsign <call> added, Nordic letters A A O, dry run off, idle` |
| 6 | Menu > Text: `key down` ×11 with a `screen shot` at each row | The twelve rows as listed above, with the values of step 5 |
| 7 | `text mode on` | `ui: Text mode on`, `radio: receiver held at 144.600 MHz, 12.5 kHz, speaker quiet`, `text: mode on at 144.600 MHz`, after about a second `text: decoding RTTY on at 144.600 MHz`; the home screen: top `RTTY 144.600`, large `Listening`, `TON 2125/2295`, `SIG` low, bottom `45.45 Bd  0 chr  0 err`, hint bar `Menu  Up/Down Text  PTT Send` (`screen shot`) |
| 8 | `text dry on` | `text: dry run on: texts go through the loopback, not on air`. **Keep it on for the rest of part A**: then no PTT, macro or `text send` keys the radio |

### A2. Self-test: decoding and CPU on the MCU

| # | Do | Expect |
| --- | --- | --- |
| 9 | `text selftest` | `text: loopback "RYRYRYRYRYRY THE QUICK BROWN FOX ..."`, `text: loopback: 83 characters at level 5 %`; about 16 s later `text: rx "RYRYRYRYRYRY THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 012"`, `text: rx "3456789 ?:-/(),."`, then `text: loopback done: exact, 0 of 80 characters wrong, ...; decoding took X % of the CPU (Q % in the quickest pass), longest pass Y us`. Record X, Q and Y. X counts time other tasks took while the decoder ran, Q does not: Q is the decoder's own cost. rtty-2 gave X 6.3-6.8 %; this build does less than half the work per sample |
| 10 | During step 9: `screen shot` on the home screen | Bottom `Loopback N s`; while the text comes, large `Receiving`, `SIG` 80-100 %, the newest lines on the panel |
| 11 | `text selftest 0`, then `text selftest -4`, then `text selftest -6` (each once the last is done) | 0 dB and -4 dB: `exact` or 1-2 wrong; -6 dB: a few wrong (on the PC about 5 % of characters). Record the counts |
| 12 | `text selftest -12` | Little or nothing decoded: `loopback done: differs`, most characters wrong, no stream of garbage |
| 13 | `text tones low`, `text selftest`; `text baud 75`, `text selftest`; `text reverse on`, `text selftest`; then `text tones high`, `text baud 45`, `text reverse off` | Each `exact`: the loopback sends and receives with the same settings |
| 14 | `text` right after `text mode on`, and again after a minute | `decoding takes X % of the CPU (Q % in the quickest pass), longest pass Y us` for the live decoder on noise. rtty-2 gave X 3.08 %, Y 376 us; expect X about half that |
| 15 | `text selftest`, and `text stop` 3 s later | `text: loopback stopped: differs, ...`; the bottom row `Sending stopped` for 5 s |

### A3. Typing on the keypad

| # | Do | Expect |
| --- | --- | --- |
| 16 | On the home screen: `key 4`, `key 4`, wait 2 s, `key 4`, `key 4`, `key 4` | The received text opens, the compose line `>HI` its last row; `text compose` prints `compose "HI", cursor 2, ABC, key 4 letters "GHI4" at 2` |
| 17 | `key 0`, then `key 2` four times | `text compose`: `"HI 2"`: the fourth tap on 2 gave the digit |
| 18 | `key #`, `key 7`, `key 3`, `key #` | `text compose`: `"HI 273"`, and `ABC` again at the end: `#` changed to `123` and back |
| 19 | `key up` twice, `key 5` | `"HI 2J73"`: the cursor moved two left, the `J` went in there |
| 20 | `key *` | `"HI 273"`: the character before the cursor deleted |
| 21 | `key # long`, `knob 2`, `key menu` | The symbol picker: the hint bar `Pick ?  Menu: put in` after the knob; Menu puts `?` in at the cursor |
| 22 | `key * long` | The line cleared; the compose row goes |
| 23 | Type `CQ TEST`: `key 2` ×3, `key 7` ×2, wait 2 s, `key 0`, `key 8`, `key 3` ×2, `key 7` ×4, `key 8`; `screen shot` | `>CQ TEST` on the last row, the cursor after it; on the home screen (`key exit`) the bottom row `Composing 7` |

### A4. Sending through the loopback (dry run)

| # | Do | Expect |
| --- | --- | --- |
| 24 | `text` | `dry run on` (step 8, or after step 44). Do not go on otherwise |
| 25 | With `CQ TEST` typed: `key ptt` | `ui: PTT: send 7 characters`, `text: loopback "CQ TEST DE <call>"`; about 5 s later `text: rx "CQ TEST DE <call>"` and `loopback done: exact`; the compose line cleared; no `fm: transmitting` line |
| 26 | `key ptt` with nothing typed | Notice `Nothing to send` |
| 27 | `text macros` | The six macros with their placeholders |
| 28 | `text macro 1` | `text: loopback "CQ CQ CQ DE <call> <call> <call> PSE K"`, then `exact` |
| 29 | `text call clear`, then `text macro 2` | `text: not sent: No call heard`, the notice `No call heard` (no call spotted in received text) |
| 30 | `text call q0test`, `text macro 2` | `loopback "Q0TEST DE <call> <call> K"`, `exact`; `text` shows `their call Q0TEST` |
| 31 | `text macro 3` | With a GPS fix: `... QTH <locator> <locator> KN`; without one: `No position` |
| 32 | `key side1` | `ui: side key 1: macro 1`: the CQ macro through the loopback |
| 33 | Menu > Text > Macros: `key menu` on the row, `key down` ×3, `key menu` | The home screen; `text: macro 4 Over`, `Q0TEST DE <call> KN` through the loopback |
| 34 | `text addcall off`, `text loopback hello`, `text addcall on` | `loopback "hello"` and `rx "HELLO"`, without `DE <call>` |
| 35 | During a loopback: `key ptt` | `ui: PTT: stop sending`, `loopback stopped` |

The Nordic letters (Ä Å Ö as A A O or AE AA OE) and the 120 s and
256-code limits cannot be typed on the console; host tests cover them.

### A5. The rest of the mode

| # | Do | Expect |
| --- | --- | --- |
| 36 | `knob -1` on the home screen | The received text, newest at the bottom; `knob -3` scrolls back (`-3` on the right), `knob 3` forward; `key exit` home |
| 37 | `key front1` | Notice `In text mode`; `fm status` still 144.600 MHz |
| 38 | `fm rx 145500` | `radio: refused: a dedicated mode holds the receiver` |
| 39 | `aprs every 60`, wait two minutes, `aprs every 0` | `aprs: automatic beacons paused in the text mode`, no beacon, no retune |
| 40 | `text log on`, then 10 minutes with nothing sent, half of it with `fm sq 0` | No `text: rx` lines; `text` counts framing errors and characters dropped without a signal, but no characters |
| 41 | Menu > Text > Frequency with the mode on | Notice `Turn the mode off` |
| 42 | The uptime line after the steps above | Unused stack words: aprs at least 100, ui at least 150, link at least 80, radio at least 80 |
| 43 | `restart` with the mode on | The text mode's home screen after the start screen; `radio: receiver held at 144.600 MHz` before any `fm: receiving`; `text` shows the settings of step 5 |
| 44 | `text` | `dry run off` after the restart: `text dry on` again before any more of part A |
| 45 | `text mode off` | `ui: dedicated mode off`, `radio: receiver back to the channel`, `text: decoding off`, `text: mode off`; the channel's home screen as before (`screen shot`, `fm status`) |

### A6. On the PC: text-decode

| # | Do | Expect |
| --- | --- | --- |
| 46 | `text-decode --encode "RYRYRY CQ DE N0CALL K" ryry.wav`, then `text-decode ryry.wav` | `RYRYRY CQ DE N0CALL K`, `[signal on ..., AFC +0 Hz]`, `0 framing errors` |

## Part B: with a second station or on air

### B1. Receiving (a second station sends, the radio only listens)

| # | Do | Expect |
| --- | --- | --- |
| 47 | **human**: a second radio sends RTTY 45.45 Bd, 170 Hz, mark 2125 Hz (fldigi: RTTY-45, audio centre 2210 Hz), narrow FM, low power, a few metres away: `RYRYRYRY CQ CQ DE <call> <call> K` and `TEST 0123456789 ?/-.,` | With `text log on`: both lines exactly; the large row becomes `<call>`; `SIG` 80-100 % while it sends (`screen shot`) |
| 48 | **human**: the same with the sender's audio centre 50 Hz off (fldigi: 2260 Hz) | The text decodes after a few characters; `text` shows `AFC` near +50 Hz while it sends, back towards 0 a few seconds after |
| 49 | **human**: reversed, the low tones, 50 and 75 Bd (`text reverse on`, `text tones low`, `text baud 50` / `75`) | Each decodes after its setting; put the settings back |
| 50 | **human**: walk away or lower the sender's power until the signal is noisy | Errors grow, then the text stops; no garbage on noise |
| 51 | **human**: an APRS packet or speech on the frequency | Nothing printed |
| 52 | **human** (optional): `audio probe dump 5000` while the sender sends, then `text-decode <log>` on the PC | The same text from the recording |

### B2. Sending

Before each **TX** step: the licence holder's OK, `text power low`, a
dummy load or the agreed station listening, `text dry off`. The USB link
may drop while the radio transmits next to the PC; every send ends by
itself (after the tail, or 2 s past the text's time at the latest).

| # | Do | Expect |
| --- | --- | --- |
| 53 | **TX** `text macro 6` | `text: send "RYRYRYRYRYRY THE QUICK ... DE <call>"`, `text: queued`, after half a second of free channel `fm: transmitting 144.600 MHz, power low, packet, ...`, `text: sending N characters, T s`, then `fm: transmit off (text sent) after T s`; the home screen `Sending N s`, then `Sent` |
| 54 | **TX**, **human** on the second station: fldigi or a phone app on RTTY-45 at 2210 Hz listens | It prints the text as sent; count the wrong characters |
| 55 | **TX** Type `TEST`, `key ptt` | `TEST DE <call>` goes out; the compose line is cleared after `Sent` |
| 56 | **human**: the second station sends on; meanwhile **TX** `key ptt` with a text typed | `Waiting for the channel` until it stops, then it sends; if it sends on for 10 s: `Channel busy`, the text stays |
| 57 | **TX** `text macro 6`, then `key ptt` within 2 s | `ui: PTT: stop sending`, `fm: transmit off (text stopped)` |
| 58 | **TX**, **human** (optional, with an SDR): record a transmission | The tones at 2125/2295 Hz, their deviation, no splatter. To change the level: `text level <1-20>` (until restart); report the value |

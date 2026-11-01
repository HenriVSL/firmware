# APRS receive on the MCU

The MCU decodes APRS itself from the received audio (PC0, ADC channel
10, sampled at 9600 Hz), next to the BT+APRS board. It runs only in APRS
mode (Menu, APRS, APRS mode: On; `docs/dedicated-modes.md`,
`docs/hw-tests/aprs-mode.md`), which holds the receiver on the codeplug's
APRS frequency; it starts a second after the radio settles there and
stops at once during a transmission. The demodulator has seven slicers that favour the 1200
or 2200 Hz tone by -6 to +9 dB, each with its own bit clock; a frame with
a right check from any of them is passed on once.

Frames from the board and from the MCU go to the same heard list; the
second copy of a frame within 3 s is dropped. Log lines:

- `aprs: MCU decoding on at 144.800 MHz` / `aprs: MCU decoding off`, and
  at each stop `audio: ADC channel 10: <n> samples, counts <min>..<max>`.
- `aprs: heard (MCU) <packet>`, or `(MCU, repaired)`: decoded by the MCU
  first. `aprs: heard <packet>` without the mark: by the board first.
- `aprs: same frame from the board` (or `from the MCU`): the other source
  heard it too, a moment later.

Console:

- `aprs rx`, five lines:
  - `APRS mode on; board receive on; MCU decoding in APRS mode, running,
    repair off`: the mode, the board, the MCU decoder's setting
    (`aprs rx mcu`), whether it decodes now, and repairs.
  - `frames: board <n> (<n> failed the check), MCU <n> (<n> repaired),
    heard by both <n>`: frames with a right check from each source; a
    frame both heard counts once for each source and once in `heard by
    both`.
  - `MCU: <s> s of audio, <n> samples lost, <n> copies between slicers`.
  - `reads: longest gap <ms> ms (at <s> s), longest decoding pass <ms> ms`.
  - `slicers (dB right/failed): -6 <right>/<failed> ...`: per slicer, the
    frames it read with a right check (copies included) and those of a
    possible length whose check failed.
- `aprs: <n> samples lost: no read for <ms> ms` whenever the sampler's
  ring (0.4 s) overflowed; the time stamp shows what else happened then.
- `aprs rx mcu on|off|any`: MCU decoding in APRS mode (default), off, or
  on any analog channel, in the mode or not (for a test on another
  frequency). Not kept over a restart.
- `aprs rx repair on|off`: also accept a frame that one wrong bit or one
  wrong tone explains (off by default; it can let a wrong frame through
  now and then).
- `aprs` shows `aprs: MCU decoding ...: <n> frames` too.

The `audio probe` pauses MCU decoding while it runs. With the squelch open the
noise clips at 0 mV on the ADC; that gives failed frames in `aprs rx`
(tens per slicer in ten minutes) but no false ones.

## Before you start

- The log port open in a terminal that logs to a file.
- The codeplug's APRS frequency set (`aprs` shows it), Menu, Settings,
  APRS, Receive: On, beacons off (Menu, APRS, Auto send: Off) for the
  comparison.
- A second APRS radio with a display of heard stations, side by side on
  the APRS frequency, both with their usual antennas.
- `afsk-decode` built on the PC (`cmake --build --preset host`; it is
  `build/host/tools/afsk_decode/afsk-decode`).

Logs and dumps hold other stations' callsigns: keep them on the PC, out of
the repository; report counts only.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | `aprs mode on` | `radio: receiver held at ...`, after about a second `aprs: MCU decoding on at ...` |
| 2 | `aprs rx` | `APRS mode on; board receive on; MCU decoding in APRS mode, running, repair off`; seconds of audio growing between two calls; lost samples 0 |
| 3 | The second radio sends a beacon, strong | `aprs: heard (MCU) ...` or `aprs: heard ...` followed by `same frame from the ...`; the station once in Menu, APRS, Heard |
| 4 | `aprs send` (beacon from this radio, on the APRS frequency) | `aprs: MCU decoding off` before the transmission, `MCU decoding on` again about a second after it |
| 5 | `aprs mode off`, then select a channel on the APRS frequency | `MCU decoding off`, and no `on` while the mode is off, on any channel |
| 6 | `aprs rx mcu any` on an analog channel elsewhere, then `aprs rx mcu off`, then `aprs rx mcu on` | Decoding on there; off; off again until `aprs mode on` |
| 7 | `audio probe 2000` while decoding | The probe runs as before (`lost 0`); decoding goes on after it (seconds of audio keep growing) |
| 8 | An hour side by side on real traffic, then `aprs rx` | Count the distinct stations each decoded: the second radio's list, `heard (MCU)` lines (MCU only or first), `heard` lines without the mark plus `same frame from the board` lines (board), `same frame from the MCU` lines (board first, MCU too). Note the counts, the per-slicer counts and the board's failed frames. Pass: the MCU decodes at least the stations the board does, and close to the second radio |
| 9 | Optionally `aprs rx repair on` for a second hour | Note `repaired` counts and check the repaired packets read sensibly |
| 10 | The uptime line (`system: uptime ...`) after the hour | The aprs task's unused stack words about 200, at least 100 |

## Human steps

Steps 1, 2, 5, 6, 7 and 10 run from the console alone. These need a
person:

- A second APRS radio beside this one: step 3 (it sends a beacon), step 8
  (an hour of real traffic, comparing its heard list) and the missed
  packet below.
- Transmitting on the APRS frequency: step 4, with a person at the radio
  who has said transmitting is fine now.
- Reading the repaired packets: step 9.

## A missed packet, offline

When the second radio decodes a packet the MCU missed, catch the next one
from that station as audio:

1. `audio probe dump 5000` started just before the station is expected
   (a regular beacon), logged to a file. The probe pauses MCU decoding.
2. On the PC: `afsk-decode --save probe.wav <log file>`. It reads the
   `d <index> <hex>` lines (lost samples are filled with silence and
   counted), prints each frame with the slicer that found it first, then
   per slicer the frames with a right check and those that failed. Listen
   to `probe.wav`.
3. `afsk-decode --repair <log file>` shows whether one wrong bit or tone
   was the problem.

`afsk-decode` reads mono WAV files too (8000 Hz or more, resampled to
9600 Hz): a recording of the second radio's speaker or a receiver's
discriminator output can be compared the same way.

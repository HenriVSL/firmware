# Microphone gain

The codeplug's Mic gain (DMR) and FM mic gain (levels 1 to 5) set the
transmit audio level: level 3 is the standard +16 dB, each level 4 dB
more or less (+8, +12, +16, +20, +24 dB). On DMR the gain is the
codec's: the most its microphone AGC may apply, with the AGC holding
louder voices at its target. On FM the microphone reaches the
transceiver directly, and the gain scales the transceiver's voice gain
against its standard setting. The console has `mic gain` (shows both),
`mic gain dmr <dB>`, `mic gain fm <dB>` and `mic gain <dB>` (both),
0 to 30 dB, from the next transmission.

## Before you start

- The log port open in a terminal.
- A DMR channel through a repeater or hotspot with a parrot (echo)
  service, and its ID. The parrot answers private calls only: `dmr pc
  <parrot ID>`; after a restart set it again.
- A second radio on an FM simplex channel, at a distance that gives a
  clean, full-quieting signal, and someone listening on it (or a
  recorder on its speaker).
- The codeplug's Mic gain and FM mic gain at 3.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Start the radio | `radio: microphone gain: DMR +16 dB, FM +16 dB` |
| 2 | Console `mic gain` | The same line |
| 3 | DMR: `mic gain dmr 8`, PTT and speak a test sentence at a normal level, listen to the parrot | The parrot plays the sentence back, quieter than before the change |
| 4 | `mic gain dmr 16`, then `mic gain dmr 24`, the same sentence each time | Louder step by step; at 24 no distortion |
| 5 | `mic gain dmr 30`, speak loudly close to the microphone | Not clipped: loud speech is held at the AGC's target; quiet speech and the room are louder |
| 6 | `mic gain dmr 0` | Clearly quieter, still intelligible when speaking up |
| 7 | FM: `mic gain fm 8`, PTT (`ptt 5000` from the console) and speak, the other radio listening | Quieter than at 16 |
| 8 | `mic gain fm 16`, then `mic gain fm 24` | Louder step by step, the steps about as large as on DMR; at 24 no distortion |
| 9 | `mic gain fm 30`, speak loudly | Note whether the voice is limited or distorted; the deviation stays within the channel (no splatter on the neighbouring channel) |
| 10 | `fm txaudio bb`, then `mic gain fm 8`, `16` and `24` | Steps as in 8, the voice through the baseband; then `fm txaudio direct` |
| 11 | While transmitting on DMR at 24 (`ptt 8000`), console `bb codec 1 3b 10` (amplifier volume 8 dB) | No audible change: with the AGC on, the AGC sets the amplifier's gain |
| 12 | Tools, Codeplug editor, Settings: Mic gain 5; save (restarts) | `radio: microphone gain: DMR +24 dB, FM +16 dB` |
| 13 | More settings: FM mic gain 1; save (restarts) | `radio: microphone gain: DMR +24 dB, FM +8 dB` |
| 14 | Write a codeplug from the PC with both at 2 | After the restart `DMR +12 dB, FM +12 dB` |

## Undo

Set Mic gain and FM mic gain back to 3 in the codeplug, or `mic gain 16`
for the running radio.

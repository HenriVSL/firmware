# Audio probe

`audio probe [ms]` samples the received audio on the MCU's ADC input (PC0,
ADC channel 10) at 9600 Hz while the radio keeps receiving, and logs one
line: sample count, samples lost, mean, minimum and maximum (mV), the RMS
of the part above the mean, the RMS levels of 1200 and 2200 Hz (averaged
over 10 ms windows) and their ratio in dB. A line before it gives the
extremes in ADC counts (0..4095 for 0..3.3 V). `audio probe dump [ms]`
(up to 5000 ms) also logs every sample: lines `d <index> <hex>`, each
sample three hex digits in mV. The console and the data port wait while
it runs.

The question is whether the received audio on that pin is clean and
strong enough to demodulate APRS on the MCU.

## Before you start

- The log port open in a terminal that keeps everything (for the dump,
  log to a file).
- The radio on the APRS frequency with a wide channel: `fm rxw 144800`;
  squelch open: `fm sq 0`.
- Beacons off (Menu, APRS, Beacon: Off), so no transmission rewrites the
  transceiver's registers.
- A second radio that sends APRS beacons, as the source.
- After any retune or transmission the transceiver's registers are set
  again; poke them again. Put back with `fm 44 9f9` and `fm 58 8085`.

Dumps of real traffic hold other stations' callsigns: keep them on the
PC, out of the repository.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | `audio probe` with no signal (open squelch, noise) | `audio: ADC channel 10: 9600 samples, counts ..` and `audio: n 9600 lost 0, mean ... mV, ...`; the mean near 1100 mV (bias), min above 0 and max below 3300 (no clipping), lost 0 |
| 2 | `audio probe 3000`, and while it runs let the second radio send a strong beacon | AC RMS clearly above step 1's; both tones well above their step 1 levels; ratio within about +-6 dB; no clipping (counts above 0 and below 4095) |
| 3 | `audio probe dump 2000` during a beacon, logged to a file | `d` lines with indices 0, 32, 64, ... without gaps; turn them into a WAV (see below) and listen: the AFSK packet, no buzz or clicks from the display or the data flash |
| 4 | `fm 44 9f0` (receive volume step 0), then step 2 again | Tells whether the pin follows the transceiver's volume: tone levels fall a lot if it does. Then `fm 44 9f9` |
| 5 | `fm 44 9ff`, step 2 again | Higher levels if the pin follows the volume; check for clipping. Then `fm 44 9f9` |
| 6 | `fm 58 8005` (de-emphasis on in the transceiver), step 2 again | The ratio moves towards 2200 Hz being lower (more positive dB) if the pin is after the de-emphasis. Then `fm 58 8085` |
| 7 | `battery` and turn the volume knob during and after the probe | Battery and volume readings as before: ADC0 is not disturbed |
| 8 | `audio probe 10` and `audio probe dump 99999` | The first works (96 samples); the second is refused with the usage line |

## Turning a dump into a WAV

Copy the `d` lines from the log into a file and run a script that takes
the text after `d <index> `, splits it into three-digit hex numbers (mV),
subtracts the mean and writes 16-bit mono samples at 9600 Hz. A gap in the
indices means lost samples (`lost` above 0 in the summary).

# Frequency offset

Every frequency the radio receives and transmits on is made from one
reference oscillator. When that reference has drifted, the radio is off
frequency in both directions by the same amount: FM sounds distorted or
weak, and DMR may decode nothing at all, because a few kHz off is
already too much for the digital receiver.

The frequency offset is added to every receive and transmit frequency,
FM and DMR, so the radio is on the nominal frequencies again. Screens and
the log keep showing the nominal frequencies. It is a workaround: the
proper fix is to calibrate the reference again, which sets the trim the
radio's calibration holds for it. DeltaMM reads that calibration but
does not change it.

## Steps and range

The FM transceiver tunes in steps of 62.5 Hz, so the offset comes in
steps of 62.5 Hz too: 32 steps make 2000 Hz. A value given in hertz goes
to the nearest step, and the radio shows and logs the value applied, to
0.1 Hz (for example `+1937.5 Hz`).

The range is 5 kHz either way. That is 11 ppm at 440 MHz and 34 ppm at
145 MHz, far beyond the drift of a working reference; a radio farther
off needs repair.

The offset is the same number of hertz on every band, while a
reference's error grows with the frequency: a radio 1500 Hz low at
435 MHz is about 500 Hz low at 145 MHz. With `+1500` it is right at
435 MHz and 1000 Hz high at 145 MHz. Find the value on the band used
most; a radio used on both bands alike takes a value between the two.

## Setting it

The console's `fm offset <Hz>` sets the offset: `fm offset -1500`,
`fm offset +62`, `fm offset 0`. The radio tunes again at once when it is
receiving. `fm status` shows the offset applied. The offset lasts until
the radio restarts.

## Finding the right value

The offset is the nominal frequency minus the one the radio is really
on: a radio that transmits 1500 Hz low needs `+1500`. Receive and
transmit share the reference, so one value corrects both.

- **With a frequency counter, service monitor or calibrated receiver.**
  Set the offset to 0, transmit an FM carrier without a tone at low power
  into a dummy load (or a short antenna close to the counter), and read
  the frequency. A software-defined receiver works too when its own
  reference is accurate (GPS-locked, or corrected on a known signal
  first). Set the offset to the difference, transmit again and check.
- **With a signal known to be on frequency.** Listen to a carrier on a
  known exact frequency (a signal generator, or a station known to be
  accurate) on FM and move the offset until the audio is cleanest and the
  signal strength highest. On a DMR repeater, step through offsets until
  calls and the repeater's idle transmissions decode reliably; take the
  middle of the range that decodes.

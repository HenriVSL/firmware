# Frequency offset

Checks the frequency offset (`docs/frequency-offset.md`) on a D878UVII.

Needs: the radio with the USB log port open in a terminal, a dummy load
or a short antenna, and a way to read the transmit frequency: a frequency
counter, a service monitor, or a receiver whose own frequency error is
known. For step 5, a DMR repeater in range and a talkgroup it carries.

Use a frequency the radio may transmit on, at low power, without a tone;
an FM channel of the codeplug set that way, or the console's
`fm rx <kHz>` and `fm tx <kHz>` on a free simplex frequency.

## 1. Without an offset

1. Restart the radio, then type `fm status`.
   - The line ends `offset 0 Hz`.
2. Hold PTT for about 3 s and read the frequency on the counter.
   - Note the difference from the nominal frequency (measured minus
     nominal); this is the radio's own error, `E`.

## 2. Setting the offset in hertz

1. Type `fm offset 2000`.
   - The log shows `frequency offset +2000 Hz`.
2. Hold PTT for about 3 s.
   - The counter reads 2000 Hz (within the counter's accuracy) above the
     reading of step 1.
3. Type `fm offset -2000`, hold PTT for about 3 s.
   - The log shows `frequency offset -2000 Hz`; the counter reads 2000 Hz
     below the reading of step 1.
4. Type `fm offset 1950`.
   - The log shows `frequency offset +1937.5 Hz`: the nearest 62.5 Hz step.
5. Type `fm offset 62`, then `fm offset -31`, then `fm offset -32`.
   - The log shows `+62.5 Hz`, `0 Hz` and `-62.5 Hz`.
6. Type `fm offset 6000` and `fm offset 12x`.
   - Both are refused as unknown commands; `fm status` still shows
     `-62.5 Hz`.

## 3. Correcting the radio

1. Type `fm offset <-E>`, with the error from step 1 (the sign turned
   round), and hold PTT for about 3 s.
   - The counter reads the nominal frequency, within 62.5 Hz.

## 4. Receive follows

1. With the offset of step 3, receive a carrier on a known exact
   frequency (a signal generator, or a station known to be accurate) on
   FM, and type `fm status` while it is on.
   - The carrier is heard clearly; the line shows the nominal frequency
     and the offset.
2. While receiving, type `fm offset 0` and then the value of step 3 again.
   - The log shows each change; the receiver retunes at once, without
     a channel change.

## 5. DMR

1. Type `fm offset 0` and select a DMR channel on the repeater. Call the
   talkgroup with PTT and listen for other calls.
   - Note whether calls decode and the repeater answers.
2. Type `fm offset <-E>` as in step 3 and repeat.
   - Calls decode and the repeater answers; on a radio with a large
     error, only with the offset.

## Undo

Type `fm offset 0`, or restart the radio: the offset lasts until restart.

# Real-time clock

The radio keeps UTC on its battery-backed real-time clock, a GPS fix
corrects it, and the status bar shows the local time from the codeplug's
time zone or Menu, GPS and time, Zone. The console has `time` and
`time set`.

## Before you start

- The log port open in a terminal, and a PC whose clock is right.
- A view of the sky for the GPS steps.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Flash; watch the log | `clock: <date> <time> UTC` close to the PC's UTC (a clock that was never set: `time unknown`) |
| 2 | Watch the status bar | The local time (HH:MM) left of the volume: UTC plus the codeplug's time zone |
| 3 | Menu, GPS and time, Zone: pick another zone | The status bar follows at once; the menu shows e.g. `UTC+2:00` |
| 4 | `time set` a time 60 s behind the PC's UTC | `clock: set from the console: ... (was 60 s ahead)`; the status bar follows |
| 5 | `gps on` and wait for a fix | `clock: set from GPS: <the PC's UTC> (was 60 s behind)` |
| 6 | Switch the radio off for a minute, then on | Step 1's line again, still matching the PC within a second or two |
| 7 | Flash the factory firmware and look at its clock; flash DeltaMM back | The same time in both: the counter and its marker carry over |

## Results

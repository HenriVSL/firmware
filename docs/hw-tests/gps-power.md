# GPS power saving

With battery save on, the GPS receiver runs only until it has a fix, stays
on ten seconds more, and sleeps until the next fix is due: once an hour
for the clock, at once while the GPS screen is open. It wakes early by
the time its last start took, plus a quarter. A start without a fix is
given up after five minutes. With battery save off, the receiver stays on.
The status bar's GPS mark is amber while the receiver sleeps.

## Before you start

- The log port open in a terminal, and a PC whose clock is right.
- A view of the sky.
- GPS on in the codeplug or the menu.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Menu, Battery: off; flash or restart | `gps: power saving off`, `gps: receiver on`; the receiver stays on after its fix |
| 2 | Menu, Battery: On | `gps: power saving on`, then `gps: receiver asleep; wakes in <a little under 3600> s`; the GPS mark turns amber, Menu, GPS and time, Status reads `Sleeping` |
| 3 | Console `gps` | `receiver asleep, sleeps between fixes, wakes in <n> s; shortest need 3600 s, start lead <n> s` |
| 4 | Open the GPS screen (Menu, GPS and time, Status) | `gps: receiver on`; the screen shows `Searching`, then `Fix` |
| 5 | Leave the GPS screen | `gps: receiver asleep; wakes in <n> s`; the mark turns amber |
| 6 | `gps need 300`; after the fix and sleep that follows, `time set` a time 60 s behind the PC's UTC and wait without touching the radio | `gps: console needs a position every 300 s`; about five minutes after the last fix, less the start lead: `gps: receiver on for a fix`, a fix, `clock: set from GPS: <the PC's UTC> (was 60 s behind)`, and sleep again ten seconds after the fix. `gps need 0` ends it |
| 7 | Indoors far from a window, switch the radio off and on | `gps: receiver on for a fix`; five minutes later without a fix, `gps: receiver asleep; wakes in 3600 s` |
| 8 | Menu, GPS: off while asleep | `gps: off`; no mark; no wake later |
| 9 | Console `save 0`, then `save 1` | Menu, Battery follows (Off, then On); `gps: power saving off` and the receiver on, then `power saving on` and asleep after its next fix |

## Results

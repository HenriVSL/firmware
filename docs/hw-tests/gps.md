# GPS receiver

The radio's GPS receiver works: on or off with the codeplug's setting or
Menu, GPS; a GPS screen from Menu, GPS, Status; a status bar mark and
console commands. The receiver sends NMEA on USART1 at 9600 baud; its
supply runs through the FM transceiver.

## Before you start

- A view of the sky: outdoors, or at a window. A first fix can take a few
  minutes.
- The log port open in a terminal. Unplug the USB cable to listen or
  talk: in the accessory jack it cuts the speaker and the microphone.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Flash; Menu, GPS | Two items: GPS (as the codeplug sets it) and Status (Off) |
| 2 | GPS: On | Log `gps: on` and `gps: receiver power on`; back in the list, Status Searching; a grey GPS mark in the status bar |
| 3 | Console `gps raw on`, then `gps raw off` after a few seconds | Sentences such as `$GNRMC,...` and `$GNGGA,...` in the log, about one burst a second; then they stop |
| 4 | Wait with a view of the sky | Log `gps: fix: <lat>, <lon>, <alt> m, <n> of <m> satellites, <time> UTC`; the mark turns green, Status shows Fix |
| 5 | Choose Status | The GPS screen: "Fix", position, altitude, satellites, speed, UTC time and date matching a clock; the seconds count up without the screen flashing |
| 6 | Menu twice on the GPS screen | "Off", then "Searching", then "Fix" again, replacing each other cleanly; log `gps: off`, `gps: on` |
| 7 | Exit | The GPS list, its Status matching the screen |
| 8 | Console `gps` | Bytes and sentences counted; 0 bad and 0 lost, or a handful |
| 9 | Receive and transmit FM on a channel with the GPS on, USB unplugged | As without GPS: clear audio both ways |
| 10 | Hold Exit on the standby screen a few times | Standby, log and key test screens only |
| 11 | Switch the radio off and on | The GPS starts as the codeplug sets it |

## If something is off

- Step 2 logs `no data from the receiver for 5 s`: the receiver sends
  nothing at 9600 baud on PA3. Note the line and run `gps` for the counts.
- Step 3 shows garbled text: the baud rate differs.

## Results

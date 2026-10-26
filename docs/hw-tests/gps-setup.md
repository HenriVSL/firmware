# GPS receiver setup

Once the GPS receiver answers after power-on, DeltaMM asks it for RMC, GGA
and GSV only, once a second, and for the satellite systems the codeplug or
Menu, GPS, Systems chooses. The console talks to the receiver with
`gps query`, `gps restart` and `gps send`.

## Before you start

- The log port open in a terminal.
- A view of the sky for the last steps.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Flash; console `gps on` | Log `gps: receiver set up: <systems>` within a second or two, then the receiver's own TXT lines (maker, module, chip, firmware) |
| 2 | `gps raw on` for a few seconds, then `gps raw off` | Only GGA, GSV and RMC, once a second; no antenna TXT line |
| 3 | `gps query` | One TXT line with the receiver's firmware version |
| 4 | Menu, GPS, Systems: GPS | `gps: receiver set up: GPS`; with `gps raw on` the sentences start with `$GP`, no `$BDGSV` |
| 5 | Systems: GPS + BeiDou | `$GN` sentences and a `$BDGSV` line |
| 6 | Menu, GPS: Status shows "Searching", then "Fix" | The whole word, not cut short |
| 7 | Wait for the uptime line | The gps task keeps well over 100 unused stack words |
| 8 | `gps restart cold`, then `gps raw on` | The receiver's TXT lines again, the fix is lost and later found again; it still sends only GGA, GSV and RMC from the chosen systems |
| 9 | Switch the radio off and on | GPS and its systems as the codeplug sets them |

## Results

# Menu: operating groups, Settings and Tools, large value screens

The top of the menu holds what is used while operating the radio:
Channel (squelch, squelch type, power), Roaming (status, on/off, roam
zone), GPS (status, receiver on/off) and APRS (send now, auto send, heard
stations). Settings holds the setup in groups of its own: Home screen,
Display (brightness, light time), Keys & battery (key lock, auto lock,
battery save), Clock (time zone, show time, calibrate), GPS (satellites),
APRS (send power, receive) and Radio (frequency offset). Tools holds the
codeplug editor, log, key test and version. Exit goes back one level, to
the row it came from.

A menu list shows a title band with the path and the place in the list
(`3/7`), five rows, a scroll bar when there are more, a panel of two lines
about the chosen row and a hint bar. Menu on a row opens its value
screen:

- up to four plain choices as large buttons, the one in use starred;
- more choices as one large value between arrows, with its place on a
  scale and the value in use under it;
- choices that carry notes (Calibrate) as a list.

The knob or Up/Down move, Menu sets the value and Exit goes back without
changing it. On the large value Up raises it and Down lowers it. The zone list (P1) and Roam status use the same list style.
The menu's rows are drawn from the item table, so the menu no longer keeps
a copy of its rows. Lists, buttons and the large value draw in place: each
pixel is painted once per update, so nothing blanks before it is redrawn.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Menu | `Menu 1/6`, Channel to Tools, the panel says what Channel holds; the hint bar reads `Menu: open   Exit: back` |
| 2 | Down through the groups | The panel follows the chosen group |
| 3 | Menu on Channel, then Menu on Squelch | `Channel > Squelch`, the level large between arrows, a scale of six steps, `In use: n` |
| 4 | Up, Down, turn the knob, then Exit | Up raises the level, Down lowers it, the scale follows; Exit leaves the squelch as it was |
| 5 | Menu on Squelch again, a step, Menu | Back in the list with the new value; it takes effect |
| 6 | Menu on Power | Four large buttons, the power in use starred; Menu sets the one chosen |
| 7 | Menu > GPS, then Status; Exit | The GPS screen; Exit returns to GPS on Status. Menu > APRS > Heard stations, Exit: back in APRS |
| 8 | Menu > Settings > Clock > Time zone | Title `Settings > Clock`, then a large `UTC+02:00` (or the zone in use) with a marker on a track; Exit, Exit: back in Settings on Clock |
| 9 | Menu > Settings > Clock > Calibrate | A list with Correction and Per day values |
| 10 | Menu > Tools > Log, then hold Exit | The log; holding Exit goes on to the key test and standby |
| 11 | Menu > Tools > Version | The panel shows the version and the minutes since start |
| 12 | P1 | The zone list in the same style, hint `Menu: use   Exit: back` |
| 13 | Menu > Roaming > Status | The roaming list in the same style; Menu on Search now searches |
| 14 | Turn the knob fast in a list, on Power and on Time zone; watch Roam status for a minute | Only the rows, buttons and values that change are redrawn; nothing blanks or flashes |
| 15 | Read the latest uptime line | Every task above 50 unused stack words |

# Screens: working screen, call and transmit screens, activity

The standby screen is now one working screen:

- A status strip: LOCK, `GPS..` (on, still looking for a fix) and `APRS`
  (beacons going out by themselves) on the left, only while they apply;
  the battery voltage on the right. The line under it turns red while
  transmitting and green while receiving.
- The channel's name large, or the contact's (Menu > Display > Big name),
  with the talkgroup number (`TG 244`) small above it; a channel without
  a contact, or an FM channel, says `CHANNEL` there. The time on the
  right, local or UTC (`11:32Z`): Menu > GPS and time > Show time.
- Frequency, slot and the colour code (dim) on a panel; power and the
  battery voltage on one line (the volume instead: Menu > Display > Next
  to power); the repeater a roaming channel works through, or the zone
  and the channel's place in it, or an FM channel's tones and offset.

A call that is heard takes the screen: `RECEIVING` and the talkgroup in
green, the caller's callsign from the user directory (else a contact's
name, else the ID) large, the name and the ID under it, and the talk time.
A transmission of the user's takes the screen too: `TRANSMITTING` in red,
the same large name with the talkgroup number under it (or the FM
channel), and the time on the air. Repeater
wake-ups and APRS packets do not.

P2 opens Activity, three pages that Up/Down switch between:
calls heard and made with their length, newest first, with the time (or
how long ago without a clock); system events (roaming moves, APRS beacons
sent, GPS fixes); and the APRS stations heard, in the same rows: when last
heard and the distance, where the knob moves the selection and Menu opens
a station. P2 or Exit go back. Each of the two
logs keeps only the eight entries that fit on the screen, numbers only;
names are looked up as the list is drawn. APRS heard is no longer in the
menu.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Start on a DMR channel with a talkgroup | `TG <number>` small, the channel's name large, the time on the right; frequency, TS and CC; power and volume. Menu > Display > Big name: Contact shows the contact's name large instead |
| 2 | Menu > Display > Next to power: Volume, then turn the volume | `VOL` and the step next to the power; only the value changes, without flicker. Back to Voltage: `BAT` and the voltage with two decimals |
| 3 | Wait for the minute to change | Only the time changes |
| 4 | Menu > GPS and time > Show time: UTC | The time shows as `hh:mmZ` |
| 5 | GPS on, indoors | `GPS..` in amber in the strip until a fix |
| 6 | PTT on the DMR channel | `TRANSMITTING`, the talkgroup, the time on the air counting; a red line under the strip; back to the working screen on release |
| 7 | A call heard | `RECEIVING`, the caller, name and ID, the talk time; `LAST CALL` for a moment after |
| 8 | P2 | Calls (1/3) lists the call heard and the transmission with their lengths, newest first |
| 9 | Roam to a repeater (PF3), then P2 and Down | System (2/3) shows `ROAM repeater N` |
| 10 | Down again | APRS heard (3/3) in the same rows as the other pages; the knob moves the selection and scrolls past eight stations, never the page; Menu opens a station, Exit there comes back to it; Up goes back to System |
| 11 | Read the latest uptime line | Every task above 50 unused stack words |

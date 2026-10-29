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
- Frequency, slot and the colour code (dim) on a panel; power and volume
  on one line; the repeater a roaming channel works through, or the zone
  and the channel's place in it, or an FM channel's tones and offset.

A call that is heard takes the screen: `RECEIVING` and the talkgroup in
green, the caller's callsign from the user directory (else a contact's
name, else the ID) large, the name and the ID under it, and the talk time.
A transmission of the user's takes the screen too: `TRANSMITTING` in red,
the same large name with the talkgroup number under it (or the FM
channel), and the time on the air. Repeater
wake-ups and APRS packets do not.

P2 opens Activity: what happened lately, newest first, with the time (or
how long ago without a clock): calls heard and made with their length,
roaming moves, APRS beacons sent, GPS fixes. The knob or Up/Down scroll,
P2, Exit or Menu go back. The log keeps the last 32 events in 384 bytes,
numbers only; names are looked up as the list is drawn.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | Start on a DMR channel with a talkgroup | `TG <number>` small, the channel's name large, the time on the right; frequency, TS and CC; power and volume. Menu > Display > Big name: Contact shows the contact's name large instead |
| 2 | Turn the volume | Only the volume value changes, without flicker |
| 3 | Wait for the minute to change | Only the time changes |
| 4 | Menu > GPS and time > Show time: UTC | The time shows as `hh:mmZ` |
| 5 | GPS on, indoors | `GPS..` in amber in the strip until a fix |
| 6 | PTT on the DMR channel | `TRANSMITTING`, the talkgroup, the time on the air counting; a red line under the strip; back to the working screen on release |
| 7 | A call heard | `RECEIVING`, the caller, name and ID, the talk time; `LAST CALL` for a moment after |
| 8 | P2 | Activity lists the call heard and the transmission with their lengths, newest first |
| 9 | Roam to a repeater (PF3) | Activity gets `ROAM repeater N` |
| 10 | Read the latest uptime line | Every task above 50 unused stack words |

# Activity pages and the transmit screen

Menu > Tools > Activity opens two pages of what happened lately, newest
first, eight rows each:

- Calls: `RX <caller>  TG<n>  <s>s` for each DMR call heard (the caller's
  contact name or callsign where the codeplug has one, else the ID),
  `TX TG<n>  <s>s` or `TX FM <MHz>  <s>s` for each call or FM
  transmission of the user's, with its length once it ends.
- System: `ROAM repeater <n>` when roaming moves to another repeater
  (counted from 1, as `roam` counts), `GPS fix` when a fix comes after
  ten minutes or more without one.

Each row starts with the local time, or `-<n>m` (minutes ago) while the
clock is not set. Up, Down and the knob turn the page (`1/2`, `2/2` top
right); Exit or Menu goes back to the menu. Repeater wake-ups are not
calls and are not listed. The pages are kept until a restart.

While the user's own call or FM transmission is on the air, the home
screen gives way to the transmit screen: `TRANSMITTING` in red, the
channel's name large (on DMR the contact's name when the home screen's
large name is Contact), `TG <n>` under it on DMR, the running time, and
the slot and power in the bottom bar. It goes back to the home screen
when the transmission ends.

## Before

- A DMR channel with a talkgroup contact, a roaming channel, and an FM
  channel, as in `home-screen.md`.
- The log port open for the console commands; keys named as on the key
  test, "look" means looking at the radio's screen.
- `A`, after a restart: press Menu, Down five times, Menu, Menu (Menu,
  Tools, Activity). The menu remembers the row last chosen in each group.

## Steps

| # | Do | Expect |
| --- | --- | --- |
| 1 | `restart`; `A`; look | `Calls`, `1/2`, `Nothing yet`; hint `Up/Down: page   Exit: back` |
| 2 | Press Down; look | `System`, `2/2`, `Nothing yet` |
| 3 | Turn the channel knob one click; look | `Calls` again |
| 4 | Press Exit | The Tools list, on Activity |
| 5 | On the DMR channel, a call heard (a test call answered by an echo service will do); after it, `A` | `RX <caller>  TG<n>  <s>s` with the local time, the length as the call lasted |
| 6 | On the roaming channel, `roam now`; once `roam` names a repeater, `A`, Down | `ROAM repeater <n>` |
| 7 | `gps on`; after a fix, `A`, Down | `GPS fix` |
| 8 | Optional, into a dummy load on the FM channel: `ptt 3000`; look during it | The transmit screen: `TRANSMITTING`, the channel's name, `0:00` counting up, `POWER <level>`; back to the home screen after it |
| 9 | After step 8, `A` | `TX FM <MHz>  3s` |
| 10 | Wait for the `system: uptime ...` line (each minute) | `unused stack words: ui` above 50 |

## Undo

`gps off` if the GPS was off before.

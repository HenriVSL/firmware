# APRS board link diagnostics

Console access to the BT+APRS board over its serial link. `aprs board
<text>` sends one line with CR LF added; `aprs board hex <bytes>` sends
bytes as they are (`aprs board hex EE 01 06`). Every line the board sends
back is logged as `aprs: board: <line>`, and for 2 s after a command its
stray control bytes as `aprs: board bytes: ..`. Anything holding UPDATE or
PROGRAM, in any case and also as hex bytes, is refused: those words can
make the board's boot loader erase its program. So are UARTE (it can
change the Bluetooth module's baud rate away from the board's) and ATEST
(a module test mode).

Packets from the board are counted by their frame check: `aprs` shows
`aprs: board frames: <n> passed the check, <m> failed`, and each failed
one is logged as `aprs: board frame failed the check: <length> bytes,
check <received> received, <computed> computed` (or `no closing flag`
when it was cut off).

## Before you start

- The log port open in a terminal.
- Menu, APRS, Receive: On (the board powered; `aprs: board: +READY` at
  start).

## Steps

Only these commands are known to be safe; do not try others on the board.

| # | Do | Expect |
| --- | --- | --- |
| 1 | `aprs board AT+BTON` | `aprs: sent 9 bytes to the board`, then `aprs: board: +BTON` |
| 2 | `aprs board AT+VER?` | A reply line containing APRSV200 |
| 3 | `aprs board hex EE 01 06` (the start command) | `sent 3 bytes`; `aprs: board bytes: 06` and possibly `aprs: board: +READY` |
| 4 | `aprs board update`, `aprs board AT+Program`, `aprs board hex 55 50 44 41 54 45` | Each refused with `refused: that could reflash the board or cut off its Bluetooth module`; nothing sent |
| 5 | Menu, APRS, Receive: Off; `aprs board AT+VER?` | `aprs: the board is off: Menu, APRS, Receive: On`; nothing sent |
| 6 | Receive: On again; `aprs`, then the second radio sends a beacon, then `aprs` | `aprs: heard ...` as before; `board frames:` passed count one higher |
| 7 | `aprs` after an hour on real traffic | The `board frames:` counts. Many failed frames: the board finds packets but loses bits; none failed while the second radio decodes more: its modem does not find them at all |

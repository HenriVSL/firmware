# Dedicated receive modes

The radio has one receiver. Most of the time it follows the channel the
user picked. A dedicated mode takes that receiver over for one job and
keeps it until the user gives it back: APRS first, RTTY and PSK31 later
(fork issue #13), and whatever else needs the receiver on one frequency
for hours. This document is the contract every such mode follows; the
APRS mode is its first user.

## 1. Turning a mode on and off

- Each mode has an On/Off row at the top of its own group in the
  operating part of the menu (Menu, APRS, "APRS mode"). Only one mode is
  on at a time; turning one on turns any other off first.
- The row's panel explains that the radio has one receiver and what the
  mode does with it, naming the frequency: "One receiver: APRS mode" /
  "listens only on 144.800". Without a frequency for the mode the second
  line says so ("needs an APRS frequency") and On is refused with a
  notice.
- Choosing On or Off goes straight to the home screen, so the change is
  seen at once.

### The mode's frequency

- APRS uses the codeplug's APRS frequency and its wide/narrow setting; it
  is changed in the codeplug (or the codeplug editor), not in the mode.
- A mode without a codeplug field for its frequency (RTTY, PSK31 and
  later ones) has a Frequency row in its menu group, under the On/Off
  row. The user enters the frequency there before turning the mode on; it
  can be changed only while the mode is off. On is refused with a notice
  until a frequency is set.
- The user is a licensed amateur and takes responsibility for the choice.
  DeltaMM offers no built-in default frequencies and no list of
  "allowed" ones; the only limits are the transmit band limits that apply
  everywhere (144-146 MHz and 430-440 MHz), for a mode that transmits at
  all.
- The last frequency entered is kept in the preferences record and shown
  again the next time.
- A mode stays on until the user turns it off in the menu. Nothing else
  turns it off: not a channel key, not a call, not a timeout.
- The console can do the same for tests (`aprs mode on|off`,
  `text mode on|off`, `text freq <MHz>`).

### The text mode

Menu, Text holds the text mode (RTTY receive for now; PSK31 and others
later as kinds of the same mode):

| Row | Does |
| --- | --- |
| Text mode | On/Off. The panel: "One receiver: text mode" / "listens only on 144.6125", or "needs a frequency (below)" |
| Frequency | Opens the digit entry: digits typed over the value, the knob or Up/Down in 12.5 kHz steps, `*` deletes a digit, Menu sets. Only inside 144-146 and 430-440 MHz; only while the mode is off ("Turn the mode off") |
| Received text | Opens the received text (below) |
| Tones | 2125/2295 or 1275/1445 Hz: mark the lower tone, 170 Hz shift |
| Baud | 45.45, 50 or 75 |
| Reverse | Mark and space swapped |
| Unshift | A space after figures goes back to letters (unshift on space, on by default) |
| Hear audio | The speaker opens with the squelch, as "Hear packets" does for APRS |

The options take effect at once, the mode on or off. The receiver listens
narrow (12.5 kHz) on the frequency. While the text mode is on the APRS
task's automatic beacons wait (they would take the receiver away), and
"Send now" still sends one.

### Kept over a restart

The mode is kept in the radio's preferences record (the codeplug's DeltaMM
record) and comes back on after a restart, once the radio is ready.

Why: the radio restarts on its own after every codeplug save made on the
radio, and a battery change is a restart too. Someone who left the radio
listening to APRS expects it to keep listening, and a mode that silently
dropped back to a voice channel would lose hours of packets. The home
screen says plainly that the mode is on (its top row names the mode and
frequency, section 3), so the radio cannot be mistaken for one on a
channel. If the mode's frequency is gone after a restart (codeplug
changed), the mode stays off and the radio works on its channel.

## 2. While a mode is on

### The receiver

- Locked to the mode's frequency, analog FM, with the mode's bandwidth.
  APRS: the codeplug's APRS frequency and its wide/narrow setting.
- The channel stays chosen underneath, untouched: the channel selector
  does not move, and the radio keeps the last channel it was given (with
  its roaming state) without tuning to it. Leaving the mode tunes back to
  exactly that channel.
- Roaming is off (the channel is not in use), the DMR receive path is off
  (analog only), battery save does not sleep the receiver (a decoder
  needs every sample), and a scan, once there is one, stops.
- The squelch level of the channel still drives the squelch gate: the
  status strip's rule turns green and the LEDs show a signal as usual, but
  a signal does not wake the backlight.
- The decoder needs the audio whether the squelch is open or not. The
  received audio on the MCU's ADC input (PC0) comes from the FM
  transceiver's audio output, ahead of the baseband and the speaker
  switch; DeltaMM leaves the transceiver's own squelch muting off, so the
  pin carries audio with the squelch closed too. The speaker stays quiet
  in the mode unless the user asks to hear packets (Menu, Settings, APRS,
  "Hear packets": On); then it opens with the squelch as on a channel.

### Keys

| Key | In the mode |
| --- | --- |
| Menu | The menu, opened in the mode's group on its On/Off row |
| Exit, P2, Exit held | As usual: back, the activity pages, the next screen |
| Knob, Up, Down | The mode's own list (APRS: the heard list; the knob then picks a station, Menu opens it, Exit comes back) |
| P1 (zones), #, digits | Refused with a notice ("In APRS mode"): each would change the channel or the frequency |
| Side keys | Their functions as usual; Roam answers "Not roaming" |
| PTT | The mode's own action (below); never voice |

The key lock works as usual, PTT included.

### PTT

A mode decides what PTT does; it never keys the microphone. APRS: a PTT
press sends one beacon now, as Menu, APRS, Send now does: the GPS
position at the beacon power (Menu, Settings, APRS, Send power) on the
APRS frequency. Holding PTT does nothing more. The transmit limits apply
as to every packet: only inside the 2 m and 70 cm amateur bands, only once
the channel has been free for a slot (it waits up to 30 s, then gives up
with "Beacon not sent"), and only with a GPS position no older than
2 minutes (it waits up to 3 minutes for one). A mode that must not
transmit refuses PTT with a notice: the text mode answers "Receive only"
until it can send.

Automatic beacons go on as set (Menu, APRS, Auto send). In the mode they
go out on the locked frequency without retuning.

### Leaving

Off in the menu (or `aprs mode off`) unlocks the receiver: the radio tunes
back to the channel it kept, with the squelch, power, tones, roaming
repeater and DMR settings it had, and the decoder stops. The heard list
and the counts stay.

## 3. The home screen

The mode uses the working screen's rows (StandbyScreen: top line with the
clock, the large name, the panel under it, the line beside the power
label, the bottom row, the hint bar), drawn the same way, filled with the
mode's data instead of the channel's. The clock follows the home screen
setting (Menu, Settings, Home screen, Clock). It must be useful at a
glance: what was heard last, how far, and whether the radio itself is
doing its job.

APRS:

| Row | Shows |
| --- | --- |
| Top | "APRS 144.800": the mode and its frequency; the clock on the right |
| Big | The callsign of the station heard last; "Listening" until one is |
| Panel | Its distance and bearing from our GPS position ("12.3 km NE"), or its comment or status without positions, and how long ago it was heard ("2 min") on the right |
| Under the panel | STN: stations in the heard list; PKT: packets heard, in amber while the decoder is not running |
| Bottom | Our beacon: "Next beacon 4:12", "Beacons off", "Beacon waits for GPS", "Beacon queued", "No APRS callsign"; "no GPS" added while beacons are on without a fix |
| Hint bar | "Menu  P2 Log  PTT Beacon" |

It is redrawn when a station is heard and each second (ages, the beacon
countdown), in place.

Text mode:

| Row | Shows |
| --- | --- |
| Top | "RTTY 144.6125": the kind and frequency; the clock on the right |
| Big | The callsign last given after "DE" in the text; "Receiving" while a signal is detected before one is; "Listening" otherwise |
| Panel | The two newest lines of received text, small, 31 characters each |
| Under the panel | TON: the tone pair (mark first, swapped when reversed); SIG: how far the two tones stand above the noise beside them (0-100 %), in amber while the decoder is not running |
| Bottom | The speed, characters received and framing errors: "45.45 Bd  123 chr  4 err" |
| Hint bar | "Menu  Up/Down Text  P2 Log" |

It is redrawn each second and as text comes in (at most four times a
second). The mode's own list (knob, Up, Down) is the received text: the
last 32 lines of 31 characters, newest at the bottom, the knob or Up/Down
scrolling back; new text keeps the view where it is. Exit goes back.

## 4. The decoder

Sampling the audio and decoding run only while the mode is on and the
radio listens on the mode's frequency: they stop at once for the radio's
own transmissions (beacons) and start again a second after. Outside a
mode nothing samples the audio in the background. For tests,
`aprs rx mcu any` decodes on any analog channel without the mode, and
`aprs rx mcu off` keeps the MCU decoder off even in the mode (the BT+APRS
board alone).

The BT+APRS board path is not part of the mode: it receives as Menu,
Settings, APRS, Receive says, in the mode or not. Frames from both go to
one heard list; the second copy of a frame within 3 s is dropped.

The audio sampler fills a ring by DMA. The decoding task reads it every
20 ms and runs above the ui, gps and console tasks, so a long screen
redraw or codeplug write cannot hold it up; the ring covers 0.4 s.
`aprs rx` reports samples lost and the longest gap between reads.

One task decodes for every mode: the aprs task holds the sampler and runs
the APRS decoder in APRS mode or the text mode's RTTY decoder in the text
mode (`text_mode::decode`). Only one mode is on at a time, so the two never
run together, and a second task's stack is saved. `text` reports the RTTY
decoder: signal, quality, characters, framing errors, the longest pass;
`text log on` copies each received line to the log (`text: rx "..."`).

## 5. Adding a mode

A new mode brings:

1. A value in `services::DedicatedMode`, and its key and PTT rules in
   `services::DedicatedModeState` (knob and Up/Down: the mode's list or
   refused; PTT: its action or refused).
2. Its frequency and bandwidth as a `services::ReceiverLock`, from one of
   two sources: the codeplug's field where the codeplug has one (APRS),
   else a Frequency row in the mode's menu group that the user enters
   while the mode is off, kept in the preferences record (section 1). No
   default frequencies, no allowed list; no frequency, no mode
   (`DedicatedModeState::turnOn` answers `NoFrequency`).
3. Its settings: on/off and the mode's own options in the preferences
   record (`RadioPreferences`), statistics in RAM only.
4. A decoder hosted by the aprs task, told by a message when the mode
   starts and stops (`aprs::ModeOn` names the mode); it samples only while
   the radio listens on the mode's frequency
   (`radio::analogListeningHz()`). The audio sampler has one user at a
   time: the aprs task holds it and lends it out for the audio probe.
5. A home screen builder: a pure function in services that turns the
   mode's state into the rows of section 3, with host tests. The ui only
   draws what it returns.
6. A menu group with the On/Off row first, its panel naming the
   frequency, the Frequency row next where the codeplug has none, and the
   mode's other rows under it.
7. A `docs/hw-tests/` checklist runnable from the console.

The radio side is shared: `radio::LockReceiver` locks and unlocks the
receiver for any mode, and the ui keeps one `DedicatedModeState`.

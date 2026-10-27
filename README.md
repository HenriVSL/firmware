# DeltaMM

Open firmware for DMR and FM handheld radios, written from scratch in
C++20.

<img src="docs/images/standby.png" width="320" alt="DeltaMM standby screen: zone, channel name, frequency, mode, tone, power and offset">

The first target is the AnyTone D878UVII. The radio features sit apart
from the board support, so a new radio, or the desktop simulator, is a
new board folder rather than a fork.

DeltaMM is for licensed radio amateurs. It is not affiliated with or
endorsed by AnyTone or the makers of the chips it drives.

## Features

**FM radio.** Receive and transmit using the radio's own factory
calibration. Squelch levels, repeater offsets, CTCSS transmit tones, four
power levels, a transmit timeout and amateur-band limits.

**Channels.** Channels, zones and the VFO come from the codeplug already
on the radio. Pick zones from a list, step channels with the knob, or type
a frequency on the keypad.

**Interface.** A first version of the user interface: a standby screen
with zone, channel, frequency, tones, power, volume and battery, and a
short grouped settings menu. Key lock
covers the keypad, side keys and knob, with auto lock and forced lock.

**Battery.** Battery save cycles the receiver while the radio is idle,
with the codeplug's setting, and the backlight goes off after its light
time. A battery monitor and auto power-off.

**GPS.** The built-in receiver, on or off with the codeplug's setting or
from the menu, using the satellite systems the codeplug or the menu
chooses (GPS, BeiDou, GLONASS). A GPS screen with position, altitude,
satellites, speed and UTC time, and a GPS mark in the status bar. With
battery save on, the receiver sleeps between fixes and wakes in time for
the next one the clock or the GPS screen needs; the mark is amber while
it sleeps.

**Clock.** The radio's real-time clock keeps UTC while the radio is off; a
GPS fix corrects it. The status bar shows the local time from the
codeplug's time zone. Menu, GPS and time, Calibrate measures the clock
against GPS time for 60 or 30 minutes and corrects its rate, so it keeps
time between fixes and while the radio is off. The receiver's timing
wanders by a few milliseconds, so a 30-minute run gets the rate to within
about 5 ppm (0.4 s a day); a 60-minute run does better.

**APRS.** Position beacons from the GPS, with the codeplug's APRS
callsign, path, symbol, comment and frequency, at the codeplug's interval
or now from the menu. Each packet waits for a free channel and goes out
simplex on the APRS frequency at low power; from another channel the radio
goes there and comes back. The radio's BT+APRS board decodes received
packets while the radio is on their frequency, and a list shows the
stations heard with their distance and bearing.

**Programming.** Codeplug tools that speak the radio's own programming
protocol read and write the codeplug over USB. Tested with QDMR.

**Platform.** Beeps, volume and backlight. A USB log with a debug
console, and crash details kept across restarts.

**Tools.** `dmm-radio` backs up the radio, prepares the update files and
flashes them. A desktop simulator runs the same firmware with a drawn
radio face.

## Roadmap

1. CTCSS and DCS decoding, DCS transmit
2. DMR receive and transmit
3. Refined user interface: screen layouts, menus, icons and fonts tuned
   as features are added
4. Programming over USB, compatible with existing codeplug tools
5. Scan and dual watch
6. Clock from GPS time, with the codeplug's time zone
7. Calibration tool
8. APRS, Bluetooth, recording, DTMF and selective calling
9. DMR roaming and SMS

## Getting started

### 1. Build

Host build: unit tests, simulators and `dmm-radio`. It needs Qt 6.8 with
Qt Quick and Qt Serial Port; without Qt, the core libraries and their
tests build.

```sh
cmake --preset host -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.x/gcc_64
cmake --build --preset host
ctest --preset host
build/host/boards/sim_qt/simqt     # the simulator
```

Firmware, with the Arm GNU Toolchain 14.2 or newer on `PATH` or in
`ARM_GNU_TOOLCHAIN`:

```sh
cmake --preset fw-d878uv2-release -DDMM_STOCK_FIRMWARE=/path/to/firmware.CDD
cmake --build --preset fw-d878uv2-release
```

The D878UVII's DMR baseband chip, the SCT3258, needs a 2 KB boot program
made for that chip, loaded into it at every power-on. The build takes it
from your own copy of the radio's factory firmware update file
(`DMM_STOCK_FIRMWARE`) and checks its hash. Everything that uses the
SCT3258, FM audio included, is built when this option is given.

### 2. Back up the radio

Save the radio's per-unit data (calibration and radio information) while
the factory firmware is still on it:

```sh
dmm-radio backup          # 2-5 minutes, includes the whole system area
dmm-radio backup --quick  # calibration and radio information only
dmm-radio list
```

Backups go to your user data folder, outside this repository. A backup is
marked verified once every check passes.

### 3. Flash

DeltaMM is sent through the radio's own update mode (power on holding
PTT and PF3, the top key) as three update files. They are made from the
DeltaMM image and your factory `.spi` and `.CDI` files, once a verified
backup exists:

```sh
cmake --preset fw-d878uv2-release -DDMM_STOCK_FIRMWARE=/path/to/firmware.CDD \
    -DDMM_RADIO_TOOL=/path/to/dmm-radio
cmake --build --preset fw-d878uv2-release --target update-files
```

Switch the radio on in update mode (red LED blinking) and send the files
with `dmm-radio`, or with the vendor's programming software:

```sh
dmm-radio flash build/fw-d878uv2-release/boards/anytone_d878uv2/package/DeltaMM_<version>.CDD
```

With `--wait 600`, the tool waits up to ten minutes for the radio to be
switched on in update mode. The radio starts the new firmware when the
transfer ends.
`docs/update-mode.md` describes update mode and its protocol.

The radio's boot loader stays as it is, so the factory firmware can be
flashed back the same way at any time, `dmm-radio flash <factory .CDD>`
included. Each release has a hardware checklist in `docs/hw-tests/`.

## Using the radio

| Key | Standby screen |
| --- | --- |
| Channel knob, Up/Down | Next or previous channel, or VFO step |
| `0`-`9` | Type a frequency |
| `#` | Switch between VFO and memory |
| P1 | Zone list |
| P2 | Channel settings |
| Menu | Settings menu |
| Hold `*` | Lock keys; Menu then `*` unlocks |
| Hold Exit | Cycle standby, log and key test screens |

Menu, GPS and time holds the receiver's on/off setting, its satellite
systems, its status and the time zone; choosing Status opens the GPS
screen, and Exit or Menu goes back to the list. The console's `gps raw on`
logs every sentence the receiver sends; `gps query`, `gps restart` and
`gps send` talk to the receiver itself; `gps need <s>` asks for a
position every so many seconds (0: none). `time` logs the clock, and
`time set YYYY-MM-DD HH:MM:SS` sets it (UTC); `time cal <min>` runs a
calibration (0 stops it) and `time cal reset` clears the correction.

Menu, APRS holds Heard (the stations heard, newest first: choose one for
its position, distance and bearing, altitude, speed and text), Send now,
Beacon (the interval, Off to stop), Power (the beacon power) and Receive
(the board on or off). An A in the status bar shows that beacons go out
on their own. The console's `aprs` logs the station and the beacon state,
`aprs send` sends a beacon (`aprs send here`: on the current channel, for
tests), `aprs every <s>` sets the interval, and
`aprs lead <ms>` and `aprs level <%>` set the packet's lead-in and audio
level.

The GPS mark in the status bar, shown while the GPS is on:

| Colour | Receiver |
| --- | --- |
| Green | has a fix |
| Grey | is searching for satellites |
| Amber | sleeps between fixes (battery save on) |
| Red | sends no data |

## Programming the codeplug

The data port also speaks the radio's programming protocol, so codeplug
tools read and write the codeplug as with the factory firmware. QDMR needs
to know DeltaMM's USB ID (1209:0001 during development); a QDMR with that
added finds the radio as a D878UVII. The radio starts again after a write,
with the new codeplug. Only the codeplug is written, never calibration or
the radio's own data; the callsign database is not written yet.

A write that stops halfway keeps the blocks already written and leaves
the rest as they were, so finish it with a complete write.

## Log and console

A radio running DeltaMM shows up as two USB serial ports, "DeltaMM data"
and "DeltaMM log". Open the log port with any terminal program at any
speed; lines written before the port was opened arrive first. Type `help`
for the console commands.

## Project layout

| Folder | Contents |
| --- | --- |
| `lib/` | Containers and utilities, heap-free and OS-free |
| `os/` | Task, queue and mutex API: FreeRTOS on radios, Qt threads on the PC |
| `hal/` | CPU core and MCU support, one folder per MCU family |
| `drivers/` | Chip drivers |
| `services/` | Radio, storage, log, power and the interfaces the app uses |
| `app/` | User interface and features |
| `boards/` | One folder per radio, plus the simulators |
| `tools/` | PC tools, `dmm-radio` among them |
| `tests/` | Host unit tests |
| `docs/hw-tests/` | One hardware checklist per release |
| `docs/update-mode.md` | Flashing through the boot loader, and its protocol |

`ARCHITECTURE.md` describes how the parts fit together, `AGENTS.md`
lists the coding and commit rules, and `CREDITS.md` thanks the people who
helped.

## Disclaimer

DeltaMM comes without any warranty. Installing it voids the radio's type
approval for anything but amateur use; you are responsible for
transmitting only where and how your licence allows. AnyTone and the chip
names used here are trademarks of their owners.

## Licence

MIT, see `LICENSE.md`. The Spleen font in `third_party/spleen/` is
BSD-2-Clause. The repository follows the REUSE specification.

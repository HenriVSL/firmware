<!-- SPDX-FileCopyrightText: 2026 Henri Lavikainen -->
<!-- SPDX-License-Identifier: MIT -->

# Radio test runner

Runs the hardware checklists of `docs/hw-tests/` on a radio running
DeltaMM, or on the headless simulator, through the console, and writes a
report: each step's result with the log lines and screen texts that show
it, pictures of the screen, and a list of what needs a person.

Python 3 and its standard library; pyserial is used when installed.

## Running a checklist

On the radio (the log port, `/dev/serial/by-id/*DeltaMM*-if02`, found by
itself and opened again after each restart):

```sh
tools/radio_test/radio_test.py list
tools/radio_test/radio_test.py run menu --no-human --out /tmp/menu-run
tools/radio_test/radio_test.py run home-screen --param dmr="Zone/Name" \
    --param roam="Zone/Name" --param fm="Zone/Name"
tools/radio_test/radio_test.py run menu --steps 1-5,8a
tools/radio_test/radio_test.py run menu --allow-qdmr-write   # with the QDMR round trip
```

On the simulator (`build/host/boards/sim_headless/sim` unless a path is
given); it is started with `--stdin` and started again after a restart:

```sh
tools/radio_test/radio_test.py run menu --sim
tools/radio_test/radio_test.py run menu --sim path/to/sim
```

The report is `report.md` in the output folder (default
`radio-test-<checklist>-<time>`), with `log.txt` (every line, with the PC's
time) and `pictures/`. The exit status is 1 when a step failed or the run
stopped.

`--no-human` never waits: steps for a person are reported as `human`.
Without it, on a terminal, such a step shows its text, waits for Enter
and asks whether it went as expected.

Single actions, for a look at the radio between runs:

```sh
tools/radio_test/radio_test.py where            # the menu screen, read
tools/radio_test/radio_test.py text             # every text on the screen
tools/radio_test/radio_test.py shot screen.png
tools/radio_test/radio_test.py goto "Settings/Home screen/Preset"
tools/radio_test/radio_test.py cmd settings
```

## Safety

- Console commands that key the radio or change its power (`ptt`,
  `key ptt`, `dmr pc`, `aprs send`, `aprs beacon`, `fm tx <kHz>`,
  `fm power`, `bbtone`) are refused before they are sent, unless the run
  has `--allow-transmit` and the step is marked `transmit=True`.
- Menu is refused on the Power value screen under the same rule (opening
  it to look is allowed).
- A log line showing a transmission or a power change (`transmitting`,
  `fm: transmit on`, `ui: power`) outside such a step stops the run.
- A step marked `changes=True` needs an undo of its own or the
  checklist's UNDO; a step that changes a setting (`settings kept`)
  with no undo registered fails. Undos run at the end of the run, last
  first, also when a step failed, the run stopped or Ctrl-C was pressed;
  a step's `undo_now=True` runs its undo right after it.
- Transmit steps are skipped without `--allow-transmit`, and listed for
  the human.
- QDMR writes are skipped (the step reports why) unless the run has
  `--allow-qdmr-write`: a write puts back the codeplug as QDMR reads it,
  which may change settings QDMR does not round-trip (with older QDMR
  builds the maximum speaker volume and APRS beacons). Steps that need a
  write's result (`after=['9']`) are skipped with it.
- One program per port: the run stops at the start when another process
  has the log port open, naming its PID, since a second reader takes
  lines away.

## The menu by name

The menu remembers the last row of each group until a restart, so a fixed
key sequence may land on another row. `goto Settings/Home screen/Preset`
instead goes back to the standby screen, presses Menu, and moves Down
until the row named is the chosen one, reading the screen after each key.

The screen is read with the console's `screen text`: the ui draws the
whole screen once and logs each run of characters with its place and
colours (`scr: t <x> <y> <font height> <fg> <bg> <text>`). The title band,
the rows, the chosen row (inverted), buttons, the value of a value screen
and its `In use:` are told apart from that (`radiotest/screen.py`).

## Checklists

A checklist is a Python file in `checklists/`: TITLE, SOURCE, STEPS, and
optionally BEFORE, UNDO and PARAMS. A step:

```python
Step(6, 'Menu, Up, Menu sets the squelch', changes=True, timeout=8,
     do=['at Channel/Squelch', 'keys Menu Up Menu'],
     expect=[r'log \bui: squelch \d', r'log@8 \bsettings kept in the codeplug'],
     undo=[restore('Channel/Squelch', 'squelch')])
```

Fields: `do` (actions), `expect` (expectations), `undo` (actions),
`human` (text for the operator), `transmit`, `radio_only` (why the
simulator cannot run it), `changes`, `timeout`. The actions (`cmd`, `key`,
`keys`, `knob`, `goto`, `open`, `at`, `choose`, `set`, `home`, `restart`,
`wait`, `mark`, `shot`, `watch`, `check`, `qdmr`, `dmm-radio`) and the
expectations (`log`, `nolog`, `title`, `row`, `value`, `place`, `hint`,
`kind`, `in_use`, `screen`, `noscreen`, `flicker`, `nodiff`, `stack`,
`answer`) are described in `radiotest/runner.py`; either may also be a
Python function of the context. `checklists/_helpers.py` holds the shared
ones (remember and restore a menu value, choose a channel by name, watch
the screen while pressing keys, check the `settings` lines).

| Checklist | From | In the simulator |
| --- | --- | --- |
| `test-tools` | the tools themselves | all |
| `menu` | `menu.md` | all but the restarts, QDMR, the stack and the hand step |
| `home-screen` | `home-screen.md` | the layouts, rows and redraws; not the signal, GPS, calls, restarts or QDMR |
| `settings-storage` | `settings-storage.md` | the console and menu changes and the codeplug line; not restarts or QDMR |
| `mic-gain` | `mic-gain.md` | the console, the record and the menu; not the chip register, QDMR or calls |
| `frequency-offset` | `frequency-offset.md` | the menu, the console and the record; not the counter or DMR |
| `write-stall` | `write-stall.md` | none: radio only |
| `loader` | `loader.md` | none: radio only (`DMM_RADIO` = the loader branch's dmm-radio) |

## Other tools

- QDMR: `qdmr read|write <file>` run `dmrconf ... -D <data port> -R
  d878uv2` with the data port (`*DeltaMM*-if00`) by its device name;
  `qdmr diff a b` compares two YAML files. The changes a write and read
  back is known to make are reported as a QDMR quirk, anything else fails
  `nodiff`: `backlightDurationRX` from seconds to `infinite`, and with
  the older `build-0.15.1` also `maxSpeakerVolume` one lower and an APRS
  `period: <n> s` added. The binary is
  `~/Documents/AnytoneSoftware/qdmr-release/build/cli/dmrconf` (the
  round-trip fixes), or `$DMRCONF`.
- `dmm-radio <args>` runs `build/host/tools/dmm_radio/dmm-radio`, or
  `$DMM_RADIO`.
- Pictures use `tools/screen_picture/screen_picture.py`, imported.

## Limits

- The simulator keeps nothing across a restart and has no RF, QDMR port,
  FM transceiver registers or stack report; such steps are skipped there.
- Reading the screen after each key makes menu steps slower than a hand
  on the keys: checks that two changes land within 3 s of each other
  (one write for several changes) are not timed as on the keypad.
- The volume knob and anything to look at or listen to stay with a
  person.

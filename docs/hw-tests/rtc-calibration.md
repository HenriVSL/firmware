# Real-time clock calibration and UTC display

Checks Menu > Settings > Clock on a D878UVII: Shown as (the home screen's
time in local time or UTC, kept across restarts) and Calibrate, which
measures how fast the real-time clock runs against GPS time for 30 or 60
minutes, keeping the receiver on, and then corrects the clock's rate. The
correction stays with the clock while the radio is off.

Needs: the radio with the USB log port open in a terminal, a PC whose
clock is right (NTP), a view of the sky for the whole run, and a codeplug
time zone other than UTC.

## Running it

Lines in `code` are typed on the log console; log lines named under
Expect come on the log port. Keys are named as on the key test (Menu,
Exit, Up, Down). Each step starts where the one before ended.

## 1. Shown as

| # | Do | Expect |
| --- | --- | --- |
| 1 | On the standby screen press Menu, Up twice (Settings, the second row from the end), Menu, Down four times (Clock), Menu | `Settings > Clock`: Time zone, Shown as `Local`, Calibrate; the group's panel read `Time zone, local or UTC,` / `and calibration` before the last Menu |
| 2 | Press Down; look | The panel reads `Home screen time: local,` / `or UTC with a Z` |
| 3 | Press Menu, Down, Menu | `ui: clock shows UTC`; the row shows `UTC`; `settings kept` about 3 s later |
| 4 | Press Exit until the standby screen is back | The clock shows UTC with a `Z`, such as `09:41Z`: the local time minus the time zone's offset |
| 5 | Type `settings` | The `preferences record:` line ends `clock UTC` |
| 6 | Type `restart`; wait for the standby screen | The clock still shows UTC with a `Z` |
| 7 | Open Settings > Clock as in 1.1; press Down, Menu, Up, Menu, then Exit until the standby screen is back | `ui: clock shows local time`; the standby clock shows local time without a `Z` |

## 2. Calibration from the console

| # | Do | Expect |
| --- | --- | --- |
| 1 | Type `gps off`, then `time cal 30` | `clock: calibration needs the GPS on`; the notice `Switch GPS on` |
| 2 | Type `gps on`, `time cal reset`, then `time` | `clock: rate correction 0.0 ppm (was ...)`; the `time` line shows no rate correction |
| 3 | Type `save 1` (battery save on), then `time cal 30`; wait two minutes | `clock: calibrating against GPS for 30 min`; the receiver stays on: no `receiver asleep` line |
| 4 | Type `time cal 0` | `clock: calibration stopped`; the correction is unchanged |
| 5 | Type `time cal 121`, then `time cal x` | Both `unknown command, try help` |

## 3. Calibration from the menu

| # | Do | Expect |
| --- | --- | --- |
| 1 | Open Settings > Clock as in 1.1; press Down twice; look | Calibrate shows `-`; the panel reads `Times the clock against` / `GPS and corrects its rate` |
| 2 | Press Menu; look | `30 min` and `60 min`, no Stop |
| 3 | Press Menu on `30 min` | `clock: calibrating against GPS for 30 min`; the row counts `0/30 min`, `1/30 min`, ... |
| 4 | Press Menu; look | `30 min` marked in use, `60 min`, `Stop` |
| 5 | Press Exit; wait for the end | `clock: calibration: the clock ran <rate> (<s> s a day) over <n> seconds, scatter <ms> ms`, then `clock: rate correction <minus the rate> (was 0.0 ppm)`; the notice `Clock calibrated`; the row shows `Done` and the correction |
| 6 | Type `restart`; after the start type `time` | During start and from `time`: `clock: <time> UTC, rate correction <the same>`; the row shows the correction |
| 7 | Start a 30 min run again and let it end | The clock now runs within about 5 ppm (0.4 s a day): the run's rate is small, and the correction moves by it |
| 8 | Start a run, then choose Stop | `clock: calibration stopped`; the row shows the correction, unchanged |
| 9 | Start a run indoors far from a window and let it end | Without enough fixes: `calibration: <n> seconds with a fix, <needed> needed; rate unchanged`, the notice `Too few fixes`, and the row `Too few fixes` |

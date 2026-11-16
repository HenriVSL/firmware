# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/home-screen.md: the home screen's rows set in the menu.

The channels are named as "Zone/Channel" in the parameters; the defaults
are the simulator's sample codeplug. On the radio, pass a DMR channel
whose contact is a named talkgroup that does not roam, a roaming channel,
an FM repeater channel with a tone and an offset, and an FM simplex
channel without tones (the zone is the text up to the first '/'):

  --param dmr="Zone/Name" --param roam="Zone/Name"
  --param fm_repeater="Zone/Name" --param fm="Zone/Name"

Step 13 waits gps_wait seconds (default 120) for a GPS fix, then is
skipped: --param gps_wait=<s>.

The menu is reached by name (`open Settings/Home screen/<row>`), not by
the remembered rows the checklist's H and M sequences depend on.
"""
import re
import time

from _helpers import (channel, large, log_is, panel, press_while_watching, remember, remember_log, restore, set_to,
                      settings_has, top_line)
from radiotest.radio import Skipped
from radiotest.runner import Step

TITLE = 'Home screen: rows set in the menu'
SOURCE = 'docs/hw-tests/home-screen.md (feat/mic-gain-db)'

PARAMS = {
    'dmr': 'UHF/DMR repeater',
    'roam': 'UHF/TG244 Finland',
    'fm_repeater': 'VHF/Repeater 2m',  # FM with a tone and an offset
    'fm': 'UHF/Calling 70cm',  # FM simplex without tones
    'parrot': '',
    'gps_wait': '120',  # seconds to wait for a GPS fix in step 13
}


def quiet_meter(ctx):
    """The meter shows `-` (no signal), waiting up to 30 s for a signal
    heard to go; a signal still there is reported, not failed."""
    end = time.monotonic() + 30
    while True:
        w = ctx.radio.where(fresh=True)
        texts = [r.text.strip() for r in w.runs]
        if '-' in texts:
            return True, 'meter `-`'
        if time.monotonic() > end:
            level = next((t for t in texts if re.fullmatch(r'-\d+', t)), '?')
            return True, 'signal present for 30 s (%s dBm): the quiet meter not seen, not a failure' % level
        time.sleep(2)


def roaming_answers(pattern, what):
    """Expectation: on the roaming channel the screen shows @p pattern
    within 30 s. Skips the step when the channel does not roam or no
    repeater answers."""
    def expectation(ctx):
        r = ctx.radio
        line = r.ask('roam', r'\broam: ')
        if re.search(r'\broam: (off\b|roaming off\b|not a roaming channel)', line.text):
            raise Skipped('not roaming: `%s`' % line.text)
        end = time.monotonic() + 30
        rx = re.compile(pattern, re.M)
        while time.monotonic() < end:
            w = r.where(fresh=True)
            if rx.search(w.text()):
                return True, '%s: %s' % (what, w.summary())
            time.sleep(2)
        raise Skipped('no roaming repeater answered within 30 s (%s)' % what)
    return expectation


def gps_fix(ctx):
    """Expectation: the position at the bottom once the receiver has a
    fix; skipped (the radio needs a view of the sky) after gps_wait s."""
    r = ctx.radio
    seconds = int(ctx.params['gps_wait'])
    end = time.monotonic() + seconds
    rx = re.compile(r'^\d+\.\d{5} [NS]  \d+\.\d{5} [EW]$', re.M)
    while time.monotonic() < end:
        w = r.where(fresh=True)
        m = rx.search(w.text())
        if m:
            return True, 'position `%s`' % m.group(0)
        time.sleep(5)
    raise Skipped('no GPS fix within %d s: place the radio by a window, or give it longer with '
                  '--param gps_wait=<s>' % seconds)

def nothing_beside(ctx):
    """Expectation: nothing on the right half of the power row."""
    w = ctx.radio.where(fresh=True)
    pwr = [r for r in w.runs if r.text.strip() == 'PWR']
    if not pwr:
        return False, 'no PWR: ' + w.summary()
    beside = [r.text.strip() for r in w.runs if abs(r.y - pwr[0].y) <= 4 and r.x >= 90 and r.text.strip()]
    return not beside, 'next to the power: %s' % (beside or 'nothing')


HOME = 'Settings/Home screen/'
UP_DOWN = HOME + r'Up\/Down'
ROWS = ['Preset', 'Top line', 'Clock', 'Large name', 'Middle row', 'Next to power', 'Bottom row', r'Up\/Down']
# The channel helper finds a channel by its name shown large.
DETAILED = set_to(HOME + 'Preset', 'Detailed')
# The `channel` answer: the zone and the channel in use.
PLACE = r'\bui: (zone \d+ \(.*\), channel \d+ of \d+: [^,]*)'


def row_key(row):
    return 'home_' + re.sub(r'\W+', '_', row)


def zone_of(place):
    return place.split(' (', 1)[0]


def vfo_up_steps(ctx):
    """Expectation: in the VFO, Up changes the frequency shown (as knob 1
    on does); # then goes back to the channel."""
    r = ctx.radio
    r.key('#')
    before = r.where(fresh=True)
    r.key('Up')
    after = r.where(fresh=True)
    r.key('#')
    return before.text() != after.text() and 'VFO' in before.text(), 'before `%s`, after `%s`' % (
        before.summary(), after.summary())


BEFORE = [remember(row_key(r), HOME + r) for r in ROWS] + [
    remember('gps', 'GPS/Enabled'),
    set_to(UP_DOWN, 'Zone'),
    'home',
]

UNDO = [restore(HOME + r, row_key(r)) for r in ROWS] + [
    restore('GPS/Enabled', 'gps'),
    'home',
]

STEPS = [
    Step(1, 'Detailed layout on the DMR channel', do=['restart', channel('{dmr}'), 'shot detailed-dmr'], expect=[
        top_line(r'^TG \d+$'), r'screen ^\d\d:\d\d$', large(r'.'), r'screen ^MHz$', r'screen ^\d{3}\.\d{3}$',
        r'screen ^TS\d$', r'screen ^CC\d+$', r'screen ^PWR$', r'screen ^BAT$', r'screen ^\d\.\d\dV$',
        r'screen ^\S.*  \d+/\d+$']),
    Step(2, 'An FM repeater channel: tones and offset at the bottom', do=[channel('{fm_repeater}'),
                                                                        'shot detailed-fm-repeater'], expect=[
        top_line('^CHANNEL$'), 'screen ^FM$', r'screen ^(25k|12\.5k)$', r'screen [-+]\d+\.\d{3}$']),
    Step('2b', 'An FM simplex channel: the zone and the place at the bottom', do=[
        channel('{fm}'), 'shot detailed-fm-simplex'], expect=[
        top_line('^CHANNEL$'), 'screen ^FM$', r'screen ^\S.*  \d+/\d+$', r'noscreen [-+]\d+\.\d{3}$']),
    Step(3, 'Settings > Home screen', do=[channel('{dmr}'), 'goto ' + HOME + 'Preset'], expect=[
        'title ^Home screen$', 'place ^1/8$', 'row ^Preset Detailed$',
        r'screen ^Top line\nTalkgroup\nClock\nOn\nLarge name\nChannel\nMiddle row\nFrequency$',
        'screen ' + panel('A whole layout at once; rows can then change')]),
    Step(4, 'Preset: three buttons, Detailed starred', do=['at ' + HOME + 'Preset', 'key Menu'], expect=[
        'title ^Preset$', 'kind ^buttons$', r'screen ^Detailed\n(\*\n)?Simple\n(\*\n)?Roaming$', 'in_use ^Detailed$']),
    Step(5, 'Preset Simple: the channel, its frequency, the power', changes=True, timeout=8, do=[
        'set Simple', 'home', 'shot simple'], expect=[
        r'log \bui: home screen Simple', top_line(None), large(r'.'), r'screen ^MHz$', r'screen ^PWR$',
        r'noscreen ^(BAT|VOL|SIG)$', r'noscreen ^No call heard yet$', r'noscreen  \d+/\d+$',
        r'log@8 \bsettings kept in the codeplug']),
    Step(6, 'Preset Roaming on a quiet DMR channel', changes=True, timeout=8, do=[
        'open ' + HOME + 'Preset', 'set Roaming', channel('{dmr}'), 'shot roaming-quiet'], expect=[
        r'log \bui: home screen Roaming', 'screen ^SIG$', 'screen ^dBm$', quiet_meter]),
    Step(7, 'The meter while another radio transmits', radio_only='needs a signal',
         human='Key another radio on this channel for 5 s, then stop, watching the meter: bars and the level '
               'while it transmits; `-` and dark bars again about 2 s after it stops.'),
    Step('8a', 'The roaming channel: the repeater at the bottom', do=[channel('{roam}')], expect=[
        roaming_answers(r'^via \d+ \S', 'the repeater at the bottom')]),
    Step('8b', 'The meter shows the repeater\'s last level', radio_only='needs a repeater heard', do=[
        channel('{roam}')], expect=[roaming_answers(r'^-\d+$', 'the level on the meter')]),
    Step(9, 'Roaming layout kept across a restart; no noise on the meter', radio_only='nothing kept across a restart',
         do=['restart', channel('{dmr}')], expect=['screen ^SIG$', 'screen ^-$']),
    Step(10, 'Top line Number; the preset shows Own', changes=True, do=[
        'open ' + HOME + 'Top line', 'set Number'], expect=['row ^Top line Number$', r'screen ^Preset\nOwn$']),
    Step(11, 'Next to power Volume, bottom row Position', changes=True, do=[
        'choose Next to power', 'key Menu', 'set Volume', 'choose Bottom row', 'key Menu', 'set Position', 'home',
        'shot own'], expect=[top_line(r'^CH \d+/\d+$'), 'screen ^SIG$', 'screen ^VOL$',
                             r'screen ^No GPS position$']),
    Step(12, 'The volume knob', human='Turn the volume knob: the volume notice, then `VOL` shows the new step.'),
    Step(13, 'The position at the bottom', radio_only='needs a GPS fix', changes=True, do=['cmd gps on'],
         expect=[gps_fix]),
    Step('14a', 'The layout in the preferences record', do=['wait 4'], expect=[
        settings_has(r'home screen 2 1 0 2 1 2\b', 'preferences')]),
    Step('14b', 'The layout kept across a restart', radio_only='nothing kept across a restart',
         do=['restart'], expect=[settings_has(r'home screen 2 1 0 2 1 2\b', 'preferences'), r'screen ^CH \d+/\d+$',
                                 'screen ^VOL$']),
    Step('15a', 'Bottom row Last caller', changes=True, do=[
        'open ' + HOME + 'Bottom row', 'set Last caller', channel('{dmr}')], expect=[
        r'screen ^No call heard yet$|^Last: ']),
    Step('15b', 'A call heard: Last: <caller> now, a minute later 1 min ago', transmit=True, timeout=20,
         human='Check that the parrot answered the call (heard from the speaker).',
         do=[channel('{dmr}'), 'cmd dmr pc {parrot}', 'cmd ptt 3000', 'wait 12'],
         expect=[r'log \bdmr: transmitting', r'screen ^Last: .*  now$', r'screen@75 ^Last: .*1 min ago$'],
         undo=['cmd dmr tg off']),
    Step(16, 'Clock Off, Top line None, Large name Zone, Signal next to power', changes=True, do=[
        channel('{dmr}'), 'open ' + HOME + 'Clock', 'set Off', 'choose Top line', 'key Menu', 'set None',
        'choose Large name',
        'key Menu', 'set Zone', 'choose Next to power', 'key Menu', 'set Signal', 'home', 'shot zone-large'],
         expect=[top_line(None), r'noscreen ^\d\d:\d\d$', large(r'^\S'),
                 'screen ^SIG$', 'screen ^-$']),
    Step(17, 'A minute without a key: nothing blanks or flashes', timeout=70, do=[
        press_while_watching(60, [])], expect=['flicker 0']),
    Step('18a', 'The channel knob one on, one back: drawn in place', timeout=10, do=[
        press_while_watching(5, ['knob 1', 'wait 1', 'knob -1'])], expect=['flicker 0']),
    Step('18b', 'The volume knob: drawn in place',
         human='Turn the volume knob, watching the screen: each change drawn in place, nothing blanks or flashes.'),
    Step(19, 'QDMR round trip keeps the layout', radio_only='no QDMR port', timeout=10, do=[
        'qdmr read home-a.yaml', 'qdmr write home-a.yaml'], expect=[
        settings_has(r'home screen ', 'preferences'), large(r'^\S'), r'noscreen ^\d\d:\d\d$']),
    Step(20, 'QDMR read back: no differences', radio_only='no QDMR port', do=[
        'qdmr read home-b.yaml', 'qdmr diff home-a.yaml home-b.yaml'], expect=['nodiff']),
    Step(21, 'ui stack', radio_only='the simulator reports no stack words', timeout=75, expect=['stack ui >50']),
    Step(22, 'Next to power None', changes=True, timeout=8, do=[
        'open ' + HOME + 'Next to power', r'check screen ^Voltage\n(\*\n)?Volume\n(\*\n)?Signal\n(\*\n)?None$',
        'set None', 'home', 'shot beside-none', 'wait 4'], expect=[
        r'screen ^PWR$', nothing_beside, settings_has(r'home screen \d \d \d \d 3 \d\b', 'preferences')]),
    Step(23, 'The # hint names the other mode', changes=True, do=[
        DETAILED, channel('{dmr}'), r'check screen # VFO$', 'key #', r'check screen ^VFO$', 'shot hint-vfo',
        r'check screen # MEM$', 'key #'], expect=[r'screen # VFO$', r'noscreen ^VFO$']),
    Step(24, 'Up/Down: Zone, Channel, Off as buttons, Zone in use', do=[
        'goto ' + UP_DOWN, 'check screen ' + panel('Up and Down: next zone, next channel or off'),
        'check row ^Up/Down Zone$', 'key Menu', 'shot up-down'], expect=[
        'kind ^buttons$', r'screen ^Zone\n(\*\n)?Channel\n(\*\n)?Off$', 'in_use ^Zone$'], undo=['home'],
         undo_now=True),
    Step(25, 'Up: the next zone, on its channel; Down: back on the channel left', changes=True, timeout=8, do=[
        DETAILED, channel('{dmr}'), 'knob 1', 'wait 5', remember_log('place', PLACE, 'channel'), 'mark', 'key Up',
        remember_log('place_up', PLACE), 'mark', 'key Down'], expect=[
        lambda ctx: (zone_of(ctx.params['place_up']) != zone_of(ctx.params['place']),
                     'before `%s`, after Up `%s`' % (ctx.params['place'], ctx.params['place_up'])),
        log_is(lambda p: r'\bui: ' + re.escape(p['place']))]),
    Step(26, 'In the VFO Up and Down do nothing', changes=True, do=[
        DETAILED, channel('{dmr}'), 'key #', r'check screen ^VFO$', 'mark', 'keys Up Down', 'wait 1'], expect=[
        r'screen ^VFO$', r'nolog \bui: zone'], undo=['key #'], undo_now=True),
    Step(27, 'Up/Down Channel: steps as the knob, in the VFO too', changes=True, timeout=8, do=[
        DETAILED, set_to(UP_DOWN, 'Channel'), r'check log \bui: up/down Channel', channel('{dmr}'),
        remember_log('place', PLACE, 'channel'), 'key Up', remember_log('place_up', PLACE, 'channel'),
        'key Down', remember_log('place_down', PLACE, 'channel')], expect=[
        lambda ctx: (zone_of(ctx.params['place_up']) == zone_of(ctx.params['place']) and
                     ctx.params['place_up'] != ctx.params['place'] and ctx.params['place_down'] == ctx.params['place'],
                     '`%s`, Up `%s`, Down `%s`' % (ctx.params['place'], ctx.params['place_up'],
                                                    ctx.params['place_down'])),
        vfo_up_steps, r'log@8 \bsettings kept in the codeplug']),
    Step(28, 'Up/Down Off: nothing', changes=True, timeout=8, do=[
        DETAILED, set_to(UP_DOWN, 'Off'), r'check log \bui: up/down Off', channel('{dmr}'),
        remember_log('place', PLACE, 'channel'), 'keys Up Down', 'wait 1',
        remember_log('place_after', PLACE, 'channel')], expect=[
        lambda ctx: (
            ctx.params['place_after'] == ctx.params['place'], '`%s`, then `%s`' % (
                ctx.params['place'], ctx.params['place_after'])), r'log@8 \bsettings kept in the codeplug']),
    Step(29, 'Up/Down Off kept across a restart', radio_only='nothing kept across a restart', do=['restart'],
         expect=[settings_has(r'up/down 2\b', 'preferences')]),
]

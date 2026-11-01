# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/home-screen.md: the home screen's rows set in the menu.

The channels are named as "Zone/Channel" in the parameters; the defaults
are the simulator's sample codeplug. On the radio, pass a DMR channel
whose contact is a named talkgroup that does not roam, a roaming channel
and an FM channel with tones and an offset:

  --param dmr="Zone/Name" --param roam="Zone/Name" --param fm="Zone/Name"

The menu is reached by name (`open Settings/Home screen/<row>`), not by
the remembered rows the checklist's H and M sequences depend on.
"""
from _helpers import channel, large, panel, press_while_watching, remember, restore, settings_has, top_line
from radiotest.runner import Step

TITLE = 'Home screen: rows set in the menu'
SOURCE = 'docs/hw-tests/home-screen.md (feat/mic-gain-db)'

PARAMS = {
    'dmr': 'UHF/DMR repeater',
    'roam': 'UHF/TG244 Finland',
    'fm': 'VHF/Repeater 2m',
    'parrot': '',
}

HOME = 'Settings/Home screen/'
ROWS = ['Preset', 'Top line', 'Clock', 'Large name', 'Middle row', 'Next to power', 'Bottom row']

BEFORE = [remember('home_' + r.replace(' ', '_'), HOME + r) for r in ROWS] + [
    remember('gps', 'GPS/Receiver'),
    'home',
]

UNDO = [restore(HOME + r, 'home_' + r.replace(' ', '_')) for r in ROWS] + [
    restore('GPS/Receiver', 'gps'),
    'home',
]

STEPS = [
    Step(1, 'Detailed layout on the DMR channel', do=['restart', channel('{dmr}'), 'shot detailed-dmr'], expect=[
        top_line(r'^TG \d+$'), r'screen ^\d\d:\d\d$', large(r'.'), r'screen ^MHz$', r'screen ^\d{3}\.\d{3}$',
        r'screen ^TS\d$', r'screen ^CC\d+$', r'screen ^PWR$', r'screen ^BAT$', r'screen ^\d\.\d\dV$',
        r'screen ^\S.*  \d+/\d+$']),
    Step(2, 'The FM channel', do=[channel('{fm}'), 'shot detailed-fm'], expect=[
        top_line('^CHANNEL$'), 'screen ^FM$', r'screen ^(25k|12\.5k)$', r'screen [-+]\d+\.\d{3}$']),
    Step(3, 'Settings > Home screen', do=[channel('{dmr}'), 'goto ' + HOME + 'Preset'], expect=[
        'title ^Home screen$', 'place ^1/7$', 'row ^Preset Detailed$',
        r'screen ^Top line\nTalkgroup\nClock\nOn\nLarge name\nChannel\nMiddle row\nFrequency$',
        'screen ' + panel('A whole layout at once; rows can then change')]),
    Step(4, 'Preset: three buttons, Detailed starred', do=['at ' + HOME + 'Preset', 'key Menu'], expect=[
        'title ^Preset$', 'kind ^buttons$', r'screen ^Detailed\n(\*\n)?Simple\n(\*\n)?Roaming$', 'in_use ^Detailed$']),
    Step(5, 'Preset Simple', changes=True, timeout=8, do=['set Simple', 'home', 'shot simple'], expect=[
        r'log \bui: home screen Simple', top_line(None), large(r'.'), r'screen ^TG \d+$',
        r'screen ^No call heard yet$', r'log@8 \bsettings kept in the codeplug']),
    Step(6, 'Preset Roaming on a quiet DMR channel', changes=True, timeout=8, do=[
        'open ' + HOME + 'Preset', 'set Roaming', channel('{dmr}'), 'shot roaming-quiet'], expect=[
        r'log \bui: home screen Roaming', 'screen ^SIG$', 'screen ^-$', 'screen ^dBm$']),
    Step(7, 'The meter while another radio transmits', radio_only='needs a signal',
         human='Key another radio on this channel for 5 s, then stop, watching the meter: bars and the level '
               'while it transmits; `-` and dark bars again about 2 s after it stops.'),
    Step('8a', 'The roaming channel: the repeater at the bottom', do=[channel('{roam}'), 'wait 3'], expect=[
        r'screen ^via \d+ \S']),
    Step('8b', 'The meter shows the repeater\'s last level', radio_only='needs a repeater heard', timeout=30,
         do=['shot roaming-repeater'], expect=[r'log@30 \broam: ', r'screen ^-\d+$']),
    Step(9, 'Roaming layout kept across a restart; no noise on the meter', radio_only='nothing kept across a restart',
         do=['restart', channel('{dmr}')], expect=['screen ^SIG$', 'screen ^-$']),
    Step(10, 'Top line Number; the preset shows Own', changes=True, do=[
        'open ' + HOME + 'Top line', 'set Number'], expect=['row ^Top line Number$', r'screen ^Preset\nOwn$']),
    Step(11, 'Next to power Volume, bottom row Position', changes=True, do=[
        'choose Next to power', 'key Menu', 'set Volume', 'choose Bottom row', 'key Menu', 'set Position', 'home',
        'shot own'], expect=[top_line(r'^CH \d+/\d+$'), 'screen ^SIG$', 'screen ^VOL$',
                             r'screen ^No GPS position$']),
    Step(12, 'The volume knob', human='Turn the volume knob: the volume notice, then `VOL` shows the new step.'),
    Step(13, 'The position at the bottom', radio_only='needs a GPS fix', changes=True, timeout=600,
         do=['cmd gps on'], expect=[r'log@600 \bgps: .*fix', r'screen ^\d+\.\d{5} [NS]  \d+\.\d{5} [EW]$']),
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
        channel('{dmr}'), 'open ' + HOME + 'Clock', 'set Off', 'choose Top line', 'key Menu', 'set None', 'choose Large name',
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
]

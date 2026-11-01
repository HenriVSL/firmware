# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/settings-storage.md: settings changed on the radio are
kept in the codeplug after 3 s, survive a restart, and wait while the PC
programs the radio.

BEFORE keeps the values the steps change (from `settings` and the menu)
and UNDO sets them back. The QDMR steps write files into the report
folder; a codeplug backup from before (`before.yaml`) is read first on the
radio.
"""
import re
import threading
import time

from _helpers import remember, restore, set_other, set_to, settings_has
from radiotest.radio import Failed
from radiotest.runner import Step

TITLE = 'Settings storage'
SOURCE = 'docs/hw-tests/settings-storage.md (feat/mic-gain-db)'


def light_seconds(text):
    """`light time` seconds for the codeplug line's light time text."""
    m = re.match(r'(\d+) (s|min)', text)
    if not m:
        return 0
    return int(m.group(1)) * (60 if m.group(2) == 'min' else 1)


def keep_console_values(ctx):
    line = ctx.radio.settings().get('codeplug', '')
    m = re.search(r'light \d (\S+(?: s| min)?), battery save (\d)', line)
    if not m:
        raise Failed('no light and battery save in `%s`' % line)
    ctx.params['light_s'] = light_seconds(m.group(1))
    ctx.params['save'] = m.group(2)
    ctx.evidence('kept: `%s`' % line)


def put_console_values(ctx):
    if 'light_s' in ctx.params:
        ctx.radio.cmd('light time %d' % ctx.params['light_s'])
        ctx.radio.cmd('save %s' % ctx.params['save'])


def count_kept(n, seconds=5.0):
    """Expectation: exactly @p n `settings kept` lines, waiting @p seconds
    for more."""
    def expectation(ctx):
        time.sleep(seconds)
        found = ctx.radio.find(r'\bsettings kept in the codeplug', ctx.since)
        return len(found) == n, '%d `settings kept` lines (want %d)%s' % (
            len(found), n, ': ' + '; '.join('`%s`' % l.text for l in found) if found else '')
    return expectation


def read_while_changing(ctx):
    """dmrconf reads the codeplug while the console changes a setting:
    the write waits for the read, then goes in."""
    r = ctx.radio
    out = {}

    def read():
        try:
            out['text'] = r.qdmr_read(ctx.path('during.yaml'))
        except Exception as e:
            out['error'] = str(e)

    t = threading.Thread(target=read)
    t.start()
    time.sleep(2.0)
    r.cmd('light time 20')
    t.join(600)
    if 'error' in out:
        return False, 'the read failed: %s' % out['error']
    line = r.wait(r'\bsettings wait: the PC is programming', 1, ctx.since)
    kept = r.wait(r'\bsettings kept in the codeplug', 10, ctx.since)
    return True, '`%s`; then `%s`' % (line.text, kept.text)


BEFORE = [
    keep_console_values,
    remember('squelch', 'Channel/Squelch'),
    remember('squelch_type', 'Channel/Squelch type'),
    remember('power', 'Channel/Power'),
    remember('roaming', 'Roaming/Roaming'),
    remember('roam_zone', 'Roaming/Roam zone'),
    remember('brightness', 'Settings/Display/Brightness'),
    remember('light_menu', 'Settings/Display/Light time'),
    remember('gps', 'GPS/Receiver'),
    remember('satellites', 'Settings/GPS/Satellites', 'in_use'),
    remember('time_zone', 'Settings/Clock/Time zone'),
    'home',
]

UNDO = [
    restore('Channel/Squelch', 'squelch'),
    restore('Channel/Squelch type', 'squelch_type'),
    restore('Roaming/Roaming', 'roaming'),
    restore('Roaming/Roam zone', 'roam_zone'),
    restore('Settings/Display/Brightness', 'brightness'),
    restore('Settings/Display/Light time', 'light_menu'),
    restore('GPS/Receiver', 'gps'),
    restore('Settings/GPS/Satellites', 'satellites'),
    restore('Settings/Clock/Time zone', 'time_zone'),
    put_console_values,
    'home',
]

STEPS = [
    Step('0', 'Codeplug backup before the steps', radio_only='no QDMR port', do=['qdmr read before.yaml']),
    Step(1, 'Start: no crash, the codeplug line, the preferences line', do=['restart', 'wait 4'], expect=[
        r'nolog \bcrash:', settings_has(r'nothing pending|pending: zone', 'pending'),
        settings_has(r'squelch \d+/\d+, zone', 'codeplug'), settings_has(r'preferences record', 'preferences')]),
    Step(2, 'save 1, light time 60: pending, then kept 3 s after', changes=True, timeout=8, do=[
        'cmd save 1', 'cmd light time 60'], expect=[
        settings_has(r'pending: settings', 'pending'), r'log@8 \bsettings kept in the codeplug']),
    Step(3, 'Kept: battery save 1, light 1 min', expect=[
        settings_has(r'nothing pending; last write saved', 'pending'),
        settings_has(r'light \d 1 min, battery save 1', 'codeplug')]),
    Step(4, 'Three light times in 2 s: one write', changes=True, timeout=8, do=[
        'cmd light time 5', 'wait 0.5', 'cmd light time 10', 'wait 0.5', 'cmd light time 15'], expect=[
        count_kept(1), settings_has(r'light \d 15 s', 'codeplug')]),
    Step(5, 'The same value again: no write', do=['cmd save 1'], expect=[
        count_kept(0), settings_has(r'last write unchanged', 'pending')]),
    Step(6, 'Kept across a restart', radio_only='nothing is kept across a restart', do=['restart'], expect=[
        lambda ctx: (True, '; '.join('`%s`' % l.text for l in ctx.radio.collect('save'))),
        r'log \bpower: battery save on, 150 ms sleep', settings_has(r'light \d 15 s, battery save 1', 'codeplug')],
         human='The backlight goes off 15 s after the last key.'),
    Step('7a', 'Channel and roaming in the menu: one write after the last', changes=True, timeout=8, do=[
        set_to('Channel/Squelch', '4'), set_other('Channel/Squelch type', 'squelch_type_new'),
        set_to('Roaming/Roaming', 'Off')], expect=[r'log@8 \bsettings kept in the codeplug']),
    Step('7b', 'Power the other one', transmit=True, changes=True, timeout=8, do=[
        set_other('Channel/Power', 'power_new')], expect=[r'log \bui: power \d',
                                                          r'log@8 \bsettings kept in the codeplug'],
         undo=[restore('Channel/Power', 'power')]),
    Step('7c', 'Roam zone another one', radio_only='one roaming zone in the sample codeplug', changes=True, timeout=8,
         do=[set_other('Roaming/Roam zone', 'roam_zone_new')], expect=[r'log@8 \bsettings kept in the codeplug']),
    Step(8, 'Display, GPS, satellites, time zone: one write after the last', changes=True, timeout=8, do=[
        set_to('Settings/Display/Brightness', '2'), set_to('Settings/Display/Light time', '30 s'),
        set_to('GPS/Receiver', 'On'), set_to('Settings/GPS/Satellites', 'GPS'),
        'open Settings/Clock/Time zone', 'key Up', 'key Menu', 'home'], expect=[
        r'log@8 \bsettings kept in the codeplug']),
    Step(9, 'Another zone, two channels on: all in the codeplug line', changes=True, timeout=8, do=[
        'home', 'key P1', 'key Down', 'key Menu', 'knob 2', 'wait 4'], expect=[
        settings_has(r'squelch 4/\d+, zone \d+/\d+, light 2 30 s, .*GPS on systems 1\b', 'codeplug'),
        settings_has(r'roaming off', 'preferences')]),
    Step(10, 'Kept across a restart: zone, channel, light, power, squelch type, roaming',
         radio_only='nothing is kept across a restart', do=['restart'], expect=[
             r'log \broam: roaming off', settings_has(r'squelch 4/\d+, zone \d+/\d+, light 2 30 s', 'codeplug'),
             lambda ctx: (ctx.radio.goto('Roaming/Roaming').value == 'Off', ctx.radio.where().summary())],
         human='The screen is dimmer (level 2) and on the zone and channel of step 9; the channel of step 7 has '
               'its new power and squelch type.'),
    Step(11, 'A codeplug read waits out a setting', radio_only='no QDMR port', changes=True, timeout=600,
         expect=[read_while_changing]),
    Step(12, 'QDMR round trip: read', radio_only='no QDMR port', do=['qdmr read a.yaml'], expect=[
        settings_has(r'squelch', 'codeplug')]),
    Step(13, 'QDMR write; the radio restarts; the record kept', radio_only='no QDMR port', timeout=10, do=[
        'qdmr write a.yaml'], expect=[settings_has(r'roaming off', 'preferences')]),
    Step(14, 'QDMR read back: no differences', radio_only='no QDMR port', do=[
        'qdmr read b.yaml', 'qdmr diff a.yaml b.yaml'], expect=['nodiff']),
    Step(15, 'ui stack', radio_only='the simulator reports no stack words', timeout=75, expect=['stack ui >50']),
]

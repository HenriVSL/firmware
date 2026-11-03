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
import os
import re
import threading
import time
import tty

from _helpers import channel, remember, restore, set_other, set_to, settings_has
from radiotest import transport
from radiotest.radio import Failed, Skipped
from radiotest.runner import Step

TITLE = 'Settings storage'
SOURCE = 'docs/hw-tests/settings-storage.md (feat/offset-numeric)'

# Step 12 changes the squelch type, which only an FM channel with a receive
# tone keeps: on the radio, pass the first of two such channels in a row
# as "Zone/Channel" (empty: the channel in use).
PARAMS = {'tone_channel': ''}


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
    """A setting changed, then about 2 s later a QDMR read (about 2 s
    long): the write waits for the read (`settings wait`), then goes in
    right after it; a second change is kept 3 s later as usual."""
    r = ctx.radio
    changed = r.cmd('light time 20')
    time.sleep(2.0)
    out = {}

    def read():
        try:
            out['text'] = r.qdmr_read(ctx.path('during.yaml'))
        except Exception as e:
            out['error'] = str(e)

    started = r.mark()
    t = threading.Thread(target=read)
    t.start()
    t.join(600)
    ended = r.mark()
    if 'error' in out:
        return False, 'the read failed: %s' % out['error']
    waits = [l for l in r.find(r'\bsettings wait: the PC is programming', changed) if started <= l.seq <= ended]
    if not waits:
        return False, 'no `settings wait` while the read ran'
    try:
        kept = r.wait(r'\bsettings kept in the codeplug', 4.0, ended)
    except Failed as e:
        return False, '`%s`; but after the read: %s' % (waits[0].text, e)
    early = [l for l in r.find(r'\bsettings kept in the codeplug', changed) if l.seq < ended]
    if early:
        return False, 'kept during the read: `%s`' % early[0].text
    again = r.cmd('light time 30')
    try:
        second = r.wait(r'\bsettings kept in the codeplug', 6.0, again)
    except Failed as e:
        return False, 'light time 30: %s' % e
    return True, '`%s` during the read; `%s` right after it; `%s` after light time 30' % (
        waits[0].text, kept.text, second.text)


class ProgramMode:
    """Holds the radio in program mode, as a codeplug tool does while it
    reads or writes: PROGRAM, an identify every few seconds, then END."""

    def __enter__(self):
        self.fd = os.open(os.path.join('/dev', transport.data_device()), os.O_RDWR | os.O_NOCTTY)
        tty.setraw(self.fd)
        os.write(self.fd, b'PROGRAM')
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self._keep, daemon=True)
        self.thread.start()
        return self

    def _keep(self):
        while not self.stop.wait(3.0):
            os.write(self.fd, b'\x02')

    def __exit__(self, *exc):
        self.stop.set()
        self.thread.join()
        os.write(self.fd, b'END')
        time.sleep(0.5)
        os.close(self.fd)


def flip_squelch_types(ctx):
    """The squelch type of this channel and the next one each set to the
    other choice (there are two)."""
    r = ctx.radio
    set_other('Channel/Squelch type', 'sq_a')(ctx)
    r.knob(1)
    set_other('Channel/Squelch type', 'sq_b')(ctx)
    r.knob(-1)


def to_tone_channel(ctx):
    if ctx.params.get('tone_channel'):
        channel('{tone_channel}')(ctx)


def other_roam_zone(ctx):
    """Roam zone set to another one; skipped with a single roaming zone."""
    w = ctx.radio.goto('Roaming/Roam zone')
    ctx.radio.home()
    try:
        set_other('Roaming/Roam zone', 'roam_zone_new')(ctx)
    except Failed as e:
        if 'did nothing' in str(e) or 'no other choice' in str(e):
            raise Skipped('one roaming zone in the codeplug (`%s`)' % w.value)
        raise


def channels_while_programming(ctx):
    """Two channels changed while the PC holds program mode: both stay kept
    while the writes are refused, and go in together once it leaves."""
    r = ctx.radio
    start = r.mark()
    with ProgramMode():
        flip_squelch_types(ctx)
        time.sleep(3.5)
        pending = r.settings().get('pending', '')
        waits = r.find(r'\bsettings wait: the PC is programming', start)
        early = r.find(r'\bsettings kept in the codeplug', start)
    left = r.mark()
    if not waits:
        return False, 'no `settings wait` in program mode'
    if early:
        return False, 'kept in program mode: `%s`' % early[0].text
    if not re.search(r'pending:.* channel \d+ channel \d+', pending):
        return False, 'not two channels pending: `%s`' % pending
    try:
        kept = r.wait(r'\bsettings kept in the codeplug', 8.0, left)
    except Failed as e:
        return False, 'after program mode: %s' % e
    a = r.goto('Channel/Squelch type').value
    r.home()
    r.knob(1)
    b = r.goto('Channel/Squelch type').value
    r.home()
    r.knob(-1)
    ok = a == ctx.params['sq_a'] and b == ctx.params['sq_b']
    return ok, '`%s`; `%s`; `%s`; squelch types now `%s` and `%s` (want `%s` and `%s`)' % (
        waits[0].text, pending, kept.text, a, b, ctx.params['sq_a'], ctx.params['sq_b'])


def put_squelch_types_back(ctx):
    at = ctx.radio.mark()
    flip_squelch_types(ctx)
    ctx.radio.wait(r'\bsettings kept in the codeplug', 8.0, at)


BEFORE = [
    keep_console_values,
    remember('squelch', 'Channel/Squelch'),
    remember('squelch_type', 'Channel/Squelch type'),
    remember('power', 'Channel/Power'),
    remember('roaming', 'Roaming/Roaming'),
    remember('roam_zone', 'Roaming/Roam zone'),
    remember('brightness', 'Display/Brightness'),
    remember('light_menu', 'Display/Light time'),
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
    restore('Display/Brightness', 'brightness'),
    restore('Display/Light time', 'light_menu'),
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
         do=[other_roam_zone], expect=[r'log@8 \bsettings kept in the codeplug']),
    Step(8, 'Display, GPS, satellites, time zone: one write after the last', changes=True, timeout=8, do=[
        set_to('Display/Brightness', '2'), set_to('Display/Light time', '30 s'),
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
    Step(11, 'A codeplug read waits for a setting to be kept', radio_only='no QDMR port', changes=True, timeout=600,
         expect=[read_while_changing]),
    Step(12, 'Two channels changed while the PC programs: both kept, then written together',
         radio_only='no data port to hold program mode', changes=True, timeout=40,
         do=[to_tone_channel], expect=[channels_while_programming], undo=[put_squelch_types_back]),
    Step(13, 'QDMR round trip: read', radio_only='no QDMR port', do=['qdmr read a.yaml'], expect=[
        settings_has(r'squelch', 'codeplug')]),
    Step(14, 'QDMR write; the radio restarts; the record kept', radio_only='no QDMR port', timeout=10, do=[
        'qdmr write a.yaml'], expect=[settings_has(r'roaming off', 'preferences')]),
    Step(15, 'QDMR read back: no differences', radio_only='no QDMR port', do=[
        'qdmr read b.yaml', 'qdmr diff a.yaml b.yaml'], expect=['nodiff']),
    Step(16, 'ui stack', radio_only='the simulator reports no stack words', timeout=75, expect=['stack ui >50']),
]

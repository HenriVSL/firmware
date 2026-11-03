# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/frequency-offset.md: the frequency offset from the menu
and the console, kept across restarts.

The transmit steps read a frequency counter: they run only with
--allow-transmit and ask the operator. Pass the radio's own error from
1.2, with its sign turned round, as --param correction=<Hz> for 5.1 and
6.2, and an FM channel the radio may transmit on as --param fm="Zone/Name".
"""
import re

from _helpers import channel, panel, settings_has
from radiotest.runner import Step

TITLE = 'Frequency offset'
SOURCE = 'docs/hw-tests/frequency-offset.md (feat/frequency-offset)'

PARAMS = {'fm': 'VHF/Calling 2m', 'dmr': 'UHF/DMR repeater', 'correction': '0'}

ROW = 'Settings/Radio/Freq offset'


def keep_offset(ctx):
    line = ctx.radio.ask('fm status', r'offset \S+ Hz')
    ctx.params['offset'] = re.search(r'offset ([-+]?[\d.]+) Hz', line.text).group(1)
    ctx.evidence('kept: `%s`' % line.text)


def put_offset(ctx):
    if 'offset' in ctx.params:
        ctx.radio.cmd('fm offset %d' % round(float(ctx.params['offset'])))


def status_ends(text):
    """The `fm status` line names the offset @p text; the FM chip's own
    figures may follow it."""
    def expectation(ctx):
        line = ctx.radio.ask('fm status', r'offset \S+ Hz')
        return re.search(r'\b' + re.escape(text) + r'(,|$)', line.text.rstrip()) is not None, '`%s`' % line.text
    return expectation


def counter(what):
    return 'Hold PTT is done by the runner (3 s). Read the frequency counter: %s' % what


def ptt(seconds=3):
    return [channel('{fm}'), 'cmd ptt %d' % (seconds * 1000), 'wait %d' % (seconds + 2)]


BEFORE = [keep_offset]
UNDO = [put_offset]

STEPS = [
    Step('1.1', 'Offset 0 after a restart', changes=True, do=['cmd fm offset 0', 'wait 5', 'restart'], expect=[
        status_ends('offset 0 Hz')]),
    Step('1.2', 'The radio\'s own error', transmit=True, do=ptt(), expect=[r'log \bfm: transmitting'],
         human=counter('note measured minus nominal: the radio\'s own error E.')),
    Step('2.1', 'Settings with Radio', do=['goto Settings/Home screen'], expect=[
        'title ^Menu > Settings$', 'place ^1/[67]$',
        r'screen ^Home screen\n>\nAudio\n>\nRadio\n>\nKeys & battery\n>']),
    Step('2.2', 'Radio at 3/6', do=['choose Radio'], expect=[
        'place ^3/[67]$', 'row ^Radio >$', 'screen ' + panel('Frequency offset for a drifted reference')]),
    Step('2.3', 'Settings > Radio', do=['key Menu'], expect=[
        'title ^Settings > Radio$', 'row ^Freq offset 0 Hz$',
        'screen ' + panel('Shifts every frequency; Up and Down 500 Hz')]),
    Step('2.4', 'The value screen', do=['key Menu', 'shot offset'], expect=[
        'title ^Freq offset$', 'kind ^spinner$', 'value ^0 Hz$', 'in_use ^0 Hz$']),
    Step('2.5', 'The knob: 62.5 Hz a click', do=['knob 1', r'check value ^\+62\.5 Hz$', 'knob -2'], expect=[
        r'value ^-62\.5 Hz$']),
    Step('2.6', 'Up four times', do=['keys Up Up Up Up'], expect=[r'value ^\+1937\.5 Hz$']),
    Step('2.7', 'Exit leaves it', do=['key Exit'], expect=['row ^Freq offset 0 Hz$', r'nolog \bfrequency offset\b']),
    Step('2.8', 'Up four times from 0', do=['at ' + ROW, 'key Menu', 'keys Up Up Up Up'], expect=[
        r'value ^\+2000 Hz$']),
    Step('2.9', 'Set +2000 Hz', changes=True, timeout=8, do=['key Menu'], expect=[
        r'log \bfm: frequency offset \+2000 Hz', r'row ^Freq offset \+2000 Hz$',
        r'log@8 \bsettings kept in the codeplug']),
    Step('2.10', '2000 Hz above the radio\'s own error', transmit=True, do=ptt(), expect=[r'log \bfm: transmitting'],
         human=counter('2000 Hz above the reading of 1.2.')),
    Step('2.11', 'The top end: +5000 Hz, no wrap', do=[
        'at ' + ROW, 'key Menu', 'keys Up Up Up Up Up Up', r'check value ^\+5000 Hz$', 'keys Up Up', 'knob 1'],
         expect=[r'value ^\+5000 Hz$']),
    Step('2.12', 'The bottom end: -5000 Hz, no wrap', do=['keys ' + ' '.join(['Down'] * 20), 'knob -1'], expect=[
        r'value ^-5000 Hz$']),
    Step('2.13', 'Exit: still +2000 Hz', do=['key Exit'], expect=[r'row ^Freq offset \+2000 Hz$']),
    Step('3.1', 'Kept across a restart', radio_only='nothing is kept across a restart', do=['home', 'restart'],
         expect=[r'log \bfm: frequency offset \+2000 Hz', status_ends('offset +2000 Hz')]),
    Step('3.2', 'In the preferences record', expect=[settings_has(r'frequency offset \+2000 Hz(,|$)', 'preferences')]),
    Step('3.3', 'The menu row', do=['goto ' + ROW], expect=[r'row ^Freq offset \+2000 Hz$']),
    Step('3.4', 'The counter as in 2.10', transmit=True, do=['home'] + ptt(), expect=[r'log \bfm: transmitting'],
         human=counter('as in 2.10.')),
    Step('4.1a', 'fm offset -2000', changes=True, do=['home', 'cmd fm offset -2000'], expect=[
        r'log \bfm: frequency offset -2000 Hz']),
    Step('4.1b', '2000 Hz below the radio\'s own error', transmit=True, do=ptt(), expect=[r'log \bfm: transmitting'],
         human=counter('2000 Hz below the reading of 1.2.')),
    Step('4.2', 'The nearest 62.5 Hz step', do=['cmd fm offset 1950'], expect=[
        r'log \bfm: frequency offset \+1937\.5 Hz']),
    Step('4.3', 'Rounding near 0', do=['cmd fm offset 62', 'cmd fm offset -31', 'cmd fm offset -32'], expect=[
        r'log \bfm: frequency offset \+62\.5 Hz', r'log \bfm: frequency offset 0 Hz',
        r'log \bfm: frequency offset -62\.5 Hz']),
    Step('4.4', 'Out of range and bad numbers refused', do=['cmd fm offset 6000', 'cmd fm offset 12x', 'wait 0.5'],
         expect=[lambda ctx: (len(ctx.radio.find(r'unknown command, try help', ctx.since)) == 2,
                              '%d refusals' % len(ctx.radio.find(r'unknown command', ctx.since))),
                 status_ends('offset -62.5 Hz')]),
    Step('4.5', 'The console\'s value in the menu', do=['goto ' + ROW], expect=[r'row ^Freq offset -62\.5 Hz$']),
    Step('5.1', 'Corrected: the channel\'s frequency', transmit=True, changes=True,
         do=['home', 'cmd fm offset {correction}'] + ptt(), expect=[r'log \bfm: transmitting'],
         human=counter('the channel\'s frequency, within 62.5 Hz.')),
    Step('5.2', 'A carrier on a known exact frequency', radio_only='needs a signal generator',
         human='Receive a carrier on a known exact frequency on FM; `fm status` while it is on shows the nominal '
               'frequency and the offset; the carrier is heard clearly.'),
    Step('5.3', 'Retuned at once', radio_only='needs a signal generator', do=[
        'cmd fm offset 0', 'wait 2', 'cmd fm offset {correction}'], expect=[r'log \bfm: frequency offset 0 Hz'],
         human='While the carrier is on: each change logged and the receiver tuned again at once.'),
    Step('6.1', 'DMR without the offset', transmit=True, do=['cmd fm offset 0', channel('{dmr}')],
         human='Call the talkgroup with PTT and listen: note whether calls decode and the repeater answers.'),
    Step('6.2', 'DMR with the correction', transmit=True, do=['cmd fm offset {correction}', channel('{dmr}')],
         human='Call again: calls decode and the repeater answers.'),
]

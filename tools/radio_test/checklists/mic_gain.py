# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/mic-gain.md: the microphone gain in dB, from the menu and
the console, kept in the preferences record.

The call steps (14-19) transmit and need a person to listen: they run only
with --allow-transmit, and ask the operator. Pass the parrot's ID and the
channels as parameters:

  --param parrot=<ID> --param dmr="Zone/Name" --param fm="Zone/Name"
"""
import re

from _helpers import channel, remember_log, settings_has
from radiotest.radio import Failed
from radiotest.runner import Step

TITLE = 'Microphone gain'
SOURCE = 'docs/hw-tests/mic-gain.md (feat/mic-gain-db)'

PARAMS = {
    'dmr': 'UHF/DMR repeater',
    'fm': 'UHF/Calling 70cm',
    'other': 'VHF/Calling 2m',
    'parrot': '',
}

GAIN = r'\bui: microphone gain: DMR \+(\d+) dB( \(codeplug\))?, FM \+(\d+) dB( \(codeplug\))?'


def keep_gains(ctx):
    line = ctx.radio.ask('mic gain', GAIN)
    m = re.search(GAIN, line.text)
    ctx.params['dmr_gain'] = 'codeplug' if m.group(2) else m.group(1)
    ctx.params['fm_gain'] = 'codeplug' if m.group(4) else m.group(3)
    ctx.evidence('kept: `%s`' % line.text)


def put_gains(ctx):
    if 'dmr_gain' in ctx.params:
        ctx.radio.cmd('mic gain dmr %s' % ctx.params['dmr_gain'])
        ctx.radio.cmd('mic gain fm %s' % ctx.params['fm_gain'])


def gain_is(pattern):
    """Expectation: `mic gain` answers a line matching @p pattern."""
    def expectation(ctx):
        try:
            line = ctx.radio.ask('mic gain', r'\bui: microphone gain: ' + pattern)
        except Failed as e:
            return False, str(e)
        return True, '`%s`' % line.text
    return expectation


def fm_register(value):
    """Expectation: the FM transceiver's voice gain register reads @p value
    on the FM channel, selected again so the gain is applied."""
    def expectation(ctx):
        channel('{other}')(ctx)
        channel('{fm}')(ctx)
        line = ctx.radio.ask('fm 41', r'register 41 = ')
        return value in line.text, '`%s` (want %s)' % (line.text, value)
    return expectation


def edit_yaml(source, target, values):
    """Action: copies the codeplug YAML @p source to @p target with the
    keys of @p values set."""
    def action(ctx):
        with open(ctx.path(source)) as f:
            text = f.read()
        for key, value in values.items():
            text, n = re.subn(r'^(\s*%s:\s*).*$' % re.escape(key), r'\g<1>%s' % value, text, count=1, flags=re.M)
            if n != 1:
                raise Failed('no %s in %s' % (key, source))
        with open(ctx.path(target), 'w') as f:
            f.write(text)
        ctx.evidence('%s: %s' % (target, ', '.join('%s: %s' % kv for kv in values.items())))
    action.__name__ = 'edit %s' % source
    return action


def yaml_has(file, values):
    def expectation(ctx):
        got = {k: ctx.radio.yaml_value(ctx.path(file), k) for k in values}
        return all(str(values[k]) == got[k] for k in values), '%s: %s (want %s)' % (file, got, values)
    return expectation


def call(kind, gain, human=None, loud=False):
    """A transmit step: one call with the microphone gain at @p gain."""
    speak = 'the sentence spoken loudly' if loud else 'the test sentence'
    do = ['cmd mic gain %s %d' % (kind, gain), 'cmd ptt 4000', 'wait 6']
    return do, human or 'Speak %s during the 4 s call (mic gain %s +%d dB).' % (speak, kind.upper(), gain)


BEFORE = [keep_gains]
UNDO = [put_gains]

STEPS = [
    Step('0', 'The codeplug as the original', radio_only='no QDMR port', do=['qdmr read mic-a.yaml'], expect=[
        lambda ctx: (True, 'micGain %s, fmMicGain %s' % (ctx.radio.yaml_value(ctx.path('mic-a.yaml'), 'micGain'),
                                                         ctx.radio.yaml_value(ctx.path('mic-a.yaml'), 'fmMicGain')))]),
    Step(1, 'The codeplug\'s levels as dB', do=['restart'], expect=[
        r'log \bradio: microphone gain: DMR \+\d+ dB, FM \+\d+ dB',
        gain_is(r'DMR \+\d+ dB \(codeplug\), FM \+\d+ dB \(codeplug\)')]),
    Step(2, 'Out of range: refused', do=[
        remember_log('before_2', r'\bui: microphone gain: (.*)$', 'mic gain'),
        'mark', 'cmd mic gain dmr 31', 'cmd mic gain fm 25', 'wait 0.5'], expect=[
        lambda ctx: (len(ctx.radio.find(r'unknown command', ctx.since)) == 2,
                     '%d `unknown command`' % len(ctx.radio.find(r'unknown command', ctx.since))),
        lambda ctx: gain_is(re.escape(ctx.params['before_2']) + '$')(ctx)]),
    Step(3, 'DMR +30, FM +24: kept in the preferences record', changes=True, timeout=8, do=[
        'cmd mic gain dmr 30', 'cmd mic gain fm 24', 'wait 5'], expect=[
        gain_is(r'DMR \+30 dB, FM \+24 dB$'), r'log \bsettings kept in the codeplug',
        settings_has(r'mic gain DMR \+30 dB, FM \+24 dB', 'preferences')]),
    Step(4, 'FM voice gain at +24 dB', radio_only='no FM transceiver registers', expect=[fm_register('0726')]),
    Step('5a', 'FM +8 dB', changes=True, do=['cmd mic gain fm 8'], expect=[gain_is(r'DMR \+30 dB, FM \+8 dB$')]),
    Step('5b', 'FM voice gain at +8 dB', radio_only='no FM transceiver registers', expect=[fm_register('0706')]),
    Step(6, 'Settings > Audio shows the dB', do=['goto Settings/Audio/DMR mic gain'], expect=[
        'title Audio$', r'screen ^DMR mic gain\n\+30 dB\nFM mic gain\n\+8 dB$']),
    Step(7, 'DMR mic gain\'s value screen', do=['at Settings/Audio/DMR mic gain', 'key Menu'], expect=[
        'kind ^spinner$', r'value ^\+30 dB$', r'in_use ^\+30 dB$']),
    Step(8, 'Kept across a restart', radio_only='nothing is kept across a restart', do=['home', 'restart'], expect=[
        gain_is(r'DMR \+30 dB, FM \+8 dB$')]),
    Step(9, 'A codeplug write keeps the values set on the radio', radio_only='no QDMR port', timeout=10, do=[
        edit_yaml('mic-a.yaml', 'mic-a2.yaml', {'micGain': 4, 'fmMicGain': 8}), 'qdmr write mic-a2.yaml'], expect=[
        gain_is(r'DMR \+30 dB, FM \+8 dB$')], undo=['qdmr write mic-a.yaml']),
    Step(10, 'Back to the codeplug\'s levels', do=['home', 'cmd mic gain dmr codeplug', 'cmd mic gain fm codeplug'],
         expect=[gain_is(r'DMR \+\d+ dB \(codeplug\), FM \+\d+ dB \(codeplug\)$')]),
    Step('10b', 'The levels written in step 9', radio_only='no QDMR port', expect=[
        gain_is(r'DMR \+12 dB \(codeplug\), FM \+20 dB \(codeplug\)$')]),
    Step(11, 'QDMR reads the same levels', radio_only='no QDMR port', do=['qdmr read mic-b.yaml'], expect=[
        yaml_has('mic-b.yaml', {'micGain': 3, 'fmMicGain': 7})]),
    Step(12, 'The menu shows Plug +N dB', do=['goto Settings/Audio/DMR mic gain'], expect=[
        r'screen ^DMR mic gain\nPlug \+\d+ dB\nFM mic gain\nPlug \+\d+ dB$']),
    Step(13, 'FM +16 dB: the standard voice gain', radio_only='no FM transceiver registers', changes=True, do=[
        'home', 'cmd mic gain fm 16'], expect=[fm_register('070f')]),
]

# The calls: each transmits, and a person listens.
_calls = [
    ('14', 'DMR +8 dB: the parrot plays it back', 'dmr', [8], 'The parrot plays the call back.'),
    ('15', 'Listen: DMR +16 and +24 dB', 'dmr', [16, 24], 'Each louder than the one before; no distortion at 24.'),
    ('16', 'Listen: DMR +30 dB, spoken loudly', 'dmr', [30], 'Not clipped; the room between words louder.'),
    ('17', 'Listen on the second radio: FM +8, +16, +24 dB', 'fm', [8, 16, 24],
     'Each louder than the one before, the steps about as large as on DMR.'),
    ('18', 'Listen: FM +24 dB, spoken loudly', 'fm', [24], 'Clean, or distorted (over-deviation): note which.'),
]
for _id, _title, _kind, _gains, _listen in _calls:
    _do = [channel('{%s}' % _kind)]
    if _kind == 'dmr':
        _do.append('cmd dmr pc {parrot}')
    for _g in _gains:
        _do += call(_kind, _g)[0]
    STEPS.append(Step(_id, _title, transmit=True, changes=True, timeout=10, do=_do,
                      expect=[r'log \b(dmr|fm): transmitting'],
                      human='Speak the test sentence%s during each 4 s call. %s' % (
                          ' loudly' if 'loudly' in _title else '', _listen)))
STEPS.append(Step('19', 'Listen: FM through the baseband, +8, +16, +24 dB', transmit=True, changes=True, timeout=10,
                  do=[channel('{fm}'), 'cmd fm txaudio bb'] + call('fm', 8)[0] + call('fm', 16)[0] +
                  call('fm', 24)[0] + ['cmd fm txaudio direct'],
                  expect=[r'log \bfm: transmitting'], undo=['cmd fm txaudio direct'],
                  human='The same steps as in 17, the voice through the baseband.'))

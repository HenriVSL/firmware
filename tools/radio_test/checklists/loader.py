# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/loader.md: updates into slot B with a trial, a
confirmation and a fallback, from the PC.

Radio only. Uses the dmm-radio of the loader branch: set DMM_RADIO to its
path. The test images are parameters:

  --param good_a=<image> --param good_b=<image> --param fault=<image>
  --param hang=<image> --param no_confirm=<image>

The first installation (steps 1-5) and the knob and battery steps need the
owner and are listed for the human.
"""
from radiotest.runner import Step

TITLE = 'DeltaMM\'s loader'
SOURCE = 'docs/hw-tests/loader.md (feat/loader)'

PARAMS = {'good_a': '', 'good_b': '', 'fault': '', 'hang': '', 'no_confirm': ''}
RADIO = 'needs the radio and its loader'


def boot(timeout=180):
    """Action: waits for the next start line."""
    def action(ctx):
        ctx.evidence('started: `%s`' % ctx.radio.wait_boot(timeout, ctx.since).text)
    action.__name__ = 'wait for a start'
    return action


STEPS = [
    Step('1-5', 'First installation', radio_only=RADIO,
         human='The owner: prepare --loader, flash in update mode (PTT+PF3), check the log and `dmm-radio info`, '
               'and that update mode still works.'),
    Step(6, 'Restart into the loader', radio_only=RADIO, do=['dmm-radio loader'], expect=[
        r'answer Restarting into the loader', r'answer Waiting: asked for']),
    Step(7, 'The loader starts the app by itself after 60 s', radio_only=RADIO, timeout=10, do=[boot(120)],
         expect=[r'log \blast reset: software']),
    Step(8, 'Update B on trial', radio_only=RADIO, timeout=10, do=['mark', 'dmm-radio update {good_b} --try', boot()],
         expect=[r'answer committed', r'answer on trial \(confirm with dmm-radio confirm\)']),
    Step(9, 'Confirm, restart: still B, confirmed', radio_only=RADIO, timeout=10, do=[
        'dmm-radio confirm', 'restart', 'dmm-radio info'], expect=[r'answer image confirmed']),
    Step(10, 'Rollback to the first version', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio rollback', boot(), 'dmm-radio info'], expect=[r'answer confirmed']),
    Step(11, 'Rollback again: B', radio_only=RADIO, timeout=10, do=['mark', 'dmm-radio rollback', boot(),
                                                                   'dmm-radio info'],
         expect=[r'answer confirmed']),
    Step(12, 'Automatic update A confirms itself after 60 s', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio update {good_a}', boot()], expect=[r'log@90 \bloader: image confirmed after 60 s']),
    Step(13, 'A crashing image falls back', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio update {fault} --try', 'wait 120'], expect=[
        r'log \bcrash: test fault', r'log@120 \bloader: .*fallback|Fallback']),
    Step(14, 'A hanging image falls back', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio update {hang} --try', boot(240)], expect=[r'log \bFallback|fallback']),
    Step(15, 'A trial never confirmed falls back after three restarts', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio update {no_confirm} --try', boot(), 'restart', 'restart', 'restart', 'dmm-radio info'],
         expect=[r'answer previous image|failed']),
    Step(16, 'Refusals', radio_only=RADIO, do=['dmm-radio update {good_b} --test-refusals'], expect=[
        r'answer Bad chunk: refused \(good\)', r'answer Wrong CRC: commit refused \(good\)',
        r'answer Restarted without a commit']),
    Step(17, 'An update cut short resumes', radio_only=RADIO,
         human='Run `dmm-radio update <good B>`, kill it by its PID during the percentage, run it again: '
               '`N already there; committed` and B runs.'),
    Step(18, 'A swap cut short resumes', radio_only=RADIO, timeout=10, do=[
        'mark', 'dmm-radio update {good_a} --swap-cut 20', boot(240)], expect=[
        r'answer Test: the loader restarts after 20 swap steps']),
    Step(19, 'The knob off during a swap', radio_only=RADIO,
         human='During the swap of an update, turn the knob off: red stays on until the swap ends, then off; '
               'switched on, the new image runs on trial.'),
    Step(20, 'Twenty updates in a row', radio_only=RADIO,
         human='Run 20 updates alternating two good images, 70 s apart (a shell loop); all succeed.'),
    Step(21, 'Force key at power-on', radio_only=RADIO,
         human='Radio off; hold PTT and PF1, switch on: loader mode, `dmm-radio info` shows `Waiting: force key '
               'held at power-on`; after 60 s the app starts.'),
    Step(22, 'Stacks', radio_only=RADIO, timeout=75, expect=['stack ui >50']),
]

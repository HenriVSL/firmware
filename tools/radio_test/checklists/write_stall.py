# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/write-stall.md: the watchdog stall record, and a codeplug
write that keeps the task feeding the watchdog running.

Radio only: the simulator has no watchdog and no QDMR port. Pass the
verified backup folder as --param backup=<folder> for step 6.
"""
import re

from radiotest.runner import Step

TITLE = 'Write stall: watchdog stall record and paced block copies'
SOURCE = 'docs/hw-tests/write-stall.md (feat/write-stall)'

PARAMS = {'backup': ''}
RADIO = 'needs the radio'
STAMP = re.compile(r'^\[(\d+)\.(\d{3})\]')


def uptime_steady(ctx):
    """The uptime lines from the write's start to its restart: one every
    10 s, each within 50 ms of the first one's fraction of a second."""
    lines = ctx.radio.lines(ctx.since)
    end = next((i for i, l in enumerate(lines) if 'codeplug written from the PC' in l.text), len(lines))
    stamps = []
    for line in lines[:end]:
        m = STAMP.match(line.text)
        if m and 'system: uptime' in line.text:
            stamps.append(int(m.group(1)) * 1000 + int(m.group(2)))
    if len(stamps) < 2:
        return False, '%d uptime lines during the write' % len(stamps)
    gaps = [b - a for a, b in zip(stamps, stamps[1:])]
    late = [s % 1000 - stamps[0] % 1000 for s in stamps]
    ok = all(abs(g - 10000) <= 50 for g in gaps) and all(abs(d) <= 50 for d in late)
    return ok, '%d uptime lines, gaps %s ms, late by %s ms' % (len(stamps), gaps, late)


STEPS = [
    Step(1, 'hang: a stall record, then a restart', radio_only=RADIO, timeout=10, do=[
        lambda ctx: ctx.radio.restart(30, command='hang')], expect=[
        r'log \bwatchdog no longer fed', r'log \blast reset: software',
        r'log \bcrash: watchdog stall in task \S+, detail 1500']),
    Step(2, 'dmrconf read', radio_only=RADIO, do=['qdmr read stall-a.yaml']),
    Step(3, 'dmrconf write: no timeout, a restart without a crash', radio_only=RADIO, timeout=10, do=[
        'qdmr write stall-a.yaml'], expect=[
        r'log \bprogramming: codeplug written from the PC: restarting', r'log \blast reset: software',
        r'nolog \bcrash:', r'answer ^(?![\s\S]*Timeout)']),
    Step(4, 'Uptime lines steady through the write', radio_only=RADIO, expect=[uptime_steady]),
    Step(5, 'dmrconf read back', radio_only=RADIO, do=[
        'qdmr read stall-b.yaml', 'qdmr diff stall-a.yaml stall-b.yaml'], expect=['nodiff']),
    Step(6, 'The backup still matches', radio_only=RADIO, timeout=600, do=['dmm-radio check-backup {backup}'],
         expect=[r'answer (?i)match']),
]

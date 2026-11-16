# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""The test tools themselves: screen text and pictures, notices through a
picture, keys, the menu by name, and the transmit guard. Runs on the
simulator and on the radio; nothing is kept and nothing transmits."""
import re
import time

from _helpers import standby
from radiotest.radio import SafetyError, Skipped
from radiotest.runner import Step

TITLE = 'Test tools: screen, keys, menu by name, guard'
SOURCE = 'tools/radio_test (feat/test-tools)'

UNDO = ['cmd unlock', 'home']


def refused(command):
    """Expectation: the guard refuses @p command before it is sent."""
    def expectation(ctx):
        before = ctx.radio.mark()
        try:
            ctx.radio.cmd(command)
        except SafetyError as e:
            sent = ctx.radio.find(r'console: > ', before)
            return not sent, 'refused: %s' % e
        return False, '`%s` was sent' % command
    return expectation


def notice_kept(ctx):
    """The key lock's notice is in a picture taken while it is up, is
    still up after it, and goes after its 1.5 s as before."""
    r = ctx.radio
    at = r.cmd('lock')
    r.wait(r'\bui: key lock on\b', 3, at)
    started = time.monotonic()
    ctx.picture('notice')
    texts = [run.text for run in r.screen_text()]
    shown = 'Keys locked' in texts
    elapsed = time.monotonic() - started
    time.sleep(max(0.0, 2.5 - elapsed))
    gone = 'Keys locked' not in [run.text for run in r.screen_text()]
    r.cmd('unlock')
    return shown and gone, 'notice in the picture and the text after it: %s (%.1f s); gone after 2.5 s: %s' % (
        shown, elapsed, gone)


def grouped_menu(ctx):
    """Skips the step on a menu without a title band (an older menu)."""
    r = ctx.radio
    r.home()
    r.key('Menu')
    w = r.where(fresh=True)
    if w.title != 'Menu':
        r.home()
        raise Skipped('this menu has no title band')


def remember_screen(ctx):
    ctx.params['screen'] = ctx.radio.where(fresh=True).text()


def same_screen(ctx):
    text = ctx.radio.where(fresh=True).text()
    # The clock may have turned meanwhile.
    strip = lambda t: re.sub(r'\d\d:\d\d', '', t)  # noqa: E731
    return strip(text) == strip(ctx.params['screen']), 'the same screen as before the knob'


STEPS = [
    Step(1, 'Standby screen read from screen text', do=['home'], expect=[standby, r'screen ^\d\d:\d\d$']),
    Step(2, 'A picture of the screen', do=['shot standby']),
    Step(3, 'A notice survives a picture and keeps its time', expect=[notice_kept]),
    Step(4, 'The menu by name', do=[grouped_menu, 'goto Settings/Clock/Time zone'], expect=[
        'title ^Clock$|> Clock$', 'row ^Time zone']),
    Step(5, 'The knob on the standby screen, one on and one back', do=[
        'home', remember_screen, 'knob 1', 'knob -1'], expect=[standby, same_screen]),
    Step(6, 'The guard refuses transmitting commands', expect=[
        refused('ptt 1000'), refused('key ptt'), refused('dmr pc 9'), refused('aprs send'), refused('fm tx 145500'),
        refused('fm power high')]),
]

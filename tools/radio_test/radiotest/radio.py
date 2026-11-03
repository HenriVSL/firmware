# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""The test primitives on a link: console commands, waits for log lines,
keys and the knob, restarts, the screen's text and pictures, the menu,
and the PC tools (dmrconf, dmm-radio)."""
import difflib
import os
import re
import subprocess
import sys
import time

from . import transport
from .screen import Where, parse_runs

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tools', 'screen_picture'))
import screen_picture  # noqa: E402

BOOT = re.compile(r'\bsystem: DeltaMM (\S+) on (\S+)')
READY = re.compile(r'\bradio: ready\b')
ANSI = re.compile(r'\x1b\[[0-9;]*[A-Za-z]')
# QDMR with the round-trip fixes; $DMRCONF names another build.
DMRCONF = os.environ.get('DMRCONF') or os.path.expanduser(
    '~/Documents/AnytoneSoftware/qdmr-release/build/cli/dmrconf')
# The older build, whose write and read back change two more fields.
OLD_QDMR = 'build-0.15.1'

# Console commands that key the transmitter or change its power.
TRANSMIT_COMMANDS = [
    re.compile(p) for p in (
        r'^ptt\b', r'^key\s+ptt\b', r'^aprs\s+(send|beacon)\b', r'^dmr\s+pc\b', r'^fm\s+tx\s+(?!off\b)',
        r'^fm\s+power\b', r'^bbtone\b', r'^beacon\b')
]
# Log lines that show the transmitter keyed or its power changed.
TRANSMIT_LINES = re.compile(
    r'\b(dmr|fm): transmitting\b|\bfm: transmit (on|power)\b|\bui: power \d|\baprs: (sending|beacon sent)')
# Log lines that show a setting changed.
SETTING_LINES = re.compile(r'\bsettings: pending: settings|\bsettings kept in the codeplug')


def clean_output(text):
    """A tool's output without its progress bars and colours."""
    lines = []
    for line in ANSI.sub('', text.replace('\r', '\n')).splitlines():
        if line.strip() and not re.match(r'^\s*\[[= ]*\]\s*\d+%\s*$', line):
            lines.append(line.rstrip())
    return '\n'.join(lines)


class Failed(Exception):
    """An expectation not met; the step fails."""


class Skipped(Exception):
    """The step cannot run here; it is reported as skipped."""


class SafetyError(Exception):
    """Something the guard refuses; the run stops."""


class Guard:
    """Refuses transmitting and power changes unless both the run
    (--allow-transmit) and the step (transmit) allow them."""

    def __init__(self, allow_transmit):
        self.allow_run = allow_transmit
        self.step_allows = False

    @property
    def armed(self):
        return not (self.allow_run and self.step_allows)

    def check_command(self, text):
        if not self.armed:
            return
        t = text.strip().lower()
        for p in TRANSMIT_COMMANDS:
            if p.search(t):
                raise SafetyError('refused `%s`: transmits or changes power; needs a step marked transmit and '
                                  '--allow-transmit' % text)


class Radio:
    def __init__(self, link, guard, settle=None):
        self.link = link
        self.guard = guard
        self.sim = isinstance(link, transport.SimLink)
        # Time for the ui to act on a key before the screen is read.
        self.settle = settle if settle is not None else (0.12 if self.sim else 0.3)
        self.version = None
        self.board = None
        self._where = None
        self.violations = []
        link.watchers.append(self._watch)

    # Log lines ---------------------------------------------------------

    def _watch(self, line):
        if self.guard.armed and TRANSMIT_LINES.search(line.text):
            self.violations.append(line)
        m = BOOT.search(line.text)
        if m:
            self.version, self.board = m.group(1), m.group(2)

    def mark(self):
        return self.link.mark()

    def note(self, text):
        self.link.note(text)

    def lines(self, since):
        return [line for line in self.link.since(since) if not line.text.startswith('[runner]')]

    def wait(self, pattern, timeout=5.0, since=None):
        """The first log line matching @p pattern from @p since (default:
        now); raises Failed after @p timeout seconds."""
        rx = re.compile(pattern) if isinstance(pattern, str) else pattern
        start = self.mark() if since is None else since
        line = self.link.wait_for(lambda l: not l.text.startswith('[runner]') and rx.search(l.text), start, timeout)
        if line is None:
            raise Failed('no log line /%s/ within %g s' % (rx.pattern, timeout))
        return line

    def find(self, pattern, since):
        rx = re.compile(pattern) if isinstance(pattern, str) else pattern
        return [line for line in self.lines(since) if rx.search(line.text)]

    # Console -------------------------------------------------------------

    def cmd(self, text):
        """Sends a console command; returns the mark from before it."""
        self.guard.check_command(text)
        self._where = None
        at = self.mark()
        self.link.send(text)
        return at

    def ask(self, text, pattern, timeout=3.0):
        """Sends @p text and returns the first answer matching @p pattern."""
        return self.wait(pattern, timeout, self.cmd(text))

    def collect(self, text, quiet=0.3, timeout=3.0):
        """Sends @p text and returns its answer: the lines until none came
        for @p quiet seconds."""
        at = self.cmd(text)
        self.wait(r'console: > ' + re.escape(text), timeout, at)
        end = time.monotonic() + timeout
        count = 0
        while time.monotonic() < end:
            time.sleep(quiet)
            now = len(self.lines(at))
            if now == count:
                break
            count = now
        return self.lines(at)

    def key(self, name, long=False):
        if name.lower() == 'ptt':
            self.guard.check_command('key ptt')
        if name.lower() == 'menu' and self.guard.armed:
            self._check_power_menu()
        self.cmd('key %s%s' % (name, ' long' if long else ''))
        time.sleep(self.settle)

    def keys(self, *names):
        for n in names:
            self.key(n)

    def knob(self, steps):
        if steps:
            self.cmd('knob %d' % steps)
            time.sleep(self.settle)

    def restart(self, timeout=60.0, command='restart'):
        """Restarts the radio and waits for its start line; the log port
        is opened again when it comes back."""
        return self.wait_boot(timeout, self.cmd(command))

    def wait_boot(self, timeout=90.0, since=None):
        """Waits for a start line, then for the radio's chips to be up:
        until then keys are refused and the start screen is shown."""
        line = self.wait(BOOT, timeout, since)
        try:
            # The simulator logs it before the start line.
            self.wait(READY, 20, line.seq if since is None else since)
        except Failed:
            self.note('no `radio: ready` after the start')
        time.sleep(1.0)
        self._where = None
        return line

    # Screen --------------------------------------------------------------

    def screen_text(self, timeout=5.0):
        at = self.cmd('screen text')
        self.wait(r'\bscr: text end\b', timeout, at)
        lines = self.lines(at)
        start = max(i for i, l in enumerate(lines) if re.search(r'\bscr: text$', l.text))
        return parse_runs(lines[start:])

    def where(self, fresh=False):
        if self._where is None or fresh:
            self._where = Where(self.screen_text())
        return self._where

    def shot(self, path, timeout=20.0):
        """Saves a picture of the screen as PNG at @p path."""
        at = self.cmd('screen shot')
        self.wait(r'\bscr: stop\b', timeout, at)
        found = list(screen_picture.streams(l.text for l in self.lines(at)))
        if not found:
            raise Failed('no picture in the log')
        result = screen_picture.decode(found[-1])
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        result.screen.png(path, 3)
        return path

    def watch(self, seconds, passes_dir=None, actions=None):
        """Streams the screen for @p seconds, running @p actions (a
        callable) meanwhile; returns the decoded stream (passes, lost,
        flickered pixel count)."""
        at = self.cmd('screen watch %d' % seconds)
        self.wait(r'\bscr: start\b', 5, at)
        if actions:
            actions()
        self.wait(r'\bscr: stop\b', seconds + 30, at)
        found = list(screen_picture.streams(l.text for l in self.lines(at)))
        if not found:
            raise Failed('no stream in the log')
        return screen_picture.decode(found[-1], passes_dir)

    # Menu ------------------------------------------------------------------

    def _check_power_menu(self):
        # Opening the power's value screen is harmless; Menu there sets it.
        w = self.where()
        if w.kind == 'buttons' and re.search(r'(^|> )Power$', w.title):
            raise SafetyError('refused Menu on %s: sets the transmit power; needs a step marked transmit and '
                              '--allow-transmit' % w.summary())

    @staticmethod
    def on_standby(w):
        return w.kind == 'other' and not w.title and re.match(r'Menu\s+P1\b', w.hint) is not None

    @staticmethod
    def on_log_or_key_test(w):
        """The key test, or the log page (lines of the small font from the
        left edge), which held Exit leaves."""
        if any(r.text.startswith('Keys (Exit=log)') for r in w.runs):
            return True
        return not w.title and sum(1 for r in w.runs if r.x == 0 and r.height == 8) >= 5

    def home(self, tries=10):
        """Back to the standby screen: Exit held on the log and the key
        test (log, key test, standby), Exit on any other screen (menu,
        zone list, GPS screen, activity pages); once in a while held."""
        seen = []
        for i in range(tries):
            w = self.where(fresh=True)
            if self.on_standby(w):
                return w
            seen.append(w.summary()[:80])
            self.key('Exit', long=self.on_log_or_key_test(w) or i % 4 == 3)
        w = self.where(fresh=True)
        if not self.on_standby(w):
            raise Failed('not back on the standby screen after %d keys; screens on the way: %s' % (
                tries, ' / '.join(seen[-4:])))
        return w

    def choose(self, label, tries=None):
        """Moves the cursor (Down) onto the row or button named @p label
        in the list on the screen. A trailing '*' in @p label matches the
        start of a row's name."""
        w = self.where(fresh=True)
        if w.kind not in ('list', 'buttons'):
            raise Failed('no list on the screen to choose %r from: %s' % (label, w.summary()))
        count = int(w.place.split('/')[1]) if w.place else max(len(w.rows), 1)

        def matches(text):
            text = (text or '').strip()
            return text.startswith(label[:-1]) if label.endswith('*') else text == label

        for _ in range((tries or count) + 1):
            if matches(w.chosen):
                return w
            before = w.chosen
            self.key('Down')
            w = self.where(fresh=True)
            if not w.chosen or w.chosen == before:
                # Read before the cursor was drawn again: read once more.
                time.sleep(self.settle)
                w = self.where(fresh=True)
        if matches(w.chosen):
            return w
        time.sleep(self.settle)
        w = self.where(fresh=True)
        if matches(w.chosen):
            return w
        raise Failed('no row %r in %s' % (label, w.summary()))

    def goto(self, path, open_last=False):
        """From anywhere: back to standby, Menu, then down the groups named
        in @p path ("Settings/Home screen/Preset"), choosing each row and
        opening all but the last (the last too with @p open_last). Does
        not depend on the rows the menu remembers."""
        parts = [p for p in path.split('/') if p]
        self.home()
        self.key('Menu')
        w = self.where(fresh=True)
        if w.title != 'Menu':
            raise Failed('Menu did not open the menu: ' + w.summary())
        for i, part in enumerate(parts):
            self.choose(part)
            if i < len(parts) - 1 or open_last:
                before = self.where()
                self.key('Menu')
                w = self.where(fresh=True)
                if w == before:
                    raise Failed('Menu on %r did nothing: %s' % (part, w.summary()))
        return self.where()

    def at(self, path):
        """goto(path) unless the screen is already the list of the group
        that holds the last row of @p path, with that row chosen."""
        parts = [p for p in path.split('/') if p]
        w = self.where(fresh=True)
        group = parts[-2] if len(parts) > 1 else 'Menu'
        if w.kind == 'list' and w.chosen == parts[-1] and (w.title == group or w.title.endswith('> ' + group)):
            return w
        return self.goto(path)

    def set_value(self, label):
        """On a value screen, chooses @p label and sets it with Menu."""
        w = self.where(fresh=True)
        if w.kind == 'spinner':
            raise Failed('set_value on a spinner: use Up/Down')
        self.choose(label)
        self.key('Menu')
        return self.where(fresh=True)

    # Reports from the radio -------------------------------------------------

    def settings(self):
        """The `settings` answer: {'pending': line, 'codeplug': line,
        'preferences': line} and 'lines'."""
        lines = self.collect('settings')
        out = {'lines': [l.text for l in lines]}
        for l in lines:
            t = l.text
            if 'settings: ' in t and ('pending' in t or 'nothing pending' in t):
                out['pending'] = t
            elif 'codeplug: ' in t:
                out['codeplug'] = t
            elif 'preferences record' in t:
                out['preferences'] = t
        return out

    def aprs_rx(self):
        return [l.text for l in self.collect('aprs rx')]

    def stack_words(self, task='ui', timeout=75.0, since=None):
        """The unused stack words of @p task from the next (or a recent)
        uptime line; it comes each minute."""
        line = self.wait(r'\bunused stack words\b.*\b%s\s+\d+' % re.escape(task), timeout,
                         since if since is not None else self.mark())
        m = re.search(r'\b%s\s+(\d+)' % re.escape(task), line.text.split('unused stack words', 1)[1])
        return int(m.group(1)), line

    # PC tools ------------------------------------------------------------------

    def dmrconf(self, *args, timeout=300):
        if self.sim:
            raise Failed('no QDMR port on the simulator')
        dev = transport.data_device()
        cmd = [DMRCONF] + [a.replace('{dev}', dev) for a in args]
        self.note('$ ' + ' '.join(cmd))
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        out = clean_output(p.stdout + p.stderr)
        if p.returncode != 0:
            raise Failed('dmrconf %s failed (%d): %s' % (args[0], p.returncode, out[-500:]))
        return out

    def qdmr_read(self, path):
        return self.dmrconf('read', '-D', '{dev}', '-R', 'd878uv2', path)

    def qdmr_write(self, path, wait_restart=True):
        at = self.mark()
        out = self.dmrconf('write', '-D', '{dev}', '-R', 'd878uv2', path)
        if wait_restart:
            self.wait_boot(90, at)
        return out

    @staticmethod
    def yaml_diff(a, b):
        """Differences between two codeplug YAML files, as unified diff
        lines (empty when they match)."""
        with open(a) as fa, open(b) as fb:
            return list(difflib.unified_diff(fa.read().splitlines(), fb.read().splitlines(), a, b, lineterm='', n=1))

    @staticmethod
    def qdmr_quirks(diff, old=None):
        """Splits the changed lines of @p diff into those a QDMR write and
        read back is known to give and the others: backlightDurationRX
        from a number of seconds to infinite; with the older build (@p
        old, by default when DMRCONF is it) also maxSpeakerVolume one lower
        and an APRS `period: <n> s` added."""
        if old is None:
            old = OLD_QDMR in DMRCONF
        removed = [l[1:] for l in diff if l.startswith('-') and not l.startswith('---')]
        added = [l[1:] for l in diff if l.startswith('+') and not l.startswith('+++')]
        quirks, others = [], []
        volume = re.compile(r'^(\s*maxSpeakerVolume:\s*)(\d+)\s*$')
        backlight = re.compile(r'^(\s*backlightDurationRX:\s*)(.*?)\s*$')
        for line in added:
            b = backlight.match(line)
            was = next((r for r in removed if b and backlight.match(r) and
                        backlight.match(r).group(1) == b.group(1)), None)
            if b and was is not None and b.group(2) == 'infinite' and re.fullmatch(r'\d+ s',
                                                                                 backlight.match(was).group(2)):
                removed.remove(was)
                quirks.append('backlightDurationRX %s -> infinite' % backlight.match(was).group(2))
                continue
            if not old:
                others.append('+' + line)
                continue
            m = volume.match(line)
            before = next((r for r in removed if m and volume.match(r) and
                           volume.match(r).group(1) == m.group(1)), None)
            if m and before is not None and int(volume.match(before).group(2)) == int(m.group(2)) + 1:
                removed.remove(before)
                quirks.append('maxSpeakerVolume %s -> %s' % (volume.match(before).group(2), m.group(2)))
            elif re.match(r'^\s*period:\s*\d+ s\s*$', line):
                quirks.append('added `%s`' % line.strip())
            else:
                others.append('+' + line)
        others += ['-' + r for r in removed]
        return quirks, others

    @staticmethod
    def yaml_value(path, key):
        """The first `key: value` in a YAML file, as text."""
        with open(path) as f:
            for line in f:
                m = re.match(r'\s*%s:\s*(.*?)\s*$' % re.escape(key), line)
                if m:
                    return m.group(1)
        return None

    def dmm_radio(self, *args, timeout=900, binary=None):
        if self.sim:
            raise Failed('dmm-radio needs the radio')
        exe = binary or os.environ.get('DMM_RADIO') or os.path.join(REPO, 'build', 'host', 'tools', 'dmm_radio',
                                                                    'dmm-radio')
        cmd = [exe] + list(args)
        self.note('$ ' + ' '.join(cmd))
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        out = clean_output(p.stdout + p.stderr)
        if p.returncode != 0:
            raise Failed('dmm-radio %s failed (%d): %s' % (args[0], p.returncode, out[-500:]))
        return out

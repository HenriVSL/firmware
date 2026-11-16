# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""What the screen shows, from the console's "screen text": the runs of
characters the ui draws, with place and colours, and where that puts the
user in the menu."""
import re

TEXT = re.compile(r'\bscr: t (\d+) (\d+) (\d+) ([0-9a-f]{4}) ([0-9a-f]{4}) (.*)$')

# Colours of the ui's palette (RGB565) that tell the parts apart.
GROUND = 0x0000
INVERTED = 0xFFFF  # a chosen row or button
PANEL = 0x18E4     # title band, panel, buttons not chosen

# The menu's title band: under the status bar, in the body font.
TITLE_MAX_Y = 16
BODY_HEIGHT = 12
LABEL_X = 6
HINT_MIN_Y = 112


class Run:
    __slots__ = ('x', 'y', 'height', 'fg', 'bg', 'text')

    def __init__(self, x, y, height, fg, bg, text):
        self.x, self.y, self.height, self.fg, self.bg, self.text = x, y, height, fg, bg, text

    def __repr__(self):
        return 'Run(%d,%d,h%d,%04x/%04x,%r)' % (self.x, self.y, self.height, self.fg, self.bg, self.text)


def parse_runs(lines):
    runs = []
    for line in lines:
        m = TEXT.search(str(line))
        if m:
            x, y, h, fg, bg, text = m.groups()
            runs.append(Run(int(x), int(y), int(h), int(fg, 16), int(bg, 16), text))
    return runs


class Where:
    """The screen read as the menu's screens are laid out.

    kind: 'list' (a menu or choice list), 'buttons', 'spinner', or 'other'
    (standby and the screens that are not the menu's).
    """

    def __init__(self, runs):
        self.runs = runs
        self.title = ''
        self.place = ''
        self.kind = 'other'
        self.rows = []  # (label, value, chosen)
        self.chosen = None  # label of the chosen row or button
        self.value = None  # chosen row's value, or a spinner's value
        self.in_use = None  # a value screen's "In use:" or starred button
        self.hint = ''
        self._read()

    def _read(self):
        runs = sorted(self.runs, key=lambda r: (r.y, r.x))
        band = [r for r in runs if r.y <= TITLE_MAX_Y and r.bg == PANEL and r.height == BODY_HEIGHT]
        for r in runs:
            if r.y >= HINT_MIN_Y and r.height == 8 and r.bg == GROUND:
                self.hint = r.text
        if not band:
            return
        self.title = min(band, key=lambda r: r.x).text.strip()
        places = [r for r in band if re.fullmatch(r'\d+/\d+', r.text.strip())]
        if places:
            self.place = places[0].text.strip()
        body = [r for r in runs if r.y > TITLE_MAX_Y + 4 and r.y < HINT_MIN_Y]
        labels = [r for r in body if r.x == LABEL_X and r.height == BODY_HEIGHT and r.bg in (GROUND, INVERTED)]
        arrows = [r for r in body if r.height == 24 and r.text in ('<', '>')]
        if labels:
            self.kind = 'list'
            for label in labels:
                same = [r for r in body if r.y == label.y and r.x > label.x]
                value = ' '.join(r.text.strip() for r in same)
                chosen = label.bg == INVERTED
                self.rows.append((label.text.strip(), value, chosen))
                if chosen:
                    self.chosen, self.value = label.text.strip(), value
        elif arrows:
            self.kind = 'spinner'
            values = [r for r in body if r.text not in ('<', '>') and r.y < arrows[0].y + 24 and r.y >= arrows[0].y]
            if values:
                self.value = self.chosen = values[0].text.strip()
            for r in body:
                if r.text.startswith('In use: '):
                    self.in_use = r.text[len('In use: '):].strip()
        else:
            buttons = [r for r in body if r.bg in (INVERTED, PANEL) and r.text.strip() not in ('*', '')]
            if buttons:
                self.kind = 'buttons'
                for b in buttons:
                    star = any(abs(r.y - b.y) < 8 and r.text.strip() == '*' for r in body)
                    chosen = b.bg == INVERTED
                    self.rows.append((b.text.strip(), '*' if star else '', chosen))
                    if chosen:
                        self.chosen = self.value = b.text.strip()
                    if star:
                        self.in_use = b.text.strip()

    def labels(self):
        return [r[0] for r in self.rows]

    def text(self):
        """Every run, a line each, top to bottom."""
        return '\n'.join(r.text for r in sorted(self.runs, key=lambda r: (r.y, r.x)))

    def summary(self):
        if self.kind == 'other':
            return 'screen: ' + ' | '.join(r.text.strip() for r in sorted(self.runs, key=lambda r: (r.y, r.x)))
        parts = ['`%s`' % self.title]
        if self.place:
            parts.append(self.place)
        if self.kind == 'list':
            parts.append('rows: ' + ', '.join(('[%s %s]' if c else '%s %s') % (l, v) for l, v, c in self.rows))
        elif self.kind == 'buttons':
            parts.append('buttons: ' + ', '.join(('[%s%s]' if c else '%s%s') % (l, v) for l, v, c in self.rows))
        else:
            parts.append('value `%s`, in use `%s`' % (self.value, self.in_use))
        return ' '.join(parts)

    def __eq__(self, other):
        return isinstance(other, Where) and [(r.x, r.y, r.text) for r in self.runs] == [
            (r.x, r.y, r.text) for r in other.runs]

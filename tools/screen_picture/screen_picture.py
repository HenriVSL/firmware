#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""Puts pictures of the radio's screen together from a log that holds its
streamed drawing (console "screen shot", "screen on", "screen watch <s>").

  screen_picture.py <log> <out.png> [--scale N] [--passes DIR] [--flicker]

Takes the last stream in the log (from "scr: start" to "scr: stop", or to
the end of the log) and writes the screen as it stood at the end. With
--passes, also writes the screen after each pass of the ui into DIR, one
numbered PNG each. With --flicker, counts the pixels painted in one colour
and then in another within one pass (a blank before a redraw), and fails
when there are any. A log of "-" is read from standard input.

Importable: streams() and decode() put pictures together from log lines.
"""
import argparse
import os
import re
import struct
import sys
import zlib

LINE = re.compile(r'\bscr: (.*)$')


def streams(lines):
    """Each stream's lines, oldest first."""
    current = None
    for text in lines:
        m = LINE.search(text.rstrip('\r\n'))
        if not m:
            continue
        words = m.group(1).split()
        if not words:
            continue
        if words[0] == 'start':
            if current is not None:
                yield current
            current = [words]
        elif current is not None:
            current.append(words)
            if words[0] == 'stop':
                yield current
                current = None
    if current is not None:
        yield current


def rgb565(value):
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


class Screen:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.pixels = [[0] * width for _ in range(height)]
        # Pixels painted in this pass, and those painted in two colours.
        self.painted = {}
        self.flickered = set()

    def put(self, x, y, colour):
        if 0 <= x < self.width and 0 <= y < self.height:
            before = self.painted.get((x, y))
            if before is not None and before != colour:
                self.flickered.add((x, y))
            self.painted[(x, y)] = colour
            self.pixels[y][x] = colour

    def pass_done(self):
        self.painted = {}

    def fill(self, x, y, w, h, colour):
        for py in range(y, y + h):
            for px in range(x, x + w):
                self.put(px, py, colour)

    def bitmap(self, x, y, w, h, fg, bg, data):
        row_bytes = (w + 7) // 8
        for r in range(h):
            for c in range(w):
                byte = data[r * row_bytes + c // 8]
                self.put(x + c, y + r, fg if byte & (0x80 >> (c % 8)) else bg)

    def png(self, path, scale):
        raw = bytearray()
        for row in self.pixels:
            line = bytearray(b'\0')
            for colour in row:
                line += bytes(rgb565(colour)) * scale
            raw += bytes(line) * scale
        width, height = self.width * scale, self.height * scale

        def chunk(kind, data):
            body = kind + data
            return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)

        with open(path, 'wb') as f:
            f.write(b'\x89PNG\r\n\x1a\n')
            f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)))
            f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
            f.write(chunk(b'IEND', b''))


class Result:
    """A stream put together: the screen at its end and what was counted."""

    def __init__(self, screen, passes, lost):
        self.screen = screen
        self.passes = passes
        self.lost = lost

    @property
    def flickered(self):
        return len(self.screen.flickered)


def decode(stream, passes_dir=None, scale=3):
    """Draws one stream's lines (as streams() gives them) onto a Screen;
    with @p passes_dir, writes a picture after each pass there."""
    screen = Screen(int(stream[0][1]), int(stream[0][2]))
    expected = 0
    passes = 0
    lost = 0
    if passes_dir:
        os.makedirs(passes_dir, exist_ok=True)
    for words in stream[1:]:
        kind = words[0]
        if kind == 'stop':
            break
        if kind not in ('f', 'b', 'e'):
            continue
        number = int(words[1])
        if number != expected:
            lost += 1
        expected = number + 1
        if kind == 'f':
            x, y, w, h = (int(v) for v in words[2:6])
            screen.fill(x, y, w, h, int(words[6], 16))
        elif kind == 'b':
            x, y, w, h = (int(v) for v in words[2:6])
            screen.bitmap(x, y, w, h, int(words[6], 16), int(words[7], 16), bytes.fromhex(words[8]))
        elif kind == 'e':
            passes += 1
            screen.pass_done()
            if passes_dir:
                screen.png(os.path.join(passes_dir, '%04d.png' % passes), scale)
    return Result(screen, passes, lost)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('log')
    parser.add_argument('out')
    parser.add_argument('--scale', type=int, default=3)
    parser.add_argument('--passes', metavar='DIR')
    parser.add_argument('--flicker', action='store_true')
    args = parser.parse_args()

    source = sys.stdin if args.log == '-' else open(args.log, encoding='utf-8', errors='replace')
    with source:
        found = list(streams(source))
    if not found:
        sys.exit('no streamed screen in the log')
    result = decode(found[-1], args.passes, args.scale)
    result.screen.png(args.out, args.scale)
    report = '%s: %d passes' % (args.out, result.passes)
    if result.lost:
        report += ', lines lost in %d places' % result.lost
    if args.flicker:
        report += ', %d pixels painted twice in one pass' % result.flickered
    print(report)
    if result.lost or (args.flicker and result.flickered):
        sys.exit(1)


if __name__ == '__main__':
    main()

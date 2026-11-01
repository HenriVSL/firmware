#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""Runs hardware checklists on a radio running DeltaMM, or on the headless
simulator, through the console, and writes a report.

  radio_test.py list
  radio_test.py run <checklist> [--sim [PATH]] [--allow-transmit] [--out DIR]
                    [--steps 1,3-5] [--no-human] [--param name=value]
  radio_test.py where | text | shot <out.png> | goto <A/B/C> | cmd <text>
                    [--sim [PATH]]

See README.md next to this file.
"""
import argparse
import datetime
import importlib.util
import os
import sys

# No __pycache__ folders in the source tree.
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from radiotest import transport  # noqa: E402
from radiotest.radio import Guard, Radio, Failed, SafetyError  # noqa: E402
from radiotest.runner import Runner  # noqa: E402

REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
DEFAULT_SIM = os.path.join(REPO, 'build', 'host', 'boards', 'sim_headless', 'sim')
CHECKLISTS = os.path.join(HERE, 'checklists')


def load_checklist(name):
    path = name if name.endswith('.py') else os.path.join(CHECKLISTS, name.replace('-', '_') + '.py')
    if not os.path.exists(path):
        sys.exit('no checklist %s (try: radio_test.py list)' % path)
    if CHECKLISTS not in sys.path:
        sys.path.insert(0, CHECKLISTS)
    spec = importlib.util.spec_from_file_location(os.path.splitext(os.path.basename(path))[0], path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_steps(text):
    if not text:
        return None
    chosen = set()
    for part in text.split(','):
        if '-' in part and '.' not in part:
            a, b = part.split('-')
            chosen.update(str(i) for i in range(int(a), int(b) + 1))
        else:
            chosen.add(part)
    return chosen


def open_link(args):
    if args.sim is not None:
        link = transport.SimLink(args.sim or DEFAULT_SIM, args.sim_arg or [])
    else:
        link = transport.SerialLink(args.port)
    return link


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('command', choices=['list', 'run', 'where', 'text', 'shot', 'goto', 'cmd'])
    parser.add_argument('arg', nargs='*')
    parser.add_argument('--sim', nargs='?', const='', default=None, metavar='PATH',
                        help='the headless simulator instead of the radio (default %s)' % DEFAULT_SIM)
    parser.add_argument('--sim-arg', action='append', help='an argument for the simulator')
    parser.add_argument('--port', default=transport.LOG_PORT, help='the log port (glob)')
    parser.add_argument('--allow-transmit', action='store_true',
                        help='run steps marked transmit (they may key the radio or change power)')
    parser.add_argument('--out', help='report folder (default radio-test-<checklist>-<time>)')
    parser.add_argument('--steps', help='only these steps, e.g. 1,3-5,2.4')
    parser.add_argument('--no-human', action='store_true', help='never wait for the operator; list human steps')
    parser.add_argument('--param', action='append', default=[], help='name=value for the checklist')
    parser.add_argument('--settle', type=float, help='seconds for the ui to act on a key')
    args = parser.parse_args()

    if args.command == 'list':
        for f in sorted(os.listdir(CHECKLISTS)):
            if f.endswith('.py') and not f.startswith('_'):
                m = load_checklist(os.path.join(CHECKLISTS, f))
                print('%-20s %-50s %d steps' % (f[:-3].replace('_', '-'), m.TITLE, len(m.STEPS)))
        return 0

    link = open_link(args)
    radio = Radio(link, Guard(args.allow_transmit), args.settle)
    try:
        if args.command == 'run':
            if len(args.arg) != 1:
                sys.exit('run <checklist>')
            checklist = load_checklist(args.arg[0])
            stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
            out = args.out or 'radio-test-%s-%s' % (os.path.splitext(os.path.basename(args.arg[0]))[0], stamp)
            params = dict(p.split('=', 1) for p in args.param)
            if radio.sim:
                radio.wait_boot(20, 0)
            runner = Runner(radio, checklist, os.path.abspath(out), args.allow_transmit,
                            interactive=False if args.no_human else None, only=parse_steps(args.steps),
                            params=params)
            runner.run()
            path = runner.report(link.describe())
            print('report: %s' % path)
            return 1 if any(r.status == 'fail' for r in runner.results) or runner.aborted else 0
        if radio.sim:
            radio.wait_boot(20, 0)
        if args.command == 'where':
            print(radio.where().summary())
        elif args.command == 'text':
            print(radio.where().text())
        elif args.command == 'shot':
            print(radio.shot(args.arg[0] if args.arg else 'screen.png'))
        elif args.command == 'goto':
            print(radio.goto(args.arg[0]).summary())
        elif args.command == 'cmd':
            for line in radio.collect(' '.join(args.arg)):
                print(line.text)
        return 0
    except (Failed, SafetyError, transport.LinkError) as e:
        print('error: %s' % e, file=sys.stderr)
        return 1
    finally:
        link.close()


if __name__ == '__main__':
    sys.exit(main())

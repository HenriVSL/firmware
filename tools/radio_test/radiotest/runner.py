# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""Checklist steps, the run, and its report.

A checklist is a Python module with TITLE, SOURCE (the checklist it
follows), STEPS (a list of Step) and optionally BEFORE and UNDO (lists of
actions run first and last) and PARAMS (names the run may set with
--param name=value, with their defaults).

Actions (do, undo, BEFORE, UNDO) are strings, or callables taking the
Context:

  cmd <text>          a console command
  key <Name> [long]   one key; keys <A> <B> ...  several in turn
  knob <n>            the channel knob, negative for back
  goto <A/B/C>        from anywhere to the row C of the group A > B
  open <A/B/C>        goto, then Menu on the last row
  choose <label>      the cursor onto a row or button of the screen's list
  set <label>         choose, then Menu
  home                Exit back to the standby screen
  at <A/B/C>          goto, unless the row C of A > B is chosen already
  check <expectation> an expectation in the middle of the actions
  restart             restart and wait for the start line
  wait <s>            wait
  mark                later log expectations look from here
  shot <name>         a picture of the screen, saved with the report
  watch <s>           stream the screen for s seconds (for `flicker`)
  qdmr read <file> | qdmr write <file> | qdmr diff <a> <b>
                      (a write skips the step without --allow-qdmr-write;
                      the diff leaves out the changes a QDMR write is
                      known to make, and reports them as a quirk)
  dmm-radio <args>    the flash tool; its output is kept

Expectations (expect) are strings, or callables taking the Context and
returning (ok, evidence):

  log <regex>         a log line since the step began (or `mark`),
                      waiting up to the step's timeout; log@<s> <regex>
                      waits s seconds
  nolog <regex>       no such line since the step began
  title|row|value|place|hint|kind|in_use <regex>
                      the menu screen read from `screen text`: its title,
                      the chosen row ("label value"), the chosen row's or
                      the spinner's value, "n/m", the hint bar, the kind
                      (list, buttons, spinner, other), the value in use
  screen <regex>      any text on the screen (all runs, a line each)
  noscreen <regex>    no such text on the screen
  flicker <n>         at most n pixels painted twice in the last watch
  nodiff              the last qdmr diff found no differences
  stack <task> ><n>   the next uptime line's unused stack words above n
  answer <regex>      the output of the last qdmr or dmm-radio action

A file name in actions may hold {out} (the report folder) and a
{param} from PARAMS. In expectations, <<name>> stands for a value
remembered in the parameters (ctx.params), escaped for the regex.
"""
import datetime
import os
import re
import sys
import time
import traceback

from .radio import Failed, SafetyError, SETTING_LINES, Skipped


class Step:
    def __init__(self, id, title, do=(), expect=(), undo=(), human=None, transmit=False, radio_only=None,
                 changes=False, timeout=5.0, undo_now=False, after=()):
        self.id = str(id)
        self.title = title
        self.do = list(do)
        self.expect = list(expect)
        self.undo = list(undo)
        self.human = human
        self.transmit = transmit
        # Why the simulator cannot run this step (no RF, no QDMR port,
        # nothing kept across a restart, ...); None when it can.
        self.radio_only = radio_only
        self.changes = changes
        self.timeout = timeout
        self.undo_now = undo_now
        # Steps that must have passed in this run, else this one is
        # skipped.
        self.after = [str(a) for a in after]


class Result:
    def __init__(self, step):
        self.step = step
        self.status = 'pass'
        self.reason = ''
        self.evidence = []
        self.pictures = []
        self.seconds = 0.0


class Context:
    """What actions and expectations work with."""

    def __init__(self, radio, out, params):
        self.radio = radio
        self.out = out
        self.params = params
        self.since = 0
        self.result = None
        self.last_watch = None
        self.last_diff = None
        self.last_output = ''
        self.shots = 0
        self.allow_qdmr_write = False

    def path(self, name):
        name = name.format(out=self.out, **self.params)
        return name if os.path.isabs(name) else os.path.join(self.out, name)

    def evidence(self, text):
        if self.result is not None:
            self.result.evidence.append(text)

    def picture(self, name):
        self.shots += 1
        file = 'pictures/%02d-%s.png' % (self.shots, re.sub(r'[^A-Za-z0-9_.-]+', '-', name))
        self.radio.shot(os.path.join(self.out, file))
        if self.result is not None:
            self.result.pictures.append(file)
        return file


def run_action(ctx, action):
    r = ctx.radio
    if callable(action):
        return action(ctx)
    verb, _, rest = action.strip().partition(' ')
    rest = rest.strip()
    if verb == 'cmd':
        r.cmd(rest.format(**ctx.params))
    elif verb == 'key':
        words = rest.split()
        r.key(words[0], long=len(words) > 1 and words[1] == 'long')
    elif verb == 'keys':
        r.keys(*rest.split())
    elif verb == 'knob':
        r.knob(int(rest))
    elif verb == 'goto':
        r.goto(rest)
    elif verb == 'at':
        r.at(rest)
    elif verb == 'check':
        check(ctx, rest, 5.0)
    elif verb == 'open':
        r.goto(rest, open_last=True)
    elif verb == 'choose':
        r.choose(rest)
    elif verb == 'set':
        r.set_value(rest)
    elif verb == 'home':
        r.home()
    elif verb == 'restart':
        line = r.restart()
        ctx.evidence('restarted: `%s`' % line.text)
    elif verb == 'wait':
        time.sleep(float(rest))
    elif verb == 'mark':
        ctx.since = r.mark()
    elif verb == 'shot':
        ctx.picture(rest or 'screen')
    elif verb == 'watch':
        ctx.last_watch = r.watch(int(rest))
        ctx.evidence('screen watch %s s: %d passes, %d pixels painted twice, lines lost in %d places' % (
            rest, ctx.last_watch.passes, ctx.last_watch.flickered, ctx.last_watch.lost))
    elif verb == 'qdmr':
        sub, _, files = rest.partition(' ')
        files = [ctx.path(f) for f in files.split()]
        if sub == 'read':
            ctx.last_output = r.qdmr_read(files[0])
        elif sub == 'write':
            if not ctx.allow_qdmr_write:
                raise Skipped('QDMR writes need --allow-qdmr-write (a write puts the codeplug back as QDMR '
                              'reads it, which may change settings)')
            ctx.last_output = r.qdmr_write(files[0])
        elif sub == 'diff':
            diff = r.yaml_diff(files[0], files[1])
            quirks, ctx.last_diff = r.qdmr_quirks(diff)
            ctx.evidence('diff %s %s: %s' % (os.path.basename(files[0]), os.path.basename(files[1]),
                                             'no differences' if not ctx.last_diff else '%d lines differ' %
                                             len(ctx.last_diff)))
            if quirks:
                ctx.evidence('QDMR quirk (expected after a write): ' + '; '.join(quirks))
            if ctx.last_diff:
                ctx.evidence('```\n' + '\n'.join(ctx.last_diff[:40]) + '\n```')
        else:
            raise ValueError('qdmr ' + sub)
        if sub != 'diff':
            ctx.evidence('dmrconf %s: `%s`' % (sub, (ctx.last_output.splitlines() or [''])[-1][:200]))
    elif verb == 'dmm-radio':
        args = [a.format(out=ctx.out, **ctx.params) for a in rest.split()]
        ctx.last_output = r.dmm_radio(*args)
        ctx.evidence('dmm-radio %s:\n```\n%s\n```' % (rest, ctx.last_output[-1500:]))
    else:
        raise ValueError('unknown action %r' % action)


SCREEN_FIELDS = ('title', 'row', 'value', 'place', 'hint', 'kind', 'in_use', 'screen', 'noscreen')


def screen_field(w, field):
    if field == 'row':
        return ('%s %s' % (w.chosen or '', w.value or '')).strip() if w.kind == 'list' else (w.chosen or '')
    if field in ('screen', 'noscreen'):
        return w.text()
    return getattr(w, field) or ''


def check(ctx, expectation, timeout):
    """Raises Failed when @p expectation is not met; notes the evidence."""
    r = ctx.radio
    if callable(expectation):
        ok, evidence = expectation(ctx)
        ctx.evidence(evidence)
        if not ok:
            raise Failed(evidence)
        return
    verb, _, rest = expectation.strip().partition(' ')
    rest = re.sub(r'<<(\w+)>>', lambda m: re.escape(str(ctx.params[m.group(1)])), rest.strip())
    wait = timeout
    if '@' in verb:
        verb, _, seconds = verb.partition('@')
        wait = float(seconds)
    if verb == 'log':
        line = r.wait(rest, wait, ctx.since)
        ctx.evidence('`%s`' % line.text)
    elif verb == 'nolog':
        found = r.find(rest, ctx.since)
        if found:
            raise Failed('unexpected log line: `%s`' % found[0].text)
        ctx.evidence('no /%s/ in the log' % rest)
    elif verb in SCREEN_FIELDS:
        rx = re.compile(rest, re.M)
        end = time.monotonic() + min(wait, 3.0)
        while True:
            w = r.where(fresh=True)
            text = screen_field(w, verb)
            hit = rx.search(text) is not None
            if hit != (verb == 'noscreen'):
                ctx.evidence('%s /%s/: %s' % (verb, rest, w.summary() if verb not in ('screen', 'noscreen')
                                              else 'ok'))
                return
            if time.monotonic() > end:
                raise Failed('%s /%s/ not met; %s: %r' % (verb, rest, verb, text if len(text) < 300 else
                                                           text[:300]) + '; ' + w.summary())
            time.sleep(0.3)
    elif verb == 'flicker':
        if ctx.last_watch is None:
            raise Failed('no screen watch before `flicker`')
        n = ctx.last_watch.flickered
        if n > int(rest) or ctx.last_watch.lost:
            raise Failed('%d pixels painted twice in one pass, lines lost in %d places' % (n, ctx.last_watch.lost))
    elif verb == 'nodiff':
        if ctx.last_diff is None:
            raise Failed('no qdmr diff before `nodiff`')
        if ctx.last_diff:
            raise Failed('the codeplugs differ in %d lines' % len(ctx.last_diff))
    elif verb == 'stack':
        task, _, limit = rest.partition(' ')
        words, line = r.stack_words(task, max(wait, 75))
        ctx.evidence('`%s`' % line.text)
        if words <= int(limit.lstrip('>')):
            raise Failed('%s: %d unused stack words, not above %s' % (task, words, limit.lstrip('>')))
    elif verb == 'answer':
        if not re.search(rest, ctx.last_output, re.M):
            raise Failed('the output has no /%s/' % rest)
        ctx.evidence('output has /%s/' % rest)
    else:
        raise ValueError('unknown expectation %r' % expectation)


class Runner:
    def __init__(self, radio, checklist, out, allow_transmit=False, interactive=None, only=None, params=None,
                 echo=print, allow_qdmr_write=False):
        self.radio = radio
        self.checklist = checklist
        self.out = out
        self.allow_transmit = allow_transmit
        self.interactive = sys.stdin.isatty() if interactive is None else interactive
        self.only = only
        self.params = dict(getattr(checklist, 'PARAMS', {}))
        self.params.update(params or {})
        self.echo = echo
        self.ctx = Context(radio, out, self.params)
        self.ctx.allow_qdmr_write = allow_qdmr_write
        self.results = []
        self.undo_stack = []  # (label, actions, transmit)
        self.undo_log = []
        self.aborted = None
        self.started = None
        self.validate()

    def validate(self):
        has_undo = bool(getattr(self.checklist, 'UNDO', None))
        for s in self.checklist.STEPS:
            if s.changes and not s.undo and not has_undo:
                raise ValueError('step %s changes a setting but has no undo, and the checklist no UNDO' % s.id)
            if s.transmit and not s.radio_only:
                s.radio_only = 'transmits'

    def selected(self, step):
        return self.only is None or step.id in self.only or step.id.split('.')[0] in self.only

    def run(self):
        os.makedirs(self.out, exist_ok=True)
        pictures = os.path.join(self.out, 'pictures')
        if os.path.isdir(pictures):
            for f in os.listdir(pictures):
                os.remove(os.path.join(pictures, f))
        self.started = time.time()
        start_mark = self.radio.mark()
        try:
            before = getattr(self.checklist, 'BEFORE', [])
            if before:
                self.echo('before: %d actions' % len(before))
                for a in before:
                    run_action(self.ctx, a)
            if getattr(self.checklist, 'UNDO', None):
                self.undo_stack.append(('checklist undo', self.checklist.UNDO, False))
            for step in self.checklist.STEPS:
                if not self.selected(step):
                    continue
                res = self.run_step(step)
                self.echo('%-6s %-8s %5.1fs  %s%s' % (step.id, res.status.upper(), res.seconds, step.title,
                                                      ('  -- ' + res.reason) if res.reason else ''))
                if self.aborted:
                    break
        except KeyboardInterrupt:
            self.aborted = 'stopped by the operator'
        except SafetyError as e:
            self.aborted = str(e)
        except Exception as e:  # the run's own trouble: still undo
            self.aborted = 'runner error: %s' % e
            traceback.print_exc()
        finally:
            self.run_undo()
            with open(os.path.join(self.out, 'log.txt'), 'w') as f:
                for line in self.radio.link.since(start_mark):
                    f.write('%9.3f %s\n' % (line.time - self.radio.link.started, line.text))
        return self.results

    def run_step(self, step):
        res = Result(step)
        self.results.append(res)
        self.ctx.result = res
        t0 = time.monotonic()
        if step.transmit and not self.allow_transmit:
            res.status, res.reason = 'skipped', 'transmits: needs --allow-transmit'
            return res
        if step.radio_only and self.radio.sim:
            res.status, res.reason = 'skipped', 'sim: ' + step.radio_only
            return res
        passed = {r.step.id for r in self.results if r.status == 'pass'}
        missing = [a for a in step.after if a not in passed]
        if missing:
            res.status, res.reason = 'skipped', 'needs step %s, which did not pass' % ', '.join(missing)
            return res
        guard = self.radio.guard
        guard.step_allows = step.transmit
        self.radio.violations.clear()
        if step.undo and not step.undo_now:
            self.undo_stack.append(('step %s' % step.id, step.undo, step.transmit))
        self.ctx.since = self.radio.mark()
        step_mark = self.ctx.since
        try:
            for a in step.do:
                run_action(self.ctx, a)
            for e in step.expect:
                check(self.ctx, e, step.timeout)
            if self.radio.violations:
                raise SafetyError('transmitted or changed power outside a transmit step: `%s`' %
                                  self.radio.violations[0].text)
            changed = self.radio.find(SETTING_LINES, step_mark)
            if changed and not self.undo_stack and not step.undo:
                raise Failed('changed a setting without an undo: `%s`' % changed[0].text)
            if step.human:
                self.ask_human(step, res)
        except Skipped as e:
            res.status, res.reason = 'skipped', str(e)
        except Failed as e:
            res.status, res.reason = 'fail', str(e)
            self.fail_picture()
        except SafetyError as e:
            res.status, res.reason = 'fail', str(e)
            self.aborted = str(e)
        except Exception as e:
            res.status, res.reason = 'fail', '%s: %s' % (type(e).__name__, e)
            traceback.print_exc()
        finally:
            guard.step_allows = False
            if step.undo and step.undo_now:
                self.undo('step %s' % step.id, step.undo, step.transmit)
            res.seconds = time.monotonic() - t0
        return res

    def fail_picture(self):
        try:
            self.ctx.picture('failed')
        except Exception:
            pass

    def ask_human(self, step, res):
        if not self.interactive:
            res.status = 'human'
            res.reason = 'needs a person: ' + step.human
            return
        print('\n--- step %s: %s\n%s' % (step.id, step.title, step.human))
        input('Press Enter when done... ')
        while True:
            answer = input('As expected? [y/n/s=skip] ').strip().lower()
            if answer in ('y', 'n', 's'):
                break
        if answer == 'n':
            note = input('What was off? ').strip()
            raise Failed('the operator: ' + (note or 'not as expected'))
        if answer == 's':
            res.status, res.reason = 'human', 'skipped by the operator: ' + step.human
        else:
            res.evidence.append('the operator confirmed: ' + step.human)

    def undo(self, label, actions, transmit=False):
        # The undo of a transmit step may set the power back.
        self.radio.guard.step_allows = transmit
        for a in actions:
            name = a if isinstance(a, str) else getattr(a, '__name__', 'action')
            try:
                run_action(self.ctx, a)
                self.undo_log.append((label, name, 'done'))
            except Skipped as e:
                self.undo_log.append((label, name, 'skipped: %s' % e))
            except Exception as e:
                self.undo_log.append((label, name, 'FAILED: %s' % e))
        self.radio.guard.step_allows = False

    def run_undo(self):
        self.ctx.result = None
        while self.undo_stack:
            label, actions, transmit = self.undo_stack.pop()
            self.echo('undo: ' + label)
            self.undo(label, actions, transmit)

    # Report --------------------------------------------------------------------

    def report(self, target):
        counts = {}
        for r in self.results:
            counts[r.status] = counts.get(r.status, 0) + 1
        total = time.time() - self.started
        lines = ['# %s: test run' % self.checklist.TITLE, '']
        lines.append('- Checklist: `%s` (%s)' % (getattr(self.checklist, '__name__', '?'),
                                                getattr(self.checklist, 'SOURCE', '')))
        lines.append('- Target: %s; firmware `%s` on `%s`' % (target, self.radio.version or '?',
                                                              self.radio.board or '?'))
        lines.append('- Started %s, took %.0f s; transmit %s; QDMR writes %s' % (
            datetime.datetime.fromtimestamp(self.started).strftime('%Y-%m-%d %H:%M:%S'), total,
            'allowed' if self.allow_transmit else 'refused',
            'allowed' if self.ctx.allow_qdmr_write else 'skipped'))
        lines.append('- Result: ' + ', '.join('%d %s' % (counts[k], k) for k in ('pass', 'fail', 'human', 'skipped')
                                              if k in counts))
        if self.aborted:
            lines.append('- **Stopped:** %s' % self.aborted)
        lines.append('- Log: [log.txt](log.txt)')
        lines += ['', '| # | Step | Result | Time |', '| --- | --- | --- | --- |']
        for r in self.results:
            lines.append('| %s | %s | %s | %.1f s |' % (r.step.id, r.step.title.replace('|', '/'), r.status.upper(),
                                                       r.seconds))
        lines += ['', '## Steps', '']
        for r in self.results:
            lines.append('### %s. %s: %s' % (r.step.id, r.step.title, r.status.upper()))
            lines.append('')
            if r.reason:
                lines.append('- %s' % r.reason)
            for e in r.evidence:
                lines.append('- %s' % e if not e.startswith('```') else e)
            for p in r.pictures:
                lines.append('- ![%s](%s)' % (os.path.basename(p), p))
            lines.append('')
        if self.undo_log:
            lines += ['## Undo', '']
            for label, name, status in self.undo_log:
                lines.append('- %s: `%s`: %s' % (label, name, status))
            lines.append('')
        human = [r for r in self.results if r.status == 'human' or (r.status == 'skipped' and r.step.transmit)]
        if human or any(r.step.human for r in self.results):
            lines += ['## For the human', '']
            for r in self.results:
                if r.status == 'human':
                    lines.append('- Step %s (%s): %s' % (r.step.id, r.step.title, r.step.human))
                elif r.status == 'skipped' and r.step.transmit:
                    lines.append('- Step %s (%s): transmits; run with --allow-transmit and a person%s' % (
                        r.step.id, r.step.title, (': ' + r.step.human) if r.step.human else ''))
            lines.append('')
        path = os.path.join(self.out, 'report.md')
        with open(path, 'w') as f:
            f.write('\n'.join(lines))
        return path

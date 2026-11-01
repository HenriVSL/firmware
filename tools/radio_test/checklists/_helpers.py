# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""Actions and expectations the checklists share."""
import re
import time

from radiotest.radio import Failed


def panel(text):
    """A regex for @p text wherever the screen breaks its lines."""
    return r'\s+'.join(re.escape(w) for w in text.split())


def remember(key, path=None, field='value'):
    """Action: keeps a field of the screen (after goto @p path) as
    ctx.params[key]; 'in_use' opens the row's value screen for it."""
    def action(ctx):
        r = ctx.radio
        # The value in use as its value screen names it, which may be
        # longer than the row's text.
        w = r.goto(path, open_last=field == 'in_use') if path else r.where(fresh=True)
        value = getattr(w, field)
        if field == 'in_use':
            r.key('Exit')
        if value is None:
            raise Failed('nothing to remember for %s: %s' % (key, w.summary()))
        ctx.params[key] = value
        ctx.evidence('remembered %s = `%s`' % (key, value))
    action.__name__ = 'remember %s' % key
    return action


def remember_log(key, pattern, command=None, timeout=3.0):
    """Action: keeps group 1 of the first log line matching @p pattern
    (after sending @p command) as ctx.params[key]."""
    def action(ctx):
        r = ctx.radio
        since = r.cmd(command) if command else ctx.since
        line = r.wait(pattern, timeout, since)
        ctx.params[key] = re.search(pattern, line.text).group(1)
        ctx.evidence('remembered %s = `%s` from `%s`' % (key, ctx.params[key], line.text))
    action.__name__ = 'remember %s' % key
    return action


def value_is(compute, field='value'):
    """Expectation: the screen's @p field equals compute(params)."""
    def expectation(ctx):
        want = str(compute(ctx.params))
        end = time.monotonic() + 2
        while True:
            w = ctx.radio.where(fresh=True)
            got = str(getattr(w, field))
            if got == want or time.monotonic() > end:
                return got == want, '%s `%s` (want `%s`): %s' % (field, got, want, w.summary())
            time.sleep(0.3)
    return expectation


def log_is(pattern_of):
    """Expectation: a log line since the step began matches the regex
    pattern_of(params) gives."""
    def expectation(ctx):
        pattern = pattern_of(ctx.params)
        try:
            line = ctx.radio.wait(pattern, 5.0, ctx.since)
        except Failed as e:
            return False, str(e)
        return True, '`%s`' % line.text
    return expectation


def set_to(path, value, back_home=True):
    """Action: opens the value screen of @p path and sets @p value (text
    as the screen shows it, or a callable of the parameters giving it);
    Exit when it is in use already."""
    def action(ctx):
        want = str(value(ctx.params) if callable(value) else value)
        r = ctx.radio
        w = r.goto(path)
        if w.value == want:
            ctx.evidence('%s already `%s`' % (path, want))
        else:
            r.key('Menu')
            w = r.where(fresh=True)
            if w.in_use == want:
                r.key('Exit')
            elif w.kind == 'spinner':
                seen = []
                for direction in ('Up', 'Down'):
                    for _ in range(80):
                        if w.value == want:
                            break
                        seen.append(w.value)
                        r.key(direction)
                        w = r.where(fresh=True)
                        if w.value == seen[-1]:
                            break  # the end of the scale
                    if w.value == want:
                        break
                if w.value != want:
                    r.key('Exit')
                    raise Failed('could not find `%s` on %s' % (want, path))
                r.key('Menu')
            else:
                r.set_value(want)
            ctx.evidence('%s set to `%s`' % (path, want))
        if back_home:
            r.home()
    action.__name__ = 'set %s' % path
    return action


def restore(path, key):
    """Action: sets @p path back to ctx.params[key]; nothing when the
    value was never remembered."""
    inner = set_to(path, lambda p: p[key])

    def action(ctx):
        if key in ctx.params:
            inner(ctx)
    action.__name__ = 'restore %s' % path
    return action


def set_other(path, key=None):
    """Action: on the buttons of @p path, sets the first choice not in
    use (keeping it as ctx.params[key])."""
    def action(ctx):
        r = ctx.radio
        r.goto(path, open_last=True)
        w = r.where(fresh=True)
        others = [label for label, _, _ in w.rows if label != w.in_use]
        if w.kind not in ('buttons', 'list') or not others:
            raise Failed('no other choice on ' + w.summary())
        r.set_value(others[0])
        if key:
            ctx.params[key] = others[0]
        ctx.evidence('%s set to `%s` (was `%s`)' % (path, others[0], w.in_use))
        r.home()
    action.__name__ = 'set other %s' % path
    return action


def press_while_watching(seconds, actions, passes_dir=None):
    """Action: streams the screen for @p seconds while running @p actions
    (action strings or callables) on the radio; `flicker` checks it."""
    from radiotest.runner import run_action

    def action(ctx):
        def run():
            for a in actions:
                run_action(ctx, a)
        ctx.last_watch = ctx.radio.watch(seconds, passes_dir, run)
        ctx.evidence('screen watched %d s: %d passes, %d pixels painted twice in one pass, lines lost in %d places' % (
            seconds, ctx.last_watch.passes, ctx.last_watch.flickered, ctx.last_watch.lost))
    action.__name__ = 'watch %d s' % seconds
    return action


def settings_has(pattern, which='codeplug'):
    """Expectation: the `settings` answer's line (codeplug, preferences or
    pending) matches @p pattern."""
    def expectation(ctx):
        s = ctx.radio.settings()
        line = s.get(which, '')
        ok = re.search(pattern, line) is not None
        return ok, '`%s`' % (line or 'no %s line in `settings`' % which)
    return expectation


def now(expectation):
    """Action: checks @p expectation (a string or callable, as in expect)
    in the middle of the actions."""
    from radiotest.runner import check

    def action(ctx):
        check(ctx, expectation, 5.0)
    action.__name__ = 'check'
    return action


def standby(ctx):
    """Expectation: the standby screen is shown."""
    w = ctx.radio.where(fresh=True)
    return ctx.radio.on_standby(w), w.summary()


def rows_count(n):
    """Expectation: the list or buttons on the screen have @p n rows."""
    def expectation(ctx):
        w = ctx.radio.where(fresh=True)
        return len(w.rows) == n, '%d rows: %s' % (len(w.rows), w.summary())
    return expectation


def large(pattern):
    """Expectation: text in the large font (the standby's large name)
    matches @p pattern."""
    def expectation(ctx):
        w = ctx.radio.where(fresh=True)
        big = [r.text.strip() for r in w.runs if r.height == 24]
        ok = any(re.search(pattern, t) for t in big)
        return ok, 'large text %s (want /%s/)' % (big, pattern)
    return expectation


def channel(spec):
    """Action: from the standby screen, the zone and channel of @p spec
    ("Zone/Channel", or a {param} naming one): the zone from the P1 list,
    then the knob until the channel's name shows. The name is looked for
    in every text on the screen, so any layout will do."""
    def action(ctx):
        r = ctx.radio
        zone, _, name = spec.format(**ctx.params).rpartition('/')
        r.home()
        if zone:
            r.key('P1')
            w = r.where(fresh=True)
            if w.title != 'Zone':
                raise Failed('P1 did not open the zone list: ' + w.summary())
            r.choose(zone)
            r.key('Menu')
        for _ in range(64):
            w = r.home()
            if any(run.text.strip() == name or run.text.strip().endswith(' ' + name) for run in w.runs):
                ctx.evidence('on channel `%s`' % spec.format(**ctx.params))
                return
            r.knob(1)
        raise Failed('no channel %r in zone %r (set it with --param)' % (name, zone))
    action.__name__ = 'channel %s' % spec
    return action


def top_line(pattern):
    """Expectation: the standby's top line (left, under the status bar)
    matches @p pattern; None: there is none."""
    def expectation(ctx):
        w = ctx.radio.where(fresh=True)
        top = [r.text.strip() for r in w.runs if 10 <= r.y <= 20 and r.height == 8 and r.x < 80]
        ok = not top if pattern is None else any(re.search(pattern, t) for t in top)
        return ok, 'top line %s (want %s)' % (top, 'none' if pattern is None else '/%s/' % pattern)
    return expectation

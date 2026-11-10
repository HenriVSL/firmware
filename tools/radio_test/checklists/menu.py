# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/menu.md: the menu's groups, value screens and kept values.

Each step checks the screen with `screen text` after its keys. A step
starts with `at` where the checklist relies on where the step before
ended, so a step that failed does not lead the next one astray.
"""
from _helpers import (log_is, now, panel, press_while_watching, remember, restore, rows_count, settings_has,
                      standby, value_is)
from radiotest.runner import Step

TITLE = 'Menu: groups by use, large value screens, values kept'
SOURCE = 'docs/hw-tests/menu.md (feat/offset-numeric)'

# The values to put back, then a restart: the menu starts on the first
# row of each group, as the checklist expects.
BEFORE = [
    remember('squelch', 'Channel/Squelch'),
    remember('squelch_type', 'Channel/Squelch type'),
    remember('power', 'Channel/Power'),
    remember('roaming', 'Roaming/Roaming'),
    remember('battery_save', 'Settings/Keys & battery/Battery save'),
    'home',
    'restart',
    'home',
    lambda ctx: learn_top(ctx),
]

UNDO = [
    restore('Channel/Squelch', 'squelch'),
    restore('Roaming/Roaming', 'roaming'),
    'home',
]

TOP = ['Channel', 'Roaming', 'GPS', 'Display', 'Settings', 'Tools']
# The rows above the panel, first; a build with an APRS group lists it
# after GPS, and has more rows in Tools.
SHOWN = r'^Channel\n>\nRoaming\n>\nGPS\n>\n(APRS\n>\n)?Display\n>'
TOOLS_ABOUT = ['The log, the key test and the version', 'Activity, the log, the key test and the version',
               'Codeplug, activity, the log, key test and version']


def learn_top(ctx):
    """The top of the menu's row count, as <<top_n>>."""
    r = ctx.radio
    r.key('Menu')
    w = r.where(fresh=True)
    ctx.params['top_n'] = w.place.split('/')[1] if w.place else str(len(TOP))
    ctx.evidence('the menu has %s rows at the top' % ctx.params['top_n'])
    r.home()


def level(text):
    """The squelch level of a value as the screen shows it (0 is Open)."""
    return 0 if str(text) == 'Open' else int(text)


def shown(n):
    """A squelch level as the screen shows it."""
    return 'Open' if n == 0 else str(n)


# The squelch's value screen goes round: Open, 1 to 5, Open.
def up(squelch):
    return (level(squelch) + 1) % 6


def roaming_on_first(ctx):
    """Roaming On first when it is Off already, kept before going on, so
    that setting Off is a change the log shows and writes (the checklist's
    UNDO puts the old value back)."""
    r = ctx.radio
    w = r.goto('Roaming/Roaming')
    if w.value == 'Off':
        ctx.evidence('Roaming was Off already: set On first')
        r.key('Menu')
        at = r.mark()
        r.set_value('On')
        r.wait(r'\bsettings kept in the codeplug', 8, at)
        r.home()
    ctx.since = r.mark()

STEPS = [
    Step(1, 'Menu from the standby screen', do=['home', 'key Menu'], expect=[
        'title ^Menu$', 'place ^1/<<top_n>>$', 'row ^Channel >$',
        'screen ' + SHOWN,
        'screen ' + panel('Squelch and power of the channel in use'),
        'hint ^Menu: open   Exit: back$']),
    Step(2, 'Tools last, then the knob wraps round', do=[
        'at Channel', 'choose Tools',
        'check row ^Tools >$', 'check place ^<<top_n>>/<<top_n>>$',
        'check screen (?:' + '|'.join(panel(t) for t in TOOLS_ABOUT) + ')',
        'knob 1'], expect=['row ^Channel >$', 'place ^1/<<top_n>>$']),
    Step(3, 'Menu opens Channel', do=['at Channel', 'key Menu'], expect=[
        'title ^Menu > Channel$', r'screen ^Squelch\n\d\nSquelch type\n\S+\nPower\n\S+',
        'screen ' + panel('Open, or 1 to 5: higher needs a stronger signal'),
        'hint ^Menu: change   Exit: back$']),
    Step(4, 'Squelch value screen', do=['at Channel/Squelch', 'key Menu', 'shot squelch'], expect=[
        'title ^Channel > Squelch$', 'kind ^spinner$', r'in_use ^(\d|Open)$', value_is(lambda p: p['squelch']),
        'screen ^In use: <<squelch>>$']),
    Step(5, 'Up, knob 2 back, Exit leaves the squelch', do=[
        'key Up', now(value_is(lambda p: shown(up(p['squelch'])))),
        'knob -2', now(value_is(lambda p: shown((up(p['squelch']) - 2) % 6))),
        'key Exit'], expect=[
        'title ^Menu > Channel$', 'row ^Squelch <<squelch>>$', r'nolog \bui: squelch\b']),
    Step(6, 'Menu, Up, Menu sets the squelch', changes=True, timeout=8, do=[
        'at Channel/Squelch', 'keys Menu Up Menu'], expect=[
        log_is(lambda p: r'\bui: squelch %d\b' % up(p['squelch'])),
        value_is(lambda p: shown(up(p['squelch']))), r'log@8 \bsettings kept in the codeplug']),
    Step(7, 'Squelch type: Carrier and CTCSS/DCS as buttons, the one in use starred', do=[
        'at Channel/Squelch', 'keys Down Menu', 'shot squelch-type'], expect=[
        'title ^Squelch type$', 'kind ^buttons$', r'in_use ^\S', rows_count(2),
        r'screen \bCarrier\n(\*\n)?CTCSS/DCS\b']),
    Step('8a', 'Power: four buttons, the power in use starred and chosen; Exit leaves it', do=[
        'key Exit', 'at Channel/Power', 'key Menu', 'shot power',
        'check title Power$', 'check kind ^buttons$', now(rows_count(4)),
        now(lambda ctx: (ctx.radio.where().chosen == ctx.radio.where().in_use, ctx.radio.where().summary())),
        'key Exit'], expect=[
        'title ^Menu > Channel$', 'row ^Power <<power>>$', r'nolog \bui: power\b']),
    Step('8b', 'Power: Down, Menu sets another power', transmit=True, changes=True, timeout=8, do=[
        'at Channel/Power', 'keys Menu Down Menu'], expect=[
        r'log \bui: power \d', r'log@8 \bsettings kept in the codeplug'],
        undo=[restore('Channel/Power', 'power')]),
    Step(9, 'Exit back to Menu, on Channel', do=['at Channel/Power', 'key Exit'], expect=[
        'title ^Menu$', 'row ^Channel >$']),
    Step(10, 'Roaming status in the menu\'s style', do=[
        'at Channel', 'keys Down Menu Menu', 'shot roaming-status',
        'check hint ^Menu: search   Exit: back$', 'key Exit'], expect=[
        'title ^Menu > Roaming$', 'row ^Status']),
    Step(11, 'GPS screen; Exit back on Status', do=[
        'at Roaming/Status', 'keys Exit Down Menu Menu', 'shot gps',
        r'check screen ^GPS$', 'check hint ^Exit: back$', 'key Exit'], expect=[
        'title ^Menu > GPS$', 'row ^Status']),
    Step(12, 'Display at the top: Brightness and its value screen; Exit leaves it', do=[
        'at GPS/Status', 'key Exit', 'choose Display', 'key Menu', 'shot display',
        'check title ^Menu > Display$', 'check row ^Brightness', r'check screen ^Light time$',
        'check screen ' + panel('Display brightness'), remember('brightness_12'),
        'key Menu', 'check title ^Brightness$', 'check kind ^spinner$', 'key Exit'], expect=[
        'title ^Menu > Display$', 'row ^Brightness', value_is(lambda p: p['brightness_12'])]),
    Step(13, 'Settings: its groups, Display no longer among them', do=[
        'at Display/Brightness', 'keys Exit Down Menu', 'shot settings'],
         expect=['title ^Menu > Settings$', 'place ^1/[4-7]$', 'row ^Home screen >$',
                 r'screen Home screen\n>\n(Audio\n>\n)?(Radio\n>\n)?Keys & battery\n>',
                 'screen ' + panel('What each row of the home screen shows'), r'noscreen ^Display$']),
    Step(14, 'Keys & battery: the long name alone', do=[
        'at Settings/Home screen', 'choose Keys & battery', 'key Menu',
        'check title ^Keys & battery$', 'key Exit'], expect=[
        'title ^Menu > Settings$', 'row ^Keys & battery >$']),
    Step(15, 'Battery save as a list; Exit leaves it', do=[
        'at Settings/Keys & battery', 'key Menu', 'choose Battery save', 'key Menu', 'shot battery-save',
        'check title ^Battery save$', 'check kind ^list$', r'check screen ^Off\nOn\n150 ms\nFast wake\n70 ms$',
        'check row ^<<battery_save>>', 'key Exit'], expect=[
        'title ^Keys & battery$', 'row ^Battery save <<battery_save>>', r'nolog \bpower: battery save\b']),
    Step(16, 'Clock > Time zone: large zone, marker on a track', do=[
        'at Keys & battery/Battery save', 'keys Exit Down Menu Menu', 'shot time-zone'], expect=[
        'title ^Clock > Time zone$', 'kind ^spinner$', r'value ^UTC', r'place ^\d+/53$', r'in_use ^UTC']),
    Step(17, 'Tools > Version: version and uptime; Menu does nothing', do=[
        'key Exit', 'goto Tools/Version',
        r'check screen ^\d+\.\d+\.\d+\S*\nup \d+ min$', 'check hint ^Exit: back$', 'key Menu'], expect=[
        'title ^Menu > Tools$', 'row ^Version']),
    Step(18, 'The log, then held Exit: key test, then standby', do=[
        'at Tools/Version', 'keys Up Up Menu', 'check screen console: >', 'key Exit long',
        r'check screen ^Keys \(Exit=log\)$', 'key Exit long'], expect=[standby]),
    Step(19, 'P2 opens Channel; Exit goes straight back', do=['home', 'key P2', 'check title ^Menu > Channel$',
                                                              'key Exit'], expect=[standby]),
    Step(20, 'P2 on the row last chosen; squelch one lower, back to standby', changes=True, timeout=8, do=[
        'home', 'key P2', 'check row ^Power', 'keys Up Up', 'check row ^Squelch',
        remember('squelch_19'), 'keys Menu Down Menu'], expect=[
        log_is(lambda p: r'\bui: squelch %d\b' % ((level(p['squelch_19']) - 1) % 6)), standby]),
    Step(21, 'P1: the zone list in the same style', do=[
        'home', 'key P1', 'shot zones', 'check title ^Zone$', 'check kind ^list$',
        'check hint ^Menu: use   Exit: back$',
        'key Exit'], expect=[standby]),
    Step(22, 'Everything drawn in place: no blank, no flash', timeout=8, do=[
        'home', press_while_watching(12, [
            'key P2', 'knob 3', 'knob -3', 'key Menu', 'knob 2', 'key Exit', 'key Exit', 'key P1', 'knob 1',
            'key Exit', 'key Menu', 'knob 1', 'knob -1', 'key Exit'])], expect=['flicker 0']),
    Step(23, 'Squelch and power kept across a restart', radio_only='nothing is kept across a restart',
         do=[remember('squelch_22', 'Channel/Squelch'), 'home', 'restart',
             now(lambda ctx: settings_has(r'squelch %d/' % level(ctx.params['squelch_22']))(ctx)),
             'goto Channel/Squelch'],
         expect=['title ^Menu > Channel$', 'screen ^Squelch\n<<squelch_22>>$', 'screen ^Power\n<<power>>$']),
    Step('24a', 'Roaming Off', changes=True, timeout=8, do=[roaming_on_first, 'open Roaming/Roaming', 'set Off'],
         expect=[
        r'log \bui: auto roaming off', 'row ^Roaming Off$', r'log@8 \bsettings kept in the codeplug']),
    Step('24b', 'Roaming Off kept across a restart', radio_only='nothing is kept across a restart',
         do=['home', 'restart', 'goto Roaming/Roaming'], expect=[
             'row ^Roaming Off$', settings_has(r'auto roaming off', 'codeplug')]),
    Step(25, 'QDMR round trip keeps the menu\'s values', radio_only='no QDMR port', timeout=10, do=[
        'home', 'qdmr read menu-a.yaml', 'qdmr write menu-a.yaml', 'goto Roaming/Roaming'], expect=[
        'row ^Roaming Off$', settings_has(r'auto roaming off', 'codeplug')]),
    Step(26, 'QDMR read back: no differences', radio_only='no QDMR port', do=[
        'home', 'qdmr read menu-b.yaml', 'qdmr diff menu-a.yaml menu-b.yaml'], expect=['nodiff']),
    Step(27, 'Fast knob by hand: only what changes is redrawn',
         human='Turn the knob fast in a list, on Power (without setting it: Exit) and on Time zone; '
               'only the rows, buttons and values that change are redrawn, nothing blanks or flashes.'),
    Step(28, 'ui stack', radio_only='the simulator reports no stack words', timeout=75,
         expect=['stack ui >50']),
]

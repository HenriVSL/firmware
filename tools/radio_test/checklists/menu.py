# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""docs/hw-tests/menu.md: the menu's groups, value screens and kept values.

Each step checks the screen with `screen text` after its keys. A step
starts with `at` where the checklist relies on where the step before
ended, so a step that failed does not lead the next one astray.
"""
from _helpers import (channel, log_is, now, panel, press_while_watching, remember, restore, rows_count,
                      settings_has, standby, value_is)
from radiotest.runner import Step

TITLE = 'Menu: groups by use, large value screens, values kept'
SOURCE = 'docs/hw-tests/menu.md (feat/offset-numeric)'

PARAMS = {
    'fm': 'UHF/Calling 70cm',  # an FM channel (step 31)
    'roam': 'UHF/TG244 Finland',  # a roaming DMR channel (step 32)
}

# The values to put back, then a restart: the menu starts on the first
# row of each group, as the checklist expects.
BEFORE = [
    remember('squelch', 'Channel/Squelch'),
    remember('squelch_type', 'Channel/Squelch type'),
    remember('power', 'Channel/Power'),
    remember('roaming', 'Roaming/Roaming'),
    remember('battery_save', 'Settings/Battery/Battery save'),
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
TOOLS_ABOUT = ['The log and the version', 'The log, the key test and the version',
               'Activity, the log, the key test and the version', 'Codeplug, activity, the log, key test and version']


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

def log_after_exit(ctx):
    """Expectation: Exit put the squelch noted at the start back."""
    return log_is(lambda p: r'\bui: squelch %d\b' % level(p['squelch']))(ctx)


def key_test_or_standby(ctx):
    """Action: from the log, held Exit: the key test in a debug build
    (its title naming held Exit, then held Exit again), else standby."""
    r = ctx.radio
    r.key('Exit', long=True)
    w = r.where(fresh=True)
    if any(run.text.startswith('Keys (hold Exit)') for run in w.runs):
        ctx.evidence('a debug build: the key test, `Keys (hold Exit)`')
        r.key('Exit', long=True)
    else:
        ctx.evidence('no key test (a release build): %s' % w.summary()[:60])


def remember_in_use(key):
    """Action: keeps the value screen's value in use as ctx.params[key]."""
    def action(ctx):
        ctx.params[key] = ctx.radio.where(fresh=True).in_use
        ctx.evidence('remembered %s = `%s`' % (key, ctx.params[key]))
    action.__name__ = 'remember %s' % key
    return action


def satellites_one_font(ctx):
    """Expectation: on the Satellites screen, the knob through every
    choice shows each in the same font, one of them All types."""
    r = ctx.radio
    heights = {}
    for _ in range(7):
        w = r.where(fresh=True)
        big = [run for run in w.runs if run.text.strip() == w.value]
        if big:
            heights[w.value] = big[0].height
        r.knob(1)
    ok = 'All types' in heights and len(set(heights.values())) == 1
    return ok, 'font heights %s' % heights


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
    Step(5, 'Up and the knob try the squelch at once; Exit puts it back', do=[
        'key Up', now(value_is(lambda p: shown(up(p['squelch'])))),
        now(log_is(lambda p: r'\bui: squelch %d\b' % up(p['squelch']))),
        'knob -2', now(value_is(lambda p: shown((up(p['squelch']) - 2) % 6))),
        now(log_is(lambda p: r'\bui: squelch %d\b' % ((up(p['squelch']) - 2) % 6))),
        'mark', 'key Exit', 'wait 4'], expect=[
        'title ^Menu > Channel$', 'row ^Squelch <<squelch>>$', log_after_exit,
        r'nolog \bsettings kept in the codeplug']),
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
    Step(10, 'Roaming: the switch first; the status in the menu\'s style', do=[
        'at Channel', 'keys Down Menu', 'check title ^Menu > Roaming$', 'check row ^Roaming (On|Off)$',
        r'check screen ^Roaming\n(On|Off)\nStatus\n\S.*\nRoam zone\n', 'keys Down Menu', 'shot roaming-status',
        'check hint ^Menu: search   Exit: back$', 'key Exit'], expect=[
        'title ^Menu > Roaming$', 'row ^Status']),
    Step(11, 'GPS: Enabled, Status, Satellites; the GPS screen', do=[
        'at Roaming/Status', 'keys Exit Down', 'check screen ' + panel('On or off, the fix and the satellite systems'),
        'key Menu', 'check title ^Menu > GPS$', 'check row ^Enabled (On|Off)$',
        r'check screen ^Enabled\n(On|Off)\nStatus\n\S.*\nSatellites\n',
        'check screen ' + panel('The GPS receiver on or off'),
        'keys Down Menu', 'shot gps', r'check screen ^GPS$', 'check hint ^Exit: back$', 'key Exit'], expect=[
        'title ^Menu > GPS$', 'row ^Status']),
    Step('11b', 'Satellites: the systems named, All types; Exit leaves them', do=[
        'at GPS/Status', 'key Down', 'check screen ' + panel('Satellite systems used: GPS, GLONASS, BeiDou'),
        'key Menu', 'check title ^GPS > Satellites$', 'check kind ^spinner$', 'shot satellites',
        now(satellites_one_font), 'mark', 'key Exit'], expect=[
        'title ^Menu > GPS$', 'row ^Satellites', r'nolog \bgps: ']),
    Step(12, 'Display: Brightness tried at once; Exit puts it back', do=[
        'at GPS/Satellites', 'key Exit', 'choose Display', 'key Menu', 'shot display',
        'check title ^Menu > Display$', 'check row ^Brightness', r'check screen ^Light time$',
        'check screen ' + panel('Display brightness'), remember('brightness_12'),
        'key Menu', 'check title ^Brightness$', 'check kind ^spinner$',
        'key Up', now(log_is(lambda p: r'\bui: brightness %d\b' % (int(p['brightness_12']) % 5 + 1))),
        'mark', 'key Exit', 'wait 4'], expect=[
        'title ^Menu > Display$', 'row ^Brightness', value_is(lambda p: p['brightness_12']),
        log_is(lambda p: r'\bui: brightness %s\b' % p['brightness_12']), r'nolog \bsettings kept in the codeplug']),
    Step(13, 'Settings: its groups, Keys and Battery apart', do=[
        'at Display/Brightness', 'keys Exit Down Menu', 'shot settings'],
         expect=['title ^Menu > Settings$', 'place ^1/[4-7]$', 'row ^Home screen >$',
                 r'screen Home screen\n>\n(Audio\n>\n)?(Radio\n>\n)?Keys\n>\nBattery\n>',
                 'screen ' + panel('What each row of the home screen shows'), r'noscreen ^Display$']),
    Step(14, 'Keys: Lock covers and Auto lock', do=[
        'at Settings/Home screen', 'choose Keys', 'key Menu',
        'check title ^Settings > Keys$', r'check screen ^Lock covers\n\S.*\nAuto lock\n', 'key Exit'], expect=[
        'title ^Menu > Settings$', 'row ^Keys >$']),
    Step(15, 'Battery save as a list; Exit leaves it', do=[
        'at Settings/Keys', 'keys Down Menu', 'check title ^Settings > Battery$', 'key Menu', 'shot battery-save',
        'check title ^Battery save$', 'check kind ^list$', r'check screen ^Off\nOn\n150 ms\nFast wake\n70 ms$',
        'check row ^<<battery_save>>', 'key Exit'], expect=[
        'title ^Settings > Battery$', 'row ^Battery save <<battery_save>>', r'nolog \bpower: battery save\b']),
    Step(16, 'Clock > Time zone: large zone, tried at once; Exit puts it back', do=[
        'at Battery/Battery save', 'keys Exit Down Menu Menu', 'shot time-zone', remember_in_use('zone_16'),
        'check title ^Clock > Time zone$', 'check kind ^spinner$', r'check place ^\d+/53$',
        'key Up', now(r'log \bui: time zone UTC'), 'mark', 'key Exit', 'wait 4'], expect=[
        'title ^Settings > Clock$', r'log \bui: time zone <<zone_16>>$', r'nolog \bsettings kept in the codeplug']),
    Step(17, 'Tools > Version: version and uptime; Menu does nothing', do=[
        'key Exit', 'key Exit', 'choose Tools', 'key Menu', 'key Up',
        r'check screen ^\d+\.\d+\.\d+\S*\nup \d+ min$', 'check hint ^Exit: back$', 'key Menu'], expect=[
        'title ^Menu > Tools$', 'row ^Version']),
    Step(18, '1 opens the log; held Exit: the key test in a debug build, then standby', do=[
        'at Tools/Version', 'key 1', 'check screen console: >', key_test_or_standby], expect=[standby]),
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
    Step('24a', 'Roaming Off', changes=True, timeout=8, do=[roaming_on_first, 'home', 'keys Menu 2 1', 'set Off'],
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
    Step(29, 'Digits open the rows at their places', do=[
        'home', 'keys Menu 1', 'check title ^Menu > Channel$', 'key 3', 'check title Power$',
        'check kind ^buttons$', 'key 9', 'check title Power$', 'keys Exit Exit Exit'], expect=[
        standby, r'nolog \bui: power\b']),
    Step(30, 'Up and Down do nothing on the standby screen', do=[
        'home', 'mark', 'keys Up Down'], expect=[
        standby, r'nolog \b(fm: receiving|ui: )', r'nolog \bdmr: ']),
    Step(31, 'An FM channel: Roaming > Status says DMR only', do=[
        channel('{fm}'), 'keys Menu 2', r'check screen ^Status\nDMR only$', 'key 2', 'shot roaming-fm'], expect=[
        'title ^Roaming$', r'screen ^In use\nDMR only$'], undo=['home']),
    Step(32, 'A roaming channel: repeaters by name, the one in use marked', radio_only='no repeaters heard',
         do=[channel('{roam}'), 'wait 10', 'keys Menu 2 2', 'shot roaming-names'], expect=[
             'title ^Roaming$', r'noscreen ^\d+[ *]\d{3}\.\d+', r'screen ^In use\n(Excellent|Good|Fair|Weak|Poor|Quiet|Lost|Search)$'],
         human='Look at the roaming status: the repeaters by name, a green pointer left of the one in use.'),
    Step(33, 'Help text Off: lists without the panel; On again', changes=True, timeout=8, do=[
        'home', 'keys Menu 4 3', 'set Off', 'check row ^Help text Off$', 'check noscreen ' + panel('These two lines'),
        'shot no-help', 'keys Exit 6 Up', r'check row ^Version \d+\.\d+', 'keys Exit 4 3', 'set On', 'home'], expect=[
        r'log \bui: menu help hidden', r'log \bui: menu help shown', r'log@8 \bsettings kept in the codeplug']),
]

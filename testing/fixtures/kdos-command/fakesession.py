#!/usr/bin/env python3
# The Plasma session's bus names, faked for the selftest of the kdos command:
# KWin's effect, scripting, virtual-desktop and night-light objects and the
# io.kdos.Crt1 the kdos_crt effect serves; one Konsole window with one tab;
# plasmashell's scripting; the notification server; KRunner; and kglobalaccel's
# kwin component. Every call is appended to $KDOS_FAKE_LOG as one line. The
# answers that a test varies come from the environment:
#
#   FAKE_SUPPORTED   isEffectSupported("kdos_crt"), 1 or 0 (default 1)
#   FAKE_LOAD        loadEffect("kdos_crt"), 1 or 0 (default 1)
#   FAKE_PROFILE     the tab's profile name (default "KDOS Bone")
#   FAKE_NO_KWIN     set: org.kde.KWin is not claimed at all
#
# The same process also serves timedate1, hostname1 and locale1, the names
# kdos-settingsd serves on the system bus; a test points
# DBUS_SYSTEM_BUS_ADDRESS at this bus to reach them. A zone that does not
# start with an upper-case letter is refused with InvalidArgs.
#
# It writes $KDOS_FAKE_READY once every name is owned.
import os, sys
import dbus, dbus.service, dbus.mainloop.glib
from gi.repository import GLib

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
LOG = os.environ.get('KDOS_FAKE_LOG', '/dev/null')

def log(*a):
    with open(LOG, 'a') as f:
        f.write(' '.join(str(x) for x in a) + '\n')

PROPS = 'org.freedesktop.DBus.Properties'

class Effects(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/Effects')
        self.loaded = set(['login', 'sessionquit'])
    @dbus.service.method('org.kde.kwin.Effects', in_signature='s', out_signature='b')
    def isEffectSupported(self, name):
        log('isEffectSupported', name)
        return os.environ.get('FAKE_SUPPORTED', '1') == '1' if name == 'kdos_crt' else True
    @dbus.service.method('org.kde.kwin.Effects', in_signature='s', out_signature='b')
    def isEffectLoaded(self, name):
        return name in self.loaded
    @dbus.service.method('org.kde.kwin.Effects', in_signature='s', out_signature='b')
    def loadEffect(self, name):
        log('loadEffect', name)
        if name == 'kdos_crt' and os.environ.get('FAKE_LOAD', '1') != '1':
            return False
        self.loaded.add(name)
        return True
    @dbus.service.method('org.kde.kwin.Effects', in_signature='s', out_signature='')
    def unloadEffect(self, name):
        log('unloadEffect', name)
        self.loaded.discard(name)
    @dbus.service.method('org.kde.kwin.Effects', in_signature='s', out_signature='')
    def reconfigureEffect(self, name):
        log('reconfigureEffect', name)

class KWin(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/KWin')
    @dbus.service.method('org.kde.KWin', in_signature='', out_signature='')
    def reconfigure(self):
        log('reconfigure')

class Scripting(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/Scripting')
    @dbus.service.method('org.kde.kwin.Scripting', in_signature='s', out_signature='b')
    def isScriptLoaded(self, name):
        return name == 'krohnkite' and os.environ.get('FAKE_KROHNKITE') == '1'

class Desktops(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/VirtualDesktopManager')
    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, prop):
        if prop == 'desktops':
            return dbus.Array([dbus.Struct((dbus.UInt32(0), 'id-1', 'Main'), signature='uss'),
                               dbus.Struct((dbus.UInt32(1), 'id-2', 'Code'), signature='uss'),
                               dbus.Struct((dbus.UInt32(2), 'id-3', 'Web'), signature='uss')],
                              signature='(uss)')
        raise dbus.exceptions.DBusException('no such property',
                                            name='org.freedesktop.DBus.Error.InvalidArgs')

class Night(dbus.service.Object):
    running = False
    def __init__(self):
        super().__init__(bus, '/org/kde/KWin/NightLight')
    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, prop):
        return dbus.Boolean(Night.running)

class Crt(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/io/kdos/Crt1')
    @dbus.service.method('io.kdos.Crt1', in_signature='', out_signature='')
    def Collapse(self): log('Crt1.Collapse')
    @dbus.service.method('io.kdos.Crt1', in_signature='', out_signature='')
    def Unfold(self): log('Crt1.Unfold')
    @dbus.service.method('io.kdos.Crt1', in_signature='', out_signature='')
    def Degauss(self): log('Crt1.Degauss')
    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, prop):
        return dbus.String('live')

class Session(dbus.service.Object):
    def __init__(self, path, profile):
        super().__init__(bus, path)
        self.p = profile
    @dbus.service.method('org.kde.konsole.Session', in_signature='', out_signature='s')
    def profile(self):
        return self.p
    @dbus.service.method('org.kde.konsole.Session', in_signature='s', out_signature='')
    def setProfile(self, name):
        log('setProfile', name)
        self.p = name

class Shell(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/PlasmaShell')
    @dbus.service.method('org.kde.PlasmaShell', in_signature='s', out_signature='s')
    def evaluateScript(self, js):
        log('evaluateScript', js)
        return ''

class Notifications(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/org/freedesktop/Notifications')
    @dbus.service.method('org.freedesktop.Notifications',
                         in_signature='susssasa{sv}i', out_signature='u')
    def Notify(self, app, rid, icon, summary, body, actions, hints, timeout):
        log('Notify', summary, '|', body)
        return dbus.UInt32(7)

class Runner(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/App')
    @dbus.service.method('org.kde.krunner.App', in_signature='ss', out_signature='')
    def querySingleRunner(self, runner, term):
        log('querySingleRunner', runner, term)
    @dbus.service.method('org.kde.krunner.App', in_signature='', out_signature='')
    def toggleDisplay(self):
        log('toggleDisplay')

class Accel(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/component/kwin')
    @dbus.service.method('org.kde.kglobalaccel.Component', in_signature='s', out_signature='')
    def invokeShortcut(self, name):
        log('invokeShortcut', name)
        if name == 'Toggle Night Color':
            Night.running = not Night.running

class Machine(dbus.service.Object):
    def __init__(self, path):
        super().__init__(bus, path)
        self.zone, self.host, self.locale, self.keymap = 'UTC', 'kdos', ['LANG=C.UTF-8'], 'us'
    @dbus.service.method('org.freedesktop.timedate1', in_signature='sb', out_signature='')
    def SetTimezone(self, zone, interactive):
        log('SetTimezone', zone, bool(interactive))
        if not zone[:1].isupper():
            raise dbus.exceptions.DBusException('Invalid or not installed time zone \'%s\'' % zone,
                                                name='org.freedesktop.DBus.Error.InvalidArgs')
        Machine.state['zone'] = zone
    @dbus.service.method('org.freedesktop.hostname1', in_signature='sb', out_signature='')
    def SetStaticHostname(self, name, interactive):
        log('SetStaticHostname', name, bool(interactive))
        Machine.state['host'] = name
    @dbus.service.method('org.freedesktop.locale1', in_signature='asb', out_signature='')
    def SetLocale(self, assignments, interactive):
        log('SetLocale', ','.join(assignments), bool(interactive))
        Machine.state['locale'] = list(assignments)
    @dbus.service.method('org.freedesktop.locale1', in_signature='ssbb', out_signature='')
    def SetVConsoleKeyboard(self, keymap, toggle, convert, interactive):
        log('SetVConsoleKeyboard', keymap, toggle, bool(convert), bool(interactive))
        Machine.state['keymap'] = keymap
    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, prop):
        st = Machine.state
        if prop == 'Timezone': return dbus.String(st['zone'])
        if prop == 'Hostname': return dbus.String(st['host'])
        if prop == 'Locale': return dbus.Array(st['locale'], signature='s')
        if prop == 'VConsoleKeymap': return dbus.String(st['keymap'])
        raise dbus.exceptions.DBusException('no such property',
                                            name='org.freedesktop.DBus.Error.InvalidArgs')

Machine.state = {'zone': 'UTC', 'host': 'kdos', 'locale': ['LANG=C.UTF-8'], 'keymap': 'us'}

keep = []
for name, path in (('org.freedesktop.timedate1', '/org/freedesktop/timedate1'),
                   ('org.freedesktop.hostname1', '/org/freedesktop/hostname1'),
                   ('org.freedesktop.locale1', '/org/freedesktop/locale1')):
    keep += [dbus.service.BusName(name, bus), Machine(path)]
if not os.environ.get('FAKE_NO_KWIN'):
    keep += [dbus.service.BusName('org.kde.KWin', bus), Effects(), KWin(),
             Scripting(), Desktops(), Night(), Crt()]
keep += [dbus.service.BusName('org.kde.konsole-1', bus),
         Session('/Sessions/1', os.environ.get('FAKE_PROFILE', 'KDOS Bone')),
         dbus.service.BusName('org.kde.plasmashell', bus), Shell(),
         dbus.service.BusName('org.freedesktop.Notifications', bus), Notifications(),
         dbus.service.BusName('org.kde.krunner', bus), Runner(),
         dbus.service.BusName('org.kde.kglobalaccel', bus), Accel()]
ready = os.environ.get('KDOS_FAKE_READY')
if ready:
    open(ready, 'w').close()
GLib.MainLoop().run()

#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   chordcheck — no chord KDOS ships is bound twice
# ---------------------------------
#
#   python3 testing/chordcheck.py [--root build/fs] [--xdg DIR]... [--list]
#   python3 testing/chordcheck.py --live [--live-dump FILE] [--xdg DIR]...
#
# Run from the repository root. A chord two owners want goes to whichever
# grabs it first, and the loser says nothing: KWin's shortcut filter runs
# before the input method's, so a chord kglobalaccel holds never reaches
# fcitx5, and kglobalaccel refuses a key one component already holds without
# telling the component that asked. Every chord KDOS adds or rebinds is
# therefore held against every other binding the session makes.
#
# THE STATIC RUN (the default) builds the session's bindings from files:
#
#   KDE's compiled-in defaults   REFERENCE below, measured from the Plasma 6.8.2
#                                and KF 6.31 sources; a component's own entry
#                                in a kglobalshortcutsrc replaces its row.
#   desktop entries              X-KDE-Shortcuts of the entries the image
#                                installs (--root) or KDOS ships (fs/, src/),
#                                for the entry and each of its actions.
#   KWin scripts and effects     registerShortcut() calls in a script's
#                                JavaScript and ShortcutHandler blocks (a
#                                `name:` and a `sequence:` string literal) in
#                                a declarative script's or effect's QML,
#                                under share/kwin/scripts and
#                                share/kwin/effects; a package its metadata
#                                leaves disabled binds nothing until enabled,
#                                and its collisions are warnings.
#   kglobalshortcutsrc           the shipped file in each XDG config dir, the
#                                earlier dir winning a key, /etc/xdg first.
#   fcitx5                       its compiled-in hotkeys (FCITX5 below), then
#                                the first fcitx5/config found in the XDG
#                                config dirs, which fcitx5 reads whole.
#
# A chord with two owners FAILS when one of them came from a KDOS file — the
# shipped kglobalshortcutsrc, a KDOS desktop entry or script, the shipped
# fcitx5 config. A collision between two upstream defaults is reported and
# does not fail: KDOS did not make it, and kglobalaccel resolves it the same
# way on every distribution.
#
# THE LIVE RUN (--live, in the rig's session) reads what kglobalaccel actually
# holds — org.kde.kglobalaccel /kglobalaccel allComponents, then each
# component's allShortcutInfos — through gdbus, adds fcitx5's effective
# hotkeys, and fails on any chord bound twice. --live-dump FILE reads a
# recorded `gdbus call` transcript instead (the fixtures do).
#
# Exit 0 clean, 1 a failing collision, 2 a file that does not parse or a usage
# error.

import argparse
import ast
import glob
import json
import os
import re
import shutil
import subprocess
import sys

# ── chords ───────────────────────────────────────────────────────────────

MOD_ORDER = ("Meta", "Ctrl", "Alt", "Shift", "Hyper")
MOD_ALIASES = {
    "meta": "Meta", "super": "Meta", "win": "Meta", "mod4": "Meta",
    "ctrl": "Ctrl", "control": "Ctrl",
    "alt": "Alt", "mod1": "Alt",
    "shift": "Shift",
    "hyper": "Hyper",
}

# X keysym and Qt portable names for one key, folded to the Qt portable name.
KEY_ALIASES = {
    "space": "Space", "return": "Return", "enter": "Enter", "kp_enter": "Enter",
    "tab": "Tab", "iso_left_tab": "Backtab", "backtab": "Backtab",
    "backspace": "Backspace", "esc": "Esc", "escape": "Esc",
    "ins": "Ins", "insert": "Ins", "del": "Del", "delete": "Del",
    "pause": "Pause", "print": "Print", "sysreq": "SysReq", "sys_req": "SysReq",
    "home": "Home", "end": "End", "left": "Left", "up": "Up", "right": "Right",
    "down": "Down", "pgup": "PgUp", "pageup": "PgUp", "page_up": "PgUp",
    "prior": "PgUp", "pgdown": "PgDown", "pagedown": "PgDown",
    "page_down": "PgDown", "next": "PgDown", "menu": "Menu",
    "slash": "/", "period": ".", "comma": ",", "minus": "-", "equal": "=",
    "plus": "+", "semicolon": ";", "apostrophe": "'", "grave": "`",
    "quoteleft": "`", "asciitilde": "~", "asterisk": "*", "backslash": "\\",
    "bracketleft": "[", "bracketright": "]", "zenkaku_hankaku": "Zenkaku Hankaku",
    "zenkaku hankaku": "Zenkaku Hankaku", "hangul": "Hangul",
    "hangul_hanja": "Hangul Hanja", "hangul hanja": "Hangul Hanja",
    "hangul_romaja": "Hangul Romaja", "hangul romaja": "Hangul Romaja",
}

# A key that is a modifier on its own, the way kwinrc's ModifierOnlyShortcuts
# and fcitx5's AltTriggerKeys name it.
MOD_ONLY = {
    "super_l": "Meta", "super_r": "Meta", "meta_l": "Meta", "meta_r": "Meta",
    "shift_l": "Shift", "shift_r": "Shift", "control_l": "Ctrl",
    "control_r": "Ctrl", "alt_l": "Alt", "alt_r": "Alt",
}


def fold_key(name):
    low = name.strip().lower()
    if low in KEY_ALIASES:
        return KEY_ALIASES[low]
    if re.fullmatch(r"f\d{1,2}", low):
        return low.upper()
    if len(name.strip()) == 1:
        return name.strip().upper()
    return name.strip()


def parse_chord(text):
    """One chord, "Meta+Shift+T" or "Super+space", as (mods, key). None for
    an empty or "none" chord. A lone modifier is ((), "<Mod> alone")."""
    s = text.strip()
    if not s or s.lower() == "none":
        return None
    if s.lower() in MOD_ONLY:
        return ((), MOD_ONLY[s.lower()] + " alone")
    if s.endswith("++"):
        parts, key = s[:-2].split("+") if s[:-2] else [], "+"
    elif s == "+":
        parts, key = [], "+"
    else:
        bits = s.split("+")
        parts, key = bits[:-1], bits[-1]
    mods = set()
    for p in parts:
        m = MOD_ALIASES.get(p.strip().lower())
        if m is None:
            raise ValueError("unknown modifier %r in %r" % (p, text))
        mods.add(m)
    if not key.strip():
        raise ValueError("no key in %r" % text)
    if key.strip().lower() in MOD_ONLY and not mods:
        return ((), MOD_ONLY[key.strip().lower()] + " alone")
    return (tuple(m for m in MOD_ORDER if m in mods), fold_key(key))


def chord_name(chord):
    mods, key = chord
    return "+".join(list(mods) + [key])


def parse_sequence(text):
    """A key sequence, chords separated by ", "; returns its canonical name."""
    chords = [parse_chord(c) for c in text.split(", ")]
    if any(c is None for c in chords):
        return None
    return ", ".join(chord_name(c) for c in chords)


def parse_keys(text, sep):
    """Every sequence in one shortcut field, `sep`-separated."""
    out = []
    for part in text.split(sep):
        name = parse_sequence(part)
        if name:
            out.append(name)
    return out


# Qt::Key values for the live run's integers. A key not here prints as hex;
# two owners of one integer still collide.
QT_MODS = ((0x10000000, "Meta"), (0x04000000, "Ctrl"), (0x08000000, "Alt"),
           (0x02000000, "Shift"))
QT_KEYS = {
    0x20: "Space", 0x01000000: "Esc", 0x01000001: "Tab", 0x01000002: "Backtab",
    0x01000003: "Backspace", 0x01000004: "Return", 0x01000005: "Enter",
    0x01000006: "Ins", 0x01000007: "Del", 0x01000008: "Pause",
    0x01000009: "Print", 0x0100000a: "SysReq", 0x01000010: "Home",
    0x01000011: "End", 0x01000012: "Left", 0x01000013: "Up",
    0x01000014: "Right", 0x01000015: "Down", 0x01000016: "PgUp",
    0x01000017: "PgDown", 0x01000055: "Menu", 0x01000070: "Volume Down",
    0x01000071: "Volume Mute", 0x01000072: "Volume Up",
    0x01000092: "Search", 0x010000b2: "Monitor Brightness Up",
    0x010000b3: "Monitor Brightness Down", 0x010000b7: "Power Off",
    0x010000ba: "Screensaver", 0x010000d1: "Display", 0x010000fe: "Battery",
    0x01000108: "Hibernate", 0x0100010b: "Power Down", 0x01000110: "Touchpad Toggle",
    0x01000111: "Touchpad On", 0x01000112: "Touchpad Off", 0x01000113: "Microphone Mute",
    0x0100111d: "Microphone Volume Up", 0x0100111e: "Microphone Volume Down",
    0x0100112a: "Zenkaku Hankaku", 0x01001131: "Hangul",
    0x01001134: "Hangul Hanja", 0x01001136: "Hangul Romaja",
    0x01020004: "Sleep",
}


def qt_chord(code):
    if not code:
        return None
    mods = tuple(n for bit, n in QT_MODS if code & bit)
    key = code & 0x01FFFFFF
    if 0x01000030 <= key <= 0x01000052:
        name = "F%d" % (key - 0x01000030 + 1)
    elif key in QT_KEYS:
        name = QT_KEYS[key]
    elif 0x21 <= key <= 0x7e:
        name = chr(key).upper()
    else:
        name = "Key(0x%08x)" % key
    return chord_name((tuple(m for m in MOD_ORDER if m in mods), name))


# ── what KDE binds before KDOS says anything ─────────────────────────────
#
# Measured from the Plasma 6.8.2 and KF 6.31 tarballs: each row is a binding a
# component registers at run time with no configuration, and its source line.
# Component and action are kglobalaccel's names, so a kglobalshortcutsrc entry
# for the same pair replaces the row. A desktop entry's row is replaced by the
# entry itself when --root holds it. Re-measure on every Plasma bump: a row
# that moved is a collision this run cannot see.
REFERENCE = [
    # kwin src/useractions.cpp
    ("kwin", "Window Operations Menu", "Alt+F3", "kwin useractions.cpp:827"),
    ("kwin", "Window Close", "Alt+F4", "kwin useractions.cpp:829"),
    ("kwin", "Window Maximize", "Meta+PgUp", "kwin useractions.cpp:831"),
    ("kwin", "Window Restore", "Meta+Backspace", "kwin useractions.cpp:837"),
    ("kwin", "Window Minimize", "Meta+PgDown", "kwin useractions.cpp:839"),
    ("kwin", "Activate Window Demanding Attention", "Meta+Ctrl+A", "kwin useractions.cpp:861"),
    ("kwin", "Window Quick Tile Left", "Meta+Left", "kwin useractions.cpp:891"),
    ("kwin", "Window Quick Tile Right", "Meta+Right", "kwin useractions.cpp:893"),
    ("kwin", "Window Quick Tile Top", "Meta+Up", "kwin useractions.cpp:895"),
    ("kwin", "Window Quick Tile Bottom", "Meta+Down", "kwin useractions.cpp:897"),
    ("kwin", "Switch Window Up", "Meta+Alt+Up", "kwin useractions.cpp:907"),
    ("kwin", "Switch Window Down", "Meta+Alt+Down", "kwin useractions.cpp:909"),
    ("kwin", "Switch Window Right", "Meta+Alt+Right", "kwin useractions.cpp:913"),
    ("kwin", "Switch Window Left", "Meta+Alt+Left", "kwin useractions.cpp:915"),
    ("kwin", "Window One Desktop to the Right", "Meta+Ctrl+Shift+Right", "kwin useractions.cpp:951"),
    ("kwin", "Window One Desktop to the Left", "Meta+Ctrl+Shift+Left", "kwin useractions.cpp:953"),
    ("kwin", "Window One Desktop Up", "Meta+Ctrl+Shift+Up", "kwin useractions.cpp:957"),
    ("kwin", "Window One Desktop Down", "Meta+Ctrl+Shift+Down", "kwin useractions.cpp:959"),
    ("kwin", "Window to Next Screen", "Meta+Shift+Right", "kwin useractions.cpp:971"),
    ("kwin", "Window to Previous Screen", "Meta+Shift+Left", "kwin useractions.cpp:973"),
    ("kwin", "Show Desktop", "Meta+D", "kwin useractions.cpp:1013"),
    ("kwin", "Kill Window", "Meta+Ctrl+Esc", "kwin useractions.cpp:1015"),
    ("kwin", "MoveMouseToFocus", "Meta+F5", "kwin useractions.cpp:1017"),
    ("kwin", "MoveMouseToCenter", "Meta+F6", "kwin useractions.cpp:1018"),
    # kwin src/virtualdesktops.cpp
    ("kwin", "Switch One Desktop to the Right", "Meta+Ctrl+Right", "kwin virtualdesktops.cpp:775"),
    ("kwin", "Switch One Desktop to the Left", "Meta+Ctrl+Left", "kwin virtualdesktops.cpp:776"),
    ("kwin", "Switch One Desktop Up", "Meta+Ctrl+Up", "kwin virtualdesktops.cpp:777"),
    ("kwin", "Switch One Desktop Down", "Meta+Ctrl+Down", "kwin virtualdesktops.cpp:778"),
    ("kwin", "Switch to Desktop 1", "Meta+F1\tCtrl+F1", "kwin virtualdesktops.cpp:884"),
    ("kwin", "Switch to Desktop 2", "Meta+F2\tCtrl+F2", "kwin virtualdesktops.cpp:885"),
    ("kwin", "Switch to Desktop 3", "Meta+F3\tCtrl+F3", "kwin virtualdesktops.cpp:886"),
    ("kwin", "Switch to Desktop 4", "Meta+F4\tCtrl+F4", "kwin virtualdesktops.cpp:887"),
    # kwin src/keyboard_layout.cpp
    ("KDE Keyboard Layout Switcher", "Switch to Next Keyboard Layout", "Meta+Alt+K", "kwin keyboard_layout.cpp:47"),
    ("KDE Keyboard Layout Switcher", "Switch to Last-Used Keyboard Layout", "Meta+Alt+L", "kwin keyboard_layout.cpp:55"),
    # kwin src/kcms/tabbox/shortcutsettings.cpp
    ("kwin", "Walk Through Windows", "Meta+Tab\tAlt+Tab", "kwin tabbox/shortcutsettings.cpp:131"),
    ("kwin", "Walk Through Windows (Reverse)", "Meta+Shift+Tab\tAlt+Shift+Tab", "kwin tabbox/shortcutsettings.cpp:132"),
    ("kwin", "Walk Through Windows of Current Application", "Meta+`\tAlt+`", "kwin tabbox/shortcutsettings.cpp:133"),
    ("kwin", "Walk Through Windows of Current Application (Reverse)", "Meta+~\tAlt+~", "kwin tabbox/shortcutsettings.cpp:134"),
    # kwin effects enabled by default, and the default scripts
    ("kwin", "Overview", "Meta+W", "kwin overvieweffect.cpp:154"),
    ("kwin", "Grid View", "Meta+G", "kwin overvieweffect.cpp:160"),
    ("kwin", "Edit Tiles", "Meta+T", "kwin tileseditoreffect.cpp:33"),
    ("kwin", "Expose", "Meta+F9\tCtrl+F9", "kwin windowvieweffect.cpp:50"),
    ("kwin", "ExposeAll", "Meta+F10\tCtrl+F10", "kwin windowvieweffect.cpp:59"),
    ("kwin", "ExposeClass", "Meta+F7\tCtrl+F7", "kwin windowvieweffect.cpp:68"),
    ("kwin", "view_zoom_in", "Meta++\tMeta+=", "kwin zoom.cpp:56"),
    ("kwin", "view_zoom_out", "Meta+-", "kwin zoom.cpp:60"),
    ("kwin", "view_actual_size", "Meta+0", "kwin zoom.cpp:66"),
    ("kwin", "Invert", "Meta+Ctrl+I", "kwin invert.cpp:45"),
    ("kwin", "InvertWindow", "Meta+Ctrl+U", "kwin invert.cpp:52"),
    ("kwin", "ClearMouseMarks", "Meta+Shift+F11", "kwin mousemark.cpp:37"),
    ("kwin", "ClearLastMouseMark", "Meta+Shift+F12", "kwin mousemark.cpp:42"),
    ("kwin", "ThumbnailAside", "Meta+Ctrl+T", "kwin thumbnailaside.cpp:32"),
    ("kwin", "ToggleMouseClick", "Meta+*", "kwin mouseclick.cpp:41"),
    ("kwin", "MinimizeAll", "Meta+Shift+D", "kwin minimizeall/contents/code/main.js:112"),
    ("kwin", "MinimizeAllOthers", "Meta+Shift+O", "kwin minimizeall/contents/code/main.js:114"),
    # plasma-workspace
    ("plasmashell", "show dashboard", "Ctrl+F12", "plasma-workspace shell/shellcorona.cpp:184"),
    ("plasmashell", "manage activities", "Meta+Q", "plasma-workspace shell/shellcorona.cpp:204"),
    ("plasmashell", "cycle-panels", "Meta+Alt+P", "plasma-workspace shell/shellcorona.cpp:271"),
    ("plasmashell", "activate application launcher", "Meta alone", "kwinrc [ModifierOnlyShortcuts] Meta"),
    ("plasmashell", "show-on-mouse-pos", "Meta+V", "plasma-workspace klipper/klipper.cpp:137"),
    ("ksmserver", "Log Out", "Ctrl+Alt+Del", "plasma-workspace startkde/session-shortcuts/main.cpp:40"),
    ("services/org.kde.krunner.desktop", "_launch", "Alt+Space\tAlt+F2\tSearch", "plasma-workspace krunner/org.kde.krunner.desktop.cmake:71"),
    ("services/org.kde.krunner.desktop", "RunClipboard", "Alt+Shift+F2", "plasma-workspace krunner/org.kde.krunner.desktop.cmake:135"),
    # plasma-desktop
    ("plasmashell", "activate task manager entry 1", "Meta+1", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 2", "Meta+2", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 3", "Meta+3", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 4", "Meta+4", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 5", "Meta+5", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 6", "Meta+6", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 7", "Meta+7", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 8", "Meta+8", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate task manager entry 9", "Meta+9", "plasma-desktop applets/taskmanager/backend.cpp:613"),
    ("plasmashell", "activate previous task", "Meta+Ctrl+PgUp", "plasma-desktop applets/taskmanager/backend.cpp:582"),
    ("plasmashell", "activate next task", "Meta+Ctrl+PgDown", "plasma-desktop applets/taskmanager/backend.cpp:589"),
    ("plasmashell", "activate previous task in group", "Meta+Ctrl+Shift+PgUp", "plasma-desktop applets/taskmanager/backend.cpp:597"),
    ("plasmashell", "activate next task in group", "Meta+Ctrl+Shift+PgDown", "plasma-desktop applets/taskmanager/backend.cpp:605"),
    ("plasmashell", "next activity", "Meta+A", "plasma-desktop imports/activitymanager/switcherbackend.cpp:179"),
    ("plasmashell", "previous activity", "Meta+Shift+A", "plasma-desktop imports/activitymanager/switcherbackend.cpp:184"),
    ("services/org.kde.plasma.emojier.desktop", "_launch", "Meta+.\tMeta+Ctrl+Alt+Shift+Space", "plasma-desktop emojier/app/org.kde.plasma.emojier.desktop:160"),
    ("services/org.kde.screenreader.desktop", "_launch", "Meta+Alt+S", "plasma-desktop kded/screenreader/org.kde.screenreader.desktop:140"),
    # kscreenlocker, kscreen, powerdevil, plasma-pa, plasma-systemmonitor
    ("ksmserver", "Lock Session", "Meta+L\tScreensaver", "kscreenlocker settings/kscreensaversettings.cpp:20"),
    ("services/org.kde.kscreen.desktop", "_launch", "Display\tMeta+P", "kscreen osd/org.kde.kscreen.desktop:105"),
    ("org_kde_powerdevil", "Increase Screen Brightness", "Monitor Brightness Up", "powerdevil powerdevilscreenbrightnessagent.cpp:143"),
    ("org_kde_powerdevil", "Increase Screen Brightness Small", "Shift+Monitor Brightness Up", "powerdevil powerdevilscreenbrightnessagent.cpp:150"),
    ("org_kde_powerdevil", "Decrease Screen Brightness", "Monitor Brightness Down", "powerdevil powerdevilscreenbrightnessagent.cpp:157"),
    ("org_kde_powerdevil", "Decrease Screen Brightness Small", "Shift+Monitor Brightness Down", "powerdevil powerdevilscreenbrightnessagent.cpp:164"),
    ("org_kde_powerdevil", "powerProfile", "Battery\tMeta+B", "powerdevil actions/bundled/powerprofile.cpp:71"),
    ("org_kde_powerdevil", "Sleep", "Sleep", "powerdevil actions/bundled/handlebuttonevents.cpp:52"),
    ("org_kde_powerdevil", "Hibernate", "Hibernate", "powerdevil actions/bundled/handlebuttonevents.cpp:57"),
    ("org_kde_powerdevil", "PowerOff", "Power Off", "powerdevil actions/bundled/handlebuttonevents.cpp:64"),
    ("org_kde_powerdevil", "PowerDown", "Power Down", "powerdevil actions/bundled/handlebuttonevents.cpp:76"),
    ("kmix", "increase_volume", "Volume Up", "plasma-pa kded/audioshortcutsservice.cpp:85"),
    ("kmix", "decrease_volume", "Volume Down", "plasma-pa kded/audioshortcutsservice.cpp:100"),
    ("kmix", "increase_volume_small", "Shift+Volume Up", "plasma-pa kded/audioshortcutsservice.cpp:115"),
    ("kmix", "decrease_volume_small", "Shift+Volume Down", "plasma-pa kded/audioshortcutsservice.cpp:130"),
    ("kmix", "increase_microphone_volume", "Microphone Volume Up", "plasma-pa kded/audioshortcutsservice.cpp:145"),
    ("kmix", "decrease_microphone_volume", "Microphone Volume Down", "plasma-pa kded/audioshortcutsservice.cpp:159"),
    ("kmix", "mute", "Volume Mute", "plasma-pa kded/audioshortcutsservice.cpp:173"),
    ("kmix", "mic_mute", "Microphone Mute\tMeta+Volume Mute", "plasma-pa kded/audioshortcutsservice.cpp:186"),
    ("services/org.kde.plasma-systemmonitor.desktop", "_launch", "Meta+Esc", "plasma-systemmonitor org.kde.plasma-systemmonitor.desktop:209"),
    # spectacle
    ("services/org.kde.spectacle.desktop", "_launch", "Print\tMeta+Shift+S", "spectacle desktop/org.kde.spectacle.desktop.cmake:213"),
    ("services/org.kde.spectacle.desktop", "FullScreenScreenShot", "Shift+Print", "spectacle desktop/org.kde.spectacle.desktop.cmake:274"),
    ("services/org.kde.spectacle.desktop", "ActiveWindowScreenShot", "Meta+Print", "spectacle desktop/org.kde.spectacle.desktop.cmake:392"),
    ("services/org.kde.spectacle.desktop", "RectangularRegionScreenShot", "Meta+Shift+Print", "spectacle desktop/org.kde.spectacle.desktop.cmake:450"),
    ("services/org.kde.spectacle.desktop", "WindowUnderCursorScreenShot", "Meta+Ctrl+Print", "spectacle desktop/org.kde.spectacle.desktop.cmake:507"),
    ("services/org.kde.spectacle.desktop", "RecordRegion", "Meta+Shift+R\tMeta+R", "spectacle desktop/org.kde.spectacle.desktop.cmake:554"),
    ("services/org.kde.spectacle.desktop", "RecordScreen", "Meta+Alt+R", "spectacle desktop/org.kde.spectacle.desktop.cmake:601"),
    ("services/org.kde.spectacle.desktop", "RecordWindow", "Meta+Ctrl+R", "spectacle desktop/org.kde.spectacle.desktop.cmake:648"),
    # Gear applications the image installs and the session starts
    ("services/org.kde.konsole.desktop", "_launch", "Ctrl+Alt+T", "konsole desktop/org.kde.konsole.desktop:11"),
    ("services/org.kde.dolphin.desktop", "_launch", "Meta+E", "dolphin org.kde.dolphin.desktop X-KDE-Shortcuts"),
    ("yakuake", "toggle-window-state", "F12", "yakuake app/mainwindow.cpp:250"),
]

# fcitx5 5.1.22 src/lib/fcitx/globalconfig.cpp, [Hotkey/*]. The candidate-list
# keys (PrevPage, NextPage, PrevCandidate, NextCandidate) act only while a
# candidate window is open and are not global, so they are not here.
FCITX5 = {
    "TriggerKeys": ["Control+space", "Zenkaku_Hankaku", "Hangul"],
    "ActivateKeys": ["Hangul_Hanja"],
    "DeactivateKeys": ["Hangul_Romaja"],
    "AltTriggerKeys": ["Shift_L"],
    "EnumerateForwardKeys": [],
    "EnumerateBackwardKeys": [],
    "EnumerateGroupForwardKeys": ["Super+space"],
    "EnumerateGroupBackwardKeys": ["Super+Shift+space"],
    "TogglePreedit": ["Control+Alt+P"],
}
FCITX5_SOURCE = "fcitx5 5.1.22 globalconfig.cpp"


class Bindings:
    """(component, action) -> (chords, source, kdos, active)."""

    def __init__(self):
        self.rows = {}

    def put(self, comp, action, chords, source, kdos, active=True):
        self.rows[(comp, action)] = (list(chords), source, kdos, active)

    def collisions(self):
        owners = {}
        for (comp, action), (chords, source, kdos, active) in self.rows.items():
            for c in chords:
                owners.setdefault(c, []).append((comp, action, source, kdos, active))
        return {c: o for c, o in owners.items() if len(o) > 1}


# ── reading the files ────────────────────────────────────────────────────

def kconfig_groups(path):
    """A KConfig file as [(group, [(key, value, lineno)])]. A group header is
    one or more [..] parts; "[services][x.desktop]" is the group
    "services/x.desktop". Raises ValueError on a line that is neither."""
    groups = []
    cur = None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.rstrip("\n")
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("["):
                if not re.fullmatch(r"(\[[^\]]*\])+", s):
                    raise ValueError("%s:%d: malformed group header %r" % (path, n, s))
                # [$i] and friends are flags on the group, not part of its name.
                parts = [p for p in re.findall(r"\[([^\]]*)\]", s) if not p.startswith("$")]
                cur = ("/".join(parts), [])
                groups.append(cur)
                continue
            if "=" not in s:
                raise ValueError("%s:%d: not a key=value line: %r" % (path, n, s))
            if cur is None:
                cur = ("<default>", [])
                groups.append(cur)
            k, v = s.split("=", 1)
            k = re.sub(r"\[[^\]]*\]$", "", k.strip())
            cur[1].append((k, v.strip(), n))
    return groups


def kconfig_unescape(v):
    return v.replace("\\t", "\t").replace("\\s", " ").replace("\\n", "\n").replace("\\\\", "\\")


def kconfig_list(v):
    """A KConfig string list: comma-separated, `\\,` a literal comma."""
    out, cur, i = [], "", 0
    while i < len(v):
        if v[i] == "\\" and i + 1 < len(v) and v[i + 1] == ",":
            cur += ","
            i += 2
            continue
        if v[i] == ",":
            out.append(cur)
            cur = ""
        else:
            cur += v[i]
        i += 1
    out.append(cur)
    return out


def read_kglobalshortcutsrc(path, b, kdos=True):
    """Every entry of one kglobalshortcutsrc. A component entry is the triple
    current,default,friendly — kglobalacceld skips any other shape, so one is
    an error here; a services entry is the keys alone."""
    errors = []
    for group, entries in kconfig_groups(path):
        if group in ("", "<default>", "General", "Data", "services"):
            continue
        for key, val, n in entries:
            if key.startswith("_k_friendly_name"):
                continue
            try:
                if group.startswith("services/"):
                    chords = parse_keys(kconfig_unescape(val), "\t")
                else:
                    triple = kconfig_list(val)
                    if len(triple) != 3:
                        errors.append("%s:%d: [%s] %s is not current,default,friendly"
                                      " — kglobalacceld ignores it" % (path, n, group, key))
                        continue
                    chords = parse_keys(kconfig_unescape(triple[0]), "\t")
            except ValueError as e:
                errors.append("%s:%d: %s" % (path, n, e))
                continue
            b.put(group, key, chords, path, kdos)
    return errors


def desktop_groups(path):
    groups, cur = {}, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            s = raw.strip()
            if s.startswith("[") and s.endswith("]"):
                cur = s[1:-1]
                groups.setdefault(cur, {})
            elif cur and "=" in s and not s.startswith("#"):
                k, v = s.split("=", 1)
                groups[cur].setdefault(k.strip(), v.strip())
    return groups


def desktop_id(path):
    base = os.path.basename(path)
    for suffix in (".cmake", ".in"):
        if base.endswith(suffix):
            base = base[: -len(suffix)]
    return base


def read_desktop(path, b, kdos):
    """X-KDE-Shortcuts of an entry and each of its actions. kglobalacceld
    reads the value with every comma turned into a tab."""
    errors = []
    g = desktop_groups(path)
    did = desktop_id(path)
    comp = "services/" + did
    for group, keys in g.items():
        if "X-KDE-Shortcuts" not in keys:
            continue
        action = "_launch" if group == "Desktop Entry" else \
            group[len("Desktop Action "):] if group.startswith("Desktop Action ") else None
        if action is None:
            continue
        try:
            chords = parse_keys(kconfig_unescape(keys["X-KDE-Shortcuts"]).replace(",", "\t"), "\t")
        except ValueError as e:
            errors.append("%s: [%s] X-KDE-Shortcuts: %s" % (path, group, e))
            continue
        b.put(comp, action, chords, path, kdos)
    return errors


SCRIPT_SHORTCUT = re.compile(
    r"""registerShortcut\s*\(\s*(["'])(.*?)\1\s*,\s*(["'])(.*?)\3\s*,\s*(["'])(.*?)\5""")


QML_HANDLER = re.compile(r"""(?:\b[A-Za-z_][A-Za-z0-9_]*\.)?\bShortcutHandler\s*\{""")
QML_PROP = r"""(?m)^\s*%s\s*:\s*(["'])(.*?)\1\s*;?\s*$"""


def qml_handlers(text):
    """(name, sequence) of each ShortcutHandler block whose two properties
    are string literals, the block taken to its matching brace."""
    out = []
    for m in QML_HANDLER.finditer(text):
        depth, i, quote = 1, m.end(), None
        while i < len(text) and depth:
            c = text[i]
            if quote:
                if c == "\\":
                    i += 1
                elif c == quote:
                    quote = None
            elif c in "\"'":
                quote = c
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            i += 1
        body = text[m.end():i - 1]
        name = re.search(QML_PROP % "name", body)
        seq = re.search(QML_PROP % "sequence", body)
        if name and seq:
            out.append((name.group(2), seq.group(2)))
    return out


def read_kwin_script(pkgdir, b, kdos):
    """registerShortcut(id, text, keys, fn) in a KWin script package, and
    ShortcutHandler blocks in a declarative script's or effect's QML."""
    errors = []
    meta = os.path.join(pkgdir, "metadata.json")
    enabled, sid = False, os.path.basename(pkgdir)
    if os.path.isfile(meta):
        try:
            with open(meta, encoding="utf-8") as f:
                kp = json.load(f).get("KPlugin", {})
            enabled = bool(kp.get("EnabledByDefault", False))
            sid = kp.get("Id", sid) or sid
        except (OSError, ValueError) as e:
            errors.append("%s: %s" % (meta, e))
    for js in sorted(glob.glob(os.path.join(pkgdir, "contents", "code", "*.js"))):
        with open(js, encoding="utf-8", errors="replace") as f:
            text = f.read()
        for m in SCRIPT_SHORTCUT.finditer(text):
            try:
                chords = parse_keys(m.group(6), "\t")
            except ValueError as e:
                errors.append("%s: %s" % (js, e))
                continue
            b.put("kwin", m.group(2), chords, "%s (script %s%s)" %
                  (js, sid, "" if enabled else ", disabled"), kdos, active=enabled)
    for qml in sorted(glob.glob(os.path.join(pkgdir, "contents", "ui", "*.qml"))):
        with open(qml, encoding="utf-8", errors="replace") as f:
            text = f.read()
        for name, seq in qml_handlers(text):
            try:
                chords = parse_keys(seq, "\t")
            except ValueError as e:
                errors.append("%s: %s" % (qml, e))
                continue
            b.put("kwin", name, chords, "%s (package %s%s)" %
                  (qml, sid, "" if enabled else ", disabled"), kdos, active=enabled)
    return errors


def read_fcitx5(path, b, kdos):
    """fcitx5's hotkeys: the compiled-in lists, each replaced whole by its
    [Hotkey/<Name>] group when the file has one — a group with no entries is
    an empty list, which is how a hotkey is switched off."""
    lists = {k: (list(v), FCITX5_SOURCE, False) for k, v in FCITX5.items()}
    errors = []
    if path:
        for group, entries in kconfig_groups(path):
            if not group.startswith("Hotkey/"):
                continue
            name = group[len("Hotkey/"):]
            if name not in lists:
                continue
            vals = [v for k, v, _ in sorted(entries, key=lambda e: int(e[0]) if e[0].isdigit() else 0)
                    if k.isdigit()]
            lists[name] = (vals, path, kdos)
    for name, (vals, source, from_kdos) in lists.items():
        chords = []
        for v in vals:
            try:
                c = parse_sequence(v)
            except ValueError as e:
                errors.append("%s: [Hotkey/%s] %s" % (source, name, e))
                continue
            if c:
                chords.append(c)
        b.put("fcitx5", name, chords, source, from_kdos)
    return errors


# ── where the files are ──────────────────────────────────────────────────

def session_xdg_dirs():
    """The running session's config dirs, the user's own first: fcitx5 reads
    the first fcitx5/config it finds along them."""
    home = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    dirs = os.environ.get("XDG_CONFIG_DIRS") or "/etc/xdg"
    return [d for d in [home] + dirs.split(":") if d and os.path.isdir(d)]


def tree_xdg_dirs(root):
    """The XDG config dirs the session reads, in XDG_CONFIG_DIRS order."""
    if root:
        return [d for d in (os.path.join(root, "etc/xdg"),
                            os.path.join(root, "usr/share/kdos/xdg")) if os.path.isdir(d)]
    dirs = [d for d in ("fs/etc/xdg", "fs/usr/share/kdos/xdg") if os.path.isdir(d)]
    for d in sorted(glob.glob("src/*/*/**/xdg", recursive=True)):
        if os.path.isdir(d) and d not in dirs:
            dirs.append(d)
    return dirs


def tree_desktop_files(root):
    if root:
        pats = ["usr/share/applications/**/*.desktop", "usr/share/kglobalaccel/*.desktop"]
        return [(p, False) for pat in pats
                for p in sorted(glob.glob(os.path.join(root, pat), recursive=True))]
    out = [(p, True) for p in sorted(glob.glob("fs/usr/share/applications/**/*.desktop", recursive=True))]
    for pat in ("src/*/*/**/*.desktop", "src/*/*/**/*.desktop.in", "src/*/*/**/*.desktop.cmake"):
        out += [(p, True) for p in sorted(glob.glob(pat, recursive=True))]
    return out


def kdos_owned(path, root):
    """Whether an installed file is one KDOS ships: fs/ has it, or a port of
    ours installs it (its database manifest is a src/ port's)."""
    if not root:
        return True
    rel = os.path.relpath(path, root)
    if os.path.exists(os.path.join("fs", rel)):
        return True
    return rel in KDOS_PATHS


KDOS_PATHS = set()


def load_kdos_paths(root):
    db = os.path.join(root, "var/lib/kpkg/db")
    ours = set()
    for kp in glob.glob("src/*/*/kpkgbuild"):
        with open(kp, encoding="utf-8") as f:
            for line in f:
                m = re.match(r"name\s*=\s*(\S+)", line)
                if m:
                    ours.add(m.group(1))
                    break
    for name in ours:
        p = os.path.join(db, name)
        if not os.path.isfile(p):
            continue
        with open(p, encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f):
                if i:
                    KDOS_PATHS.add(line.strip().lstrip("./").rstrip("/"))


def tree_script_dirs(root):
    if root:
        return [(d, kdos_owned(d, root)) for d in
                sorted(glob.glob(os.path.join(root, "usr/share/kwin/scripts/*")) +
                       glob.glob(os.path.join(root, "usr/share/kwin/effects/*"))) if os.path.isdir(d)]
    dirs = {os.path.dirname(os.path.dirname(os.path.dirname(p))) for p in
            glob.glob("src/*/*/**/contents/code/*.js", recursive=True) +
            glob.glob("src/*/*/**/contents/ui/*.qml", recursive=True)}
    return [(d, True) for d in sorted(dirs) if kwin_package(d)]


def kwin_package(d):
    """A KWin script or effect package: its metadata names the structure,
    and a JavaScript script's own contents/code is one by its path."""
    meta = os.path.join(d, "metadata.json")
    try:
        with open(meta, encoding="utf-8") as f:
            st = json.load(f).get("KPackageStructure", "")
        return st in ("KWin/Script", "KWin/Effect")
    except (OSError, ValueError):
        return bool(glob.glob(os.path.join(d, "contents", "code", "*.js")))


# ── the live session ─────────────────────────────────────────────────────

GV_TYPES = re.compile(r"@[a-z(){}]+ |\b(?:objectpath|signature|u?int(?:16|32|64)|byte|double|boolean) ")


def gvariant(text):
    """A `gdbus call` reply as Python values; gdbus prints GVariant text,
    which is a Python literal once its type annotations are dropped."""
    return ast.literal_eval(GV_TYPES.sub("", text.strip()))


def gdbus(path, method):
    out = subprocess.run(
        ["gdbus", "call", "--session", "--dest", "org.kde.kglobalaccel",
         "--object-path", path, "--method", method],
        capture_output=True, text=True, timeout=20)
    if out.returncode:
        raise RuntimeError("gdbus %s %s: %s" % (path, method, out.stderr.strip()))
    return out.stdout


def live_dump_sections(path):
    """A recorded transcript: `== <object path> <method>` then the reply."""
    sections, cur = {}, None
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("== "):
                cur = tuple(line[3:].split())
                sections[cur] = ""
            elif cur:
                sections[cur] += line
    return sections


def read_live(b, dump):
    calls = live_dump_sections(dump) if dump else None

    def call(path, method):
        if calls is not None:
            return calls.get((path, method), "([],)")
        return gdbus(path, method)

    comps = gvariant(call("/kglobalaccel", "org.kde.KGlobalAccel.allComponents"))[0]
    for cpath in comps:
        infos = gvariant(call(cpath, "org.kde.kglobalaccel.Component.allShortcutInfos"))[0]
        for info in infos:
            unique, _friendly, comp, _cfriendly, ctx = info[0], info[1], info[2], info[3], info[4]
            chords = [c for c in (qt_chord(k) for k in info[6]) if c]
            action = unique if ctx in ("default", "") else "%s@%s" % (unique, ctx)
            b.put(comp, action, chords, "kglobalaccel %s" % cpath, True)


# ── the run ──────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="no chord KDOS ships is bound twice")
    ap.add_argument("--root", help="a built tree: its installed entries, scripts and XDG files")
    ap.add_argument("--xdg", action="append", default=[],
                    help="an XDG config dir to read instead of the tree's (repeatable, first wins)")
    ap.add_argument("--desktop", action="append", default=[],
                    help="a KDOS desktop entry to read instead of the tree's (repeatable)")
    ap.add_argument("--no-reference", action="store_true",
                    help="leave KDE's compiled-in defaults out")
    ap.add_argument("--live", action="store_true", help="read the running session's kglobalaccel")
    ap.add_argument("--live-dump", help="a recorded gdbus transcript in place of the session")
    ap.add_argument("--list", action="store_true", help="print every binding read")
    ap.add_argument("--quiet", action="store_true", help="print only failures")
    a = ap.parse_args()

    if a.root and not os.path.isdir(a.root):
        print("chordcheck: no tree at %s" % a.root, file=sys.stderr)
        return 2
    if a.live and not a.live_dump and not shutil.which("gdbus"):
        print("chordcheck: --live needs gdbus and a session bus", file=sys.stderr)
        return 2
    if a.root:
        load_kdos_paths(a.root)

    b = Bindings()
    errors = []
    live = a.live or bool(a.live_dump)
    xdg = a.xdg or (session_xdg_dirs() if a.live and not a.live_dump else tree_xdg_dirs(a.root))

    if live:
        try:
            read_live(b, a.live_dump)
        except (RuntimeError, ValueError, SyntaxError, IndexError, OSError,
                subprocess.TimeoutExpired) as e:
            print("chordcheck: cannot read kglobalaccel: %s" % e, file=sys.stderr)
            return 2
    else:
        if not a.no_reference:
            for comp, action, keys, src in REFERENCE:
                b.put(comp, action, parse_keys(keys, "\t"), src, False)
        desktops = [(p, True) for p in a.desktop] if a.desktop else \
            ([] if a.xdg else tree_desktop_files(a.root))
        # An entry the tree installs is the authority for its own component:
        # a reference row the entry has since dropped would be a stale chord.
        if a.root:
            have = {"services/" + desktop_id(p) for p, _ in desktops}
            for key in [k for k in b.rows if k[0] in have]:
                del b.rows[key]
        for path, kdos in desktops:
            if a.root:
                kdos = kdos_owned(path, a.root)
            errors += read_desktop(path, b, kdos)
        if not a.xdg:
            for d, kdos in tree_script_dirs(a.root):
                errors += read_kwin_script(d, b, kdos)
        # The cascade: a later dir's key is read first and an earlier dir's
        # replaces it, which is KConfig's order for XDG_CONFIG_DIRS.
        for d in reversed(xdg):
            p = os.path.join(d, "kglobalshortcutsrc")
            if os.path.isfile(p):
                try:
                    errors += read_kglobalshortcutsrc(p, b, kdos=True)
                except ValueError as e:
                    errors.append(str(e))

    fc = next((os.path.join(d, "fcitx5", "config") for d in xdg
               if os.path.isfile(os.path.join(d, "fcitx5", "config"))), None)
    try:
        errors += read_fcitx5(fc, b, kdos=True)
    except ValueError as e:
        errors.append(str(e))

    if errors:
        for e in errors:
            print("chordcheck: %s" % e, file=sys.stderr)
        return 2

    if a.list:
        for (comp, action), (chords, src, kdos, active) in sorted(b.rows.items()):
            if chords:
                print("%-28s %-40s %-30s %s%s" % (comp[:28], action[:40], ", ".join(chords)[:30],
                                                  "kdos " if kdos else "", src))

    fails = warns = notes = 0
    for chord, owners in sorted(b.collisions().items()):
        active = [o for o in owners if o[4]]
        who = "; ".join("%s %s (%s)" % (o[0], o[1], o[2]) for o in owners)
        if len(active) < 2:
            if any(o[3] for o in owners):
                warns += 1
                if not a.quiet:
                    print("  warn  %s would be bound twice once a disabled script is on: %s" % (chord, who))
            continue
        if live or any(o[3] for o in active):
            fails += 1
            print("  FAIL  %s is bound twice: %s" % (chord, who))
        else:
            notes += 1
            if not a.quiet:
                print("  note  %s is bound twice upstream, not by KDOS: %s" % (chord, who))
    if not a.quiet:
        n = sum(len(v[0]) for v in b.rows.values())
        kd = sum(len(v[0]) for v in b.rows.values() if v[2])
        held = "from the live session and fcitx5" if live else "%d from KDOS files" % kd
        print("chordcheck: %d chords in %d bindings (%s); "
              "%d failing, %d upstream only, %d warnings" %
              (n, len(b.rows), held, fails, notes, warns))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())

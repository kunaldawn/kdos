## About

`kwin-script-kdos-window-memory` makes a program's window open where its last window closed, at the
same size and, when it closed maximized, maximized, and centres again a new window that KWin placed
while its size was a placeholder. On Wayland a program cannot place its own
window, so without it every window opens wherever KWin's placement puts it, at the size its program
picked.

It is a KWin script written in QML, a KWin/Script package installed to
`/usr/share/kwin/scripts/kdos_window_memory/`, on by default and turned off on System Settings'
KWin Scripts page. It records one place per program, by application id, in
`$XDG_STATE_HOME/kdos/window-memory`, most recent first and at most 200 programs.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests. The test is QML (`tests/tst_memory.qml.in`), run by
  `qmltestrunner`; `testing/plasmatest.sh` builds it in the development container, where Qt's test
  runner is.

## Notes

A window's maximised and full-screen state is read after the commit that carries it, through
`Qt.callLater` from `frameGeometryChanged`, `maximizedChanged` and `fullScreenChanged`: KWin emits
the new frame before it updates `maximizeMode` and `fullScreen`, so a sample taken in the signal
would record the maximised frame as the floating one and miss the rectangle an un-maximise returns
to. The record is the rectangle the window floats at.

What it leaves alone is the point of it: a window that opens maximized, full screen or tiled, whose
first restore is sent to the remembered rectangle because KWin has no floating geometry of its own
for it (a KXmlGui program reopens maximized from its own state file); a dialog, a panel, a window
that cannot be resized, or any window that is not a program's ordinary top-level window; a prelude;
a window that would land exactly on an open window of the same program; and the programs in its
exclusion list, which place their own windows or are prompts. A window rule that forces a position
wins, because KWin refuses the move; a rule that only sets the first position is overridden.

A Wayland client's window carries no type, so `splash` and `dialog` are false for a `QSplashScreen`
or a parentless message box. The fixed size drops the notices of qBittorrent and KGet and
Hydrogen's splash, none of which can be resized. A resizable splash or notice (FreeCAD's splash,
Chromium's terms window) is a prelude: a window its process closes within three seconds before
opening the next, or within three seconds after opening the next beside it, as a `QSplashScreen`
does on `finish()`. Its rectangle is not recorded, and the record is marked `lead`. Nothing about a
splash is known until it closes, so the mark is what spares the next one: the first window of a
launch whose record carries it is left at KWin's placement and its own size, and the window that
follows it within three seconds, or opens beside it, is restored. A first window that outlives that
interval and has no successor within it was the program's own, so its close records its rectangle
without the mark; a program that stops showing its notice (Chromium's appears once) is left at
KWin's placement for one launch.

A remembered place is moved and shrunk into the usable area of the screen nearest to it, so a window
remembered on a screen that is gone, or on a wider one, opens whole on one that is there. The record
keeps the remembered rectangle while the window holds the shrunk one, and takes a new one only after
the window has reached it and then changed, or is moved or resized by hand. Each record carries the
screen's device pixel ratio; a record made at another ratio restores the place and leaves the size
to the program. A record without a ratio matches any.

KWin places a window once, at its first size, and a later resize keeps the top-left. A window with
no record whose top-left lies within 64 pixels of the centre of the usable area, and that is larger
than 128 pixels in either direction, was placed at a placeholder size (VICE maps 1x1 and grows to
789x604; the Help Center's first commit is tiny), and is centred again and fitted into the area.
KWin does not shrink a window a program sizes larger than the usable area either (Chromium's first
window, with no record, maps taller than the screen), so any other new window with no record that
runs past the area is moved and shrunk into it. Both are tried when the window opens and on each
frame change for two seconds, until one fires once or the window is moved by hand.

Two settings, in `kwinrc` under `[Script-kdos_window_memory]`: `Exclude`, a comma-separated list of
application ids that replaces the built-in one, and `RememberDesktop`, off by default, which also
moves a window back to the virtual desktop it closed on, and the view with it.

The file is written by QSettings through a temporary file and a rename, so a crash leaves the old
record or the new one, never half of one. A program opened and closed in the same place costs no
write.

Plasma 6.9 is to bring KWin's own per-arrangement window memory; this script is KDOS's until then.

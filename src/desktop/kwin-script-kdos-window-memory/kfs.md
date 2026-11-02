## About

`kwin-script-kdos-window-memory` makes a program's window open where its last window closed, at the
same size and, when it closed maximized, maximized. On Wayland a program cannot place its own
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

What it leaves alone is the point of it: a window that opens maximized, full screen or tiled; a
dialog, a panel or any window that is not a program's ordinary top-level window; a window that would
land exactly on an open window of the same program; and the programs in its exclusion list, which
place their own windows or are prompts. A window rule that forces a position wins, because KWin
refuses the move; a rule that only sets the first position is overridden.

A remembered place is moved and shrunk into the usable area of the screen nearest to it, so a window
remembered on a screen that is gone, or on a wider one, opens whole on one that is there.

Two settings, in `kwinrc` under `[Script-kdos_window_memory]`: `Exclude`, a comma-separated list of
application ids that replaces the built-in one, and `RememberDesktop`, off by default, which also
moves a window back to the virtual desktop it closed on, and the view with it.

The file is written by QSettings through a temporary file and a rename, so a crash leaves the old
record or the new one, never half of one. A program opened and closed in the same place costs no
write.

Plasma 6.9 is to bring KWin's own per-arrangement window memory; this script is KDOS's until then.

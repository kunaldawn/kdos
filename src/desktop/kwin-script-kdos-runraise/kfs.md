## About

`kwin-script-kdos-runraise` gives each of KDOS's terminal programs one key. The key raises the
program's window when one is open and opens one in Konsole when none is; pressed again from one of
the program's windows it moves to the next, so repeated presses walk all of them.

| Key | Program |
|---|---|
| Meta+Shift+M | `mc`, files |
| Meta+Shift+E | `aerc`, mail |
| Meta+Shift+B | `lynx`, the web |
| Meta+Shift+U | `rmpc`, music |
| Meta+Shift+C | `ikhal`, the calendar |
| Meta+Shift+G | `iamb`, chat |
| Meta+Shift+W | `micro`, the editor |

It is a KWin script written in QML, a KWin/Script package installed to
`/usr/share/kwin/scripts/kdos_runraise/`, with one desktop entry per program in
`/usr/share/kglobalaccel/`. The keys are KWin actions, so they are listed under KWin on System
Settings' Shortcuts page, where each can be changed, and on the key card.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests. The test is QML (`tests/tst_raise.qml.in`), run by
  `qmltestrunner`; `testing/plasmatest.sh` builds it in the development container, where Qt's test
  runner is.

## Notes

Konsole gives every window one application id, `org.kde.konsole`, so a program's window is found by
its caption. A window the key opened runs `konsole --hold -p tabtitle=<program> -e <program>`,
whose caption is the program's name; a window opened any other way, from the application menu or by
typing the name in a shell, carries Konsole's own tab title `<dir> : <program>`, and both are
matched. A program that sets its own window title does not change either, because Konsole shows
that title in the caption only when "Show window title on the title bar" is turned on.

A KWin script cannot start a program. Each program's desktop entry in `/usr/share/kglobalaccel/` is
loaded by kglobalacceld as a component of its own, and the script opens a window by invoking that
component's `_launch` action over D-Bus, which runs the entry's `Exec` line through KIO. The entries
carry no key: a key has one owner, and it is the script's. They are on the Shortcuts page as
"<program> in a New Window", which is what they do, and a person can bind one to a key of its own.
CMake refuses to configure when the programs the script names and the entries beside it differ.

`--hold` keeps the window after the program exits, so a program that cannot start (`rmpc` with no
MPD to connect to, `iamb` with no configuration) leaves its error on screen instead of a window
that closes before it can be read. A window held after a normal quit keeps its caption, so the key
raises it until it is closed. A program that is not installed still has its key; Konsole then says
the program was not found and starts a shell in its place. No program here is a dependency of the port: each is a terminal program a person may
remove without losing the rest.

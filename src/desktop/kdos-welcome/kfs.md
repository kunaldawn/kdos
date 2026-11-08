## About

`kdos-welcome` is the KDOS tour inside Plasma's Welcome Center: QML pages only, in the
distribution-page folder `/usr/share/plasma/plasma-welcome/extra-pages`, and the
`intro-customization.desktop` that puts the KDOS mark, one sentence and the "Plasma by KDE" credit
on the first page. The pages are Accessibility, Look (the accents, light and dark, Phosphor mode,
Night Light, reduced motion), Desk (the three desks, with undo), Your System, Software and Boxes,
the Offline Library, Keys, Privacy and Network, and Help. Two more are not part of every tour: on
the live medium a page that keeps the session's changes on the stick, and a What's New page opened
after an update.

## Flags

- `"$PORT_SRC"/extra-pages/kdos/*.qml` — The pages' shared parts (the command runner, the accent
  list, the swatches, the desk pictures). They go in a subfolder because Welcome Center loads every
  file in `extra-pages` as a page and skips folders.

## Notes

**Commands.** A page may only run programs through Welcome Center's `Utils.runCommand`, which
splits one string on spaces and starts the program directly, with no shell. `kdos/Run.qml` takes
an argument list and refuses to run one with an empty or space-holding argument, which would
otherwise arrive as different arguments. Every switch runs the command that owns the setting:
`kdos theme`, `kdos phosphor`, `kdos desk`, `kdos tiling` and `kdos persist` from
[kdos-tools](#port-kdos-tools), `kreadconfig6` and `kwriteconfig6 --notify` for the KDE keys
Plasma watches, and `dbus-send` for the screen reader and KWin's effects, as KDE's own desktop
files call them. A command that fails shows Welcome Center's own failure sentence, which ends with
the command's last line of output, in a message at the top of the page. When a command fails,
Welcome Center hands back that sentence instead of the command's output, so a page that reads JSON
treats anything that does not parse as the command's "nothing to report" answer.

**Colours.** The accent swatches are each accent's colour scheme, read with `kreadconfig6` from the
files [kdos-plasma-look](#port-kdos-plasma-look) installs; everything else is the running scheme's
roles. "High contrast" offers the dark and the light accent whose text measures the highest
contrast on its ground, computed from the same files rather than named.

**Phosphor mode.** The Look page offers it only while neither a screen reader nor a magnifier is
on: KWin's zoom and magnifier effects are in its list of active effects exactly while they
magnify. When the session renders in software the switch is disabled and says why, in the words
`kdos phosphor status` gives.

**Order.** Welcome Center puts distribution pages after its own. The Accessibility page is `00-`
so that the [plasma-welcome](#port-plasma-welcome) patch that shows `00-` to `09-` pages straight
after the welcome page puts it before anything else is asked; without the patch it is the first
KDOS page. No other page is numbered below `10-`.

**Modes.** The live-medium page, `35-Persist.qml`, sits after Your System and shows only when
Welcome Center was started with `--live-environment`, which its kded module passes whenever
`plasma-welcomerc` says `LiveEnvironment=true`; `show` is read the moment a page is made, so the
page reads the argument rather than waiting on a command. What's New sets `show: false`, so it is
never in the tour, and is opened as `plasma-welcome --pages 90-WhatsNew.qml`, which shows a page
whatever `show` says.

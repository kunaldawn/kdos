## About

`kdos-welcome` is the KDOS tour inside Plasma's Welcome Center: QML pages only, in the
distribution-page folder `/usr/share/plasma/plasma-welcome/extra-pages`, and the
`intro-customization.desktop` that puts the KDOS mark, one sentence and the "Plasma by KDE" credit
on the first page. The pages are Accessibility (switches for the screen reader, reduced motion,
sticky, slow, bounce and mouse keys and dwell click, the Fonts page for text size, the two
high-contrast accents, and the zoom chords named), Look (the accents, light and dark, Phosphor mode,
Night Light, reduced motion), Desk (the three desks, with undo), Your System, Software and Boxes,
the Offline Library, Keys, Privacy and Network, and Help. Two more are not part of every tour: on
the live medium a page that keeps the session's changes on the stick, and a What's New page opened
after an update.

## Flags

- `"$PORT_SRC"/extra-pages/kdos/*.qml` — The pages' shared parts (the command runner, the accent
  list, the swatches, the desk pictures). They go in a subfolder because Welcome Center loads every
  file in `extra-pages` as a page and skips folders.
- `"$PORT_SRC/extra-pages/kdos/qmldir"` — Makes the accent list a singleton, one object for every
  page that shows swatches. A folder with a `qmldir` exposes only the types it names, so it lists
  every file in the folder; a part added without a line there cannot be used by any page.

## Notes

**Commands.** A page may only run programs through Welcome Center's `Utils.runCommand`, which
splits one string on spaces and starts the program directly, with no shell. `kdos/Run.qml` takes
an argument list and refuses to run one with an empty or space-holding argument, which would
otherwise arrive as different arguments. Every switch runs the command that owns the setting:
`kdos theme`, `kdos phosphor`, `kdos desk`, `kdos tiling` and `kdos persist` from
[kdos-tools](#port-kdos-tools), `kreadconfig6` and `kwriteconfig6 --notify` for the KDE keys
Plasma watches, and `dbus-send` for the screen reader and KWin's effects, as KDE's own desktop
files call them. A command that changes a setting is counted until it exits, and the page greys its
controls meanwhile; a read is not counted, and a program that stays open (a settings page,
Discover, the Help Center) is started with `launch`, which is not counted either, so the page stays
usable while it is open and reports only a failure. A command that fails shows Welcome Center's
own failure sentence in a message at the top of the page, cut by `Run.qml` to the sentence and the
command's last line of output: Welcome Center's ends with all of the command's error output, which
for a graphical program is a page of driver and font warnings. When a command fails, Welcome
Center hands back that sentence instead of the command's output, so a page that reads JSON treats
anything that does not parse as the command's "nothing to report" answer.

**Freshness.** The Accessibility and Look pages read their settings again, and the Keys page its
chords, whenever the page is shown and whenever the Welcome Center window comes back to the front,
so a setting changed in System Settings, by a chord or by `kdos theme` while the tour is open is
shown as it is. The Keys page also needs it at a first login, where the clipboard history's action
is registered only after Welcome Center has made the page.

**Colours.** The accent swatches are each accent's colour scheme, read with `kreadconfig6` from the
files [kdos-plasma-look](#port-kdos-plasma-look) installs, three keys an accent, once for every
page that shows them; everything else is the running scheme's roles. "High contrast" offers the dark and the light accent whose text measures the highest
contrast on its ground, computed from the same files rather than named.

**Phosphor mode.** The Look page offers it only while neither a screen reader nor a magnifier is
on: KWin's zoom and magnifier effects are in its list of active effects exactly while they
magnify. When the session renders in software the switch is disabled and says why: the page has its
own sentence for each of the two refusals `kdos phosphor status --json` names in `refusal`
(`session`, `gpu`), the same two the Look settings page shows, and shows the `reason` of any other
refusal as the command words it.

**Order.** Welcome Center puts distribution pages after its own. The Accessibility page is `00-`
so that the [plasma-welcome](#port-plasma-welcome) patch that shows `00-` to `09-` pages straight
after the welcome page puts it before anything else is asked: the second page at an installed
first login, and the third on the live medium, where the live page comes before the welcome page.
Without the patch it is the first KDOS page. No other page is numbered below `10-`.

**Keys.** The Keys page shows, for each action, the chord KDOS teaches while it is bound and the
first bound chord with a modifier otherwise, read with the `globalShortcutTexts` call that
plasma-welcome's `shortcut-texts.patch` adds; kglobalaccel returns an action's chords in no stable
order. The chord column is as wide as the widest chord. The Accessibility page names the screen
reader and zoom chords the same way, so zoom in reads `Meta+=` and not its second chord, `Meta++`.

**Your System.** `kdos-bootctl status --json` is read three ways: two slots (`configured` true and
both `slot_a` and `slot_b` set), one slot (`configured` true with one of them empty, where an update
is applied over the running system with `kdos update apply --in-place`), and no boot state
(`configured` false, the live medium). Each has its own text, and only two slots show the previous
version and its System Versions button: the other two cases have no previous version to start.

**Modes.** The live-medium page, `35-Persist.qml`, sits after Your System and shows only when
Welcome Center was started with `--live-environment`, which its kded module passes whenever
`plasma-welcomerc` says `LiveEnvironment=true`; `show` is read the moment a page is made, so the
page reads the argument rather than waiting on a command. What's New sets `show: false`, so it is
never in the tour, and is opened as `plasma-welcome --pages 90-WhatsNew.qml`, which shows a page
whatever `show` says.

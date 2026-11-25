## About

`kdos-welcome` is the KDOS tour inside Plasma's Welcome Center: QML pages only, in the
distribution-page folder `/usr/share/plasma/plasma-welcome/extra-pages`, installed by a CMake
project that installs files and builds nothing, and the
`intro-customization.desktop` that puts the KDOS mark, one sentence and the "Plasma by KDE" credit
on the first page. The pages are Accessibility (switches for the screen reader, reduced motion,
sticky, slow, bounce and mouse keys and dwell click, the Fonts page for text size, the two
high-contrast accents, and the zoom chords named), Look (the accents, light and dark, Phosphor mode,
Night Light, reduced motion), Desk (the three desks, with undo), Your System, Software and Boxes,
the Offline Library, Keys, Privacy and Network, Help, Getting Started, and Make It Yours, which
installs groups of programs prebuilt; Plasma's own Enjoy It! page follows it and ends the tour.
Two more are not part of every tour: on
the live medium a page that keeps the session's changes on the stick, and a What's New page opened
after an update, or after going back by hand from a newer version this account used. The port
also installs `io.kdos.releasenotes.desktop`, the Release Notes entry in the application menu,
which Kickoff and KRunner find under "release notes", "what's new" and "changelog". It opens What's
New on its own, with Finish as its one button, or, while the tour is open, raises the tour's
window with What's New over it, where Back returns to the tour: Welcome Center is a unique D-Bus
application, and [plasma-welcome](#port-plasma-welcome)'s `release-notes-pages.patch` has the
running window show the pages a second start names.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests. The test is QML (`tests/tst_groups.qml.in`, the Make
  It Yours page's rules in `extra-pages/kdos/groups.js`), run by `qmltestrunner`;
  `testing/plasmatest.sh` builds it in the development container, where Qt's test runner is.

## Notes

**Layout.** The pages' shared parts (the command runner, the accent list, the swatches, the desk
pictures, the groups page's rules) are in `extra-pages/kdos/`, because Welcome Center loads every
file in `extra-pages` as a page and skips folders. Its `qmldir` makes the accent list a singleton,
one object for every page that shows swatches. A folder with a `qmldir` exposes only the types it
names, so it lists every QML file in the folder; a part added without a line there cannot be used
by any page. `groups.js` is imported by its path and needs no line.

**Commands.** A page may only run programs through Welcome Center's `Utils.runCommand`, which
splits one string on spaces and starts the program directly, with no shell. `kdos/Run.qml` takes
an argument list and refuses to run one with an empty or space-holding argument, which would
otherwise arrive as different arguments. Every switch runs the command that owns the setting:
`kdos theme`, `kdos phosphor`, `kdos desk`, `kdos tiling` and `kdos persist` from
[kdos-tools](#port-kdos-tools), `kreadconfig6` and `kwriteconfig6 --notify` for the KDE keys
Plasma watches, and `dbus-send` for the screen reader and KWin's effects, as KDE's own desktop
files call them. A command that changes a setting is counted until it exits, and the page greys its
controls meanwhile; a read is not counted, and a program that stays open (a settings page,
Discover, the browser) is started with `launch`, which is not counted either, so the page stays
usable while it is open and reports only a failure. A command that fails shows Welcome Center's
own failure sentence in a message at the top of the page, cut by `Run.qml` to the sentence and the
command's last line of output: Welcome Center's ends with all of the command's error output, which
for a graphical program is a page of driver and font warnings. When a command fails, Welcome
Center hands back that sentence instead of the command's output, so a page that reads JSON treats
anything that does not parse as the command's "nothing to report" answer.

**Freshness.** The Accessibility and Look pages read their settings again, and the Keys page its
chords, whenever the page is shown and whenever the Welcome Center window comes back to the front,
so a setting changed in System Settings, by a chord or by `kdos theme` while the tour is open is
shown as it is. The Accessibility page also reads them every 1.5 seconds while it is shown, skipping
a tick while one of its own writes is pending: `Meta+Alt+S` and KWin's accessibility chords change
a setting without moving the window, and a page that can only start processes cannot subscribe to
the signal that says so. The Keys page also needs it at a first login, where the clipboard history's action
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

**Make It Yours.** `95-MakeItYours.qml` lists `kdos update groups --json` and installs the ticked
groups one at a time through `pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <id>`,
the prebuilt helper of [kdos-discover](#port-kdos-discover), whose `kdos update install --prebuilt`
takes a group id as well as a port name; its polkit grant, `io.kdos.ports.install`, is kept, so one
password covers the queue. A group whose size the command gives as null is not offered: the size is
null when the signed binhost index is not on the machine, or when a package of the group is not
prebuilt, and a prebuilt install refuses both. The page has no root path to fetch the index, so
with none it sends the person to the Refresh button on Discover's Updates page and offers to look
again. Where every row's `binhost` is false, nothing names a binhost for this tree and no refresh
can give it one (the last binhost step found none named, the last fetch refused the published tree
as older, or the machine has no second slot and its refresh fetches no tree), so the page says
instead that this release installs no group ready-made and
offers Discover, which can build programs on the computer; its header then says only that Discover
has every program and can build them here, without the "downloaded ready to run" the message under
it would contradict.

**Order.** Welcome Center puts distribution pages after its own, and before its closing Enjoy It!
page. The Accessibility page is `00-` so that the [plasma-welcome](#port-plasma-welcome) patch that
shows `00-` to `09-` pages straight after the welcome page puts it before anything else is asked:
the second page at an installed
first login, and the third on the live medium, where the live page comes before the welcome page.
Without the patch it is the first KDOS page. No other page is numbered below `10-`. Make It Yours
is `95-`, the last KDOS page.

**What is installed.** The image carries the KDOS book but not KDOS from Scratch, which
`kdos-kfs` installs by name, nor the Help Center or the library applications, which are groups and
ports in Discover. The Offline Library and Help pages ask Welcome Center's `ApplicationInfo`
whether a desktop entry exists: the books paragraph and the Help page's KDOS from Scratch point
read `kdos-library-kfs-book` and without it name the install command as it is typed,
`sudo kdos update install kdos-kfs`, since `kdos update install` writes the running root; the Help page greys its
Open button rather than send the browser to a missing file, and the Library's "On this machine"
heading is shown only over at least one installed library application.

**Spacing.** `kdos/Flick.qml` insets every page's body by a grid unit, the inset of Welcome
Center's own page heading and description, so a KDOS page's content lines up with them. The Look
page's Accent heading is drawn as the form's section headings are (level 3, primary, centred), so
its four groups read alike.

**Keys.** The Keys page shows, for each action, the chord KDOS teaches while it is bound and the
first bound chord with a modifier otherwise, read with the `globalShortcutTexts` call that
plasma-welcome's `shortcut-texts.patch` adds; kglobalaccel returns an action's chords in no stable
order. The chord column is as wide as the widest chord. The Accessibility page names the screen
reader and zoom chords the same way, so zoom in reads `Meta+=` and not its second chord, `Meta++`,
and so does the Offline Library page for KRunner, whose launch action carries four chords: it
names `Meta+Space`, the Keys page's, on every open.

**Your System.** `kdos-bootctl status --json` is read three ways: two slots (`configured` true and
both `slot_a` and `slot_b` set), one slot (`configured` true with one of them empty, where an update
is downloaded with `sudo kdos update fetch` and written over the running system with
`sudo kdos update apply`, the same two commands System Versions' help names), and no boot state
(`configured` false, the live medium). Each has its own text, and only two slots show the previous
version and its System Versions button: the other two cases have no previous version to start.

**Modes.** The live-medium page, `35-Persist.qml`, sits after Your System and shows only when
Welcome Center was started with `--live-environment`, which its kded module passes whenever
`plasma-welcomerc` says `LiveEnvironment=true`; `show` is read the moment a page is made, so the
page reads the argument rather than waiting on a command. The launcher entry passes it too on the
live medium alone: `/usr/share/kdos/data-live/applications/org.kde.plasma-welcome.desktop`
(from the `fs/` tree) is upstream's entry with that `Exec`, and `kdos_session_live_data` puts
the directory in front of `XDG_DATA_DIRS` only when the root is the live overlay, so a tour
reopened from Kickoff keeps the Install and Keep Your Changes pages and an installed machine's
launcher opens the ordinary tour. The Privacy page reads the same argument: on the live medium,
whose `xdg-live/kwalletrc` turns the wallet off, its wallet paragraph says the live session keeps
no wallet and an installed system's asks for its own password, and it offers no Wallet Settings
button, which would open on the switched-off subsystem; elsewhere the paragraph shows only on a
machine that logs in automatically. What's New sets `show: false`, so it is
never in the tour, and is opened as `plasma-welcome --pages 90-WhatsNew.qml`, which shows a page
whatever `show` says: by [kdos-notice](#port-kdos-notice) after an update or after going back by hand from a
newer version this account used, and by the Release Notes entry. `tests/launcher.cmake` checks that the entry's `Exec` names a page in
`extra-pages` that reads `kdos update notes`, and its name and keywords.

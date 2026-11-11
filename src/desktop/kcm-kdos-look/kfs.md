## About

`kcm-kdos-look` is the KDOS Look page of System Settings, under Appearance & Style → Colors &
Themes. It holds the two things the stock pages cannot: the one action that carries an accent
past Plasma, and Phosphor mode. The eight accents themselves are colour schemes and global
themes on the Colors and Global Theme pages. A swatch here runs `kdos theme <accent>`, which also
repaints open Konsole windows, the lock and login screens, the splash, the boot menu and the
text consoles, and each swatch is painted from its own `KDOS<Label>.colors` through
KColorScheme. The list of accents is the one `kdos theme list` prints, so the page offers no
accent the command would refuse.

Below the swatches the page compares the three places an accent is held: Plasma's
`kdeglobals [General] ColorScheme`, the KDOS accent the terminals and command-line tools wear,
and `/etc/kdos/accent`, which the screens before login wear. When Plasma is ahead or the machine
behind, Repaint Everything runs `kdos theme --from-plasma <Plasma's accent>`, the command
[kded-kdos](../kded-kdos/kfs.md) runs, so Plasma's choice is kept and not applied twice. The page
follows that module over `io.kdos.Kded1` at `org.kde.kded6 /modules/kdos`: while it carries an
accent down the page is busy and starts nothing, and when its last run failed while the
accents still disagree, its reason is shown above Repaint Everything. The line under the swatches
reads the greeter set `/usr/share/kdos/greeter/<accent>/kdeglobals` for the login screen's scheme,
so a light accent's dark partner is named, and names the boot menu only where `/boot/efi` exists.
The Phosphor section reads `kdos phosphor status --json` and runs `kdos phosphor on|off`,
`preset subtle|retro`, `fullscreen on|off` and `terminus on|off`; with the effect unsupported and
the mode off, every option is disabled with the switch, and the record's `refusal` picks the
page's own sentence for why. Every command is an argument vector given to QProcess, never a shell, and
the port is KDOS's own code with no upstream source, built with CMake against KF6 KCMUtils,
KColorScheme and KConfig.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests into the package. `testing/plasmatest.sh` configures
  the same sources with tests on; `looktest` runs the page's logic against `tests/fake-kdos`,
  which plays `kdos theme` and `kdos phosphor` and logs each argument vector, and against a fake
  `io.kdos.Kded1` on the test's own session bus, and asserts that every accent picked leaves
  `ColorScheme=KDOS<Accent>` in `kdeglobals`.

## Notes

The page has no Apply button: an accent is a command that repaints the machine, and holding it
until Apply would show a swatch picked while nothing moved. One command runs at a time, in
order, and a second click while one is in flight is refused rather than queued; so is any click
while kded-kdos runs `kdos theme`, because two runs at once would write the same files in an
order nobody chose. An accent whose scheme file is not installed is listed and cannot be
picked, because applying it would move every surface except Plasma.

The page asks kded-kdos with D-Bus activation off. kded6 belongs to the session, and a settings
page that activated one would start a second set of modules in a session that runs without
them. With no module answering, the page is the only runner, and Repaint Everything is how a
pick made on the Global Theme page reaches the screens before login. A run of the module ends
with a signal and starts with none, so the page asks for the module's state whenever the colour
scheme changes and every few seconds while it says it is running.

The page never writes `kwinrc`, `kdeglobals` or a Konsole profile itself. What Phosphor mode
switches, the pairing of Bone and Paper with the Phosphor accent and what turning it off
restores belong to `kdos phosphor`, which the welcome tour and KRunner run as well. A `kdos`
without the verb answers `status --json` with an error, and the section then says the mode is
not available instead of showing switches that do nothing.

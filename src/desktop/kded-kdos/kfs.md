## About
`kded-kdos` is a kded6 module, `kf6/kded/kdos.so`, with three jobs a session has to do while
nobody is looking at a KDOS window.

**It keeps the accent in step with Plasma.** An accent chosen inside Plasma — a KDOS global theme
on the Global Theme page, a KDOS colour scheme on the Colours page, Quick Settings' light and dark
flip — changes the colour scheme and nothing below the desktop. The module watches kdeglobals
`[General] ColorScheme` through a KConfigWatcher and, when it names a scheme KDOS ships
(`KDOS<Title>` under `/usr/share/color-schemes`) that differs from the accent in
`~/.config/kdos/accent`, runs `kdos theme --from-plasma <accent>`; that carries the choice to the
terminal programs, the Qt 5 palette, the wallpaper, the lock screen, the greeter, the boot menu and
the console, and skips applying the global theme, so the run it starts cannot start another. A
scheme that is not KDOS's, or one that lives only in the user's own `color-schemes` directory,
changes nothing. See [kdos theme](../../../docs/kdos/04-programs/kdos-command.md#kdos-theme).

**It offers the application sets on a stick.** When a removable volume is mounted, the top of it
is listed once, off kded's thread, for `*.ktar` files, the sets `kdos app export` writes; up to
three are offered in one notification whose buttons open each in Discover
(`plasma-discover --local-filename <file>`), where KDOS's backend imports it offline and verifies
every pack. Nothing is imported by the module itself.

**It starts Samba for the first shared folder.** [samba](#port-samba)'s service script starts
`smbd` at boot only when something is shared, and a folder shared from Dolphin's Share tab is a
file `net usershare` writes into `/var/lib/samba/usershares` while `smbd` is down. The module
watches that directory and, when it goes from holding no share to holding one, calls
`StartFileSharing` on [kdos-powerd](#port-kdos-powerd)'s `io.kdos.Power1`, which asks polkit
(`io.kdos.filesharing.start`, granted to the person at the machine) and runs the service script's
`start`. A failure raises one notification per reason in a session.

Its state is on the session bus at `org.kde.kded6` `/modules/kdos`, interface `io.kdos.Kded1`:
`accent()`, `syncState()` (`idle`, `running` or `failed`), `lastError()` and `resync()`, with the
signals `accentSynced` and `accentSyncFailed`, so a settings page or a script can ask what the last
run did and start another. `resync()` runs `kdos theme --from-plasma` even when the accent file
already names the scheme's accent: the command writes that file before its other steps, so a run
that failed half way leaves it in step, and a failure is `failed` until a run succeeds or the file
is written again by a `kdos theme` run by hand.

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no tarball,
  and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the plugin into Qt's own plugin directory,
  `/usr/lib/qt6/plugins`, where kded6 looks for `kf6/kded`.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/` is run by
  the Qt regression net in the development container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build.

## Notes
The module id is the bare word `kdos`, not a dotted `org.kdos.*` plugin id: kded registers every
module on the bus at `/modules/<id>`, and a dot is not allowed in an object path, so a dotted id
would load and never be reachable.

A notified write is what wakes the watcher. Plasma's colour and global-theme pages, and
`plasma-apply-colorscheme` and `plasma-apply-lookandfeel`, all write kdeglobals with KConfig's
notify flag; a scheme set by editing the file by hand is noticed at the next login, when the
module compares once on load.

`kdos theme` is looked for on the session's `PATH` and then in `/usr/local/bin`, where kdos-tools
installs it, because kded6 inherits whatever `PATH` the session started with. A run that has not
finished after a minute is stopped. A run that fails, or a command that cannot be started, raises
one notification per accent and reason in a session, with the last line the command printed. A
boot menu and login screen left alone because kdos-powerd refused or did not answer is not a
failure: `kdos theme` says so and exits 0, which is what keeps a user outside `wheel`, and the live
medium, from a notification at every accent change.

Every file name in the stick offer comes off the stick, so it is escaped before it reaches the
notification's rich text and reaches Discover only as one element of an argument vector. Only a
name ending in `.ktar`, in lower case, is offered: that is the spelling KDOS's Discover backend and
the installer open, and an offer Discover cannot open would be an empty page. A symlink, a hidden
file, an empty file and anything below the top directory are never offered, and the listing stops
after 4096 entries. A volume already mounted when the session starts is offered the next time it
is mounted, not at every login.

The usershare directory is root's, group `sambashare`, mode 1770, so only a session of a member can
watch it; in any other session the module logs once that it is not watching, and that session
cannot share a folder either. An account the Share tab has just added to `sambashare` is watched
from its next login. A share already there at login was there at boot, when the service script
decided about it, so only a change after login makes the call. A file whose name begins with `:`
is a share `net usershare` is still writing, and is not counted.

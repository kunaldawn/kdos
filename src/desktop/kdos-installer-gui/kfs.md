## About

kdos-installer-gui is the graphical installer, Install KDOS: a Kirigami window that walks Welcome
(the time zone), Keyboard, Disk, Encryption, Account, Look, Applications and Summary, then installs
and says how it ended. It is a front end and nothing more. The machine is described to it by
[kinstall](#port-kdos-installer)'s dumps (`kinstall --dump probe|look|apps|plan --json`), and the
install is `pkexec /usr/bin/kinstall --config - --unattended --events json`, with the answers, the
passwords among them, written to kinstall's standard input and the progress read back as one JSON
event per line. The summary page is kinstall's own plan for the same answers, so what it shows is
what runs, and answers kinstall would refuse are refused there. The Disk page's "Partition
Manager…" opens Partition Manager and probes the disks again when it closes. One window runs per
session: the program is unique on the session bus as `io.kdos.kdos-installer-gui` through
KDBusAddons, and a second launch raises the first window. The port is KDOS's own code with no
upstream source, C++ and QML built with CMake against Kirigami, KI18n, KConfig, KColorScheme,
KItemModels, KDBusAddons and KIO; every program it starts is an argument vector given to QProcess,
never a shell.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests into the package. `testing/plasmatest.sh` configures
  the same sources with tests on: `installertest` reads the answer file the pages produce back as
  kinstall's `key = value` lines, replays event streams kinstall wrote (`tests/fixtures`) through
  the parser in pieces that cut lines, checks that a stream pkexec ended before any event reads as
  permission refused, parses a fixture `evdev.lst` against a fixture symbols directory, writes
  the live session's `kxkbrc` for German (no dead keys) into a temporary configuration directory,
  checks that a `KConfigWatcher` on `kxkbrc` hears the change notice for its `[Layout]` keys, reads a fixture `zone1970.tab`, tells the live medium from two fixture
  mounts tables, checks that choosing a disk with no partitions turns to the erase plan, and
  checks the words naming a disk and what an erase destroys (an LVM physical volume partition by
  its group, never `LVM2_member`).
- `-D KDOS_KINSTALL=/usr/bin/kinstall` — The binary the installer runs and the pkexec policy
  `io.kdos.installer.run` names by its `exec.path`. pkexec matches the path exactly, so it is
  compiled in and never looked up on `$PATH`; the two must name the same file.

## Notes

The port installs its desktop entry and the live medium's polkit rule under
`/usr/share/kdos/installer`, where nothing reads them, and the policy under
`/usr/share/polkit-1/actions`. On the live medium the boot step `/etc/init.d/63_live-installer.sh`
copies the entry into `/usr/share/applications` and onto the live account's desktop, and the
rule, which grants the install and kpmcore's helper to the local active session with no password,
into `/etc/polkit-1/rules.d`; kinstall leaves all of them out of the copy. An installed machine
therefore offers no "Install KDOS" entry, and running the program there asks for an
administrator's password. The welcome tour's live page opens the same entry: the live medium's
`plasma-welcomerc` names `LiveInstaller=kdos-installer`.

`kinstall` is not in `depends`: it is built by `10_bootstrap` into every tree from the first
bootable one, and no phase list names its port, so a dependency on the port would have `kpkg`
build and install a second copy over it.

On the live medium (a root that is an overlay) the first probe, Look Again and the Summary's plan
run `pkexec /usr/bin/kinstall --config - --dump probe|plan --json` under `io.kdos.installer.run`:
only a root probe activates the LVM groups the live boot leaves inactive, and only a root plan sees
an inactive volume group named kdos on another disk, which the install refuses. The Disk page's
swap check runs the plan as the user, and Next waits for it. The user's payload walk cannot enter
what only root may and measures less than root's, so the last root probe's `payload_kb` and
`appbox_kb` are given (`--payload-kb`, `--appbox-kb`) to every plan and to every probe as the
user, and the Disk page and the Summary advise the same swap size. A root probe is given none and
measures again, so Look Again takes in a tree that has grown. A probe asked for while the first
root probe runs waits for it rather than replacing it, so its payload is kept; until one is kept,
the Disk page's swap check runs as root too. A root dump superseded by a newer one is detached
rather than killed: the user cannot signal it, and destroying its QProcess would block the window
until it ended. Open the Log hands the log to KIO's OpenUrlJob as `text/plain`, so it opens in
the text editor and never in whatever claims the file's extension.

On the live medium the Keyboard page's choice is applied to the live session as well
(`Backend::applyLiveLayout`): `~/.config/kxkbrc`'s `[Layout]` lists the layout with `us` second,
written through KConfig, then KConfig's change notice for it (`ConfigChanged` on `/kxkbrc`,
interface `org.kde.kconfig.notify`, naming `LayoutList`, `VariantList` and `Use` in `[Layout]`).
KWin loads the layout list again only from that notice, through its `KConfigWatcher`; a file
written without it changes nothing, and KConfig sends none itself for a file opened by its
absolute path. fcitx5 is left alone: with its XKB override off (`/etc/xdg/fcitx5/conf/wayland.conf`)
it decodes keys with the keymap KWin hands it. Without this, the passwords typed on the Encryption
and Account pages would be typed on the live medium's US keys and differ from what the installed
greeter receives.

A password or passphrase that starts or ends with a space is refused on its page, because
kinstall's answer-file parser trims the ends of every value; a line break cannot be typed into a
field and is flattened if one arrives anyway, so a value can never become a second key.

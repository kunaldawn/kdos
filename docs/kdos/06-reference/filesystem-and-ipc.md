# Filesystem and IPC

This chapter is the lookup table for a running KDOS system: every path KDOS owns on the installed
machine, every socket and bus name its programs talk over with the requests each one accepts, the
files one program writes for another to read, and the environment variables that change
behaviour, including those `make build` and the source-archive scripts read on the build machine.
It is for administrators tracking down where something lives, for people scripting the desktop,
and for contributors who need to know which program owns which file. Read
[Architecture overview](../03-architecture/overview.md) first if you want the picture these paths
belong to; this chapter assumes it.

*IPC* (inter-process communication) here means the Unix sockets, D-Bus names and shared files one
KDOS program uses to tell another something. Terms such as *accent*, *desk*, *box*, *pack*,
*graft*, *binhost*, *shim*, *root slot* and *Phosphor mode* are defined in the
[Glossary](glossary.md).

The chapter runs from the filesystem to the protocols to the environment. Look a path up under
[The target filesystem](#the-target-filesystem), which is grouped by where a path lives: the
machine's own directories, the boot medium, the boot files, your home directory and the session's
runtime directory. To talk to a root daemon, read [Protocol conventions](#protocol-conventions) and
then the socket's own section under [Sockets](#sockets); the names on the system and session buses
are under [D-Bus names](#d-bus-names). What each configuration file's keys mean is in
[Configuration](configuration.md); this chapter says only where the file is and who writes it. For
the source tree rather than the installed system, see [Repository layout](repository-layout.md).

## The target filesystem

Paths below are those of the installed or live system, not of the repository. Most per-user paths
written with `~/.config`, `~/.local/share`, `~/.local/state` or `~/.cache` follow
`$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `$XDG_STATE_HOME` or `$XDG_CACHE_HOME` when that variable is
set; the table shows the default. These are always under the home directory, whatever the
variables say: `~/.config/kdos/boxes/`, `~/.config/kdos/a11y` and `~/.local/share/kdos/packs/`. A
box's directory under `~/.local/share/kdos/boxes/` is resolved both ways: the launcher places a
private home under `$XDG_DATA_HOME` when it is set, while the pack daemon's writable layer and
`kdos-appbox`'s box subcommands always use the home directory.

### `/etc/kdos/` — machine configuration

| Path | Holds | Shipped |
|---|---|---|
| `menu.conf` | The routes: a stable `verb.noun` name for every place in the system, and the command that opens it | yes |
| `packd.conf` | How many superseded versions of each pack the store keeps (`retain = 1`) | yes |
| `zram.conf` | Compressed-swap size and algorithm | yes |
| `xkb.conf` | The graphical keyboard layout `locale1` reports, `XKBLAYOUT` and its three siblings | yes, all four empty |
| `timers.d/` | The system's periodic jobs, plus a `README` | yes: `10-update-check.timer` and `20-fstrim.timer` |
| `user-services/` | Every user's own services, `NN_name.sh`, which `ksvc` runs as the user from the first login to the last | yes: `10_dbus`, `20_pipewire`, `21_wireplumber`, `22_pipewire-pulse`, `25_mpd`, `30_timers`, `40_boxes` |
| `user-services/timers.d/` | Periodic jobs every user's `timers` service runs | yes: `40-box-gc.timer` |
| `keys/` | Trusted keys for host packages, the binhost index, the slot image manifest and the release index that finds them | yes, with no keys: only a `README` stating the policy |
| `keys/packs/` | Trusted keys for application packs | yes, one key: `kdos-packs.pub` |
| `keys/tree/` | Trusted keys for the ports tree `kdos update fetch` installs, and the slot image's manifest | yes: a `README`, and `kdos-release.pub` when the build had a release key |
| `update.conf` | Where `kdos update` takes a newer tree, binhost and slot image from, and what an armed slot keeps (`tree`, `binhost`, `image`, `carry`) | no: create it |
| `accent` | The accent name, so the boot splash and the login screen wear it before anyone logs in | no: written by `kdos-powerd` when the accent is set |
| `autologin-xdg/` | A `kscreenlockerrc` read by an automatic login alone, which starts that session locked | no: written by the installer on a single-account encrypted machine that logs in by itself |
| `chroot.env` | The compiler flags the image was built with, which `kdos update apply --source` builds with | yes, written by the build |

Beside it in `/etc`: `kdos-release`, the version and slot record of this root, which a slot image
replaces; `locale.conf`, `keymap`, `hostname`, `machine-info` and `localtime`, which
`kdos-settingsd` writes; `sddm.conf.d/`, `elogind/` and `turnstile/`, the login's configuration;
and `/etc/kpkg.conf`, the host package manager's, under
[`/var/lib/kpkg/`](#varlibkpkg--the-host-package-database).

### `/var/lib/kdos/` — state written by the system

| Path | Holds | Written by |
|---|---|---|
| `packs/` | Installed application packs, `<id>.kpack`, owned by root, mode 0644. On an installed machine `/home/.kdos/packs` is bound here by `fstab`, so both root slots share one store | The pack daemon, as root |
| `packs/mnt/` | Mount points for packs | The pack daemon |
| `packs/staging/` | The one place an unprivileged download may land, mode 01777; a staged pack is removed once it is installed, and an import's signed index sits there as `PACKAGES.<pid>` with `PACKAGES.<pid>.sig` while that import runs | Any user, and the application store |
| `packs/pack-manifest` | Every graft made, so removing one is exact; in the shared store, so both slots read one record, and the daemon makes at start every host graft it names that this root lacks. `packs/pack-manifest.lock` serialises its writes | The pack daemon |
| `packs/pack-manifest.ungrafted` | The paths an ungraft dropped from the record, which the other slot's daemon unlinks at its next start while each is still a link into the store's mountpoints | The pack daemon |
| `fs-manifest` | Every path the build's static tree (`fs/`) provided | The build |
| `march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `apps-pending` | Applications chosen in the installer that could not be installed | `kinstall` |
| `world` | The packages somebody installed on purpose, one name per line, which every binhost update installs into the new slot as well | `kdos update install`, and Discover's ports backend |
| `update.json` | What `kdos update check --json` found, what is offered and the last problem; Discover's update notifier, the System Versions page and `kdos-notice` read it | The update-check timer, nightly at 04:17, and `kdos update fetch` and `apply` when they finish |
| `update.state` | The outcome of the last `fetch`, of the binhost and image surveys and of the last `apply`, each with a time and a sentence | `kdos update` |
| `update.state.lock` | The lock `fetch` and `apply` hold, so a tree is never replaced under a build | `kdos update fetch` and `apply` |
| `update.state.sig` | The signature of the tree last installed; a published signature equal to it is the same tree, and the tree is not downloaded again | `kdos update fetch` |
| `binhost/` | The signed binhost index of the tree in place, `PACKAGES` and `PACKAGES.sig`, the signed release index it was fetched by, `INDEX` and `INDEX.sig`, with `ORIGIN` (where that came from), and its AppStream catalogue under `swcatalog/` | `kdos update fetch` |
| `image/MANIFEST` | The record of the slot image this root was written from; the next unpack copies every file whose hash it already lists from the running root rather than downloading it | `kdos image unpack`, into the slot it writes |
| `image/offer/` | The newest slot image manifest fetched and verified, `MANIFEST` and `MANIFEST.sig`, the release index that names it and its bundles, `INDEX` and `INDEX.sig`, and `ORIGIN` (where that came from); `kdos-sysupdated` offers its version | `kdos update fetch` |

### `/usr/share/kdos/` — shipped data

| Path | Holds |
|---|---|
| `appstore/catalogue` | Every application the store can build, one row per image layer, `<kind> <id> <parent> <apt packages…>`, with further rows (`meta`, `group`, `env` and others) that name, group and configure them |
| `alien-apps` | Application name to the command line that runs it in its box |
| `secdb.txt` | The vendored security-advisory database, read by `kdos cve` |
| `bundled.txt` | The bundled-library inventory, read by `kdos cve`: `<port> <crate> <component> <version>`, one row per library a port compiles into its own binary |
| `SOURCES` | The written source offer: the repository, the commit the image was built from, and each `sources-<N>` release that archives a source of an installed package. Written by `70_image/105_ports.sh` |
| `reasons/` | The explanations `kdos why` and `kdos explain` print |
| `xdg/` | KDOS's defaults for Plasma and the KDE programs, the last directory on `XDG_CONFIG_DIRS`; see [Plasma's files KDOS ships](configuration.md#plasmas-files-kdos-ships) |
| `xdg-vm/`, `xdg-live/` | The overlays put in front of `/etc/xdg` in a virtual machine and on the live medium |
| `greeter/<accent>/` | The login screen's look in each accent: the files the image (for the default accent), the installer and `kdos-powerd` copy into the `sddm` account's home |
| `keycard-groups.conf` | How the key card groups KWin's shortcuts |
| `update/io.kdos.system.metainfo.xml` | The AppStream component Discover shows the system update as |
| `discover/` | Discover's featured applications, `featured.json`, and `debian-exclude`, the Debian packages the KDOS backend never offers to build in a box (one name a line, `#` comments) |
| `installer/` | The graphical installer's data |
| `logo.txt` | The login banner's picture, with its terminal colour codes |
| `kdos.png` | The mascot artwork the marks and logo are cut from |
| `splash.psf` | The boot splash's font |
| `boot/` | Boot artwork: `kdos-backdrop.png`, `kdos-banner.png`, `os_kdos.png` |
| `memtest86plus/` | The memory tester carried on the boot medium |
| `syncthing-offline.xml` | The first-run configuration `kdos-syncthing` gives a user who has none: syncing between machines on one LAN without reaching outside it. Installed by the `syncthing` package |

### The desktop's shared data

Where KDOS's Plasma components and artwork install, outside `/usr/share/kdos`:

| Path | Holds | Port |
|---|---|---|
| `/usr/share/plasma/look-and-feel/org.kdos.<accent>.desktop/` | The eight global themes, one per accent | `kdos-plasma-look` |
| `/usr/share/plasma/layout-templates/org.kdos.desk.{kdos,dock,tiling}/` | The three desks' panel layouts | `kdos-plasma-look` |
| `/usr/share/plasma/desktoptheme/kdos/` | The Plasma style `kdos` | `kdos-plasma-look` |
| `/usr/share/color-schemes/KDOS*.colors`, `/usr/share/konsole/`, `/usr/share/qt5ct/colors/` | The colour schemes, Konsole profiles and qt5ct palettes of every accent | `kdos-plasma-look` |
| `/usr/share/org.kde.syntax-highlighting/themes/KDOS-*.theme` | The editor theme of every accent, which Kate and every KTextEditor program pick by the scheme's view background | `kdos-theme` |
| `/usr/share/wallpapers/KDOS-*/` | The wallpapers, one Plasma wallpaper package per accent | `kdos-wallpapers` |
| `/usr/share/icons/KDOS/`, `KDOS-Light/` | The icon themes, the KDOS marks over Breeze | `kdos-icons` |
| `/usr/share/sounds/kdos/` | The sound theme | `kdos-sounds` |
| `/usr/share/sddm/themes/kdos/` | The login screen theme | `kdos-sddm-theme` |
| `/usr/share/kwin/builtin-effects/kdos_crt.json` | The Phosphor mode effect, built into KWin by `kdos-crt.patch` | `kwin` |
| `/usr/share/kwin/effects/kdos_{unfold,login,quit,keys}/` | The KDOS window motion and the key card | `kdos-kwin-motion`, `kwin-effect-kdos-keys` |
| `/usr/share/kwin/scripts/kdos_{runraise,window_memory}/` | Run-or-raise and window memory | `kwin-script-kdos-runraise`, `kwin-script-kdos-window-memory` |
| `/usr/share/kglobalaccel/org.kdos.*.desktop` | The global shortcuts KDOS's scripts and `kdos tiling` register | `kdos-tools`, `kwin-script-kdos-runraise` |
| `/usr/share/krunner/dbusplugins/kdos.desktop`, `kdos-library.desktop` | The KDOS runner and the offline library in KRunner | `kdos-runner` |
| `/usr/share/khelpcenter/plugins/kdos.desktop`, `kfs.desktop` | The two books in the Help Center | `kdos-doc` |
| `/usr/share/plasma/plasma-welcome/extra-pages/` | The KDOS pages of the Welcome Center | `kdos-welcome` |
| `/usr/share/plasma-systemmonitor/kdos-overview.page`, `/usr/share/plasma-systemmonitor/kdos.page`, `/usr/share/ksysguard/sensorfaces/org.kdos.meters/` | System Monitor's Overview and KDOS pages, and the KDOS Meters face | `ksystemstats-kdos` |
| `/usr/share/swcatalog/xml/kdos-ports.xml` | The AppStream catalogue of the ports this root has installed, which Discover lists | `70_image`'s `086_swcatalog.sh`, and `kdos-discover-catalog` after every install |
| `/usr/share/swcatalog/xml/kdos-debian-exclude.xml` | `remove-component` merges that take the Debian applications `debian-exclude` names out of every AppStream pool, KRunner's software results included | `70_image`'s `086_swcatalog.sh` |
| `/usr/share/fwupd/remotes.d/kdos-snapshot.conf`, `kdos-snapshot/` | The fwupd remote that reads the firmware metadata snapshot offline, and the snapshot, `firmware.xml.zst` with its signature | `fs/`, and `fwupd-snapshot` |
| `/usr/share/doc/kdos/`, `/usr/share/doc/kfs/` | The two books as HTML | `kdos-doc`, `kdos-kfs` |
| `/usr/share/doc/HTML/en/kdos` | A link to `/usr/share/doc/kdos`, through which `help:/kdos/<page>.html` reaches the book | `kdos-doc` |

### `/usr/local/bin/` — session commands and application shims

Holds `kdos` and the other session-side names of the `ksvc` binary, `kdos-appbox` with its names
`kdos-box` and `xdg-open`, and the session scripts from `fs/`; the
[command index](command-index.md#where-the-commands-live) lists them. It is also where the build puts a
*shim*, a symbolic link to `kdos-appbox` named after a boxed application. Running a shim runs
`kdos-appbox`, which dispatches on the name it was called by. The build writes one shim per
application in a pack the medium carries (`kdos-appbox genlaunchers`, from
`script/phases/70_image/030_launchers.sh`); the default medium carries no packs, so it has none. An application you install
gets its shim in `~/.local/bin` instead.

`/usr/local/lib/kdos/` holds what those scripts share: `session-common.sh`, sourced by
`kdos-session-wrapper` and Plasma's environment script; `timers.sh`, the timer-table parser; and
`user-timers`, the daemon of the `timers` user service.

### `/var/lib/kpkg/` — the host package database

The state and caches of `kpkg`, the host package manager. Every directory is a default in
`/etc/kpkg.conf`, which is a list of `NAME="${NAME:-default}"` lines.

| Path | Holds |
|---|---|
| `/var/lib/kpkg/db/<name>` | One entry per installed package, with its file list |
| `/var/lib/kpkg/db/.recipe/<name>` | The recipe hash a package was installed from |
| `/var/lib/kpkg/db/.pkgsha/<name>` | The SHA-256 of the package file it was installed from, which the build's [package store](../03-architecture/packaging.md#the-package-store) keys dependent ports on |
| `/var/lib/kpkg/db/.files/<name>` | The SHA-256 and mode of every regular file the install placed, which [`kpkg check`](../03-architecture/packaging.md#checking-an-installed-root) compares the root against |
| `/var/lib/kpkg/db/.lock` | The database's writer lock: `kpkgadd` and `kpkgdel` hold an exclusive `flock` on it while they change the tree, so one runs at a time |
| `/var/cache/kpkg/sources/` | Downloaded source files (`SOURCE_DIR`) |
| `/var/cache/kpkg/packages/` | Built package archives (`PACKAGE_DIR`), and the `<name>.pending` record of each package `kpkg install --build-only` built and `--commit` has not yet installed |
| `/var/cache/kpkg/work/` | Build working directories (`WORK_DIR`) |

The ports tree `kpkg` builds from is named by `PORT_REPO`: a space-separated list of recipe
directories, each searched for a port at `<name>/` and one shelf down at `<shelf>/<name>/`. Its
default is the tree every image carries at `/ports`, recipes without their sources: `/ports/core`,
and `/ports/src/system`, `art`, `desktop` and `daemons` for KDOS's own programs, beside
`/ports/shelves`, `/ports/sources.idx` and `/ports/TREE`, which names the commit the tree is and
that commit's time. The two books are there too, under `/ports/docs`, for the programs that
render them. `kdos update fetch` replaces the whole of `/ports` with a newer signed tree. Every port is listed in
[The ports catalogue](ports-catalogue.md). No source archive is on the image and `/var/cache` is
not carried into the installed system, so a build on the machine needs each source it names
fetched over the network; a medium made
with `KDOS_ISO_SOURCES=1` also carries the whole tree, every port with its fetched sources, under
`/mnt/iso/sources`, and
[Keeping it current](../02-user-guide/administration.md#keeping-it-current) gives the `PORT_REPO`
that names it.

### The boot medium

A live session mounts its boot medium at `/mnt/iso`. The installer and the pack daemon read it
there. While it installs, the installer mounts the target root beside it at `/mnt/kinstall`, and
the target's ESP at `/mnt/kinstall/boot/efi`.

| Path | Is |
|---|---|
| `/mnt/iso/system.sfs` | The root filesystem image |
| `/mnt/iso/packs/` | Packs available on the medium; the pack daemon lists them as `available` without copying them. Only a medium built with `KDOS_PACK_KDOS=1` has one, holding the base pack `kdos.kpack` |
| `/mnt/iso/appstore/catalogue` | Where the installer looks for the application catalogue when `/usr/share/kdos/appstore/catalogue` is missing; the build puts none there |
| `/mnt/iso/sources/` | The KDOS tree that built the image, present only on a medium built with `KDOS_ISO_SOURCES=1`; `kdos rebuild` reads it |

### `/run/` — system runtime

Emptied at every boot.

| Path | Is |
|---|---|
| `kdos-powerd.sock`, `kdos-energyd.sock`, `kdos-oomd.sock`, `kdos-packd.sock` | The root daemons' sockets; see [Sockets](#sockets) |
| `kdos-svc.<name>.pid` | The process ID of the `ksvc` supervisor running service `<name>`. The supervisor leads its own process group, so stopping the service kills the daemon under it too. A daemon's own pid file, often `<name>.pid` here, is not read |
| `kdos-svc.<name>.log` | A supervised service's output, capped at 64 KiB with one `.old`. Every line also goes to syslog |
| `kdos-svc.<name>.state` | The supervisor's one line about its daemon: `up` or `wait`, the daemon's pid, since when (seconds since boot), its failing exits in a row, its last exit status and the stop grace the supervisor was started with. `service status` reads it to report a crash loop as `[FAIL]`; see [ksvc and service](../04-programs/kdos-command.md#ksvc-and-service) |
| `kdos-svc.<name>.stopping` | Empty; exists while `service stop <name>` runs, so the supervisor does not pass the group a second `SIGTERM` |
| `kdos-init.<name>.log` | What a service's start script printed at boot |
| `kdos-getty.<tty>.log` | `kdos-getty`'s messages for that terminal |
| `kdos-fsck.log` | What the boot-time filesystem check reported, written only when it said something |
| `user/<uid>/` | Each logged-in user's runtime directory, made by `turnstiled` at the first login and removed after the last; see [`$XDG_RUNTIME_DIR`](#xdg_runtime_dir--per-user) |
| `kdos/kdos-dm.starts` | The times `kdos-dm` last started SDDM, from which it decides that SDDM keeps exiting |
| `kdos/bless/userspace`, `graphical`, `confirmed` | The two marks a trial boot is confirmed by, and the record that it was; see [Boot files](#boot-files) |
| `kdos/bootstate.json`, `kdos/boot-entries.json` | The boot state and the boot entries, published by `kdos-bootctl` for programs that are not root: `kdos-notice`, the System Versions page, the logout screen's boot menu |
| `kdos/sysupdate/io.kdos.system.metainfo.xml` | The AppStream text of the offered system update, which `kdos-sysupdated` serves to Discover |
| `kdos/slot-<a\|b>/` | Where `kdos update apply --mount` mounts an inactive slot nobody mounted |
| `systemd/reboot-to-boot-loader-entry` | The boot entry the logout screen's *Restart into* chose for the next boot; `kdos-bootctl` reads it. Only the path is systemd's |
| `utmps/` | Owned by `utmp`: the live login records (`utmp`) and the sockets of `utmpd` and `wtmpd` (`.utmpd-socket`, `.wtmpd-socket`), which libutmps talks to; see [Login records](../03-architecture/boot-and-init.md#login-records) |

### Other system state

| Path | Holds |
|---|---|
| `/var/lib/sddm/` | The `sddm` account's home: the greeter's `kdeglobals`, `plasmarc` and `kcminputrc` in the accent, and `kdos-background.png`. The image ships the default accent's; the installer and `kdos-powerd` (as that account) replace them |
| `/var/log/sddm.log`, `sddm.log.old` | SDDM's own log; a session's output is in its user's `~/.local/share/sddm/wayland-session.log` |
| `/var/lib/elogind/linger/` | One file per user whose services keep running with nobody logged in (`loginctl enable-linger`) |
| `/var/spool/cron/` | Each user's crontab, kept by `cronie` |

### Boot files

An installed machine set up for A/B updates has two root filesystems, slots `a` and `b`. A *trial
boot* starts the candidate slot, and the machine keeps it only when `kdos-bootctl mark-good` runs
for that boot; otherwise the next boot returns to the active slot. `kdos-sysupdated` runs it once
both marks under `/run/kdos/bless/` exist: `userspace`, which the end of `rcS` writes when every
service has started, and `graphical`, which a Plasma session that stayed up for a minute writes
through `io.kdos.Slots1.Bless` (`kdos-notice --bless`), or the login screen staying up for a minute
on a machine nobody logs in to. See
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

| Path | Is |
|---|---|
| `/boot/initramfs.cpio.gz` | The image's initramfs |
| `/boot/initramfs.modules` | The kernel modules the initramfs carries, one per line |
| `/boot/initramfs-kdos.cpio.gz` | The image's initramfs with a newer kernel's modules appended, written when the `linux` package is upgraded |
| `/boot/efi/EFI/kdos/<slot>/` | Each A/B root slot's `vmlinuz` and `initramfs.cpio.gz`, on the EFI system partition, for slots `a` and `b` |
| `/boot/efi/EFI/kdos/vmlinuz`, `initramfs.cpio.gz` | A flat pair that boots either slot. Nothing in KDOS writes one (`kinstall` puts slot `a`'s pair in `EFI/kdos/a/`); where one exists, `kdos-bootctl` boots it for a slot with no directory of its own and deletes it once no menu entry names it |
| `/boot/efi/EFI/kdos/bootstate` | Each slot's root UUID and encrypted container, which slot is active, and whether a trial boot is pending with its attempt count; `kdos-bootctl` owns it |
| `/boot/efi/EFI/kdos/trial/` | A trial boot's copy of the loader, present only while a trial is pending |

### Per user

| Path | Holds |
|---|---|
| `~/.config/kdos/` | Your KDOS configuration; every file's keys are in [Configuration](configuration.md) |
| `~/.config/kdosrc` | KDOS's KConfig file: the desk in force and when it was laid out |
| `~/.config/kdos/accent` | The accent in force, one word |
| `~/.config/kdos/phosphor` | Present while Phosphor mode is on: what it changed, so `off` restores it |
| `~/.config/kdos/boxes/<name>.conf` | A box's profile |
| `~/.config/kdos/sandbox/<profile>.conf` | A `kdos sandbox` profile |
| `~/.config/kdos/favorites` | The boxes warmed up at login, and the KDOS runner's two-letter codes |
| `~/.config/kdos/timers.d/` | This account's periodic jobs; the default account ships `10-backup.timer` and `20-updatedb.timer` |
| `~/.config/kdos/backup.conf`, `backup.pass` | What `kdos backup run` copies, and the repository's password |
| `~/.config/kdos/a11y` | Present means boxed applications always get the accessibility bus |
| `~/.config/kdos/checklist` | The first-run checklist items you dismissed |
| `~/.config/kdos/desk-undo/` | The panels as they were before the last `kdos desk`, which `kdos desk undo` puts back |
| `~/.config/kdos/library-place` | Present once the `library:/` entry has been added to Dolphin's Places, so removing it keeps it removed |
| `~/.config/kdos/user-services.disabled` | The user services `service --user disable` turned off |
| `~/.config/kdos/apps-offered` | Written once the login has sent the notification offering the applications in `/var/lib/kdos/apps-pending`; delete it to be offered them again |
| `~/.config/plasma-workspace/env/` | Your own scripts that set variables for every Plasma program, run after KDOS's |
| `$XDG_DATA_HOME/kdos/alien-apps` | Your own launcher table (`~/.local/share/kdos/alien-apps` when `XDG_DATA_HOME` is unset); its entries win over the system one |
| `~/.local/share/kdos/boxes/<name>/` | A box's writable layer (`upper`, `work`), its `snapshots/<tag>/`, and its home when the profile says `home = private` |
| `~/.local/share/kdos/packs/` | Where a data pack's contents are grafted for boxed applications |
| `~/.local/share/kdos/desk-backup/` | A dated copy of the panels from before each `kdos desk`, which nothing removes |
| `~/.local/share/applications/` | Launchers for applications you installed |
| `~/.local/bin/` | Shims for applications you installed |
| `~/.local/share/user-places.xbel` | Dolphin's Places, which `kdos places add` adds to |
| `~/.local/share/sddm/wayland-session.log` | The output of the last Plasma session SDDM started for you |
| `~/.local/state/kdos/session-boxes` | The boxes, and the applications in them, that were running when the last session ended: `box<TAB><name>` and `app<TAB><box><TAB><launcher-id><TAB><argument>…`. Written by `kdos-appbox session save` at logout and read by `session restore` at login |
| `~/.local/state/kdos/window-memory` | Where each program's window last closed, for the window memory script |
| `~/.local/state/kdos/notice` | What `kdos-notice` last posted about each kind of notice, so the same news is not posted at every login |
| `~/.local/state/kdos/kglobalshortcutsrc.seeded` | Present once the KDOS chords have been added to your shortcuts, so a chord you removed stays removed |
| `~/.local/state/kdos/fwask` | The services the firewall's question has already asked about, per network |
| `~/.cache/kdos/plocate.db` | This account's file index, built from `$HOME` by `kdos-updatedb` at the first login that finds none and rebuilt nightly; `LOCATE_PATH` names it |
| `~/.cache/kdos/runner-books` | The KDOS runner's index of the two books, rebuilt when `kdos-doc` or `kdos-kfs` is reinstalled |
| `~/.cache/kdos/library-owners` | Which pack or package owns each item of the offline library, trusted until kpkg's database changes |
| `~/.local/share/Trash/` | The freedesktop trash |
| `~/Mail/` | The account's Maildir |
| `~/.local/share/whisper.cpp/models/` | Speech models, `ggml-*.bin`. `kdos speech get` writes here; the desktop only searches it |

A speech model is looked for in the order `$KDOS_WHISPER_MODEL`, then
`~/.local/share/whisper.cpp/models/`, then `whisper.cpp/models/` under each system data directory.

`~/Mail` is named by `~/.mbsyncrc` and by notmuch's `mail_root`, and is created for the default
account when the image is built. Both programs report an error on a missing directory rather than
creating one, so a new account needs `mkdir ~/Mail`.

### `$XDG_RUNTIME_DIR` — per user

`/run/user/<uid>`, mode 0700, on a tmpfs that does not survive a reboot. `turnstiled` makes it at
your first login, from any console, over SSH or through SDDM, and removes it after your last. The
session's own sockets are here; a box is given a private directory of its own instead, under
`kdos/box/<box>`, holding only what it may reach.

| Path | Is |
|---|---|
| `bus` | The session message bus (D-Bus), started by the `dbus` user service. Plasma, every box and every user service are on this one bus; `kdos-session-wrapper` refuses to start a session that cannot reach it |
| `wayland-1` | KWin's Wayland socket: the first free `wayland-N` from 1, which is `wayland-1` in a session alone on the machine |
| `ksvc/` | The user services' supervisor files: `kdos-svc.<name>.pid`, `kdos-svc.<name>.state` and `kdos-svc.<name>.log`, as `/run` holds the system's, and `kdos-user-services.log` |
| `kdos/user-timers.pid` | The `timers` user service's process ID, written once both timer tables have been read |
| `kdos/stay-awake.pid` | The process holding the `stay-awake` inhibition, while the switch is on |
| `kdos/box/<box>/` | A box's private runtime directory, which the box sees as its own `/run/user/<uid>`: its tagged Wayland sockets `wl[.<stem>]@<display>` from `kdos-boxsock`, `dbus-proxy`, its filtered session bus, hard links to the session's `pipewire-0` and `pulse/native`, and `at-spi/` only when the box is given the accessibility bus. The session's own `wayland-1` and `bus` are never in it, nor the document portal's mount |
| `kdos/boxes/<box>.state` | Which boundary held for the box at its last launch: `confined` or `tagged` |
| `kdos/boxes/<box>.apps` | The box's launch ledger: one line per running launch, marked when it ended during a logout |
| `kdos/boxes/<box>/root` | The merged root of a box composed from packs |
| `kdos/boxes/<box>/upper`, `work` | A composed box's writable layer when `$HOME` cannot hold one (a live session, whose home is itself on an overlay); lost at logout. The launch prints nothing about it: `kdos-appbox status` marks such a box `ephemeral`, and `kdos-box freeze` warns before freezing one |
| `ssh-agent.socket` | The account's `ssh-agent`, the `ssh-agent` user service (`/etc/kdos/user-services/15_ssh-agent.sh`); `SSH_AUTH_SOCK` names it |
| `podman/podman.sock` | The rootless Podman API, while `podman system service` runs; `kdos-podman-api` starts it on demand |
| `pipewire-0`, `pulse/native` | PipeWire's socket and the `pipewire-pulse` socket. See [Reaching the graph from inside a box](../03-architecture/session.md#reaching-the-graph-from-inside-a-box) |
| `kdos-appbox.trace` | Stage timings for the most recent boxed launches |
| `kdos-appbox.warmup.lock`, `kdos-appbox.create.lock` | Serialise the login warm-up and box creation, so two launches do not build one box twice |
| `kdos-appbox.label.<label>` | Whether the box image carries that label, cached so `kdos-appbox` asks `podman image inspect` once a boot rather than at every launch |
| `kdos-sfx.route` | Which player the sound effects use, cached; only `pw-cat` is cached |

## Protocol conventions

The four root daemons with a socket share one shape:

- **One socket in `/run`, mode 0666.** Anyone can connect; the mode is not the gate.
- **Authorisation is the caller's user ID, read from the kernel** (`SO_PEERCRED`), never anything in
  the message. Root and members of `wheel` are always allowed; each socket's section says who else
  is.
- **One request per connection:** a short line in, a reply out, then the socket closes. There is no
  session state.
- **Replies** are `ok`, optionally followed by data, or `err <reason>`. A caller who is not allowed
  gets `err not permitted`, and a verb the daemon does not know gets `err unknown command`.
- **Each verb takes a fixed number of words**, with two exceptions: `compose` on `kdos-packd`
  takes a box name and up to 32 pack IDs, and `firewall` on `kdos-powerd` takes `list`, `knocks`
  or a service and `on`/`off`. `kdos-packd` stops reading a line after 34 words and acts on what it
  has.
- **No verb takes a path.** Where a request names a thing, it names it by an identifier the daemon
  published in a list. The single exception is the pack daemon's `install`, which names a file in
  its own staging directory.
- **Test modes.** `kdos-energyd`, `kdos-oomd` and `kdos-packd` accept `--fixture`, which reads
  recorded state instead of the machine's and prints what it would do instead of doing it.
  `kdos-powerd --explain USER` prints which verbs that user may use.

You can try any of these by hand with `socat`, which the image ships:

```sh
printf 'ping\n' | socat - UNIX-CONNECT:/run/kdos-oomd.sock
```

The daemons System Settings and Discover talk to are on the system bus as well, or only; see
[D-Bus names](#d-bus-names).

## Sockets

### `/run/kdos-powerd.sock`

The firewall's named services and the look of the machine before a login, served by
`kdos-powerd`; the client is `kdos-power`, the same binary under a second name. The same verbs are
on the system bus as `io.kdos.Power1`.

| Verb | Argument | Who | Does |
|---|---|---|---|
| `ping` | none | anybody | Liveness |
| `firewall` | `list` | anybody | The firewall's named services, one row each, and whether each is open |
| `firewall` | `knocks` | anybody | For each closed service, how many packets were turned away |
| `firewall` | `<service> on` / `<service> off` | root and `wheel`; anyone else as polkit's `io.kdos.firewall.set` answers without a prompt, which by default is no | Opens or closes one, rewriting `/etc/nftables.d/50-kdos-services.nft` and reloading the ruleset after `nft --check` passes |
| `accent` | One of the eight accent names: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect`, `paper` | root and `wheel`; anyone else as `io.kdos.accent.set` answers, which by default is yes for the person at the machine | Writes `/etc/kdos/accent`, runs `kdos-bootctl theme`, which recolours the boot menu and `/etc/vtrgb`, and copies that accent's greeter files into the `sddm` account's home. It does not change the desktop; `kdos theme` does that |
| `splash` | `phosphor` or `modern` | as `accent` | The boot splash's motion: `kdos-bootctl splash`, which sets `kdos.splash=` on every KDOS boot entry |

`kdos-power suspend`, `poweroff` and `reboot` never reach this socket: the client runs `loginctl`,
and elogind decides, so there is one path to the power state.

The firewall takes names, never ports: a client that can only name `ssh` can open exactly what the
daemon's table says `ssh` is. `ssh` is a daemon as well as a port: `firewall ssh on` also removes
`/etc/service.disabled/sshd` and starts `sshd`, and `off` puts the marker back and stops it, so a
closed port never hides a listening daemon.

### `/run/kdos-energyd.sock`

Per-application energy use, from `kdos-energyd`; the client is `kdos-energy`. Root and members of
`wheel`.

| Verb | Argument | Answers |
|---|---|---|
| `report` | none | Each application's share of attributable energy, the idle floor, and the sample count |
| `report-json` | none | The same, as JSON |
| `ping` | none | Liveness |

The raw energy counter and the sampling interval are never published, and the interval is fixed by
the daemon, so this socket cannot be turned into a fine-grained power measurement instrument (a
known side channel). The daemon refuses to start on a machine whose energy counter it cannot read,
rather than report a machine that uses no energy.

### `/run/kdos-oomd.sock`

The out-of-memory killer, `kdos-oomd`. Root, members of `wheel`, and the person at the machine: a
user whose session elogind reports as active. The daemon waits on a kernel pressure-stall
information (PSI) trigger: a threshold on the time tasks spend stalled for memory.

| Verb | Argument | Answers |
|---|---|---|
| `status` | none | `ok trigger '<psi trigger>', kills <n>`, and the last kill when there has been one |
| `protected` | none | The process names it never kills: the desktop's (`kwin_wayland`, `plasmashell`, `ksmserver`, `kded6` and the rest), the session's plumbing and the root daemons |
| `ping` | none | Liveness |

Nothing in this protocol names a process, so it cannot be used to aim a kill. Killing is the
daemon's own decision or it does not happen.

### `/run/kdos-packd.sock`

The pack daemon, `kdos-packd`, which installs, mounts and composes application packs. Root and
members of `wheel`. Thirteen verbs.

| Verb | Argument | Answers |
|---|---|---|
| `list` | none | Every pack the machine can see, one row each: ID, version, kind, state (`mounted`, `installed` or `available`), size, and origin (`store` or `medium`) |
| `info` | A pack ID | Its metadata |
| `mount` | A pack ID | |
| `unmount` | A pack ID | |
| `compose` | A box name, then one or more pack IDs | The merged root of those packs, for that box |
| `decompose` | A box name | |
| `install` | A file name in the staging directory, and optionally a second one naming a signed index (its signature staged as `<index>.sig`); the pack is copied into the store, not renamed, and removed once the install succeeds, and the index is left for the caller to remove | |
| `remove` | A pack ID | |
| `rollback` | A pack ID | |
| `graft` | A data pack ID | |
| `ungraft` | A data pack ID | |
| `status` | none | The store, medium and staging directories, the retention count, how packs are mounted (`file-backed`, `loop device`, or `not yet used` before the first mount), pack and box counts, and each composed box's merged root, marked `ephemeral` or `persistent` |
| `ping` | none | Liveness |

`status` publishes the staging directory and the retention count, so a client writing a download
does not have to work either out.

Every ID comes from `list`. The single exception is `install`, which names a file in the daemon's
own staging directory, `/var/lib/kdos/packs/staging`, the one place an unprivileged user may
write. Relative traversal and absolute paths are both refused. Installing copies the staged file, and
the index the request names with its `.sig` when it names one, into private root-owned files in the store;
the daemon verifies the copy, renames it into place as `<id>.kpack`, mode 0644, and then removes
the staged file. A staged symbolic link or anything that is not a regular file is refused. Signatures are checked at install and at mount against
`/etc/kdos/keys/packs/`; see [Packs and boxes](../03-architecture/packs-and-boxes.md).

A composed box's merged root is `$XDG_RUNTIME_DIR/kdos/boxes/<box>/root`, and its writable layer
is `~/.local/share/kdos/boxes/<box>/upper` where the home directory's filesystem can hold an
overlay upper, and a tmpfs directory beside the root where it cannot. A `graft` makes symbolic links
under `/usr/share` for the host and under `~/.local/share/kdos/packs` for boxes, and records each
link in `/var/lib/kdos/packs/pack-manifest`; `ungraft` removes only links, only those the manifest
names.

## D-Bus names

KDOS's own names are under `io.kdos`. Three daemons also serve names systemd defines, so that
upstream KDE programs reach them with no patch. Every method that changes something asks polkit
about the caller's bus name; the action names are in [The security model](../03-architecture/security-model.md).

### On the system bus

| Name | Object | Served by | For |
|---|---|---|---|
| `io.kdos.Power1` | `/io/kdos/Power1` | `kdos-powerd` | `ListFirewallServices`, `ListFirewallKnocks`, `SetFirewallService` (`io.kdos.firewall.set`), `SetAccent` and `SetSplash` (`io.kdos.accent.set`), `StartFileSharing` (`io.kdos.filesharing.start`); the properties `Accent` and `Accents`; the signal `FirewallServicesChanged`. System Settings' Firewall and KDOS Look pages, and `kded-kdos` at the login of a `sambashare` member |
| `org.freedesktop.hostname1` | `/org/freedesktop/hostname1` | `kdos-settingsd` | The host name, pretty name, chassis and the machine's identity, over `/etc/hostname`, `/etc/hosts` and `/etc/machine-info` |
| `org.freedesktop.timedate1` | `/org/freedesktop/timedate1` | `kdos-settingsd` | The time zone, the clock, and *Set time automatically*, over `/etc/localtime`, `/etc/profile.d/20-timezone.sh`, the Wi-Fi country and the `chrony` service |
| `org.freedesktop.locale1` | `/org/freedesktop/locale1` | `kdos-settingsd` | The locale and the console and graphical keyboard layouts, over `/etc/locale.conf`, `/etc/keymap` and `/etc/kdos/xkb.conf` |
| `org.freedesktop.sysupdate1` | `/org/freedesktop/sysupdate1`, one target `host` | `kdos-sysupdated` | System updates, as Discover's sysupdate backend asks for them: `ListTargets`, then on the target `GetVersion`, `GetAppStream`, `CheckNew`, `List`, `Describe`, `Acquire` (the download and the build, which shows progress), `Install` (arming the inactive slot) and `Vacuum`, with a job object per running operation |
| `io.kdos.Slots1` | `/io/kdos/Slots1` | `kdos-sysupdated` | The A/B slots for the System Versions page and the update notifier: the properties `Configured`, `Live`, `RunningSlot`, `ActiveSlot`, `TrySlot`, `TryAttempts`, `TryBootNext`, `Channel` and `State`; `ListSlots`, `Try`, `CancelTry`, `SetChannel`, and `Bless`, the session's half of a trial boot's confirmation; the signal `SlotsChanged` |
| `org.freedesktop.login1` | `/org/freedesktop/login1` | `elogind` | Sessions, seats, power and lid; what Plasma, SDDM and `kdos-power` ask |
| `org.freedesktop.Accounts` | `/org/freedesktop/Accounts` | `accountsservice` | The accounts System Settings' Users page and SDDM list |

`kdos-settingsd` serves the three `org.freedesktop` names with systemd's own methods, properties
and polkit action names, so the Date & Time, Region and Keyboard pages, `plasma-setup` and KWin's
keyboard layout all work unchanged. `kdos-sysupdated` serves the sysupdate API exactly as Discover
6.8.2 calls it, and nothing on `localhost:3129`; the update's size reaches Discover as the download
size of an artifact of the offered release in the AppStream file it names.

### On the session bus

| Name | Object | Served by | For |
|---|---|---|---|
| `io.kdos.Runner` | `/runner`, `/library` | `kdos-runner`, started by the bus on the first query | KRunner's D-Bus runner interface, `org.kde.krunner1`: the KDOS rows (kdos verbs, favourites, routes, packs on the boot medium, shortcuts, `lib`, `ask`) on `/runner`, the offline library on `/library` |
| `io.kdos.Notice` | `/io/kdos/Notice` | `kdos-notice` | `io.kdos.Notice1.Rescan`: look at every source of notices now |
| `org.kde.kded6` | `/modules/kdos`, interface `io.kdos.Kded1` | the `kded-kdos` module | Keeps the accent in step with the global theme chosen in System Settings, and offers a set of add-ons found on a stick |
| `org.kde.KWin` | `/io/kdos/Crt1`, interface `io.kdos.Crt1` | KWin's `kdos_crt` effect | Phosphor mode's pass: its preset and the power-down and degauss motions `kdos phosphor` asks for |
| `org.mpris.MediaPlayer2.mpd` | `/org/mpris/MediaPlayer2` | `kdos-mpctl mpris` | mpd as an MPRIS player, while mpd runs |

The `kdos` command calls upstream names on the session bus as well: `org.kde.KWin` (`/KWin`,
`/Effects`, `/Scripting`, `/VirtualDesktopManager`, `/org/kde/KWin/NightLight`) for `kdos hey`,
`kdos tiling`, Phosphor mode and the Night Light switch's state; `org.kde.plasmashell` for
`kdos desk` and `kdos panel`; `org.kde.krunner` for `kdos menu`; `org.kde.kglobalaccel` for
`kdos toggle night-light`, which presses KWin's *Suspend/Resume Night Light* action only to lift a
suspension; `org.freedesktop.Notifications` for
`kdos notify`; and each Konsole window's `org.kde.konsole-<pid>` for run-or-raise.

## Files used as an interface

Neither configuration nor storage: these files are how one program tells another something.

| File | Written by | Read by |
|---|---|---|
| `~/.config/kdos/accent` | `kdos theme`, and `kded-kdos` through `kdos theme --from-plasma` | `kdos theme`, `kdos version` and the About page's accent row |
| `/etc/kdos/accent` | The image build, with the default accent; the installer; `kdos-powerd`'s `accent` verb | The boot scripts, which tell the running splash to repaint in it; the Look page and the About page's accent row |
| `/usr/share/kdos/alien-apps` | The build (`kdos-appbox genlaunchers`) | Every shim, and the launcher dispatcher |
| `/usr/local/bin/<name>`, `~/.local/bin/<name>` | The build, one symbolic link to `kdos-appbox` per application in a pack the medium carries (none on the default medium); `kdos app install` for an application you install | You, as an ordinary command; `kdos-appbox` dispatches on the name it was called by |
| `$XDG_DATA_HOME/kdos/alien-apps` (`~/.local/share/kdos/alien-apps` when unset) | `kdos-appbox genlaunchers --packs --user`, and `kdos-appbox install`, `uninstall` and `import`; every one of them regenerates the whole table from the installed packs and the store's boxes together | The same, winning over the system table |
| `~/.config/kwinrulesrc`, groups `kdos-box-<box>` | `kdos-box profile` and `kdos-appbox genlaunchers`, which rewrite only those groups | KWin: each box's title-bar colour |
| `/var/lib/kdos/packs/pack-manifest` | The pack daemon, under `flock` of `pack-manifest.lock`, each change written whole and renamed into place | Itself, so ungrafting removes exactly what was added and every slot's start grafts what it names; `kio-library`, for the pack an item came from |
| `/var/lib/kdos/update.json` | The update-check timer, and `kdos update fetch` and `apply` when they finish | Discover's update notifier, `kdos-sysupdated`, the System Versions page and `kdos-notice` |
| `/run/kdos/bootstate.json` | `kdos-bootctl`, at every change of boot state | `kdos-notice` and the System Versions page |
| `/run/kdos/bless/*` | The end of `rcS` (`userspace`); `kdos-sysupdated` for a session's `Bless` or a greeter that stayed up (`graphical`), and after `mark-good` (`confirmed`) | `kdos-sysupdated` |
| `/etc/os-release` | `fs/etc/os-release`, with `BUILD_ID` and `KDOS_BUILD_DATE` appended by `70_image/045_identity.sh` | About this System, and anything else that identifies the system |
| `/etc/kdos-release` | The build, and `kdos image unpack` with each slot it writes | `kdos update`, `kdos version` and the About page's slot row |
| `<file>.kdos-new` under `/etc` | `kdos image carry`, when a file you edited has a newer default in the new slot | You: the new default, beside your copy, to compare |
| `/boot/initramfs.modules` | The initramfs build step, one module per line | The `linux` package's post-install, which carries a new kernel's copies of that set into `/boot/initramfs-kdos.cpio.gz` |
| `/boot/initramfs-kdos.cpio.gz` | The `linux` post-install: the image's initramfs with the new kernel's modules appended | `kdos-bootctl deploy` and `kinstall`, which use it in place of `/boot/initramfs.cpio.gz` |
| `EFI/kdos/<slot>/vmlinuz`, `EFI/kdos/<slot>/initramfs.cpio.gz` on the EFI system partition | `kinstall` for slot `a`, `kdos-bootctl deploy` for either | The boot loader (Limine), through the `/KDOS` menu entries `kdos-bootctl` regenerates on every change of boot state |
| `EFI/kdos/trial/` on the EFI system partition: a copy of the loader and a `limine.conf` defaulting to the candidate slot | `kdos-bootctl try` on UEFI; removed by every state that is not a pending trial | The firmware, for the one boot `BootNext` asks for |
| `Boot####` "KDOS update trial" and `BootNext` in `efivarfs` | `kdos-bootctl try` on UEFI; removed by `kdos-bootctl mark-good` | The firmware, once |

**What a release carries for an update.** Beside the ISO, the release `<version>` holds the signed
recipe tree `kdos-ports.tar.xz` with its `.sig`, and the signed release index `kdos-update.idx` with
its `.sig`. The slot image (`kdos-image.manifest` with its `.sig`, the whole image as
`kdos-image-<version>.full.NNN` parts, and a delta from each earlier version,
`kdos-image-<version>.from-<previous>.delta`) is in the pre-release `image-<version>`, and the
binhost, every name flat, in the pre-releases `binhost-<version>-01`, `-02`, … of at most 1,000
assets each. `kdos update fetch` finds each file through the index and nowhere else;
`ports/publish --release` writes the index and uploads all of it. See
[Publishing a release](../03-architecture/packaging.md#publishing-a-release).

## Environment variables

KDOS programs and build scripts read more than a hundred distinct `KDOS_*` variables (counted as
`getenv` calls and shell expansions across `src/`, `fs/`, `script/`, `ports/` and `testing/`). The
tables below list those a person may want to set; header guards, internal constants and variables
set only between KDOS's own programs are left out.

### Debugging

| Variable | Effect |
|---|---|
| `KDOS_BB_DEBUG=1` | The `kdos-bb` demo reports how its audio mixer is fed |
| `KDOS_PACKD_VERBOSE=1` | Under `kdos-packd --fixture`, print every step rather than only the result |

### Behaviour

| Variable | Effect |
|---|---|
| `KDOS_A11Y` | `1` gives one boxed launch the accessibility bus, `0` withholds it; unset, a running screen reader or `~/.config/kdos/a11y` decides |
| `KDOS_ALSA_DEFAULT=<pcm>` | The device ALSA's `default` opens. Unset, it is `kdos_pipewire`, the sound server; `kdos_card` sends the program straight to the sound card, which it then holds to itself. The BRLTTY service sets `kdos_card`, since no sound server runs at boot. See [Reaching the graph from a plain ALSA program on the host](../03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host) |
| `KDOS_BOX` | Set inside every box to its name |
| `KDOS_REQUIRE_SIG=1` | `kdos-packd` refuses to `install` a pack with no signature, and `kdos-pack verify` fails on one. An unsigned pack already on the boot medium still mounts |
| `KDOS_PACK_KEY=<file>` | The private key the application store signs exported pack indexes with; without it the index is unsigned and says so |
| `KDOS_UPDATE_TREE=<url or dir>` | Where `kdos update fetch` takes the ports tree from, overriding `tree =` in `/etc/kdos/update.conf` |
| `KDOS_UPDATE_BINHOST=<url or dir>` | Where the release index the binhost is found by is, overriding `binhost =` |
| `KDOS_UPDATE_IMAGE=<url or dir>` | Where the release index the slot image is found by is, overriding `image =` |
| `KDOS_TREE_KEYS=<dir>` | The keyring `kdos update fetch` verifies a tree against, instead of `/etc/kdos/keys/tree` |
| `KDOS_UPDATE_STATE=<file>` | Where `kdos update` records and reads its outcome, instead of `/var/lib/kdos/update.state`; set, `fetch` and `apply` leave the update document alone |
| `KPKG_FETCH=1` | Every `kpkg` build downloads the sources it lacks before verifying them; `kdos update apply --source` sets it |
| `KDOS_SOURCES=<dir>` | The KDOS tree `kdos rebuild` builds from; otherwise the first of `/mnt/iso/sources`, `/kdos` and the current directory that is one |
| `KDOS_WHISPER_MODEL=<file>` | One speech model, named exactly. When set, no directory is searched after it |
| `KDOS_NO_ANIM=1` | Skip the login banner's animation |
| `KDOS_BANNER_DELAY=<seconds>` | The delay between the login banner's raster lines; default `0.012` |
| `KDOS_BANNER_LOGO=<file>` | A picture for the login banner in place of `/usr/share/kdos/logo.txt` |
| `KDOS_NO_SFX=1` | Silence sound effects |
| `KDOS_ASCII=1` | Force the plainest, ASCII-only glyph set in the terminal programs |
| `KDOS_MARCH_RUNS=<n>` | How many samples `kdos march` takes per measurement: default 5, clamped to 3–31 and made odd |
| `KDOS_MARCH_LEVEL=<level>` | The micro-architecture level `kdos march` measures, in place of the best one the CPU supports |
| `KDOS_RUNNER_DEADLINE_MS=<ms>` | How long the KDOS runner waits for the offline library's servers before answering KRunner without them; default 250 |

### Testing seams

These point a program at recorded state instead of the live machine. They are for tests and are
honoured only where noted.

| Variable | Points at |
|---|---|
| `KDOS_CHECKLIST_ROOT` | A recorded root whose `etc/locale.conf` and `proc/self/mounts` `kdos checklist` reads in place of the system's |
| `KDOS_ABOUT_ROOT` | A recorded root the About page's `about-*` scripts read in place of `/` |
| `KDOS_ZONEINFO` | A time-zone database for the installer |
| `KDOS_ENERGY_PROC`, `KDOS_ENERGY_POWERCAP`, `KDOS_ALIEN_APPS` | Recorded trees and a launcher table for the energy daemon |
| `KDOS_BOX_PROFILES` | A directory of box profiles for the out-of-memory killer |
| `KDOS_PACK_STORE`, `KDOS_PACK_MEDIUM`, `KDOS_PACK_MANIFEST`, `KDOS_PACK_SHARE`, `KDOS_PACK_HOME` | Alternative pack store, medium, manifest, host graft root and home directory for the pack daemon |
| `KDOS_PACK_RETAIN` | A retention count in place of `packd.conf`'s `retain` |
| `KDOS_KEYS` | An alternative pack key directory for `kdos-pack` and the pack daemon's fixture |
| `KDOS_POWERD_ETC` | An alternative `/etc` for `kdos-powerd`'s configuration verbs |
| `KDOS_SETTINGSD_ETC`, `KDOS_SETTINGSD_SHARE`, `KDOS_SETTINGSD_SYS` | An alternative `/etc`, `/usr/share` and `/sys` for `kdos-settingsd` |
| `KDOS_SWCATALOG` | The one AppStream catalogue Discover's KDOS backend reads, in place of the system's |
| `KDOS_LIBRARY_ROOT` | A directory put in front of every system path the `library:/` worker reads |
| `KDOS_INITRD` | An alternative boot image, for the microcode check |
| `KDOS_BOOTSTATE` | An alternative boot-state file. With it, no firmware variable is written unless `KDOS_EFIVARS` is also set, and no kernel command line is read unless `KDOS_CMDLINE` is |
| `KDOS_CMDLINE` | A file standing in for `/proc/cmdline`, whose `kdos_slot=` tells `kdos-bootctl mark-good` which slot's kernel is running |
| `KDOS_EFIVARS`, `KDOS_ESP_DISK` | A directory standing in for `efivarfs`, and `<disk image>:<partition number>` standing in for the EFI system partition's disk, for testing the `BootNext` trial |
| `KDOS_LIMINE_CONF`, `KDOS_VTRGB` | Alternative boot-menu configuration and console palette files for `kdos-bootctl` |
| `KDOS_MOUNTS` | A file standing in for `/proc/mounts`, whose `/` of type `overlay` makes `kdos-bootctl` report the live medium |
| `KDOS_POWERD_SOCKET`, `KDOS_ENERGYD_SOCKET`, `KDOS_OOMD_SOCKET`, `KDOS_PACKD_SOCKET` | Move a daemon's socket. This grants nothing, since authorisation never depends on the path |

`kdos-sysupdated --session --root DIR` serves its names on the session bus over a fixture tree,
which is how it and Discover's backend are tested without root.

### Build

Read by `make build` and the build scripts on the build machine; see
[The build system](../05-developer/build-system.md), and
[How KDOS is built](../05-developer/how-kdos-is-built.md) for the build these variables steer.

| Variable | Effect |
|---|---|
| `KDOS_ISO_SOURCES=1` | Copy `src/`, `script/`, `fs/`, `ports/` (every fetched source beside its recipe, without the source cache and the host helpers), the `Makefile` and the `Dockerfile` onto the boot medium as `/sources`, with a `SOURCES` stamp, and the binary host in `build/binhost/` as `/sources/binhost` when the same build sets `KDOS_MAKE_BINHOST=1`; a live session sees it at `/mnt/iso/sources` |
| `KDOS_MAKE_BINHOST=1` | Keep every package the chroot phases build, and have `70_image` write a signed binary host of them to `build/binhost/` |
| `KDOS_ISO_COMP=` | The codec of `system.sfs`: `xz` or `zstd:<1-22>`; empty is `zstd:15`, the release setting. `zstd:3` is a faster developer image about 10% larger. Any other value stops the image step |
| `KDOS_CCACHE=0` | Turn off the compiler cache, default `1`, which CMake ports in the chroot compile through into `build/ccache`; a cached object is byte-identical to a compiled one |
| `KDOS_PKG_STORE=1` | Install a port from the package store `build/pkgstore` when its recipe, environment and dependencies' bytes match a stored package, and store every package built; `check` builds every hit anyway and logs differences to `build/logs/pkgstore-check.log`; default `0`, off. See [The package store](../03-architecture/packaging.md#the-package-store) |
| `KDOS_PKG_STORE_MAX=SIZE` | The size `70_image` evicts the package store down to, least recently used first; default `60G` |
| `KPKG_STORE`, `KPKG_STORE_CHECK`, `KPKG_STORE_SALT` | Set by the build from `KDOS_PKG_STORE`, never by hand: the store directory inside the chroot, check mode, and kdosbuild's hash of the bootstrap phases. `KPKG_STORE_BASE`, from `script/env/chroot.env`, names the ports every store key depends on |
| `KDOS_PACK_KDOS=1` | Also pack this root filesystem as a base pack named `kdos`, written to `build/kdos-base` and carried on the boot medium as `packs/kdos.kpack` |
| `KDOS_JOBS=N` | The job count every phase uses: `MAKEFLAGS=-jN`, `CMAKE_BUILD_PARALLEL_LEVEL` and `CARGO_BUILD_JOBS`, and a `--cpus` cap on the build container no higher than the host's thread count. Unset or empty, `script/env/common.env` computes it: the thread count, clamped to one job per 2 GiB of memory, at least 1. Under `kdosbuild --port-jobs` it is the size of the jobserver at `build/jobserver` the ports of a level share |
| `KDOS_CPU_SHARES=N` | The build container's CPU weight, a `make build` variable only; default `256`. A weight, not a cap |
| `KDOS_REPLAY=1` | A build step's "already done" guard stands down. Set for steps a build plan named explicitly |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY` | Recorded in each phase's snapshot manifest as `git_commit` and `git_dirty`, and shown by the snapshot picker, which marks a snapshot stale when either disagrees with the tree. `70_image/045_identity.sh` also stamps them into the image's `/etc/os-release` as `BUILD_ID` |
| `KDOS_GIT_DATE` | The commit's date, `YYYY-MM-DD`, set by `make build`; stamped into `/etc/os-release` as `KDOS_BUILD_DATE` |
| `KDOS_GIT_TIME` | The commit's time in Unix seconds, set by `make build`; stamped into `/ports/TREE`, which `kdos update fetch` orders two trees by |
| `KDOS_SNAPSHOT_PATHS`, `KDOS_SNAPSHOT_EXCLUDE`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC` | A phase's metadata block in its `script/phases/<phase>/phase.env`. The orchestrator parses these keys from the file's own text without sourcing it, so it follows none of the file's `source` lines and each `phase.env` sets them itself; the phase's steps source the whole file as shell. Each `KDOS_SNAPSHOT_EXCLUDE` pattern is matched with `fnmatch(3)`, no flags, against an entry's whole path relative to `build/`, so `*` crosses `/`, and an excluded directory is left out whole |
| `KDOS_RES=WxH` | The virtual screen size for `make run` and its variants; default `1920x1080` |

The build runs its later steps inside a chroot entered with a cleared environment, so a variable a
chroot step reads must also be named in `script/chroot/exec.sh`. Exactly fourteen are forwarded:
`KDOS_REPLAY`, `KDOS_JOBS`, `KDOS_ISO_SOURCES`, `KDOS_PACK_KDOS`, `KDOS_ISO_COMP`, `KDOS_CCACHE`,
`KDOS_PKG_STORE`, `KDOS_PKG_STORE_MAX`, `KPKG_STORE_SALT`, `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY`,
`KDOS_GIT_DATE`, `KDOS_GIT_TIME` and `KDOS_MAKE_BINHOST`, which also
arrives as `KPKG_KEEP_CACHE`; `KDOS_PKG_STORE` also arrives as `KPKG_STORE` and
`KPKG_STORE_CHECK`. A new one added to the `Makefile` and not
there reaches every host step and no chroot step; see
[Entering the chroot](../05-developer/how-kdos-is-built.md#entering-the-chroot).

### Sources

Read by `ports/fetch` (`make fetch`), `ports/publish` and the pre-push hook, which fetch and archive
the upstream source tarballs. The shared defaults are set in `ports/srclib.sh`. How `make fetch`
resolves a source and when the pre-push hook refuses a push are in
[Writing ports](../05-developer/writing-ports.md) and
[Fetching](../05-developer/how-kdos-is-built.md#fetching-the-only-step-that-uses-the-network).

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The GitHub repository whose releases hold the source archive |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | Where archived sources are downloaded from, as `$KDOS_SOURCES_BASE/<tag>/<asset>` (parts as `<asset>.partNN`), with the tag and asset from the index. A directory laid out the same way works as an archive disk. Set it empty to fetch from upstream only |
| `KDOS_SRCCACHE` | `ports/.srccache` | The local source cache, one file per hash, stored as `sha256-<first two hex digits>/<hash>`. Point two checkouts at one cache to download each file once |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | The index saying which archive volume (`sources-<N>`, or a legacy `src-<shelf>`) and asset hold each hash; read only in format 2 |
| `KDOS_RELEASE_CAP` | `1000` | Assets one archive volume may hold, each part of a split file counted; `ports/publish` puts a file its volume has no room for into the lowest-numbered volume with room, or a new one. GitHub's asset limit. `ports/publish --release` also cuts a binhost into releases `binhost-<tag>-NN` of at most this many, and refuses a slot image of more |
| `KDOS_VOLUME_FILL` | `650` | The assets per volume `ports/publish --plan` packs shelves to when it gives shelves without a volume one |
| `KDOS_PART_SIZE` | `1992294400` (1,900 MiB) | A file larger than this is stored in parts of this size, under GitHub's 2 GiB asset limit. Changed only by tests |
| `FETCH_JOBS` | `1` | How many ports `ports/fetch` (and `make fetch`, `make fetch-check`) resolves or checks at once. Two ports naming one hash take turns on it; vendor bundles are still generated one at a time |
| `KDOS_FETCH_HOST=1` | unset | Run `ports/fetch` entirely on this machine, generating vendor bundles with its own toolchains, instead of handing missing bundles to the fetch container |
| `SOURCE_DATE_EPOCH` | `1735689600` | The timestamp written into generated vendor bundles, so they are reproducible |
| `KDOS_SOURCES_TOKEN` | read from `~/.config/kdos/sources-token` (mode 600) | The GitHub token `ports/publish` uploads with |
| `KDOS_REPO` | `kunaldawn/kdos` | The repository whose release `<tag>` `ports/publish --freeze <tag>` attaches `sources.sha256` to, and `ports/publish --release <tag>` fills |
| `KDOS_RELEASE_KEY` | `build/keys/kdos-release.key` | The secret key `ports/publish --release` signs `SHA256SUMS`, the recipe tree and the release index `kdos-update.idx` with, when `--key` is not given; the `.pub` beside it is uploaded as `kdos-release.pub` |
| `KDOS_PUBLISH_DELAY` | `8` | Seconds between uploads |
| `KDOS_PUBLISH_JOBS` | `16` | Archive `HEAD`s `ports/publish` runs at once to check presence |
| `KDOS_LFS_STORE` | the clone's `lfs/objects` | Where `ports/publish --history` reads Git LFS (Large File Storage) objects that earlier commits' recipes name, to archive them under their hashes |
| `KDOS_GITHUB_API`, `KDOS_GITHUB_UPLOADS` | `https://api.github.com`, `https://uploads.github.com` | Replace the API and upload endpoints, for testing against a local stand-in |
| `KDOS_ALLOW_UNVERIFIED=1` | unset | Let `kpkg` build from a source file whose recipe has no `sha256`, and quiet `ports/fetch`'s warning about it. Without it such a file is refused. `ports/fetch`, and `kpkg install` or `kpkgbuild` run by hand, read it; a `make build` step in the chroot does not see it (see [Build](#build)) |
| `KDOS_SKIP_PUBLISH_CHECK=1` | unset | Skip the pre-push hook's check that every source a pushed recipe names is archived. The layout, patch and book checks still run |
| `KDOS_SKIP_LAYOUT_CHECK=1` | unset | Skip the pre-push hook's offline check of the ports layout: every recipe one listed shelf down, one name per port, no reserved or port-named shelf |
| `KDOS_SKIP_PATCH_CHECK=1` | unset | Skip the pre-push hook's check that every new or changed patch has a `Subject:` line |
| `KDOS_SKIP_BOOK_CHECK=1` | unset | Skip the pre-push hook's run of `testing/docscheck.sh` and `testing/kfscheck.py` over both books |

## See also

- [Configuration](configuration.md): every setting, with defaults
- [The daemons](../04-programs/daemons.md): what each socket's and bus name's owner does
- [Discover](../04-programs/discover.md): the system update and the KDOS backend over these names
- [Architecture overview](../03-architecture/overview.md): where state lives, in summary
- [The security model](../03-architecture/security-model.md): why authorisation is the caller's
  credential, and the polkit actions
- [Packs and boxes](../03-architecture/packs-and-boxes.md): the pack store, composition and grafts
- [Repository layout](repository-layout.md): the source tree, rather than the installed system
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that the [Build](#build) and
  [Sources](#sources) variables steer
- [The ports catalogue](ports-catalogue.md): every port the package database can hold

<!-- book-nav -->
---

*Part VI — Reference, chapter 41.* Previous: [40. Configuration](configuration.md) · [Contents](../README.md) · Next: [42. Repository layout](repository-layout.md)

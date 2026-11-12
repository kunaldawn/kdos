# Seeding the theme

A new account on KDOS starts themed: Plasma, Konsole, Qt 5 applications and the terminal programs
are all in the default [accent]{.idx} before anything has been configured. The Plasma half is
already on disk: the colour schemes, global themes, Konsole profiles and qt5ct palettes are
installed under `/usr/share` by [kdos-plasma-look](#port-kdos-plasma-look) and
[kdos-theme](#port-kdos-theme). What a home carries is the accent's name and the files of the programs
that read only a home; the global theme the first login applies comes from `/etc/xdg/kdeglobals`.
`script/phases/70_image/050_theme.sh` writes those into `/etc/skel`, from which
[070_user.sh](070-user.md) makes every home. It first checks that the accent it seeds is the one
compiled into libkcolor, checks what it wrote and the contrast of every installed scheme, and
seeds the login screen's look into the `sddm` account's home, and afterwards rebuilds two shared
indexes that `fs/` feeds.

## The accent check

<!-- kfs:include script/phases/70_image/050_theme.sh lines=31-36 pin=8a2745828c13 -->

`kdos-bootctl theme --print` prints the Limine [theme]{.idx} block of an accent. With no name it uses
libkcolor's compiled default, `KCOL_DEFAULT_ID`; with a name, that accent. Identical output for
the two means [KDOS_ACCENT](intro.md#the-environment) is the compiled default, and different
output stops the phase.

The two must agree. A program of KDOS's own that has no accent configured uses the compiled
default; the files seeded here use `KDOS_ACCENT`. Were they different, the medium would seed one
accent while every unconfigured program picked the other, and the desktop would change colour the
first time anything was opened, which looks like a fault in that program.

## Writing the theme into /etc/skel

<!-- kfs:include script/phases/70_image/050_theme.sh lines=43-45 pin=3184cbaf816a -->

`kdos theme <accent>` is the command a user runs to switch accents (see
[Theming](../../kdos/02-user-guide/theming.md)). Here it is run with `HOME` and the three XDG
directories pointed into `/etc/skel`, so it writes into the template exactly what it would write
into a home, outside a session: no Plasma call is made, and the global theme is left for the first
login to apply. KConfig writes no key whose value `/etc/xdg/kdeglobals` already gives, so the home's
`kdeglobals` stays without `LookAndFeelPackage` here, and the accent the installer writes to that
file reaches a new account. The seeded files are therefore the same, byte for byte, as those of a
`kdos theme` to the same accent.

## The files it checks

<!-- kfs:include script/phases/70_image/050_theme.sh lines=46-68 pin=93a53891309f -->

A value that is not the one asked for stops the phase, through `set -e` or an explicit `FATAL`. In
order:

| Check | What it protects |
|:--|:--|
| `.config/kdos/accent` names the accent | The KDOS accent the terminal programs and `kdos theme` read |
| `kreadconfig6`, through the cascade a login reads, answers `org.kdos.<accent>.desktop` | The global theme Plasma applies at the first login, and with it the colour scheme, icons, fonts and wallpaper |
| `.config/qt5ct/qt5ct.conf` names a KDOS palette the image carries | Qt 5 applications, which have no Plasma platform theme and reach the accent only through qt5ct |
| `kdos-theme contrast` over every `KDOS*.colors` | Every installed colour scheme clears the contrast floors; a palette edit that drops a role under 4.5:1 stops the image here |

## The login screen's look

<!-- kfs:include script/phases/70_image/050_theme.sh lines=70-96 pin=1bd98d0d94a8 -->

The SDDM greeter runs as the `sddm` account, and the KDOS greeter theme takes its look from that
account's home: the colour scheme, Plasma style and cursor from `~/.config/kdeglobals`, `plasmarc`
and `kcminputrc`, and its background from `~/kdos-background.png`. One set per accent is built into
`/usr/share/kdos/greeter/<accent>/` by [kdos-plasma-look](#port-kdos-plasma-look). On an
installed machine the installer copies the installed accent's set, and `kdos theme` replaces it
through `kdos-powerd`; on the live medium nothing chooses an accent, so the step copies the default
accent's set into `/var/lib/sddm` here. A set missing a file stops the phase, as `kdos-powerd`
refuses a half set. The home and everything in it are handed to the account, whose greeter KWin
writes its output configuration into `~/.config`. The step then writes the default accent's name to
`/etc/kdos/accent`, the machine's record of the accent the splash and the login screen wear, which
`kdos-powerd` and the installer rewrite later; the Look and About pages read it to say what the
screens before login wear.

## The two shared indexes

<!-- kfs:include script/phases/70_image/050_theme.sh lines=106-127 pin=fbcf29e8da8b -->

`fs/` ships two kinds of file that reach their readers only through an index. KDOS's GSettings
defaults, `fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override`, take effect only once
[`glib-compile-schemas`]{.idx} has written `gschemas.compiled`. The desktop entries `fs/` ships, the
`kdos-*` handlers, reach `kdos-appbox open` and Plasma's Open With only through
[`/usr/share/applications/mimeinfo.cache`]{.idx}, which `update-desktop-database` writes.

kpkg rebuilds both whenever a package installs into those directories. A build that changes only
`fs/` installs no package, so kpkg never does, and the step rebuilds both itself from what the
directories hold. Each is skipped when its tool is not installed, the desktop index also when
`/usr/share/applications` does not exist, and the schema index also when there is no schema to
compile: `glib-compile-schemas` then writes no index at all, and the
`test -s` after it would stop the phase.

## What it leaves

On the image, the seeded files under `/etc/skel`, which `070_user.sh` copies into
`/home/kdos` and which `useradd -m` copies into an account made later; the greeter's set in
`/var/lib/sddm`, owned by `sddm`; and
`/usr/share/glib-2.0/schemas/gschemas.compiled` and `/usr/share/applications/mimeinfo.cache`,
rewritten.

## See also

- [Theming](../../kdos/02-user-guide/theming.md): the accents, switching them, and what reads which file
- [kdos theme](../../kdos/04-programs/kdos-command.md#kdos-theme): the command
- [Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes): the indexes kpkg rebuilds on install

<!-- book-nav -->
---

*Part XV. The image, chapter 370.* Previous: [369. Stamping the build identity](045-identity.md) · [Contents](../README.md) · Next: [371. The udev hardware database](060-udev-hwdb.md)

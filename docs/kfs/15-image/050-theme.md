# Seeding the theme

A new account on KDOS starts themed: GTK, Qt and KDE applications, the cursor, the icons, the
window frames, the terminal and the prompt are all in the default [accent]{.idx} before anything has been
configured. The desktop's own programs draw from libkcolor and need no files for that, but other
software reads its colours from files in the home directory.
`script/phases/70_image/050_theme.sh` writes those files into `/etc/skel`, from which
[070_user.sh](070-user.md) makes every home. It first checks that the accent it seeds is the one
compiled into libkcolor, and afterwards rebuilds two shared indexes that `fs/` feeds.

## The accent check

<!-- kfs:include script/phases/70_image/050_theme.sh lines=30-35 pin=8a2745828c13 -->

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
[Switching the accent](../../kdos/02-user-guide/theming.md#switching-the-accent)). Here it is run
with `HOME` and the three XDG directories pointed into `/etc/skel`, so it writes into the template
exactly what it would write into a home. The seeded files are therefore the same, byte for byte,
as those of a live `kdos theme` to the same accent.

The command is also what writes `~/.themes/KDOS` and `~/.icons/KDOS`. The theme packages install
only the system copies and the generator; the home copies are the ones a boxed application sees,
because a box shares the home and not `/usr/share`. See
[How the theme is generated](../../kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## The files it checks

<!-- kfs:include script/phases/70_image/050_theme.sh lines=46-75 pin=4d607408d104 -->

`test -s` fails on a file that is missing or empty, and `set -e` then stops the phase. In order:

| File | Read by |
|:--|:--|
| `.cache/kdos/theme` | kdos-comp and kdos-shell, which take the accent name from it |
| `.config/gtk-3.0/settings.ini`, `.config/gtk-4.0/gtk.css` | GTK 3 and GTK 4 applications |
| `.themes/KDOS-<accent>/gtk-3.0/gtk.css`, and `.themes/KDOS` linking to it | GTK, through the theme name `KDOS`. GTK reloads a theme when its name moves, never when a file under an unchanged name is rewritten, so the stylesheet's directory carries the accent |
| `.icons/KDOS/index.theme` | The icon theme |
| `.config/kdos-comp/themerc-override` | kdos-comp's window frames, which it reads again on the `SIGHUP` `kdos theme` sends |
| `.icons/KDOS-cursors/cursors/default` | The cursor theme. The `kdos-cursors` package installs a build of its own here; this rewrites it from the same generator and art |
| `.config/kdeglobals`, `.local/share/color-schemes/KDOS.colors` | KDE applications, including boxed ones, which read them out of the shared home |
| `.config/qt5ct/qt5ct.conf` and `colors/KDOS.conf`, the same under `qt6ct` | Qt 5 applications, and Qt 6 ones run under qt6ct, with no KDE platform theme |

## The two shared indexes

<!-- kfs:include script/phases/70_image/050_theme.sh lines=84-98 pin=2e28026933ab -->

`fs/` ships two kinds of file that reach their readers only through an index. KDOS's GSettings
defaults, `fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override`, take effect only once
[`glib-compile-schemas`]{.idx} has written `gschemas.compiled`. The desktop entries `fs/` ships, the
`kdos-*` handlers, reach `kdos-appbox open` and the shell's Open With only through
[`/usr/share/applications/mimeinfo.cache`]{.idx}, which `update-desktop-database` writes.

kpkg rebuilds both whenever a package installs into those directories. A build that changes only
`fs/` installs no package, so kpkg never does, and the step rebuilds both itself from what the
directories hold. Each is skipped when its tool is not installed, and the schema index also when
there is no schema to compile: `glib-compile-schemas` then writes no index at all, and the
`test -s` after it would stop the phase.

## What it leaves

On the image, the seeded files under `/etc/skel`, which `070_user.sh` copies into
`/home/kdos` and which `useradd -m` copies into an account made later; and
`/usr/share/glib-2.0/schemas/gschemas.compiled` and `/usr/share/applications/mimeinfo.cache`,
rewritten.

## See also

- [Theming](../../kdos/02-user-guide/theming.md): the accents, switching them, and what reads which file
- [kdos theme](../../kdos/04-programs/kdos-command.md#kdos-theme): the command
- [Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes): the indexes kpkg rebuilds on install

<!-- book-nav -->
---

*Part XV. The image, chapter 347.* Previous: [346. Stamping the build identity](045-identity.md) · [Contents](../README.md) · Next: [348. The udev hardware database](060-udev-hwdb.md)

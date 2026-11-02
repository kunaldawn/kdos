## About

xdg-user-dirs manages the well-known folders of a home directory: Desktop, Downloads, Documents,
Music, Pictures, Videos, Templates, Public and Projects. `xdg-user-dirs-update` writes where each
one is to `~/.config/user-dirs.dirs` and creates any that is missing, and `xdg-user-dir` prints
one of them for a script.

On KDOS it runs at every Plasma login: the port installs
`/etc/xdg/autostart/xdg-user-dirs.desktop`, which Plasma starts in autostart phase 1.
Dolphin's Places panel, the file dialogs and every program that saves to "Downloads" or
"Pictures" read the folders from `user-dirs.dirs`. `/etc/skel` ships no `user-dirs.dirs`: the
first Plasma login writes it from `/etc/xdg/user-dirs.defaults` and makes the folders, Projects
among them. A copied file naming folders that a `useradd -m` home lacks would have every one of
them reassigned to `$HOME`. Later runs keep every entry that names an existing directory and add
what the defaults list and the file lacks.

## Flags

- `-Ddocs=true` — Build the five manual pages from DocBook with `xsltproc`, through
  [libxslt](#port-libxslt), [docbook-xsl](#port-docbook-xsl) and [docbook-xml](#port-docbook-xml).
  It is upstream's default, stated.
- `-Dnls=false` — Build no message catalogues, and so no translated folder names: the folders
  are always named in English, whatever the locale (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The recipe deletes `/usr/lib/systemd`. The build installs a systemd user unit there whether or not
systemd is found, and the
[package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint) refuses the
directory; the autostart entry does the same job in a Plasma session.

`xdg-user-dirs-update` calls `gettext` even with `-Dnls=false`. On KDOS the functions come from
musl, whose `<libintl.h>` is the one installed. A build root whose `<libintl.h>` is GNU gettext's
renames them to `libintl_*` and the link fails unless `-lintl` is added; that happens in an Alpine
container with `gettext-dev`, not in the tree.

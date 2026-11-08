## About
`kdos-doc` puts the KDOS book on the machine. Its `build.sh` is one call to `mkbook.py`, which
renders every page of `docs/kdos` to HTML under `/usr/share/doc/kdos` with
[lowdown](#port-lowdown), with `kdos.css` from the port directory in a `<style>` element of every
page; writes `kdos(7)`, which
says where the book is and lists its chapters; and writes a manual page for every `kdos-*` command
in the book's command index, and for each command of ours named without the prefix that `OURS` in
`mkbook.py` lists (`kpkg` and its four other names, `ksvc`, `service`, `kinstall`,
`xdg-terminal-exec`), whose DESCRIPTION is the section of the book that command's row links
to. So `man kdos-appbox` and the book say the same thing, in the same words. How a reader finds
them is in
[Manual pages and the book](../../../docs/kdos/04-programs/kdos-command.md#manual-pages-and-the-book).

`/usr/bin/kdos-doc`, built from `kdos-doc.c` beside the recipe, opens the book at a page:
`kdos-doc 04-programs/kdos-command#kdos-theme` hands KHelpCenter that page's file URL, or the
desktop's browser through `xdg-open` where there is no Help Center, or prints the file's path on a
console with neither. The page name is checked against letters, digits and `._-/`, with no `..`,
before it is used, so it can only name a page under `/usr/share/doc/kdos`, and it reaches the
viewer as one argument through `exec`.

The book is read in the Help Center. The package installs KHelpCenter's two entries,
`/usr/share/khelpcenter/plugins/kdos.desktop` and `kfs.desktop`, whose `X-DOC-Weight` of -100 and
-99 put the KDOS book first in the Help Center's tree and KDOS from Scratch second, above KDE's
own handbooks, which stay under `help:/`. The book is under `help:/` as well: the package links
`/usr/share/doc/HTML/en/kdos` to `../../kdos`, and KIO's `help:` worker, which looks a path up
among the handbooks under `/usr/share/doc/HTML/<language>`, serves `help:/kdos/<page>.html` from
the book's own files. That is the address a KDOS settings page's Help button opens, from its
`X-DocPath` (see [Help](../../../docs/kdos/04-programs/kdos-settings.md#help)), and kpkg's manual
prune names the link as one it keeps. KHelpCenter leaves out an entry whose `X-DocPath` file
does not exist, so the KDOS from Scratch entry shows only where [kdos-kfs](#port-kdos-kfs) put the
book. It also installs the menu's two Library launchers, `kdos-library-kdos-book.desktop` and
`kdos-library-kfs-book.desktop`, which open each book in the Help Center and name it as an item
of `library:/` (see [kio-library](#port-kio-library)); `TryExec=khelpcenter` keeps them out of a
menu with no Help Center.

`mkbook.py` takes three arguments: the directory that holds `kdos/` and `screenshots/` (the port
directory itself), the package root, and the repository's `src/`, which it searches for manual
pages that programs ship themselves. Nothing is fetched, and lowdown is run with an argument
vector.

## Flags
- `-std=gnu11`, `-O2`, `-Wall`, `-Wextra` — The tree's C dialect and optimisation level, named
  again for `kdos-doc.c`, with the compiler's common and extra warnings.

## Notes
The stylesheet is in every page rather than linked: KHelpCenter answers every request a `help:`
page makes, its stylesheet included, as `text/html`, and its browser engine refuses a linked
stylesheet of that type, so a linked `kdos.css` would style the book under `file:` and leave it
bare under `help:`.

KHelpCenter adds KDE's handbook stylesheet to every page it shows, as a `<style id="default">`
appended to the page's head, after `kdos.css`, so its element rules would replace the book's
colours with white paper and black text. `kdos.css` carries a second set of rules under
`html:has(> head > style#default)`, which match only a page carrying that element and outrank the
handbook's: in the Help Center the book keeps its own colours, and takes the desktop's
proportional face for its text and the Help Center's margins.

The book reaches the port through two symbolic links in its directory, `kdos` to `docs/kdos` and
`screenshots` to `docs/screenshots`. The recipe hash follows them, so an edit anywhere in the book
or its pictures rebuilds this port on the next `41_system` run, and an edit elsewhere under
`docs/` does not; a link to the whole of `docs/` would rebuild it for the other book too. The
chroot sees `docs/` read-only at `/kdos/docs`. The image carries `docs/kdos` and
`docs/screenshots` under `/ports/docs` so that the links resolve on an installed machine as well:
a dangling link drops out of the recipe hash, and the machine would then take the installed book
for stale. See
[Rebuilding one thing](../../../docs/kdos/05-developer/developing.md#rebuilding-one-thing).

The build fails when a row of the command index links a section the book does not have, so a
renamed heading is caught here rather than shipped as a manual page with no body.

A command whose own port ships a `kdos-*.1` page, as [kdos-bb](#port-kdos-bb) does, is left to that
port. Two packages that wrote the same path would be a conflict kpkg refuses, which is also why a
name without the prefix is listed by hand in `OURS` rather than taken from every row: the command
index names upstream programs too (`ping`, `getent`, `init`), whose own ports install their pages.
`ksvc` and `service`, installed in `/usr/sbin`, are section 8.

Headings are given the ids GitHub gives them, by the same rule as `script/book/kfsgen.py` and
`testing/anchorcheck.py`, so a link that works on GitHub works in the installed book; a change to
the rule in one of the three has to be made in all of them.

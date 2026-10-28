## About
`kdos-doc` puts the KDOS book on the machine. Its `build.sh` is one call to `mkbook.py`, which
renders every page of `docs/kdos` to HTML under `/usr/share/doc/kdos` with
[lowdown](#port-lowdown), styled by `kdos.css` from the port directory; writes `kdos(7)`, which
says where the book is and lists its chapters; and writes a manual page for every `kdos-*` command
in the book's command index, and for each command of ours named without the prefix that `OURS` in
`mkbook.py` lists (`kpkg` and its four other names, `ksvc`, `service`, `kinstall`,
`xdg-terminal-exec`), whose DESCRIPTION is the section of the book that command's row links
to. So `man kdos-appbox` and the book say the same thing, in the same words. How a reader finds
them is in
[Manual pages and the book](../../../docs/kdos/04-programs/kdos-command.md#manual-pages-and-the-book).

The port name is not a program: `kdos-doc` the documentation viewer is a surface of
[kdos-shell](#port-kdos-shell), and this package installs only documents.

`mkbook.py` takes three arguments: the directory that holds `kdos/` and `screenshots/` (the port
directory itself), the package root, and the repository's `src/`, which it searches for manual
pages that programs ship themselves. Nothing is fetched, and lowdown is run with an argument
vector.

## Notes
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

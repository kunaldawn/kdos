## About
`kdos-kfs` puts this book, *KDOS from Scratch*, on the machine: the HTML edition under
`/usr/share/doc/kfs`, one page per section, and the whole book as one PDF,
`/usr/share/doc/kfs/kfs.pdf`. It is the book `make book` renders into `build/kfs`, made by the
same two scripts in the same order (see
[Make targets](../../../docs/kdos/05-developer/developing.md#make-targets)): `kfsgen.py` writes
the Markdown source of every hand-written page and every package page, the package pages generated
from the recipes and the phase lists, and `kfsrender.py` turns it into HTML and, through typst, the
PDF. The pandoc and typst are the tree's own ports, which is why both are in `depends`, with
[python3](#port-python3) for the two scripts.

The port name is not a program. It installs only documents, and nothing is fetched: the port has
no `source =`, and builds out of its own directory.

## Notes
The book reaches the port through two symbolic links in its directory, `docs` to `docs/kfs` and
`book` to `script/book`, the renderer. The recipe hash follows them, so an edit to a hand-written
page, to the committed snapshot under `docs/kfs/data` or to the renderer rebuilds this port on the
next `41_system` run. A recipe, a `kfs.md` or a phase list is in no part of that hash; each is
covered by `docs/kfs/data/recipes.stamp`, and `make book-stamp` rewriting the stamp is what
rebuilds this port for them (see
[The port's note: kfs.md](../../../docs/kdos/05-developer/writing-ports.md#the-ports-note-kfsmd)).

`kfsgen.py` reads the whole repository, not only the two links: the recipes under `ports/core`
and `src/`, `script/phases`, and, for the listings of Part I, the `Makefile` and the `Dockerfile`.
`build.sh` takes the repository as `$PORT_SRC/../../..`, which in the chroot is `/kdos`, and stops
with a message when `ports/core`, `script/phases` or `docs/kfs` is missing there. The chroot binds
`docs/`, `ports/` and the two files under `/kdos` for it (see
[Entering the chroot](../../../docs/kfs/04-chroot/entering.md)). The image's `/ports` carries the
two books and the renderer but has no `ports/core` or `script/phases` beside them, so on an
installed machine the port does not build.

`--buildfs /` points the renderer at the build root's own package database, from which the HTML
gives every package's full file list. The database holds only what was installed before this
port was built, so a package of a later phase gets the summary from
`docs/kfs/data/contents.tsv` instead. The PDF never carries the full lists.

`PYTHONDONTWRITEBYTECODE=1` keeps Python from writing a `__pycache__` beside any module the two
scripts import. `script/book` is the checkout's, and a checkout should hold only the files it
commits; the recipe hash skips such a directory when one is written all the same.

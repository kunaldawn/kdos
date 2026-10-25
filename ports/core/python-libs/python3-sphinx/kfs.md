## About
Sphinx is the documentation generator of the Python world and of many C and C++ projects: it
reads reStructuredText, or Markdown through MyST, and builds HTML, LaTeX, EPUB or manual pages
from it. It installs `sphinx-build`, `sphinx-quickstart`, `sphinx-apidoc` and
`sphinx-autogen`.

In KDOS it is a manual-page generator. [llvm](#port-llvm), [clang](#port-clang),
[lldb](#port-lldb), [cmake](#port-cmake), [mpd](#port-mpd), [flashrom](#port-flashrom),
[btrfs-progs](#port-btrfs-progs), [ghc](#port-ghc), [notmuch](#port-notmuch), [khal](#port-khal)
and others build their manual pages with Sphinx's man builder, and a Sphinx recipe ships that
builder's output alone; see [Manual pages](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#manual-pages).

The port is unusual in where it installs. Sphinx needs a closure of Python packages that are
not ports (the default theme alabaster, Babel, the `sphinxcontrib-*` builders, `myst-parser` and
`sphinx-argparse`, among others), and some it needs, such as requests and urllib3, are ports
already, installed in `site-packages` by a later phase. Two packages owning one path is a
conflict kpkg refuses ([Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)),
so Sphinx and that whole closure go into a prefix of their own, `/usr/lib/python3-sphinx`, and
the commands in `/usr/bin` are a small launcher that puts the prefix on `PYTHONPATH`. The
closure is the recipe's `pypackages` list, vendored as source distributions (see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).

## Flags
- `--find-links=vendor` — Look for distributions in the unpacked `vendor` directory, the only
  place pip looks with `--no-index`. The `pip3` line names every member of the closure
  explicitly, with `.` for Sphinx itself, and `--no-deps` keeps pip from adding anything the
  recipe did not name.
- `--no-build-isolation` — Build Sphinx and each member of the closure with the backends already
  installed in the build root rather than in temporary environments. The closure builds with
  flit_core, hatchling, hatch-vcs and setuptools, which is why
  [python3-flit-core](#port-python3-flit-core), [python3-hatchling](#port-python3-hatchling),
  [python3-hatch-vcs](#port-python3-hatch-vcs) and [python3-setuptools](#port-python3-setuptools)
  are in `depends`.
- `--prefix=$_home` — Install under `/usr/lib/python3-sphinx` instead of `/usr`: the modules
  in that prefix's own `lib/python3.*/site-packages`, the entry points in its `bin`. Nothing of
  the closure lands in the interpreter's `site-packages`.
- `-b man` — Build Sphinx's own manual pages with the man builder: `sphinx-build.1`,
  `sphinx-quickstart.1`, `sphinx-apidoc.1`, `sphinx-autogen.1` and `sphinx-all.1`, the whole
  manual as one page.
- `-D` — Override a `conf.py` setting. `extensions` is given as `doc/conf.py`'s own list
  without `sphinx.ext.intersphinx`, whose only work in a man build is to download other
  projects' inventories; the build has no network, and every failed download would be a warning.
- `-d` — Keep the doctree cache in `$SRC_ROOT/sphinx-doctrees`, beside the source tree and
  `$PKG` rather than in either; the pages themselves are written to `$SRC_ROOT/sphinx-man` and
  installed from there.
- `-q` — Print only warnings and errors.

## Notes
`import sphinx` from a plain `python3` fails, because the interpreter does not search the
private prefix. A build that probes for Sphinx as a Python module instead of running
`sphinx-build` does not find it, and has to be pointed at the command. A program that imports
Sphinx at run time has to add the prefix to its own path: [spyder](#port-spyder)'s package writes
a `.pth` file naming the prefix's `site-packages`.

The launcher is one script, `/usr/bin/sphinx-build`, and the other three commands are links to
it. It asks `python3` for the `purelib` path of `/usr/lib/python3-sphinx` (its
`lib/python3.<minor>/site-packages`), puts that path in front of `PYTHONPATH`, and
runs the entry point of the name it was called by from `/usr/lib/python3-sphinx/bin`. The
`for` loop stops the build when one of those entry points is missing, so no link can point at
nothing.

The pages are built after the install, by the Sphinx just installed into `$PKG`, found through
the same `purelib` computation the launcher makes, so the build runs the packaged copy at least
once.

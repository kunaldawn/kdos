## About
Docutils turns reStructuredText, the plain-text markup most Python documentation is written in,
into HTML, LaTeX, XML, OpenDocument and manual pages. It installs the `docutils` command and the
`rst2*` front ends, `rst2man` among them.

In KDOS it is chiefly a manual-page generator: ports that write their pages in reStructuredText
run `rst2man` at build time, among them [glib](#port-glib), [gtk4](#port-gtk4) and
[mpv](#port-mpv), and the tree lists it with the other generators that are ports in
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages). [python3-sphinx](#port-python3-sphinx) is built on it.
docutils highlights the contents of a `code` directive with
[python3-pygments](#port-python3-pygments), named in `depends`, when it can import it, and leaves
them plain when it cannot.

## Flags
- `--find-links=vendor` — Look for distributions in the unpacked `vendor` directory, the only
  place pip looks with `--no-index`. docutils caps its backend's version
  (`requires = ["flit_core>=3.11,<4"]`) below that of the tree's
  [python3-flit-core](#port-python3-flit-core). The line has no `--no-build-isolation`, so pip
  builds docutils in a temporary environment and installs into it the older flit_core that the
  bundle carries; the bundle holds nothing else.

## Notes
The `for` loop after the install links each `/usr/bin/*.py` script to its name without `.py`.
docutils declares its commands as console scripts with no suffix (`rst2man`, `rst2html5` and
the rest), so the glob matches nothing and the loop links nothing; the
`[ -e "$i" ] || continue` line is what keeps an unmatched glob from failing the build.

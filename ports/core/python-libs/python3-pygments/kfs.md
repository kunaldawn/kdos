## About
Pygments is a syntax highlighter written in Python: it splits source code in several hundred
languages into tokens and writes them out as HTML, LaTeX, RTF, SVG, images or terminal escape
sequences. It installs the `pygmentize` command beside the module.

KDOS uses it through other programs. [python3-docutils](#port-python3-docutils) and
[python3-sphinx](#port-python3-sphinx) highlight code blocks with it,
[python3-rich](#port-python3-rich) and [ipython](#port-ipython) colour code in a terminal, and
[gdb](#port-gdb), [cppcheck](#port-cppcheck), [qutebrowser](#port-qutebrowser) and
[jupyterlab](#port-jupyterlab) name it in their `depends` too.

## Flags
- `--find-links=vendor` — Look for distributions in the unpacked `vendor` directory, which,
  with `--no-index`, is the only place pip looks. Pygments builds with hatchling
  (`requires = ["hatchling>=1.27"]`), and this `pip3` line has no `--no-build-isolation`, so
  pip builds it in a temporary environment and installs hatchling there from the bundle. Every
  file in the bundle is a source distribution: pip builds hatchling, and the backends hatchling's
  own dependencies build with (flit_core, setuptools, setuptools-scm, poetry-core, calver), from
  source inside that environment. None of it is installed into `$PKG`.

## Notes
pip installs no manual pages. Pygments ships a finished `doc/pygmentize.1`, so `build.sh`
installs it by hand, as the tree's rule on
[manual pages](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#manual-pages) asks of a project whose installer skips them.

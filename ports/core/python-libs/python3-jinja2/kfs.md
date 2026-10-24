## About
Jinja is a template engine for Python: templates with variables, loops, conditions and
inheritance, compiled to Python code, with HTML escaping through
[python3-markupsafe](#port-python3-markupsafe).

KDOS needs it mostly at build time. Every file under libplacebo's `src/shaders/` is generated
from a Jinja template during the build, and libplacebo's own copy of Jinja is an empty
submodule in its release archive, so [libplacebo](#port-libplacebo), and through it mpv, needs
this port. [libcamera](#port-libcamera), [fwupd](#port-fwupd) and [libei](#port-libei) generate
code or manual pages with it as well, and [python3-sphinx](#port-python3-sphinx), [python3-flask](#port-python3-flask),
[qutebrowser](#port-qutebrowser) and [jupyterlab](#port-jupyterlab) use it at run time.

## Flags
- `--find-links=vendor` — Look for distributions in the unpacked `vendor` directory, the only
  place pip looks with `--no-index`. Jinja caps its backend's version
  (`requires = ["flit_core<4"]`) below that of the tree's
  [python3-flit-core](#port-python3-flit-core). The line has no `--no-build-isolation`, so pip
  builds Jinja in a temporary environment, with the older flit_core from the bundle. The bundle
  also holds MarkupSafe and setuptools, and neither is installed: `--no-deps` keeps pip from
  installing MarkupSafe, which is a port of its own.

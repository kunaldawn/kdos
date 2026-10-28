## About
python-xlib is the X11 client protocol written in Python: it talks to an X server over its
socket with no C library underneath. In KDOS [linuxcnc](#port-linuxcnc) imports it. Xwayland is
the X server it reaches; see
[No Xorg server](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Patches
- `no-pkg-resources.patch` — In `setup.py`, import `setup` from setuptools and drop the check that setuptools is at least 30.3, which imported `pkg_resources` only to parse that requirement. The setuptools this tree builds with ships no `pkg_resources`, so the unpatched `setup.py` stops on its first line.

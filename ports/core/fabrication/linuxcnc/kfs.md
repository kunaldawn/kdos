## About

LinuxCNC controls machine tools (mills, lathes, routers, plasma cutters) from G-code: a realtime
motion layer, the HAL component system that wires it to the hardware, the RS274NGC interpreter, the
Tk interfaces AXIS and the configuration picker, and GTK 3 configuration wizards. In KDOS it is
built and shipped as the simulator: the package carries no setuid file, so its realtime threads run
without realtime priority and it cannot drive a machine (see
[LinuxCNC is the simulator only](../../../../docs/kdos/06-reference/known-gaps.md#linuxcnc-is-the-simulator-only)).

## Flags

- `-lintl` — Added to `LDFLAGS` inside `--push-state,--no-as-needed` … `--pop-state`. gettext's
  `libintl.h` renames `gettext()` to `libintl_gettext()`, which only libintl defines, while
  configure probes for plain `gettext()` and finds it in the C library, so libintl is named on
  every link line through `LDFLAGS`, the one variable LinuxCNC's link rules share (two that do
  not are patched, below). The shared-library rules put `LDFLAGS` before the objects, where
  `--as-needed` would drop the library, so it is linked unconditionally.
- `-g0` — Appended to `CFLAGS` and `CXXFLAGS`. The makefile puts its own `-Os` and `-g` ahead of
  them, so the exported optimisation level wins and the trailing `-g0` takes the debug information
  back out.
- `PYTHON_BIN` — With `PYTHON=/usr/bin/python3`: configure's Python check tries a fixed list of
  `python3.N` names, then plain `python`, and does not reach the tree's Python. `PYTHON_BIN`
  names the interpreter it looks for and the library it links; `PYTHON` the one the
  development-headers check runs.
- `--with-realtime=uspace` — The realtime layer runs as POSIX threads in userspace on the stock
  kernel, instead of on an RTAI kernel.
- `--with-tclConfig=/usr/lib/tclConfig.sh` — With `--with-tkConfig=/usr/lib/tkConfig.sh`: where
  [tcl](#port-tcl) and [tk](#port-tk) put their configuration scripts.
- `--with-boost-python=boost_python$_py` — The Boost.Python library for the running interpreter,
  named by its major and minor version, as [boost](#port-boost) installs it.
- `--enable-gtk` — The parts of LinuxCNC that depend on GTK 3, the StepConf and PnCConf
  wizards among them. Upstream's default, named.
- `--disable-gtk2` — The one GTK 2 part, classicladder, is not built; GTK 2 is not a port.
- `--disable-check-runtime-deps` — Skips configure's checks for the Tcl packages and Python
  modules that are needed only at run time. One of them stops configure when Tclx is missing, and
  Tclx is not a port.
- `--disable-build-documentation` — No HTML or PDF manuals; the manual pages are still built.
- `EXTRA_DEBUG=-g0` — The realtime components compile with upstream's `-Os` and `-g` and none
  of the exported flags. `EXTRA_DEBUG` comes after that `-g` on their command line, so this
  `-g0` cancels it.
- `ULFLAGS` — The flags the Tcl, Togl and Python-embedding objects add after `CFLAGS`, given as
  upstream's own value without `-g` and `-Os`. Its paths and defines stay make references,
  single-quoted in `build.sh` so make expands them.
- `XHC_WHB04B6_DEBUG=-funwind-tables` — That pendant driver's `-g -funwind-tables`, without the
  `-g`.

## Patches

- `musl.patch` — musl has no `mallopt()`, no `pthread_attr_setaffinity_np()` and only the POSIX `basename()`; its `stderr` is not assignable; its socket calls take a plain `struct sockaddr` pointer; and its headers declare `struct timeval`, `u_short` and the byte-order helpers only where they are included by name. The patch pins the realtime thread with `pthread_setaffinity_np()` once the thread exists, reopens `stderr` in place with `freopen`, gives the interpreter a GNU `basename()`, casts the socket addresses, includes `<endian.h>`, `<sys/time.h>` and `<sys/types.h>` where they are needed, and keeps the glibc-only calls behind `__GLIBC__`.
- `tcl9.patch` — This release builds against Tcl/Tk 8 only, and Tcl/Tk 9 is the one the tree has. This is upstream's Tcl/Tk 9 port of the C extensions (`emcsh`, `halsh`), Togl, the Tcl programs (halshow, pickconfig, tooledit) and AXIS, taken from its master branch, with the matching `configure.ac` change.
- `no-preconv.patch` — Writes the HAL component manual pages straight from `halcompile` instead of passing them through groff's `preconv`, which escapes non-ASCII characters. The pages are UTF-8 and are read by mandoc, which takes UTF-8 as it is, so the build does not need groff.
- `link-ldflags.patch` — Adds `$(LDFLAGS)` to the two link rules that leave it out, `panelui` and `linuxcnc_module_helper`, so the tree's linker flags, and the `-lintl` above, reach them too.
- `pygi-glib.patch` — In `gremlin_view.py` and `pyngcgui.py`, imports GLib through PyGObject (`from gi.repository import GLib as glib`) instead of the static `glib` module of the old PyGTK bindings, which is not a port.

## Notes

The install marks `rtapi_app` and `linuxcnc_module_helper` setuid root; `build.sh` clears every
setuid bit in the package (see [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries)).

No package may install under `/etc/X11`. The install puts the `TkLinuxCNC` X resource file there;
`build.sh` moves it beside the Tcl sources in `/usr/lib/tcltk/linuxcnc`, where it is read from
instead, and removes the directory. The install also makes an empty `/lib/linuxcnc`, named
outright rather than under the prefix; `/lib` is a link to `usr/lib`, so `build.sh` removes both
from the package.

configure falls back to Debian's `dist-packages` when the interpreter has no Debian install
scheme, and this Python imports from its own `site-packages`, so `build.sh` moves the Python
modules there. The Tcl message catalogs other than English are deleted, and so are the symbolic
links under `/usr/share/doc/linuxcnc` that point at nothing in the package.

The launcher and the latency test are Tk, so they open X11 windows under Xwayland; the wizards are
GTK 3. Upstream's entry for the latency histogram is removed: it draws with BLT, which is not a
port. The latency test's entry is `NoDisplay`, as a diagnostic.

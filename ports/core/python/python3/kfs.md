## About

CPython, the Python 3 interpreter, with its standard library and the shared `libpython`. It is one
of the tools `30_foundation` installs before any shelf (see [the pinned
run](#06-foundation--the-pinned-run)): [meson](#port-meson) is a Python program, many build systems
run Python scripts to generate code, and every `python3-*` port is built and installed with it. Its
`depends` line is the set of C libraries its standard extension modules link: `_ctypes`
[libffi](#port-libffi), `_decimal` [mpdecimal](#port-mpdecimal), `_sqlite3` [sqlite](#port-sqlite),
`pyexpat` [expat](#port-expat), `_ssl` and `_hashlib` [openssl](#port-openssl), the compression
modules [zlib](#port-zlib), [bzip2](#port-bzip2), [xz](#port-xz) and [zstd](#port-zstd), `_curses`
[ncurses](#port-ncurses), `readline` [readline](#port-readline), `_gdbm` and `_dbm`
[gdbm](#port-gdbm), and `_uuid` [util-linux](#port-util-linux)'s libuuid.

The package has no Tk binding. `python3` is built in `30_foundation`, long before Tk exists, so
`_tkinter` is turned off and the Tk-only parts of the standard library are deleted after the
install. [python3-tkinter](#port-python3-tkinter) configures the same tarball again once Tk is
installed and packages that module with the Tk-only parts: the `tkinter` package, `turtle`,
`turtledemo`, `idlelib` and the `idle3` command; both recipes carry `group =
python3` so the version checker offers them as one bump, as [A second build of the same
source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)
describes.

The system Python's `site-packages` belongs to kpkg. `build.sh` writes the PEP 668
`EXTERNALLY-MANAGED` marker, so `pip install` outside a virtual environment is refused; see [Files
kpkg keeps to
itself](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#files-kpkg-keeps-to-itself).
pip itself is not part of this package. It is [python3-pip](#port-python3-pip), which also puts the
pip wheel where `python3 -m venv` looks for it.

## Flags

- `--enable-optimizations` — profile-guided optimisation: the interpreter is built once with
  profiling instrumentation, runs a training workload from its own test suite (`-m test --pgo`), and
  is rebuilt using the profile.
- `--with-lto` — link-time optimisation of the interpreter and the built-in modules; configure adds
  `-flto -fuse-linker-plugin -ffat-lto-objects` to the flags it keeps for the build alone, not to
  the ones it records for extension modules.
- `--enable-shared` — build `libpython` as a shared library, the versioned one and the
  `libpython3.so` of the stable ABI, and link `python3` against it, so a program that embeds Python
  links the same library. The static `libpython*.a` under `config-*` is installed as well.
- `--with-computed-gotos` — dispatch bytecode in the evaluation loop with GCC's computed `goto`
  rather than a `switch`. configure turns it on for a compiler that supports it in any case.
- `--enable-ipv6` — IPv6 in the `socket` module; also configure's default where it is supported.
- `--enable-loadable-sqlite-extensions` — let `sqlite3.Connection.enable_load_extension()` load
  SQLite extension libraries; without it the method does not exist.
- `--with-system-expat` — build `pyexpat` against [expat](#port-expat)'s `libexpat` instead of the
  copy of expat in CPython's `Modules/expat`.
- `--with-system-libmpdec` — build `_decimal` against [mpdecimal](#port-mpdecimal)'s `libmpdec`
  instead of the copy in `Modules/_decimal/libmpdec`. See the Notes: a missing library does not stop
  configure.
- `--with-readline=readline` — build the `readline` module on GNU readline rather than editline.
- `--with-ensurepip=no` — do not run `ensurepip` at `make install`, which would install pip into the
  system `site-packages` from the wheel CPython ships. `build.sh` also deletes that wheel,
  `ensurepip/_bundled`.
- `--with-wheel-pkg-dir=/usr/share/python-wheels` — the directory `ensurepip` takes its pip wheel
  from before falling back to `_bundled`. With `_bundled` deleted, it is the only source, and
  [python3-pip](#port-python3-pip) installs the wheel there; without that wheel, `python3 -m venv`
  fails unless it is given `--without-pip`.
- `--with-tzpath=/usr/share/zoneinfo` — the time-zone search path `zoneinfo` uses, in place of
  upstream's default search list. [tzdata](#port-tzdata) installs its zones there.
- `--disable-test-modules` — build no `_test*` extension modules and install no `test` package
  (`Lib/test` and `idlelib/idle_test`). The `rm -rf` of `test` after the install is a second guard
  for the same thing.
- `ac_cv_header_bluetooth_bluetooth_h=no` — with `ac_cv_header_bluetooth_h=no`, answers configure's
  checks for the Bluetooth headers in advance, so the `socket` module is built without
  `AF_BLUETOOTH` whether or not [bluez](#port-bluez)'s headers are installed when python3 builds.
  BlueZ is a port of `41_system`.
- `ac_cv_lib_intl_textdomain=no` — answers configure's check for `textdomain` in `-lintl` in
  advance, so `libpython` does not link [libintl](#port-libintl), which the recipe does not
  declare. musl's C library carries the gettext functions the `locale` module calls, and the
  `undeclared` gate in `70_image` fails a `libpython` that names `libintl.so.8`.
- `py_cv_module__tkinter=n/a` — marks `_tkinter` as not available, so configure never builds it,
  even when a rebuild finds Tcl and Tk installed. [python3-tkinter](#port-python3-tkinter) builds
  it.
- `ac_cv_prog_cc_g=no` — tells configure the compiler takes no `-g`, so neither its default `OPT`
  nor its link-time-optimisation flags (to which it adds `-g` otherwise) carry debugging
  information.
- `OPT="-O3 -Wall"` — the optimisation and warning flags configure would choose itself for GCC, less
  the `-g`. Given on the command line, it stops configure computing `OPT` and also stops it setting
  `CFLAGS_ALIASING`, which is set in the same place; see the next switch.
- `CFLAGS_ALIASING=-fno-strict-aliasing` — the flag configure sets for GCC when it computes `OPT`.
  `Python/dtoa.c` must be compiled without strict aliasing to be correct, so with `OPT` given by
  hand the flag has to be given too.
- `CPPFLAGS` — adds `-DOPENSSL_NO_SSL3`, `-DOPENSSL_NO_SSL3_METHOD`, `-DOPENSSL_NO_TLS1_METHOD`,
  `-DOPENSSL_NO_TLS1_1_METHOD` and `-DOPENSSL_NO_TLS1_2_METHOD`. The tree's [openssl](#port-openssl)
  has no SSLv3 and no per-version method constructors, and declares no `OPENSSL_NO_*` guard for
  either. `_ssl.c` declares `SSLv3_method()`, `TLSv1_method()` and the rest itself unless the
  matching guard is defined, and without the guards the module refers to functions the library does
  not have and fails to import.
- `EXTRA_CFLAGS` — on both `make` lines, set to the tree's `CFLAGS`. The Makefile puts
  `EXTRA_CFLAGS` last in `PY_CFLAGS`, after `OPT`; see the Notes for what that does to the
  optimisation level.

## Notes

The interpreter is compiled at `-O2`, not `-O3`. `PY_CFLAGS` is `OPT` followed by the tree's
`CFLAGS` three times over, once as recorded by configure, once from the environment of `make` and
once as `EXTRA_CFLAGS`, and GCC takes the last `-O` it is given. The recorded value is in
`sysconfig.get_config_var('PY_CFLAGS')`. A C extension built later compiles with the recorded
`CFLAGS` followed by the exported ones, and so at `-O2` as well; see [Python, Perl, Haskell, Node
and OCaml
modules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).

`--with-system-libmpdec` does not make a missing mpdecimal an error. configure warns, "no system
libmpdec found; falling back to bundled libmpdec", and builds the copy from its own source. The
`depends` line puts mpdecimal first; `readelf -d` on `_decimal`'s `.so` under `lib-dynload` shows a
`NEEDED` entry for `libmpdec` when the build used it. `--with-system-expat` has no such fallback:
configure links `pyexpat` with `-lexpat` without checking, so a missing expat fails the build.

After the install, `build.sh` removes `tkinter`, `idlelib`, `turtledemo`, `turtle.py` with its bytecode in `__pycache__` and the
`idle3` commands, which need Tk and which [python3-tkinter](#port-python3-tkinter) ships; links `python` to `python3`, and its manual page, so that both
names work; and writes the `EXTERNALLY-MANAGED` file.

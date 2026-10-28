## About

lm-sensors reads the kernel's hardware monitoring (hwmon) devices: `libsensors` turns the raw
temperatures, fan speeds and voltages under `/sys/class/hwmon` into labelled, scaled readings
according to `/etc/sensors3.conf`, and `sensors` prints them. [htop](#port-htop),
[libksysguard](#port-libksysguard) and [mesa](#port-mesa) link the library.

KDOS installs the reader and not the detector. The drivers are loaded without it: most hwmon
drivers are bound by udev at boot, and the three that declare no modalias (the Nuvoton and ITE
Super-I/O chips and `drivetemp`) are loaded by `fs/etc/modules-load.d/kdos-hwmon.conf`.

There is no `configure`: every setting is a make variable, and the same set is passed to both
`make` lines. A variable that differed between them would leave the library built for one layout
and installed in another; `ETCDIR`, for one, is compiled into `libsensors` as where to find
`sensors3.conf`.

## Flags

- `PREFIX=/usr`, `LIBDIR=/usr/lib`, `ETCDIR=/etc`, `MANDIR=/usr/share/man` — Where everything is
  installed. The makefile's own default prefix is `/usr/local`.
- `PROG_EXTRA=` — Build no extra programs, which is also the makefile's default. The one upstream
  offers, `sensord`, logs readings through rrdtool.
- `BUILD_STATIC_LIB=0` — Build no `libsensors.a`, which the makefile builds and installs by
  default.
- `LIBICONV=` — Link `sensors` against no separate iconv library; musl's C library has iconv
  built in. `iconv-probe.patch` makes the makefile respect this without running its probe.
- `EXLDFLAGS="$LDFLAGS"` — Link with the tree's `LDFLAGS`. The makefile's own value is
  `-Wl,-rpath,$(LIBDIR)` followed by `LDFLAGS`, so the one difference is that the programs carry
  no run path; `/usr/lib` is on the loader's search path anyway.

## Patches

- `iconv-probe.patch` — In `prog/sensors/Module.mk`, changes `LIBICONV :=` to `LIBICONV ?=`. The makefile works out whether to link `-liconv` by running `/sbin/ldconfig -p` and searching its output for `libiconv.so`. With `:=`, make runs that command while it reads the makefile even when `LIBICONV` is given on the command line; with `?=` the command line's empty value stands and the probe never runs. musl keeps no loader cache, and the tree's `ldconfig` prints nothing, so the probe has nothing to read.

## Notes

`build.sh` removes `sensors-detect`, its manual page and its zsh completion from the package,
because the makefile installs them unconditionally. `sensors-detect` finds sensor chips by probing
I/O ports and SMBus addresses, which on KDOS is done instead by the kernel's own driver binding
described above.

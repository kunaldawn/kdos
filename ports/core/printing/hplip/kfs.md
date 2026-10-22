## About

[HPLIP]{.idx} is HP's printing and scanning software for Linux. This port ships its print and scan
path only: the `hpcups` CUPS filter, the `hp` CUPS backend for USB and network printers, the
printer descriptions as a CUPS driver file and as PPDs, and the `hpaio` SANE backend. The Qt
toolbox, fax support and the `hp-*` Python tools are not built or are removed after the install;
among those tools is `hp-plugin`, which downloads HP's closed plugin, and the system does not
carry it.

## Flags

- `save_CFLAGS="$CFLAGS"` — set in configure's environment. `configure.in` replaces `CFLAGS` with
  Python's include flags for its `Python.h` check and then restores it from `$save_CFLAGS`, a
  variable it never sets; without this the tree's `CFLAGS` would be emptied for the whole build.
- `--disable-doc-build` — Build no HTML documentation.
- `--disable-gui-build`, `--disable-qt3`, `--disable-qt4`, `--disable-qt5` — No HP Device Manager
  or other Qt tools.
- `--disable-fax-build` — No fax support.
- `--disable-policykit` — No polkit helper for the tools; upstream's default.
- `--disable-imageProcessor-build` — `libImageProcessor` is a prebuilt closed object in the
  tarball; with the switch on, `hpcups` links it.
- `--enable-network-build` — Build the network half of `hpmud`, the `hp:/net` backend and network
  scanning through `hpaio`. It links [net-snmp](#port-net-snmp) for device status and
  [avahi](#port-avahi)'s client library for mDNS discovery; configure stops without either.
- `--enable-scan-build`, `--enable-dbus-build`, `--enable-hpcups-install` — The `hpaio` SANE
  backend, D-Bus support, and the `hpcups` filter; all three are upstream's defaults.
- `--disable-class-driver` — Build the full HPLIP driver set rather than the class driver alone;
  upstream's default.
- `--enable-cups-drv-install`, `--enable-cups-ppd-install` — Install both the dynamic `.drv`
  description CUPS generates PPDs from and the static, gzipped PPDs.

## Patches

- `fix-memmove.patch` — uses `memmove` instead of `memcpy` where `io/hpmud/musb.c` strips the two length bytes from a device id, a copy whose source and destination overlap.
- `disable_upgrade.patch` — makes `upgrade.py` print an error and exit before it does anything. Its message names `apk`, the package manager of the distribution the patch comes from; on KDOS the package is updated through `kpkg`.
- `more-imageprocessor-removes.patch` — removes every mention of the prebuilt `libImageProcessor` from `Makefile.in` (as a distributed file, its header as an `hpcups` source, the library `hpcups` links, and the install rule that copies it to `$(libdir)` and links `libImageProcessor.so`) and the link from `hplipclassdriver.spec`. It also makes the install put `HPLIP.conf` in `$(DESTDIR)/usr/share/ipp-usb/quirks`, where upstream copies it to the build machine's `/usr/share/ipp-usb/quirks` only when that directory exists.
- `no-empty-glob.patch` — makes `createPPD.sh` skip a glob that matched nothing, instead of running `gzip` on the literal pattern.
- `types-musl.patch` — defines `__S64_TYPE` and `__U64_TYPE` from the word size when the C library is not glibc, and drops OrbliteScan's `SInt64`, `UInt64`, `int64_t` and `uint64_t` typedefs in `LinuxCommon.h`. Those macros are glibc's internal names, which musl does not provide.
- `add-cast.patch` — adds the `(void **)` cast that the `bb_orblite_open` call in `orblite.c` needs to match its declaration.
- `add-const-to-cast.patch` — writes the `const SANE_Device ***` cast for `orblite_get_devices` in `hpaio.c`, and `const SANE_Char* const*` in place of `SANE_Char**` in the casts of the Orblite option tables, to match the types those members are declared with.
- `add-missing-headers-and-macros.patch` — defines `_GNU_SOURCE` in `common/utils.c`, `protocol/hp_ipp.c` and `scan/sane/sclpml.c`, includes `<unistd.h>`, `<dlfcn.h>`, `<sys/time.h>` and `orblite.h` in the files that call their functions without them, and includes `<string.h>` as a system header in `common/utils.c`.
- `add-missing-prototypes.patch` — declares `_releaseCupsInstance`, `addCupsPrinter`, `delCupsPrinter`, `setDefaultCupsPrinter` and `controlCupsPrinter` in `protocol/hp_ipp.h`.
- `add-missing-type-to-variable-decl.patch` — writes `static int column` in `prnt/hpps/pserror.c`, which declared it with no type.
- `dont-return-value-from-void-func.patch` — rewrites `sane_hpaio_cancel`, a `void` function, as an `if`/`else if` chain that calls each backend's cancel function instead of returning its result.
- `fix-func-decl-missing-arg.patch` — gives `bb_is_paper_in_adf` its `struct ledm_session *` parameter in `scan/sane/ledmi.h`, where it was declared with an empty parameter list.
- `use-correct-macro.patch` — uses `DBG` instead of the undefined `_DBG` in `orblite.c`.
- `hplip-no-urlopener.patch` — the opener classes in `base/device.py` derive from `urllib.request.URLopener`; the patch makes them plain classes and calls their `open_hp` method directly. The standard library of the tree's Python has no `URLopener`, so without the patch the module fails at import.

The header, prototype, type, cast and `void`-return patches correct C that GCC rejects as an
error by default.

## Notes

After the install `build.sh` removes `/usr/bin` (the `hp-*` tools) and their manual pages, the udev
rules, the HAL and systemd files, the desktop entries and the XDG autostart entry, the `pstotiff`
filter, and the documentation. It also removes `/etc/sane.d`, whose `dll.conf` belongs to
[sane-backends](#port-sane-backends), and enables `hpaio` instead through a file of its own,
`/etc/sane.d/dll.d/hpaio`.

The tarball carries prebuilt closed objects: `libImageProcessor` and the `hbpl1-*` and `lj-*`
printer plugins. None of them may reach the package, so `build.sh` fails the build if a file by
any of those names is in `$PKG`, and checks that `HPLIP.conf`, the quirks file for
[ipp-usb](#port-ipp-usb), is.

`make install` runs with `-j1`.

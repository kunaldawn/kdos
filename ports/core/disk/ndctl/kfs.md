## About

ndctl is the set of user-space tools and libraries for the kernel's persistent-memory and CXL memory
subsystems: `ndctl` manages NVDIMM regions and namespaces, `daxctl` manages DAX devices (including
handing device memory to the kernel as ordinary system RAM), and `cxl` lists and configures CXL
memory devices, ports and regions. Its libraries, `libndctl`, `libdaxctl` and `libcxl`, are what
[libblockdev](#port-libblockdev), configured `--with-nvdimm`, links.

## Flags

- `-Dversion-tag=$version` — Writes the version into `version.h` directly. Without it the build runs
  `tools/meson-vcs-tag.sh`, which asks `git describe` for the version and falls back to the project
  version only when the source is not a git checkout.
- `-Dsystemd=disabled` — On by default, it installs `ndctl-monitor.service`, `cxl-monitor.service`,
  the `daxdev-reconfigure@.service` template and `90-daxctl-device.rules`, a udev rule whose only
  action is to ask systemd to start that template for a new DAX device. KDOS has no systemd (see [No
  systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)),
  so none of them would do anything.
- `-Ddocs=enabled`, `-Dasciidoctor=enabled` — Build the manual pages, with
  [asciidoctor](#port-asciidoctor) rather than asciidoc and xmlto. Both are upstream's defaults,
  stated; with both enabled, a missing `asciidoctor` is a configure error.
- `-Dlibtracefs=enabled` — On by default. Builds `cxl monitor` and `cxl list --media-errors`, which read kernel
  trace events through [libtracefs](#port-libtracefs) and [libtraceevent](#port-libtraceevent).
- `-Dkeyutils=enabled` — On by default. Builds NVDIMM passphrase management (`ndctl setup-passphrase`,
  `update-passphrase`, `remove-passphrase` and `load-keys`), which stores the keys in the kernel
  keyring through [keyutils](#port-keyutils).
- `-Dtest=disabled` — Does not build or install the test programs; this is upstream's default as
  well.
- `-Dbashcompletiondir=/usr/share/bash-completion/completions` — Where the completion for the three
  tools goes. Left empty, meson asks bash-completion's pkg-config file and falls back to the same
  directory; named, the result does not depend on that lookup.

## Patches

- `strerror.patch` — In `util/help.c`, replaces the two `strerror_r(errno, sbuf, sizeof(sbuf))` calls whose result is printed with `%s`, and their buffers, with `strerror(errno)`. The code expects glibc's GNU `strerror_r`, which returns the message as a `char *`; musl has only the POSIX `strerror_r`, which returns an `int` and writes the message into the buffer.

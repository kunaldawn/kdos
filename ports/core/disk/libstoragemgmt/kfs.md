## About

libstoragemgmt is a vendor-neutral library for managing storage: RAID controllers, storage arrays
and the local disks of the machine, through one API (`liblsm`), the `lsmcli` command and plugins for
each kind of hardware. The plugins run under `lsmd`, a daemon that clients reach through sockets in
`/run/lsm/ipc`. In KDOS its consumer is [udisks2](#port-udisks2), built with `--enable-lsm`, whose
LSM module reaches `lsmd` through those sockets. The port installs the library, `lsmcli`, `lsmd`,
the plugins, and a service script that starts `lsmd` at boot.

## Flags

- `CFLAGS`, `CXXFLAGS` — The tree's flags with `-Wno-error` added. `configure.ac` adds `-Werror` to
  `CPPFLAGS`, which come before `CFLAGS` and `CXXFLAGS` on every compile line, so the later
  `-Wno-error` wins and a warning a newer compiler adds does not stop the build.
- `--without-test` — Leaves out the test suite. Its configure checks need the `check` library and
  `chrpath`, which the tree does not carry, and `valgrind` for the memory-leak tests.
- `--without-smispy` — Leaves out the SMI-S plugin for storage arrays, which is Python and needs
  `pywbem`. The tree has no pywbem port.
- `--without-ledmon` — Leaves out LED slot management, which needs the ledmon library. The tree has
  no ledmon port; with the default `yes`, configure stops when it does not find one.
- `--with-bash-completion-dir=/usr/share/bash-completion/completions` — Where `lsmcli`'s completion
  goes. Without it configure asks pkg-config, and falls back to `/etc/bash_completion.d` when that
  fails.
- `--with-systemdsystemunitdir=no`, `--with-systemd-sysusersdir=no`, `--with-systemd-tmpfilesdir=no`
  — Each defaults to the directory pkg-config reports for systemd; `no` installs no unit, sysusers
  or tmpfiles file. KDOS has no systemd (see [No
  systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd));
  the service script and `postinstall.sh` do the work of the three files.
- `PYTHON=python3` — The interpreter `AM_PATH_PYTHON` checks and records; it decides where the
  Python binding and the Python plugins are installed.

## Notes

- **The service script is written by `build.sh`.** `/etc/init.d/51_lsmd.sh` creates `/run/lsm` and
  `/run/lsm/ipc` with mode 775 and the group `libstoragemgmt`, the modes and group of upstream's
  tmpfiles entry, then starts `lsmd -d` under `supervise`. `lsmd` exits at start when it cannot read
  and write the socket directory, and `-d` keeps it in the foreground instead of forking away from
  the supervisor. How the service scripts run is in [rcS and the service
  scripts](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts).
  It runs after `01_udev.sh`, whose device database the local-disk plugin reads.
- **`postinstall.sh` creates the `libstoragemgmt` account.** `lsmd`, started as root, drops to it
  for every plugin that does not need root; without the account the plugins keep root. The script
  works on `PKG_ROOT`, the root `kpkgadd` is installing into, and creates the group and the user
  only when `/etc/group` and `/etc/passwd` there do not already name them.

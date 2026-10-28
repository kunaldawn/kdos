## About
libcap-ng is a second library for POSIX capabilities, beside [libcap](#port-libcap), meant to make
programming with them easier, with tools for finding programs that hold more privilege than
they need: `pscap` reports the capabilities of running processes, `netcap` those of processes with
listening sockets, and `filecap` the capabilities set on files. In KDOS it is linked by [util-linux](#port-util-linux), whose `setpriv` is
built with `--with-cap-ng`, by [openvpn](#port-openvpn), which requires it, and by
[qemu](#port-qemu), [libvirt](#port-libvirt), [virtiofsd](#port-virtiofsd),
[tcpdump](#port-tcpdump), [cifs-utils](#port-cifs-utils) and
[smartmontools](#port-smartmontools).

## Flags
- `--without-python` — Build no Python bindings. They are made with SWIG, and by default configure
  builds them whenever it finds `swig`, a Python 3 interpreter and its headers; this phase installs
  all three, so without the switch the package's contents would depend on build order.
  `--without-python3` names the same setting a second time.
- `--disable-cap-audit` — Do not build `cap-audit`, which traces a program with eBPF and reports the
  capabilities it uses. It needs clang, `llvm-strip`, `bpftool`, libbpf and libaudit; libaudit has
  no port (see
  [Features whose library has no port](../../../../docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port)).
  Off is upstream's default.

## Notes
The source is a GitHub tag archive with no generated `configure`, so `build.sh` runs
`./autogen.sh` first; the recipe names autoconf, automake, libtool and pkgconf in `depends` for it.

The package installs `libcap-ng.bash_completion` in `/usr/share/bash-completion/completions` and
links `cap-audit`, `filecap`, `netcap` and `pscap` to it there; the `cap-audit` link is installed
although the program itself is not built.

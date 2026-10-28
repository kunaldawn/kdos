## About
libcap is the library for POSIX [capabilities]{.idx}, the pieces root's power is split into, on
processes and on files, with `setcap`, `getcap`, `getpcaps` and `capsh`; `libpsx` beside it lets a
threaded program change the capabilities of all its threads at once. In KDOS it is linked by
[coreutils](#port-coreutils), whose `ls` shows a file's capabilities, by
[iproute2](#port-iproute2), [iputils](#port-iputils), [bubblewrap](#port-bubblewrap),
[crun](#port-crun) and [basu](#port-basu), and by others.

libcap has no configure script: `build.sh` is a single `make install` whose variables decide the
build.

## Flags
- `GOLANG=no` — Do not build the Go packages (`cap` and `psx`). Left unset, the makefiles build them
  whenever `go version` runs; [go](#port-go) is a port of `31_compilers`, so a rebuild after it
  would change the package.
- `PAM_CAP=no` — Do not build `pam_cap.so`, the PAM module that grants inheritable capabilities at
  login. Left unset, it is built whenever `/usr/include/security/pam_modules.h` exists;
  [pam](#port-pam) is a port of `41_system`, so the same applies.
- `USE_GPERF=yes` — Generate the table that turns a capability's name into its number with gperf, a
  perfect hash, instead of a linear search. Left unset, it is used whenever `gperf` is on `PATH`;
  the recipe depends on [gperf](#port-gperf) and makes it unconditional.
- `RAISE_SETFCAP=no` — Install `setcap` without the file capability `cap_setfcap` in its
  inheritable set. With `yes`, any user whose own inheritable set holds `cap_setfcap`, as some
  distributions give every user, can run `setcap` to put any capability on any file. `no` is
  also upstream's default, and the `setcap` run that `yes` adds to the install would be lost in the
  package anyway (see below).
- `lib=lib` — The library directory under the prefix. Left unset, the makefiles guess it from the
  dynamic loader's path, taking the line of `ldd /usr/bin/ld` that matches `ld-linux` or `ld.so`.
- `prefix=/usr` — Install under `/usr`. Without a prefix the makefiles put the tools in `/sbin` and
  the libraries in `/lib`.

## Notes
No program on the image is given a file capability by its package. A capability is an extended
attribute, and the package archive is written without `--xattrs`, so it would not survive; KDOS
uses setuid bits instead. `setcap` sets one by hand on a running system, as the security model
suggests for packet capture tools (see
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries)).

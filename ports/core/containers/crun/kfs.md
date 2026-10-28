## About

crun is an [OCI runtime]{.idx}: the program that takes a container's configuration and root
filesystem and starts its processes in new namespaces and cgroups, with the capabilities, seccomp
filter and mounts the configuration asks for. [podman](#port-podman) and [buildah](#port-buildah)
depend on it, and the shipped `/etc/containers/containers.conf` names it as the runtime, with
`cgroupfs` as the cgroup manager because there is no systemd to delegate one (see
[Containers](../../../../docs/kdos/06-reference/configuration.md#containers)).
A KDOS box is a rootless Podman container (see
[kdos-appbox](../../../../docs/kdos/04-programs/kdos-appbox.md)), so
every boxed application starts through crun.

The package is the `crun` program and its manual page, with `libcrun.a`, the static library the
program is linked from. No shared `libcrun` is built.

## Flags

- `./autogen.sh` — Regenerates the build system with `autoreconf -fi` before configure runs. The
  release tarball ships a `configure` of its own; the recipe replaces it. The script's `git
  submodule update` finds no repository in an unpacked tarball, fails, and is ignored; the
  vendored `libocispec` is already in the tarball.
- `--disable-systemd` — Builds no systemd support: crun cannot then create the cgroup through
  systemd's D-Bus API and needs no libsystemd. Support is on unless this is given, and configure
  stops with `Missing libsystemd headers` when the headers are absent, as they are on KDOS.
  Containers use the `cgroupfs` manager instead.
- `--enable-embedded-blake3` — Compiles the BLAKE3 hash from the copy inside crun's source instead
  of linking libblake3, which is not a port. On `auto` configure would choose the same, but only
  after looking for the library.
- `--disable-criu` — Builds no checkpoint and restore. That needs CRIU, which is not a port; see
  [Features whose library has no port](../../../../docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port).
- `--disable-shared`, `--enable-static` — Build `libcrun` as a static library only and link it
  into `crun`. This is the default of crun's configure (`LT_INIT([disable-shared])`), named
  here.

## Notes
crun parses its command line with `argp`, which glibc provides and musl does not; configure stops
with `argp functions not found` unless a library supplies `argp_parse`, which is why
[argp-standalone](#port-argp-standalone) is in `depends`.

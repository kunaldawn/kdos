## About
passt and pasta give a virtual machine or a container a network connection without root and
without a tap device or a bridge: they translate the guest's traffic into ordinary sockets of the
unprivileged user that runs them. `passt` serves a QEMU or libvirt guest over a UNIX socket;
`pasta` attaches to a network namespace, and it is the network podman gives a rootless container,
set as `default_rootless_network_cmd` in
[containers-common](#port-containers-common)'s `containers.conf`. The port also installs
`passt-repair`, the helper that sets the `TCP_REPAIR` socket options passt needs to move a guest's
connections during a migration, `pesto`, which shows and changes the port forwarding of a running
passt or pasta, and the manual pages.

## Flags
- `VERSION="$version"` — passt's Makefile takes the version from `git describe`, and a release
  archive has no git history, so without it every binary reports `unknown version`.
- `prefix=/usr` — The Makefile's install prefix, `/usr/local` by default; it is a make variable,
  not a configure switch.

## Patches
- `musl-close-range.patch` — Includes `linux_dep.h` in `isolation.c`. `close_range()` and `CLOSE_RANGE_UNSHARE` are glibc's, declared in `<unistd.h>`, and musl has neither; passt carries fallbacks for both in `linux_dep.h`, but `isolation.c`, the one file that calls `close_range()`, does not include that header, so without the patch it does not compile against musl.

## Notes
On x86-64 the default target compiles `passt` twice, once for any x86-64 processor and once as
`passt.avx2` with AVX2 code; `pasta` and `pasta.avx2` are symbolic links to them. At start the
plain binary re-executes its own executable, as `/proc/self/exe` names it, with `.avx2` appended
when the processor supports AVX2. Both are installed side by side; without the `.avx2` file the
plain binary warns and carries on with its own code.

`build.sh` puts a `fmt` that only copies its input ahead of the real one on `PATH`. `seccomp.sh`,
which generates the seccomp filters during the build, pipes the list of system calls each profile
allows through `fmt -t` to wrap it to the terminal's width. The script runs under `sh -eu`, so a
`fmt` that rejects `-t` would stop it with the seccomp header half written; the message goes to
standard output and nowhere into the generated headers, so the substitute changes only how the
build log looks.

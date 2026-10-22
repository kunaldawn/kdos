## About
NSPR, the Netscape Portable Runtime, is the platform layer under [nss](#port-nss): threads, locks,
time, file and socket I/O, and name lookup, behind one API on every system Mozilla supports.
Firefox ESR, LibreWolf, Thunderbird, Chromium and Qt WebEngine link it through NSS. It depends on
nothing but the C library.

## Flags
- `-D_PR_POLL_AVAILABLE`, `-D_PR_HAVE_OFF64_T`, `-D_PR_INET6`, `-D_PR_HAVE_INET_NTOP`,
  `-D_PR_HAVE_GETHOSTBYNAME2`, `-D_PR_HAVE_GETADDRINFO`, `-D_PR_INET6_PROBE` — Tell NSPR what the
  C library provides. `pr/include/md/_linux.h` decides these by testing `__GLIBC__`, which musl
  does not define, so without them NSPR builds as for an old C library: `select()` instead of
  `poll()`, the branch for a system with no large-file interfaces, and IPv4-only name lookup. With
  them it uses `poll()`, the 64-bit file interfaces, IPv6 sockets, `inet_ntop()`,
  `gethostbyname2()` and `getaddrinfo()`; `_PR_INET6_PROBE` makes it test once, by opening an
  `AF_INET6` socket, whether the kernel has IPv6 before it uses it.
- `--with-mozilla` — Build NSPR as Mozilla's own products expect it, with `MOZILLA_CLIENT`
  defined.
- `--with-pthreads` — Use POSIX threads as the thread system.
- `--enable-64bit` — Build the 64-bit library.
- `--enable-ipv6` — Compile in IPv6 support.
- `--enable-optimize`, `--disable-debug` — An optimised build, without NSPR's debug code.

## Patches
- `nspr-musl-lfs64.patch` — Makes the 64-bit file types that `_PR_HAVE_OFF64_T` selects `struct stat` and `off_t` rather than `struct stat64` and `off64_t`. musl's `off_t` is 64 bits already, and its headers declare the `*64` names only under `_LARGEFILE64_SOURCE`.

## Notes
The source carries NSPR in a subdirectory, `nspr/`, and its configure is run from a separate
`build` directory. After the install `build.sh` removes the static libraries and two files of
NSPR's own build machinery that it installs into `/usr/bin`, `compile-et.pl` and
`prerr.properties`.

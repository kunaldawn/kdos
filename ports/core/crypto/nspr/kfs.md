## About
NSPR, the Netscape Portable Runtime, is the platform layer under [nss](#port-nss): threads, locks,
time, file and socket I/O, and name lookup, behind one API on every system Mozilla supports.
Firefox ESR, LibreWolf, Thunderbird, Chromium and Qt WebEngine link it, directly and through
NSS. It depends on nothing but the C library.

## Flags
- `-D_PR_POLL_AVAILABLE`, `-D_PR_INET6`, `-D_PR_HAVE_INET_NTOP`, `-D_PR_HAVE_GETHOSTBYNAME2`,
  `-D_PR_HAVE_GETADDRINFO`, `-D_PR_INET6_PROBE` — Tell NSPR what the C library provides.
  `pr/include/md/_linux.h` decides these by testing `__GLIBC__`, which musl does not define, so
  without them NSPR builds as for an old C library: `select()` instead of `poll()`, and IPv4-only
  name lookup. With them it uses `poll()`, IPv6 sockets, `inet_ntop()`, `gethostbyname2()` and
  `getaddrinfo()`; `_PR_INET6_PROBE` makes it test once, by opening an `AF_INET6` socket, whether
  the kernel has IPv6 before it uses it. The large-file choice is made by the patch instead, not
  by a flag: `-D_PR_HAVE_OFF64_T` would select `open64`, `stat64` and `lseek64`, which musl
  declares only under `_LARGEFILE64_SOURCE`.
- `--with-mozilla` — Build NSPR as Mozilla's own products expect it, with `MOZILLA_CLIENT`
  defined.
- `--with-pthreads` — Use POSIX threads as the thread system.
- `--enable-64bit` — Build the 64-bit library.
- `--enable-ipv6` — Compile in IPv6 support.
- `--enable-optimize`, `--disable-debug` — An optimised build, without NSPR's debug code.

## Notes
The source carries NSPR in a subdirectory, `nspr/`, and its configure is run from a separate
`build` directory. After the install `build.sh` removes the static libraries and two files of
NSPR's own build machinery that it installs into `/usr/bin`, `compile-et.pl` and
`prerr.properties`.

`nspr-musl-lfs64.patch` makes `pr/include/md/_linux.h` define `_PR_HAVE_LARGE_OFF_T` where it
would define `_PR_NO_LARGE_FILES` for any C library that is not glibc. musl's `off_t` is 64 bits on
every architecture, so NSPR's 64-bit file calls go to plain `open`, `stat`, `fstat`, `lseek` and
`mmap`, as on the BSDs. Under `_PR_NO_LARGE_FILES` `PR_Seek64` would go through NSPR's own seek
wrapper, which refuses an offset past 2 GiB with `EFBIG`. The
32-bit `PR_GetFileInfo` reports `PR_FILE_TOO_BIG_ERROR` for a file over 2 GiB; `PR_GetFileInfo64`
does not.

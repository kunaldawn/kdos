## About
This is OpenBSD's netcat, `nc`, as Debian maintains it for Linux: it opens TCP, UDP and UNIX
socket connections or listens on them, and copies standard input and output across. The source is
Debian's packaging repository, which holds the OpenBSD code and the patch series that ports it to
Linux through [libbsd](#port-libbsd); two files from Alpine's netcat-openbsd package make it
build against musl.

## Flags
- `SRCS` — The `Makefile`, as Debian's patch series leaves it, builds `netcat.c`, `atomicio.c`
  and `socks.c`. `build.sh` adds `base64.c`, Alpine's copy of the BIND `b64_ntop()` and
  `b64_pton()`: `socks.c` calls `b64_ntop()` to encode the user name and password it sends to an
  HTTP CONNECT proxy, and musl has no such function.
- `-DIPTOS_DSCP_VA=0xb0` — Defines the DSCP value for the `va` keyword of `nc -T`, the Voice
  Admit codepoint of RFC 5865. `netcat.c` names the constant in its table of TOS keywords, and
  musl's `<netinet/ip.h>` does not define it.
- `-DDEBIAN_VERSION` — The version string `nc -h` prints as "Debian patchlevel"; Debian's own
  `debian/rules` sets it from the package version, and here it is `$version-$debrev`.

## Patches
- `b64.patch` — Declare `b64_ntop()` beside the other prototypes near the top of `socks.c`,
  because musl's `<resolv.h>` has no prototype for it and GCC rejects a call to an undeclared
  function.

## Notes
`build.sh` applies every patch in `debian/patches/series`, in order, and then Alpine's
`b64.patch`. The series includes
`build-without-TLS-support.patch`, so this `nc` has none of OpenBSD's TLS options and links no TLS library.
The program installs as `/usr/bin/nc`, where Debian names it `nc.openbsd`.

## About

xhost edits an X server's access list. Xwayland admits only the account that started it, so a
program that runs as root on the session's display must be added to that list first; GParted's
launcher runs `xhost +SI:localuser:root` before pkexec and removes the entry when GParted exits.

## Flags

- `--disable-secure-rpc` — No SUN-DES-1 entries. They need Secure RPC from libtirpc, and nothing
  on this system authenticates that way.
- `--with-localedir=no` — Compile the gettext calls out. xhost ships no translations, and a
  `libintl.h` from GNU gettext ahead of musl's would turn them into `libintl_gettext` references
  that configure's link test, run against musl, never saw.

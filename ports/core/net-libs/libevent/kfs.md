## About
libevent runs callbacks when a file descriptor becomes ready, a timer expires or a signal arrives,
over the kernel's own mechanism (epoll on Linux), and adds buffered I/O, DNS, HTTP and RPC layers
on top. [tmux](#port-tmux), [transmission](#port-transmission), [firefox-esr](#port-firefox-esr)
and [thunderbird](#port-thunderbird) are among the ports that link it.

## Flags
- `--enable-openssl` — Build `libevent_openssl`, the TLS layer for libevent's buffered events.
  It is configure's default, and with it configure stops with an error when it cannot find
  OpenSSL, through `pkg-config` or by linking `SSL_new`; `--disable-openssl` is the only way to
  build without it. [openssl](#port-openssl) is in `depends` because the build needs it.
- `--disable-samples` — Build none of the example programs under `sample/`.

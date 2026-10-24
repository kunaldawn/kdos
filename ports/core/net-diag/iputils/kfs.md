## About
iputils provides `ping`, `arping`, `tracepath` and `clockdiff`. `ping` needs no privilege: it opens
an ICMP datagram socket, which `/etc/sysctl.conf` lets every user do through
`net.ipv4.ping_group_range`; see
[setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries).

## Flags
- `-DNO_SETCAP_OR_SUID=true` — Install every program without a setuid bit or a file capability.
  `ping` needs neither on a kernel with ICMP datagram sockets; `arping` and `clockdiff` are run as
  root.
- `-DUSE_IDN=false` — Build without internationalised domain names. With IDN on, `ping` and
  `tracepath` pass `AI_IDN` to `getaddrinfo`, a glibc extension that musl rejects with
  `EAI_BADFLAGS`, so every lookup, a numeric address included, fails with `Invalid flags`. Without
  it a host name is looked up as written, and a non-ASCII name must be given in its `xn--` form.
- `-DUSE_GETTEXT=false` — No message catalogues are built; the programs print English.
- `-DBUILD_MANS=false` — The manual pages are built with DocBook XSL; the release tarball carries
  them already built under `doc/`, which `build.sh` installs.

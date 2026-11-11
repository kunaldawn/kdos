## About
babeld is the reference implementation of Babel, a distance-vector routing protocol for networks
that mix wired and wireless links, such as community meshes. `31_babeld` starts it only once
`/etc/babeld.conf` exists and is not empty, so an image that routes nothing runs nothing.

## Flags
- `-I ''` — No pid file. babeld's default, `/var/run/babeld.pid`, is opened with `O_EXCL`, so a
  file left by a babeld that was killed makes the next one exit at once, and the supervisor would
  restart it every few seconds. The supervisor already knows the daemon's pid and keeps it in
  `/run/kdos-svc.babeld.pid`.
- `-S /var/lib/babeld/state` — Where babeld keeps what it needs across a restart, in a directory
  the package creates. Without the file a started babeld stays passive for three minutes, and its
  neighbours may ignore it at first.

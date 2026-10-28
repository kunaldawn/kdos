## About
Trippy is a traceroute and ping in one terminal program, `trip`: it traces the route to a host
continuously and shows, for each hop, the loss, latency and jitter. It is built from its
[Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)
with no network.

## Flags
- `--generate-man` — `trip` prints its own manual page, rendered from its command-line
  definition; `build.sh` runs the program it has just built to write `trip.1`. The three
  `--generate bash|zsh|fish` runs after it write the shell completions the same way.

## Notes
`trip` opens raw sockets, which Linux allows only with `CAP_NET_RAW`. The package installs it with
neither a setuid bit nor a file capability: run it as root, or grant the capability yourself with
`setcap CAP_NET_RAW+p /usr/bin/trip`, which upstream's privileges guide documents and which
`trip` raises into its effective set itself and drops once the sockets are open. See
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).
Its `--unprivileged` mode is supported on macOS only, so on KDOS it is not a way round the
capability.

A reinstall or upgrade of the package replaces `/usr/bin/trip` and with it any capability set on
the file, so the `setcap` has to be run again.

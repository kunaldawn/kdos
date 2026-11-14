## About

Which vendor port ships each file of [linux-firmware](#port-linux-firmware):
`/usr/share/kdos/firmware-map`, one `<port> <path>` line per file and link, 6,576 lines and 321,604
bytes, read by
[`kdos firmware sync`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-firmware) to turn a
firmware file a driver asks for into the port to install. It is built from the same tarball by the
same `build.sh` as the vendor ports, so it answers with their rule, not a copy of it; the
installation medium carries it.

## Flags

- `--zstd` — The staging tree is the vendor ports' own, so each path is recorded as they install it
  and then without its `.zst`, the name a driver requests; see
  [linux-firmware](#port-linux-firmware).

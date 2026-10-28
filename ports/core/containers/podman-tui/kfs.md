## About
[podman-tui]{.idx} is a terminal interface for [Podman](#port-podman): images, containers, pods,
volumes, networks and secrets, browsed and managed from one screen. It is a client of the Podman
API, not of the `podman` command, so it needs a running API service to talk to. It is a Go program
built from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `-tags "remote containers_image_openpgp exclude_graphdriver_btrfs"` — the same build tags as
  upstream's `Makefile` (`BUILDTAGS`). `remote` compiles the Podman bindings as an API client only,
  without the local container engine; `containers_image_openpgp` verifies image signatures with a
  pure-Go OpenPGP implementation instead of binding gpgme; `exclude_graphdriver_btrfs` leaves out
  the btrfs storage driver, which needs the btrfs C headers. The last two are what let the program
  build with `CGO_ENABLED=0`, which upstream's `Makefile` also sets.

## Notes
The menu entry, *Containers*, runs `kdos-podman-api podman-tui` in a terminal. That wrapper starts
`podman system service` when nothing answers on the account's API socket and gives podman-tui a
default connection to it; run bare, podman-tui has no connection until one is added, with
`podman system connection add` or in podman-tui's own system view. See
[Containers](../../../../docs/kdos/06-reference/configuration.md#containers).

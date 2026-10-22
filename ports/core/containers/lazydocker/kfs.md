## About
[lazydocker]{.idx} is a terminal interface for containers: containers, images, volumes and their
logs and statistics on one screen, with single keys for the common actions. It speaks the Docker
API, which [Podman](#port-podman)'s API service also serves, so on KDOS it manages Podman's
containers. It is a Go program built from its vendor bundle, the second `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `-X main.version=$version` — inside the one `-ldflags` string, set the version `lazydocker
  --version` reports. Upstream's release builds stamp it the same way (`.goreleaser.yml`).
  Unstamped, `main.go` falls back to the commit Go records from a git checkout, and the unpacked
  tarball has none (the tree's `GOFLAGS` also carries `-buildvcs=false`), so it would report
  `unversioned`.

## Notes
The menu entry runs `kdos-podman-api lazydocker` in a terminal. The wrapper starts
`podman system service` when nothing answers on the account's API socket and points `DOCKER_HOST`
at it; typed at a prompt, `lazydocker` reaches the socket only while a service is already
running. See [Containers](../../../../docs/kdos/06-reference/configuration.md#containers).

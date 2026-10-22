## About

Pat is a Winlink email client for amateur radio, written in Go: it sends and receives Winlink
messages over a radio link (ARDOP through [ardopcf](#port-ardopcf), AX.25 packet, VARA) or over
telnet to a Winlink server, and serves its own web interface for reading and writing them. The
recipe's desktop entry runs `pat http --addr localhost:8083` in a terminal, and the interface is
then at that address in a browser.

## Flags

- `-trimpath` — Strip the build machine's file-system paths from the binary. The exported
  `GOFLAGS` already carries it (see
  [go](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go)), so on the command line it repeats
  that setting.
- `--addr localhost:8083` — In the desktop entry, not the build: serve the web interface on port
  8083 of the local machine only. Pat's default port is 8080, and 8080 to 8082 are taken by the
  init scripts of kiwix-serve, kolibri and llama-server (`fs/etc/init.d`), so on a machine running
  them the default port is in use and `pat http` exits at once.

## Notes

The build sets no `libax25` tag. That transport speaks the kernel's AX.25 sockets, and the KDOS
kernel is built without AX.25, so packet radio goes through the `ax25+agwpe` transport to the AGW
port of a soundcard modem such as [direwolf](#port-direwolf) instead. The web interface is
compiled in from `web/dist` as upstream publishes it, which is built JavaScript (see
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).

The Go modules are not downloaded at build time. `ports/fetch` runs `go mod vendor` against the
release and packs the module `vendor/` tree into `pat-vendor-<version>.tar.xz`, whose `sha256 =`
line in the recipe holds it like any other source; `build.sh` unpacks it and builds with
`-mod=vendor` (see [Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

## About
termshark is a terminal interface for packet captures, written in Go. It does no dissection of its
own: it runs `tshark` from [wireshark](#port-wireshark), reads its PDML output (`tshark -T pdml`)
and draws the packet list, the protocol tree and the hex view, which can be scrolled and filtered
interactively. wireshark in `depends` is therefore a run-time dependency the program cannot do
without: with no `tshark` on the `PATH`, termshark stops at start with
`Could not find tshark in your PATH`.

## Notes
The build is the canonical Go shape, with cgo off; see
[go](../../../../docs/kdos/05-developer/writing-ports.md#go). Its module dependencies come from the
vendor bundle, `termshark-vendor-<version>.tar.xz`, which `ports/fetch` makes by running
`go mod vendor` at the top of the source (`vendoring = go`) and which is held to its own
`sha256 =` line in the recipe; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). `build.sh` unpacks it
beside `go.mod` and builds with `-mod=vendor`, so the build reaches no module proxy.

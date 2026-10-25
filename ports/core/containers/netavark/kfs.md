## About
netavark is podman's network backend: when a container joins a podman network it creates the
bridge and the veth pair, assigns the addresses, and writes the NAT and port-forwarding rules
with the nftables firewall driver that
[containers-common](#port-containers-common)'s `containers.conf` selects; it starts
[aardvark-dns](#port-aardvark-dns) for name resolution between containers. Podman runs it as a
helper, so the port installs it at `/usr/libexec/podman/netavark`, a directory podman searches for
its helper binaries, with its manual pages.

## Notes
netavark is built from a vendor bundle, `netavark-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). Its build script
compiles `src/proto/proxy.proto`, the protocol of the DHCP proxy, with `protoc`, which is why
[protobuf](#port-protobuf) is in `depends`; the manual pages are converted from Markdown with
[go-md2man](#port-go-md2man) by `docs/Makefile`. The `nft` program of [nftables](#port-nftables)
is what the firewall driver runs.

netavark writes its rules into a table of its own, `inet netavark`, beside the system firewall's. The
firewall's ruleset replaces only its own `inet filter` table, so a reload leaves rootful
containers' address translation in place; see
[Adding your own rules](../../../../docs/kdos/02-user-guide/administration.md#adding-your-own-rules).

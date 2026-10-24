## About

nftables is the kernel's packet filtering framework and `nft`, the program that loads and lists
its rules. The KDOS [firewall]{.idx} is written in it: `/etc/init.d/25_nftables.sh` checks
`/etc/nftables.conf` and the drop-ins in `/etc/nftables.d/` with `nft -c` and loads them at boot,
and `kdos-powerd` rewrites `50-kdos-services.nft` and reloads the ruleset when an administrator
opens or closes a service. The shipped ruleset and how it is applied are in
[The firewall](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#the-firewall).

Container networking uses it as well. [netavark](#port-netavark), podman's network backend, runs
with its nftables firewall driver and drives `nft` in JSON mode.
[networkmanager](#port-networkmanager), [libvirt](#port-libvirt), [dnsmasq](#port-dnsmasq) and
[wireguard-tools](#port-wireguard-tools) depend on this port too.

## Flags

- `--with-cli=readline` — Builds `nft -i`, the interactive shell, with GNU
  [readline](#port-readline) for line editing. configure's default is editline.
- `--with-json` — Builds JSON input and output (`nft -j`) on [jansson](#port-jansson). netavark's
  vendored nftables library runs `nft -j` both to read the ruleset and to apply its own, so
  without this switch every one of those calls fails.
- `--without-xtables` — Builds without libxtables, the [iptables](#port-iptables) library
  upstream uses "for iptables interaction": rules that carry iptables match or target extensions.
  It is upstream's default, and the KDOS ruleset is written in native nftables.
- `--enable-extended-parser-errors` — Builds the rule parser with Bison's `parse.error=custom`
  and LAC (lookahead correction) for its syntax error messages. It needs a [bison](#port-bison)
  recent enough for `parse.error=custom`; left unset, configure turns it on only when it finds
  one.
- `--enable-man-doc` — Installs the manual pages `nft(8)`, `libnftables(3)` and
  `libnftables-json(5)`. The release tarball carries them already built, so `a2x` is not needed.
  It is upstream's default.
- `--disable-debug` — Leaves out the `-g -DDEBUG` that the Makefile adds to every compile when
  this option is on, as it is by default.

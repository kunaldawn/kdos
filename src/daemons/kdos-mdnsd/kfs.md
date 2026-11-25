## About

`kdos-mdnsd` answers DNS questions about `.local` host names from Avahi, so that `ping`, `ssh`, a
browser, `getent hosts` and CUPS resolve `<host>.local`. musl has no NSS, so no `nss-mdns` module
can plug into `getaddrinfo()`; every lookup goes to NetworkManager's dnsmasq on `127.0.0.1`
instead, and `/etc/NetworkManager/dnsmasq.d/kdos-mdns.conf` forwards the `local` domain to this
daemon on UDP `127.0.0.1:5380`. It asks [avahi](#port-avahi) over the daemon's simple-protocol
socket, `/run/avahi-daemon/socket`, and answers with the address at a ten-second TTL. What it
answers for each kind of question is in
[kdos-mdnsd](../../../docs/kdos/04-programs/daemons.md#kdos-mdnsd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and nothing else; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
It depends on avahi because every answer is Avahi's: without the daemon it could only say
`SERVFAIL`.

## Notes

`/etc/init.d/45_mdns.sh` starts it after `45_avahi.sh`, under `ksvc` with `--restart-with
avahi-daemon`, and skips it when `avahi-daemon` is not installed or `/etc/service.disabled/avahi`
exists. It binds its port as root and then becomes `nobody`, with no supplementary groups and
`no_new_privs`, before it reads a packet.

A miss — no address from Avahi within three seconds, a `-` reply, or an IPv6 link-local address —
is `NOERROR` with no answer and an SOA for `local`, never `NXDOMAIN`. musl's `getaddrinfo()` sends
the `A` and `AAAA` questions together and fails the whole lookup on an `NXDOMAIN` to either, so
answering one would make an IPv4-only host unresolvable; the SOA's ten-second minimum lets dnsmasq
cache the miss. A link-local address is left out because a DNS record cannot carry its interface.

The parser refuses a compression pointer that does not point before the last one it followed, so
a pointer loop cannot keep it reading. `kdos-mdnsd --answer QUERY-FILE AVAHI-REPLY` runs one packet
through the parser and answer builder offline, which `testing/selftest.sh` drives with
`testing/fixtures/mdnsd/drive.py`, under ASan as well when `CC` asks for it.

A machine whose network is run by `30_network`'s dhcpcd has no dnsmasq to forward to it, and
resolves `.local` only through Avahi's own tools and libcups; see
[Known gaps](../../../docs/kdos/06-reference/known-gaps.md#local-names-resolve-only-through-networkmanagers-dnsmasq).

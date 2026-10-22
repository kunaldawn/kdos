## About
libnl is a set of libraries over the kernel's netlink sockets: `libnl-3` for the protocol itself,
`libnl-route-3` for links, addresses and routes, `libnl-genl-3` for generic netlink, and smaller
ones for netfilter and the rest. KDOS's wireless stack talks to the kernel through it —
[wpa_supplicant](#port-wpa_supplicant), [hostapd](#port-hostapd), [iw](#port-iw) and
[NetworkManager](#port-networkmanager) — and [libpcap](#port-libpcap) uses it to put a wireless
interface into monitor mode.

## Notes
The build is configure's plain shape. [flex](#port-flex) and [bison](#port-bison) are in
`depends` because the route library parses classifier expressions (`ematch`) and packet location
aliases (`pktloc`) with scanners and grammars the release tarball carries only as `.l` and `.y`
sources, so the build generates them.

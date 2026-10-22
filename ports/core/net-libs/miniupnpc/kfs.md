## About
miniupnpc is a UPnP Internet Gateway Device client library: it finds the home router on the local
network and asks it to forward a port or report the external address. The games and peer-to-peer
programs that can open their own port on a router link it, among them
[transmission](#port-transmission), [ddnet](#port-ddnet), [ppsspp](#port-ppsspp) and
[warzone2100](#port-warzone2100).

## Flags
- `-DUPNPC_BUILD_SHARED=ON` — Build `libminiupnpc.so`; with `-DUPNPC_BUILD_STATIC=OFF` no static
  archive is built beside it. Upstream builds both by default.
- `-DUPNPC_BUILD_SAMPLE=OFF` — Build none of the sample programs, `upnpc` and `upnp-listdevices`,
  so the package is the library alone. `-DUPNPC_BUILD_TESTS=OFF` likewise builds no tests.

## Patches
- `includes.patch` — Include `<stddef.h>` in `include/upnpcommands.h`, whose prototypes use `size_t` while neither it nor the two headers it includes declare it, so a source that includes it before any standard header fails to compile.

## Notes
Upstream installs `external-ip.sh` whatever the options; it is a one-line wrapper around `upnpc`,
which this build does not install, so `build.sh` removes it.

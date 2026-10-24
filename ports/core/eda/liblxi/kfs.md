## About
liblxi talks to LXI test instruments (oscilloscopes, power supplies, signal generators) over the
VXI-11 RPC protocol and raw TCP, and finds them on the network with mDNS through
[avahi](#port-avahi). [lxi-tools](#port-lxi-tools) is its one consumer: the `lxi` command and the
`lxi-gui` window.

## Flags
- `-Db_lundef=true` — Link the library with `--no-undefined`. Upstream's `meson.build` sets
  `b_lundef=false` in its default options, which lets a library with unresolved symbols link
  and fail only when a program loads it; with the switch a missing library stops the build.

## Notes
VXI-11 is Sun RPC. musl carries no RPC, so every `xdr_*` and `clnt_*` call resolves in
[libtirpc](#port-libtirpc), and `link-tirpc.patch` adds it to the library's dependencies on
every system that uses it; upstream adds it only on Cygwin. Without it `liblxi.so.1` has no
`libtirpc.so.3` in `NEEDED`, and `lxi` stops at load with `xdr_u_char: symbol not found`.

The `grep` after setup stops the build when configure did not find `avahi-client/client.h`:
`src/meson.build` then compiles no mDNS backend, and mDNS discovery finds nothing.

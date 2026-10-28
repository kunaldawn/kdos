## About
[esbuild]{.idx} bundles and minifies JavaScript and TypeScript. It is one Go program, built here
with cgo off from its vendor bundle. [chromium](#port-chromium) is the reason for the port: its
build deletes every ELF file in its source tree and links `/usr/bin/esbuild` into
the DevTools front end's `third_party/esbuild` in its place.

## Flags
- `-mod=vendor` — Build from the `vendor/` directory the vendor bundle unpacks into the source,
  never from the module cache or the network.
- `-trimpath` — Leave the build directory's paths out of the binary. The tree's exported
  `GOFLAGS` carries it already; see
  [go](../../../../docs/kdos/05-developer/writing-ports.md#go).
- `-ldflags "-s -w"` — Leave the symbol table and the DWARF debugging information out of the
  binary. The Go linker writes both by default, and `kpkg` does not strip them.

## Notes
`series` holds the port at one exact release, the one the DevTools front end's `DEPS` in the
Chromium source pins its esbuild to (the `infra/3pp/tools/esbuild` package). The version
checker offers no other release while `series` names it (see
[Filtering](../../../../docs/kdos/05-developer/writing-ports.md#filtering)),
and a Chromium release that moves the pin needs `version` and `series` moved to match.

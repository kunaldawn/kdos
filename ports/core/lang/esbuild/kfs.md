## About
[esbuild]{.idx} bundles and minifies JavaScript and TypeScript. It is one Go program, built here
with cgo off from its vendor bundle. [chromium](#port-chromium) is the reason for the port: its
build deletes every prebuilt executable in its source tree and links `/usr/bin/esbuild` into
the DevTools front end's `third_party/esbuild` in its place.

## Flags
- `-trimpath` — Leave the build directory's paths out of the binary. The phase's exported
  `GOFLAGS` carries it already; see
  [go](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go).

## Notes
`series` holds the port at one exact release, the one the DevTools front end's `DEPS` in the
Chromium source pins its esbuild to (the `infra/3pp/tools/esbuild` package). The version
checker offers no other release while `series` names it (see
[Filtering](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#filtering)),
and a Chromium release that moves the pin needs `version` and `series` moved to match.

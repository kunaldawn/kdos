## About
CNCjs is a machine controller for Grbl, Marlin, Smoothieware and TinyG boards, written in
JavaScript. It is a [nodejs](#port-nodejs) server that talks to the machine over a serial port and
serves the controller's interface to a web browser. Upstream builds it with Yarn and packages it as
an Electron application; this port builds only the server and its browser client, and installs it
under `/usr/lib/cncjs` with `cncjs` on the path. Its desktop entry runs the server in a terminal,
and the interface is then opened at `http://localhost:8000`.

## Flags
- `--legacy-peer-deps` — npm installs the tree without enforcing peer-dependency ranges. Upstream
  resolves its tree with Yarn and pins a peer that npm's strict check refuses. The same word is the
  recipe's `npmflags`, the flags of the `npm install` that writes the vendor bundle, and every
  offline `npm` call in `build.sh` must repeat it, or npm resolves a tree different from the
  bundle's (see [Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).
- `--omit=dev` — `npm prune` removes every development dependency (Babel, webpack and the rest of
  the build toolchain) from `node_modules` once the build is done, leaving what the server loads at
  run time.
- `--offline` — npm uses only what is on disk and never asks the registry; the build has no
  network, and the bundle carries the `package-lock.json` the prune reads.
- `--ignore-scripts` — npm runs no package's install scripts during the prune; the one native
  module the server needs is compiled explicitly, by `node-gyp rebuild`, just before it.
- `--nodedir` — `node-gyp` compiles `@serialport/bindings-cpp`, the serial-port native module,
  against the headers of the installed node rather than downloading a header tarball for its
  version.
- `--host 127.0.0.1` — On the desktop entry's `cncjs` command line: the server listens on the
  loopback address only. Run without `--host`, the server binds `0.0.0.0` and serves the controller,
  which drives a physical machine, to the whole network.

## Patches
- `no-update-check.patch` — Sets the default of `checkForUpdates` in the server's configuration store to `false`. With it on, the interface asks the npm registry for the latest `cncjs` release. The default is merged under the user's configuration file, so a `state.checkForUpdates` saved there, as the switch on the About settings page saves it, still wins.

## Notes
The vendor bundle, `cncjs-vendor-<version>.tar.xz`, is `node_modules/` and the
`package-lock.json` that `ports/fetch` writes by running `npm install --ignore-scripts
--legacy-peer-deps` against the source, packed reproducibly and pinned by its own `sha256 =` line.
A version bump regenerates it, and its hash line changes with it.

npm downloads prebuilt native binaries into that tree, compiled elsewhere for Linux, macOS,
Windows and Android: the `@serialport/bindings-cpp` prebuilds, the glibc builds of
`@parcel/watcher` and of the `@unrs` resolver, and the executables in `app-builder-bin`,
`7zip-bin` and `term-size/vendor`. `build.sh` deletes them
all before building, because nothing on the host may run a binary it did not compile (see
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).
The serial-port module is then compiled from its source by `node-gyp`; without it the server cannot
open a machine's port.

The build steps are upstream's `scripts/build-prod.sh`, run as plain commands: no Yarn, no
`i18next-scanner` pass, and no Electron application. Translations other than English are removed
from the client and the server, and source maps are deleted.

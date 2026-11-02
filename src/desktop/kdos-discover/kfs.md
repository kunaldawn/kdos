## About
`kdos-discover` is what makes [Discover](#port-discover) KDOS's software centre. It installs
four things:

- **`kdos-backend.so`**, a Discover backend in `/usr/lib/qt6/plugins/discover/`. It lists the
  catalogue's applications and sets and the packs in the pack store as `kdos-packs`, the packs and
  `.ktar` sets on the boot medium as `medium`, Debian desktop applications the catalogue does
  not name as `kdos-store`, boxes nothing else accounts for as `kdos-boxes`, and host software as
  `kdos-ports`, and it opens a `.ktar` or `.kpack` handed to Discover from a file manager. Every
  answer comes from the tool that owns it: `kdos-appbox catalogue` says what is installed,
  `kdos-box list` what each box is made of, [kdos-packd](#port-kdos-packd)'s socket what the pack
  store and the medium hold, `kpkg list` which ports are installed, `kdos-box profile` what a box
  is allowed, asked only when its page opens. An install or a removal is a
  list of argument vectors and kdos-packd requests, run one transaction at a time, with no shell.
  A catalogue row whose pack is already in the store or on the medium becomes a box over that
  pack, offline; only a row with no pack anywhere is built by the store lane, which reaches the
  Debian archive and says so before it starts.
- **`KdosNotifier.so`**, in `discover-notifier/`, the plugin that puts the A/B system update in
  Discover's tray. It reads kdos-sysupdated's io.kdos.Slots1 State for the offered version, the
  update job in progress and a slot armed and waiting for a restart, `kdos cve --json` for the
  known vulnerabilities an update closes, and, when kdos-sysupdated does not answer,
  `/var/lib/kdos/update.json` and `kdos-bootctl status --json` in its place; it fetches nothing.
- **`kdos-discover-catalog`**, which writes `/usr/share/swcatalog/xml/kdos-ports.xml`, the
  AppStream catalogue of the host's ports, from kpkg's manifests.
- **`kdos-ports-helper`**, the root step behind installing host software from the signed binhost,
  run through `pkexec` under the polkit action `io.kdos.ports.install`.

It also points Discover's featured list at `/usr/share/kdos/discover/featured.json`, and defines
the MIME types `application/vnd.kdos.ktar` and `application/vnd.kdos.kpack` with a hidden
desktop entry that opens both in Discover. How Discover, the backend and the system update fit
together is in [Discover](../../../docs/kdos/04-programs/discover.md).

## Flags
- `-D BUILD_TESTING=OFF` — Build no tests here. The tests in `tests/` read fixtures of every
  tool's output and need Qt's test library; `testing/plasmatest.sh` builds and runs them in the
  development container.
- `-D KF_SKIP_PO_PROCESSING=ON` — Install no translations; the image is English only. Every
  string still goes through `i18n()` under the domain `kdos-discover`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the two plugins under Qt's own plugin
  directory, `/usr/lib/qt6/plugins`, where Discover's loaders look for `discover/` and
  `discover-notifier/`.
- `-I"$LIBS/libkbase"`, `"$LIBS"/libkbase/*.c` — `kdos-ports-helper` is C, compiled with
  libkbase for its argument-vector runner, and links no Qt. `$LIBS` is `src/libs`.

## Notes
- **It is locked to one discover release.** libdiscover is private: Discover installs no header
  and promises no ABI, and both plugin interfaces carry discover's full version in their IID
  (`org.kde.discover.6.8.2.AbstractResourcesBackendFactory`). Discover's loaders skip a plugin
  whose IID differs with nothing but a debug line, so a stale build simply vanishes from Discover.
  The discover recipe copies libdiscover's headers to `/usr/include/discover`, and
  `CMakeLists.txt` refuses to configure against any discover but `KDOS_DISCOVER_VERSION`. A
  discover bump, a point release included, therefore rebuilds this port: re-test the backend and
  the notifier against the new headers and change that line.
- **Nothing downloads.** A refresh asks local tools and reads local files, and Discover's "Check
  for updates" is the same refresh. Screenshots are shown only when they are files on the
  machine. The Debian lane's metadata is the DEP-11 catalogue
  [debian-dep11](#port-debian-dep11) installs under `/usr/share/swcatalog`, read lazily: a resource is made for a Debian application only when a
  search or a category asks for it, or when its box exists.
- **Debian applications need kdos-appbox's `deb:` form.** `install deb:<package>` and
  `uninstall deb:<package>` build and remove a box named `deb.<package>`, with each `+` in the
  package name spelled `_`; the backend reads that name back to tell a Debian application
  installed.
- **Host software is read-only here until a binhost is fetched.** A port arrives and leaves with
  the system version, so the `kdos-ports` lane lists what is installed and offers no removal. Once
  `kdos update fetch` has put the release's signed binhost index and its catalogue in
  `/var/lib/kdos/binhost`, a port that catalogue lists can be installed. `kdos-ports-helper`
  checks the name, clears the environment pkexec handed it and runs `kdos update install
  <port>`, which downloads the port and every package it lacks from that binhost, installs them
  matched under `/etc/kdos/chroot.env`'s build environment, and adds the port to
  `/var/lib/kdos/world` so the next update keeps it; then the helper writes the catalogue again.
  The helper is a separate program because pkexec's grant names a program and not its
  arguments: granting `/usr/bin/kdos` would grant every subcommand.
- **Host software is read from its two catalogue files, not from the AppStream pool.** The pool
  keys every component of the OS catalogue by a data id with the origin `os`, so `org.kde.kate`
  from `kdos-ports.xml` and `org.kde.kate` from Debian's DEP-11 file would be one component,
  whichever loaded last. The backend parses `kdos-ports.xml` and each binhost's
  `kdos-binhost.xml` directly at every refresh, keeps the pool for the Debian lane's search and
  categories, and never offers in a box a Debian application whose component id a host port
  already has.
- **The catalogue is written wherever packages change.** `kdos-discover-catalog` rewrites the
  whole of `kdos-ports.xml` from kpkg's manifests: a port's own metainfo is its component, given
  the package name, and a desktop entry no metainfo launches is read by libappstream's
  desktop-entry reader. The image step runs it once over the finished tree.

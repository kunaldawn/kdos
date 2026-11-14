## About
`kdos-discover` is what makes [Discover](#port-discover) KDOS's software centre. It installs
five things:

- **`kdos-backend.so`**, a Discover backend in `/usr/lib/qt6/plugins/discover/`. It lists the
  catalogue's sets, the packs in the pack store and the catalogue rows whose pack is there as
  `kdos-packs`, the packs and `.ktar` sets on the boot medium and the rows whose pack is only there
  as `medium`, the rows with no pack anywhere and the Debian desktop applications the catalogue does
  not name as `kdos-store`, boxes nothing else accounts for as `kdos-boxes`, and host software as
  `kdos-ports`, with each binhost port not installed offered built here as `kdos-ports-source`, and it opens a `.ktar` or `.kpack` handed to Discover from a file manager. Every
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
  AppStream catalogue of the host's ports, from kpkg's manifests, and, with `--debian-exclude`,
  `kdos-debian-exclude.xml`, the `remove-component` merges that take the applications
  `debian-exclude` names out of every AppStream pool.
- **`kdos-ports-helper`**, the root step behind installing host software prebuilt from the signed
  binhost, run through `pkexec` under the polkit action `io.kdos.ports.install`.
- **`kdos-ports-builder`**, the root step behind building host software on this computer, the
  `kdos-ports-source` origin, run through `pkexec` under `io.kdos.ports.build`.

It also points Discover's featured list at `/usr/share/kdos/discover/featured.json`, ships
`/usr/share/kdos/discover/debian-exclude`, the Debian packages the backend never offers to build in
a box, and defines the MIME types `application/vnd.kdos.ktar` and `application/vnd.kdos.kpack` with a hidden
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
- `-I"$LIBS/libkbase"`, `"$LIBS"/libkbase/*.c` — `kdos-ports-helper` and `kdos-ports-builder` are
  C, compiled with libkbase for its argument-vector runner, and link no Qt. `$LIBS` is `src/libs`.
- `-DKPH_BUILD` — Compiles `helper/kdos-ports-helper.c` a second time as `kdos-ports-builder`,
  which runs `kdos update install --source` where the helper runs `--prebuilt`. pkexec picks the
  polkit action by the program's path, so the two routes are two programs, each under its own
  action: `io.kdos.ports.build` asks for the password every time, because the build runs a
  recipe's `build.sh` as root, and a grant kept from a prebuilt install must not cover it.

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
- **A link waits for the machine to be read.** Discover asks for the featured list's
  `kdos-backend://` links and for KRunner's `appstream://` links as soon as the backend loads,
  before its first refresh lands; the backend answers each once that refresh has, and an
  `appstream://` link once the AppStream pool has loaded too, so neither is answered "no entry".
- **Every resource has an `appstreamId`.** Discover's lists drop every resource whose
  `appstreamId` an earlier one in the list has, so a resource with no AppStream component gets
  `io.kdos.<lane>.<id>`; its link stays `kdos-backend://<lane>/<id>`.
- **Debian applications need kdos-appbox's `deb:` form.** `install deb:<package>` and
  `uninstall deb:<package>` build and remove a box named `deb.<package>`, with each `+` in the
  package name spelled `_`; the backend reads that name back to tell a Debian application
  installed.
- **Host software is read-only here until a binhost is fetched.** A port arrives and leaves with
  the system version, so the `kdos-ports` lane lists what is installed and offers no removal. Once
  `kdos update fetch` has put the release's signed binhost index and its catalogue in
  `/var/lib/kdos/binhost`, a port that catalogue lists can be installed, prebuilt or built here.
  A port not installed is two resources with one AppStream id, `kdos-ports` and
  `kdos-ports-source`, so Discover lists one application with two origins in its source picker.
  Which of two resources with one id a Discover list keeps is its sort's, not the backend's, so a
  list carries only the one `install =` in `/etc/kdos/update.conf` prefers and a link answers both,
  that one first. Before `pkexec`, the plan runs the install's `--dry-run` as the user and asks
  through `Transaction::proceedRequest`, so the counts come before the password.
  `kdos-ports-helper` checks the name, clears the environment pkexec handed it and runs `kdos
  update install --prebuilt <port>`; `kdos-ports-builder` does the same with `--source`, passing
  `--build-deps=` only when given one, so `build-deps =` in `/etc/kdos/update.conf` decides
  otherwise. `kdos update install` owns the route: the binhost download and its checks, the
  build under `/etc/kdos/chroot.env`'s environment, the origin file and the world set. Then the
  helper writes the catalogue again. The helpers are separate programs because pkexec's grant
  names a program and not its arguments: granting `/usr/bin/kdos` would grant every subcommand.
- **Host software is read from its two catalogue files, not from the AppStream pool.** The pool
  keys every component of the OS catalogue by a data id with the origin `os`, so `org.kde.kate`
  from `kdos-ports.xml` and `org.kde.kate` from Debian's DEP-11 file would be one component,
  whichever loaded last. The backend parses `kdos-ports.xml` and each binhost's
  `kdos-binhost.xml` directly at every refresh, keeps the pool for the Debian lane's search and
  categories, and never offers in a box a Debian application whose component id a host port
  already has.
- **The exclusion list reaches KRunner only through the pool.** KRunner's software results search
  the AppStream pool themselves, so the backend's own filter does not reach them; the image step
  `086_swcatalog.sh` runs `kdos-discover-catalog --debian-exclude` to write the
  `remove-component` merges that take each listed application out of the pool. A merge removes
  every lower-priority component of its id, a host port's in `kdos-ports.xml` included, which
  costs nothing: the backend reads `kdos-ports.xml` itself, and KRunner offers no installed
  application. It is written at the image step and not in `build.sh`, because the DEP-11
  catalogue it reads is in the build tree but not in the development container that builds this
  port for `plasmatest`.
- **The catalogue is written wherever packages change.** `kdos-discover-catalog` rewrites the
  whole of `kdos-ports.xml` from kpkg's manifests: a port's own metainfo is its component, given
  the package name, and a desktop entry no metainfo launches is read by libappstream's
  desktop-entry reader. The image step runs it once over the finished tree.

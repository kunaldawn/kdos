## About

Angelfish is KDE's web browser for Plasma Mobile and the desktop: a Kirigami interface over
[qt6-qtwebengine](#port-qt6-qtwebengine), with tabs, bookmarks, history, a private mode, an ad
blocker and web apps. A page can be installed as a web app, which `angelfish-webapp` then opens
in a window of its own from a desktop entry Angelfish writes into the user's
`~/.local/share/applications`. History and bookmarks live in SQLite, reached through
[futuresql](#port-futuresql) and [qcoro](#port-qcoro).

The ad blocker is a Rust static library, `angelfish-adblock`, built on Brave's `adblock` crate and
bridged to C++ with `cxx`. CMake builds it through [corrosion](#port-corrosion), so the port
carries a [Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Corrosion=ON` — Make [corrosion](#port-corrosion) a
  requirement. Upstream marks it recommended, and without it builds the browser with no ad
  blocker and only a line in the configure summary to say so.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6QQC2DesktopStyle=ON` — Make
  [qqc2-desktop-style](#port-qqc2-desktop-style) a requirement. Upstream asks for it as an
  optional run-time component; `main.cpp` selects its `org.kde.desktop` style whenever
  `QT_QUICK_CONTROLS_STYLE` is unset, and without it the browser draws Qt's plain controls.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Qt6Feedback=ON` — No Qt Feedback, the unmaintained haptics
  module, which is not a port. Its QML is reached through a `Loader`, so leaving it out drops only
  vibration on touch; refusing the find keeps a stray copy from being picked up.
- `-C target-feature=-crt-static` — In `RUSTFLAGS`: build the crate for musl's shared C
  library. The musl target otherwise links its C library statically, and the static library
  Corrosion links into `angelfish` would carry that choice into a program that uses the shared
  one; see [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).
- `CARGO_NET_OFFLINE=true` — Keep cargo off the network. With the bundle's
  `.cargo/config.toml`, every crate resolves from `vendor/`; Corrosion runs cargo from
  `src/rs/adblock`, and cargo finds that configuration by walking up to the top of the tree, where
  `build.sh` unpacks the bundle.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The vendor bundle holds the 86 crates of the workspace `Cargo.lock` at the top of the tarball,
whose only member is `src/rs/adblock`; `ports/fetch` makes it with `cargo vendor`. The crates are
linked into the program, so `license` names theirs too: `adblock` is MPL-2.0, the rest MIT,
Apache-2.0, Unicode-3.0 or Zlib. Upstream's `Cargo.toml` builds the crate with `lto = true` and
`panic = 'abort'` in its release profile, and the recipe leaves both as upstream chose them.

The ad blocker starts without filter lists. On first use the browser asks before downloading
them (uBlock Origin's, EasyList, EasyPrivacy and AdGuard's, the defaults in
`angelfishsettings.kcfg`), and the question also offers to turn the ad blocker off.

The desktop entry claims `text/html` and the `http` and `https` schemes, as every browser's entry
does; which browser opens them is set in `mimeapps.list`, not by the entry. `main.cpp` sets the
desktop file name `org.kde.angelfish`, so the Wayland app_id is the entry's own name.
[qt6-qt5compat](#port-qt6-qt5compat) provides the `Qt5Compat.GraphicalEffects` QML module the
interface imports, and [purpose](#port-purpose) its share menu.

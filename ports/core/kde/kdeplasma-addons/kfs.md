## About

kdeplasma-addons is the second half of Plasma's widget set: the comic, dictionary, weather, timer,
notes, colour picker, quick launch, Konsole and Kate profile, web browser, user switcher and
fifteen-puzzle widgets among others, the unit converter, character, colour, date, dictionary and
spell-check runners for KRunner, the picture-of-the-day and other wallpapers, the cover, flip and
big-icons window switchers, the desktop cube effect, and the alternate and astronomical calendar
plugins. It also carries `kameleon`, the kded module that colours a keyboard's LEDs with the
accent colour.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_ICU=ON` — Make [icu](#port-icu) a requirement. Upstream builds the
  alternate calendar plugin's Chinese, Indian, Hebrew, Islamic and Persian calendars from ICU only
  when it is found, and leaves only the systems `QCalendar` knows otherwise.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineQuick=ON` — Make Qt WebEngine's Quick module a
  requirement. Upstream builds the dictionary and web browser widgets only when it is found, and
  drops both with a line in the feature summary otherwise. It comes from
  [qt6-qtwebengine](#port-qt6-qtwebengine).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Quick3D=ON` — Make [qt6-qtquick3d](#port-qt6-qtquick3d) a
  requirement. The desktop cube effect is QML that imports Qt Quick 3D at run time; upstream
  looks for it only to report it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KirigamiAddons=ON` — Make
  [kirigami-addons](#port-kirigami-addons) a requirement; the user switcher imports it at run time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6ItemModels=ON` — Make [kitemmodels](#port-kitemmodels) a
  requirement; the dictionary, comic, Kate sessions and Konsole profiles widgets import it at run
  time.
- `CARGO_HOME="$SRC_ROOT/.cargo"` — Keep cargo's own registry and cache in the work directory,
  so the build writes nothing to the builder's home and starts from the same empty state on every
  machine.
- `RUSTFLAGS="-C target-feature=-crt-static"` — Link the Rust helper against musl dynamically. The
  musl target links statically unless told otherwise; see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).
- `CARGO_NET_OFFLINE=true` — Keep cargo off the network, so a crate missing from the bundle fails
  the build by name instead of being fetched.

## Notes

`kameleon` drives two kinds of keyboard. Keyboards whose LEDs the kernel exposes under
`/sys/class/leds` are written through a KAuth helper, `kameleonhelper`. QMK keyboards that speak
the VIA protocol are written by `kameleon-qmk-helper`, a Rust program built through
[corrosion](#port-corrosion) from `kdeds/kameleon/qmk/kameleon-qmk-helper`, which runs as root on
the system bus (`org.kde.kameleon.qmk.helper`, started by D-Bus activation) and talks to the
keyboard's HID device. Corrosion's `find_package(Corrosion REQUIRED)` has no switch, so the Rust
toolchain is a build requirement of the whole port.

The helper's 100 crates come from a vendor bundle, `kdeplasma-addons-vendor-<version>.tar.xz`.
`vendordir = kdeds/kameleon/qmk/kameleon-qmk-helper` makes `ports/fetch` run `cargo vendor`
beside the helper's own `Cargo.toml` and `Cargo.lock`, and `build.sh` unpacks the bundle into the
same directory: Corrosion runs cargo with the manifest's directory as its working directory, and
cargo finds the bundle's `.cargo/config.toml` by walking up from there (see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).
The bundle's `sha256 =` line is its identity, so a version bump means a new bundle and
`ports/publish kdeplasma-addons`.

The `hidapi` crate the helper reaches through `qmk-via-api` is built with its default
`linux-static-hidraw` feature, so it compiles its own copy of hidapi's hidraw backend (hidapi
0.15.0, in crate 2.6.7) into the helper and links [eudev](#port-eudev)'s libudev; it does not use
the [hidapi](#port-hidapi) port, and its copy is updated only with the bundle. It is a bundled copy
in the sense of [Bundled copies](../../../../docs/kdos/03-architecture/packaging.md#bundled-copies).

The window switchers and the cube are KWin packages that import `org.kde.kwin`, and the clock and
user-switcher widgets import modules of [plasma-workspace](#port-plasma-workspace); both are in
`depends` for run time. The cover and flip switchers and the picture-of-the-day wallpaper's
chooser import `Qt5Compat.GraphicalEffects` from [qt6-qt5compat](#port-qt6-qt5compat), which the
configure does not look for; without it those three fail to load.

The configure looks for Purpose quietly, but nothing this package builds links or imports it, so
it is not in `depends`.

Four of the widgets reach the network, and only once somebody adds one: the weather widget asks
its forecast providers, the comic widget downloads strips, the picture-of-the-day wallpaper
downloads an image a day, and the dictionary widget looks words up on `dict.org`. The comic
widget's "Get New Comics" list comes from KNewStuff, which the system-wide `ghns=false`
restriction greys out (see
[Offline by construction](../../../../docs/kdos/01-philosophy/principles.md#offline-by-construction)).

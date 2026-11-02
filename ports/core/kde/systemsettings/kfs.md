## About

System Settings is the window that hosts KDE's settings modules (KCMs): it lists the modules every
installed package registers under `/usr/lib/qt6/plugins/plasma/kcms/systemsettings*`, groups them
into categories, searches them, and opens one by name (`systemsettings kcm_kscreen`). The modules
themselves come from the packages that own each subject: [plasma-workspace](#port-plasma-workspace),
[plasma-desktop](#port-plasma-desktop), [kwin](#port-kwin), [powerdevil](#port-powerdevil) and the
rest, and KDOS's own. It also installs a KRunner plugin that finds a settings page by name.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as an optional component and skips
  quietly.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

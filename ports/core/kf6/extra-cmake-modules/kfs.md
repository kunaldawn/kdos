## About

Extra CMake Modules ([ECM]{.idx}) is KDE's collection of CMake find modules, helper modules and
toolchain settings: `KDEInstallDirs`, `KDECompilerSettings`, `ECMGenerateHeaders`,
`ECMQtDeclareLoggingCategory` and the rest. It installs no program and no library, only CMake files under
`/usr/share/ECM` and their manual pages, and every KDE Frameworks library and KDE application, and a good number of other
Qt programs (fcitx5, flameshot, obs-studio among them), require it at configure time.

## Flags

- `BUILD_MAN_DOCS=ON` — Build the `ecm(7)` family of manual pages with Sphinx: `ecm(7)` itself and
  one page per module group (`ecm-modules`, `ecm-find-modules`, `ecm-kde-modules`, `ecm-toolchains`,
  `ecm-developer`). `python3-sphinx` is a dependency for this.
- `BUILD_HTML_DOCS=OFF` — With `BUILD_QTHELP_DOCS=OFF`, build neither the HTML reference nor the Qt
  Help collection. Both are on by default whenever Sphinx (and for Qt Help, Qt's `qhelpgenerator`)
  is found.

## Notes

All three documentation switches are CMake dependent options: when `sphinx-build` is not found they
are forced off and the configure succeeds anyway, so `BUILD_MAN_DOCS=ON` alone does not guarantee
the pages. `build.sh` checks for `/usr/share/man/man7/ecm.7` after the install and stops the build
if it is missing.

ECM's `KDEInstallDirs` picks its library directory by itself (`lib64` on x86-64), apart from CMake's
`GNUInstallDirs`, which is why KDE recipes pass `-DKDE_INSTALL_LIBDIR=lib`; see [A graphical
application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application).

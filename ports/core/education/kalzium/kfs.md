## About

Kalzium is KDE's periodic table of the elements: each element's properties, isotopes and
spectrum, a molar mass calculator, a chemical equation solver, and a 3D molecule viewer and
editor. The viewer is built on [openbabel](#port-openbabel) and
[avogadrolibs](#port-avogadrolibs); the equation solver is OCaml code using the
[ocaml-facile](#port-ocaml-facile) constraint library.

## Flags

- `-Wl,--strip-debug` — Drop debug sections at the link. The OCaml runtime that the equation
  solver links into `kalzium` is a static archive compiled with debug information, which no
  compile flag of this build reaches (see
  [What kpkg does around build.sh](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenBabel3=ON` — This switch and the other
  `CMAKE_REQUIRE_FIND_PACKAGE_` switches (`AvogadroLibs`, `Eigen3`, `OCaml`, `Libfacile`) make each
  of those `find_package` calls fail configure when its package is missing. All five are optional
  upstream: the molecule editor (`compoundviewer`) is built only when Open Babel, AvogadroLibs and
  [eigen](#port-eigen) are all found, and the equation solver only when OCaml and facile are, and
  either is otherwise left out with only a status line.

## Notes

The equation solver is compiled by `ocamlopt -output-obj`, which links the solver and the OCaml
runtime into the `kalzium` executable itself; `FindLibfacile` looks for facile in the directory
`ocamlc -where` reports.

Translations are not built (`KF_SKIP_PO_PROCESSING`), and the package prune removes the
translated handbooks; see
[Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy).

Both menu entries are replaced. The main one gives `StartupWMClass=org.kde.kalzium`, the Wayland
app_id `KAboutData` sets. The `chemical/x-cml` handler opens a file in the molecule editor, and
upstream's writes `-molecule %f`: `QCommandLineParser` reads that as `-m` with the value
`olecule`, and the file is never opened. The replacement writes `--molecule %f`.

## About
Qt Tools for Qt 6: Designer, Linguist with `lrelease` and `lupdate`, Assistant, `qdbus`,
`qtdiag`, `qtplugininfo`, `pixeltool` and the distance field generator, and the QtHelp,
QtUiTools and QtDesigner libraries. A Qt 6 port that ships translations runs this port's
`lrelease` at build time, and a program that loads `.ui` files at run time links QtUiTools.

## Flags
- `-DFEATURE_clang=OFF` — with `-DFEATURE_qdoc=OFF`: build nothing against libclang, and leave
  out QDoc, Qt's documentation generator, the one part of the module that uses it. The configure
  still looks for libclang; [clang](#port-clang) is installed in an earlier phase and is not in
  `depends`, so left to the configure, QDoc would be built whenever libclang is found.
- `-DFEATURE_assistant=ON` — with `designer`, `linguist`, `qdbus`, `qtdiag`, `pixeltool`,
  `qtplugininfo` and `distancefieldgenerator`: each tool named, so one that cannot be built
  stops the configure.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)). The
user-facing tools are linked into `/usr/bin` with a `6` suffix by the layout
`qt6-qtbase` sets, beside Qt 5's `-qt5` names.

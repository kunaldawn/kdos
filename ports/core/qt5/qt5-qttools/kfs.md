## About
Qt Tools for Qt 5: Designer, Linguist with `lrelease` and `lupdate`, Assistant, `qdbus`,
`qtpaths`, `qtdiag`, `qtplugininfo` and `pixeltool`, and the QtHelp, QtUiTools and QtDesigner
libraries. A Qt 5 port that ships translations runs this port's `lrelease` at build time.

## Flags
- `-no-feature-qdoc` — leave out QDoc, the generator of Qt's own documentation, which links
  libclang. [clang](#port-clang) is installed in an earlier phase and is not in `depends`, and the
  module builds QDoc whenever it finds libclang, so without the switch the package would link a
  library it does not declare.
- `-feature-assistant` — with `-feature-designer`, `-feature-linguist`, `-feature-qdbus`,
  `-feature-qtpaths`, `-feature-qtdiag`, `-feature-qtplugininfo` and `-feature-pixeltool`: each
  tool named. These features carry no condition in the module's configure, so each is on by
  default and naming it stops nothing: a tool whose Qt module is missing is skipped by
  `src/src.pro` without an error (Designer, Assistant and pixeltool need QtWidgets, `qdbus`
  needs QtDBus).

## Notes
The switches follow `--` on the `qmake` line: qmake hands everything after it to the module's own
configure step, which reads them as features.

After the install, `build.sh` links every tool in `/usr/lib/qt5/bin` into `/usr/bin` with a `-qt5`
suffix, such as `lrelease-qt5` and `designer-qt5`, so the Qt 5 tools sit beside Qt 6's.

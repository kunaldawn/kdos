## About

Kirigami App Components is a QML library of application-level pieces for Kirigami programs, the
first of them an action collection: actions declared in QML with their default shortcuts, a
shortcut editor that lets the user rebind them, and `ActionsExplorer`, a searchable list of
every action. It is the QML counterpart of the `KActionCollection`
that widget applications take from KXmlGui. The QML module is
`org.kde.kirigami.actioncollection`, over `libKirigamiActionCollection`, and C++ code finds it with
`find_package(KF6KirigamiAppComponents)`. KeepSecret and Koko are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_EXAMPLES=OFF` — Build no example application. Off is upstream's default too; the
  recipe names it.

## Notes

The configure requires the `org.kde.ki18n` QML module from [ki18n](#port-ki18n). The QML files
also import `org.kde.coreaddons`, `org.kde.kitemmodels` and `org.kde.kirigami` at run time, which
is why [kcoreaddons](#port-kcoreaddons), [kitemmodels](#port-kitemmodels) and
[kirigami](#port-kirigami) are in `depends`; a missing module fails only when a window loads the
component.

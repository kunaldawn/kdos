## About

KItemModels is the KDE Frameworks set of proxy models over Qt's item models: flattening a tree
(`KDescendantsProxyModel`), extra or rearranged columns, checkable rows, selections that follow
another view's (`KSelectionProxyModel`, `KLinkItemSelectionModel`) and a breadcrumb selection. Its
QML module, `org.kde.kitemmodels`, offers the same models and a sort-and-filter proxy to Qt Quick
programs. Plasma's task manager and System Settings, [kdeconnect](#port-kdeconnect),
[gwenview](#port-gwenview) and the PIM applications are built on it.

## Notes

The library needs nothing beyond Qt Core. The QML module is built only when the configure finds Qt
Qml, which upstream looks for as optional and without a message when it is missing;
[qt6-qtdeclarative](#port-qt6-qtdeclarative) is in `depends`, so the module is always built. The
source carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.

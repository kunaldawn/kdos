## About

Kirigami is KDE's set of Qt Quick components for applications that run on a desktop and on a
phone: pages, drawers, cards, the page row and the toolbar. It is a QML module, `org.kde.kirigami`,
which a Kirigami application draws its whole window with;
[knewstuff](#port-knewstuff)'s Qt Quick dialogs need it at run time.

## Notes

The components' shadows and rounded corners are drawn by shaders, which the build compiles with
`qsb` from [qt6-qtshadertools](#port-qt6-qtshadertools); without it the configure stops.

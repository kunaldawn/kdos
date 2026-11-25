## About

KCMUtils is how a KDE settings page is written and loaded. A configuration module (KCM) is a
plugin, a Qt Widgets one built on `KCModule` or a Qt Quick one built on `KQuickConfigModule`, and
this framework finds it by its plugin metadata, loads it and shows it, in System Settings, in a
program's own settings dialog (`KPluginWidget`, `KCMultiDialog`), or alone with `kcmshell6`.
`kcmdesktopfilegenerator` writes the desktop entry that lists a module in the application menu.
Plasma's System Settings, [dolphin](#port-dolphin)'s settings and the KCMs of every Plasma module
are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Patches

`kcmoduleqml-backtab.patch` makes Shift+Tab from a widget after a Qt Quick settings page, the
Help or Apply button of System Settings, land on the page's last control with its name spoken.
Qt 6.12's `QQuickWidget` moves a backtab to the scene's last item inside its own `focusInEvent`,
before the widget focus is announced, so the screen reader's last focus event is the unnamed
`QQuickWidget` ("layered pane" in Orca) and the control it reached is never read. Upstream
KCMUtils has a step for a backtab entering the page, but it runs on a backtab focus arriving at
the page's root item, and Qt 6.12 sends none: the last control already holds the focus. The patch
records a backtab that enters the `QQuickWidget` from the widgets and, once the widget holds the
focus, moves the focus through the root to the item before the root in the focus chain, which Qt
keeps to visible, enabled tab stops; it never stops on the root. A page with no control is passed
through, and only a backtab arriving from a control of the page leaves it. It is to be offered
upstream as a merge request against `frameworks/kcmutils`.

The patch covers the page; System Settings' sidebar, another `QQuickWidget`, is announced by
[qt6-qtdeclarative](#port-qt6-qtdeclarative)'s `quickwidget-focus-accessible.patch`.

`kcmshell-page-stack-accessible.patch` reports KPageView's page stack in `kcmshell6`, the
`KPageStackedWidget` of the `KCMultiDialog` that holds the module, as a plain container instead of
a layered pane, so a screen reader passes over it; Orca otherwise speaks it as "layered pane 0
items" after the window title when `kcmshell6` opens and before the first control on every Tab or
Shift+Tab into the page. An accessibility factory installed in `kcmshell6`'s `main()` before the
dialog is built matches `KPageStackedWidget` alone, so every other stacked widget keeps its role.
It covers `kcmshell6` alone: a program's own `KCMultiDialog` keeps the stack's default role, and
System Settings carries the same factory in [systemsettings](#port-systemsettings)'
`page-stack-accessible.patch`. It is to be offered upstream as a merge request against
`frameworks/kcmutils`.

## Notes

The configure looks up the QML module `org.kde.kirigami` with `ecm_find_qmlmodule(… REQUIRED)`,
so [kirigami](#port-kirigami) must be installed before this port builds, not only at run time; the
Qt Quick modules draw their pages with it.

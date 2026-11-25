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

## Patches

`page-stack-accessible.patch` reports System Settings' two unnamed page stacks as plain
containers instead of layered panes, so a screen reader passes over them: the window's page stack,
the `QStackedWidget` SettingsBase sets as its central widget, which Orca otherwise speaks as
"layered pane 0 items" after the window title whenever a window opens, and KPageView's page stack,
spoken the same way before the first control on every Tab or Shift+Tab into a settings page. Its
accessibility factory matches a `QStackedWidget` only when SettingsBase is its parent, so every
other stacked widget in a module keeps its role. It is to be offered upstream as a merge request
against plasma/systemsettings. The KPageView stack's role could be set in
[kwidgetsaddons](#port-kwidgetsaddons)' KPageView instead, for every KPageView in every
application; that change is not taken here, and `kcmshell6` carries the same factory for its
own dialog in [kcmutils](#port-kcmutils)' `kcmshell-page-stack-accessible.patch`.

`sidebar-list-focus.patch` names the sidebar's two lists for a screen reader, the category list
"Settings categories" ("Search results" while a search is typed) and a category's page list by the
category's title ("Colors & Themes"), and names each section heading of the category list by its
full text, '&' included ("Security & Privacy"), where the heading's button type would strip the
'&' as a mnemonic marker. Entering a list gives its current item the focus, so the list's name and
then that item are read; the item takes the focus with no Tab reason, which keeps the style's own
focus ring off. The patch draws a frame in the theme's focus colour around the
current item while its list holds the active focus that the keyboard brought there. A Tab entering
the window gives the active focus to the list, and the list, not the item, records how the focus
came: every focus entry into a list turns the frame on, by Tab,
Backtab, an arrow key or the Right or Enter that opens a category's pages, unless a pointer press
on a sidebar item caused it. A press marks the sidebar in `SideBarItem.qml` until the event loop
has run the click it ends in and turns off the frame of the list it lands in; the list losing the
active focus turns it off too. A mouse click on an item therefore draws no frame and Space keeps
it; a click on the empty space below the items that brings the focus back from a page shows it,
since no item sees that press.
Tab from a page's Help button returns to the sidebar, on a category's page list to the list, and
Tab from a list returns to the page, as upstream orders it. Shift+Tab from a page reaches the Show menu button, then Search on the first
column (Back on the second), then leaves the sidebar for the page's Help button: the search field
hands its Backtab to `systemsettings.focusPrevious()` as the Back button does, where upstream's
binding to a `KeyNavigation.left` the field does not have lets it wrap inside the sidebar to the
list. The announcement
itself on entering the list comes from [qt6-qtdeclarative](#port-qt6-qtdeclarative)'s
`quickwidget-focus-accessible.patch`. It is to be offered upstream as a merge request against
plasma/systemsettings.

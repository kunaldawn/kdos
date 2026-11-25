## About

System Versions is the System Settings page for KDOS's two root slots, under System. It lists slot A
and slot B with the version, build date, tree commit and kernel each holds and the state each is
in (running, confirmed, on trial, failed), and its one button, "Start This Version Next Time",
arms a one-boot trial of the other slot: the previous version after an update, or the new one
again after a rollback. Its settings are where `kdos update` takes a newer tree from (the KDOS
releases, a folder or removable medium, or another address) and, under "Installing software",
the route `kdos update install` takes with no flag and the route of what a build lacks: the
`install =` and `build-deps =` lines of `/etc/kdos/update.conf`, each prebuilt or built on this
computer, both prebuilt by default. Beside the settings it shows what the
update service reports — an update that is available, one prepared in the other slot that Discover
has not yet installed, and why the last one stopped — and opens Discover to act on it.

The page holds no privilege and touches no file of the boot state. Every change is a call to
`io.kdos.Slots1` on the system bus, which [kdos-sysupdated](#port-kdos-sysupdated) serves and
authorises through polkit (`io.kdos.slots.try`, `io.kdos.slots.channel`,
`io.kdos.slots.install-route`) before it runs
`kdos-bootctl try <slot> 1`; a refusal from kdos-bootctl is shown in its own words. The interface
is `io.kdos.Slots1.xml` beside the recipe, and it is the contract both sides are held to: the
page's test fixture generates its adaptor from it, and kdos-sysupdated serves the same members and
keys.

## Flags

- `-D BUILD_TESTING=OFF` — Leave the QtTest suites out of the package. They start a fixture
  `io.kdos.Slots1` server on a private session bus and load the page offscreen, which
  `testing/plasmatest.sh` does in the development container; the package build has no session bus.

## Notes

The QML and the C++ share the translation domain `kcm_kdos_system`, the plugin id, because
`KQuickConfigModule::mainUi()` sets the QML engine's domain to the plugin id; a second name would
split the catalogue in two.

`X-DocPath` in `kcm_kdos_system.json` is `kdos/04-programs/kdos-settings.html#system-versions`,
the page's section of the KDOS book under `help:/`, which `testing/anchorcheck.py` checks against
the book's headings. System Settings' Help button opens `help:/` followed by the key's text, and
KIO's `help:` worker finds the page through `/usr/share/doc/HTML/en/kdos`, the link
[kdos-doc](#port-kdos-doc) installs (see
[Help](../../../docs/kdos/04-programs/kdos-settings.md#help)).

System Settings shows a KCM in a scene of its own beside its sidebar and footer, so a dialog drawn
in the page's scene blocks only the page, and a click on the sidebar goes through and discards it.
The Start This Version Next Time prompt is `popupType: QQC2.Popup.Window` with `modal: true`: a
window of its own, application-modal, so the rest of System Settings waits until it is answered.
Qt makes the prompt's window transient for the window of the item it is declared in, and for a KCM
in System Settings that is QQuickWidget's offscreen window, which is never mapped and has no
`xdg_toplevel`; the prompt would map with no parent, and a click on System Settings would raise it
over its own modal prompt. `SystemVersionsModule::holdOver()` sets the offscreen window's transient
parent to System Settings' top level before the prompt's first open, and Qt's Wayland plugin
resolves a parent by walking transient parents to the nearest window with a shell surface, so the
prompt is mapped as a child of System Settings. The prompt's own transient parent stays the
offscreen `QQuickWindow`: `QQuickPopupPrivate::adjustPopupItemParentAndWindow()` casts it to a
`QQuickWindow` on every open and close, and hiding and reshowing a mapped prompt to send a new
parent rejects the dialog, because `QQuickPopupWindow::hideEvent` closes a visible dialog.
KWin offers maximise to any `xdg_toplevel` whose minimum size is below its maximum, so
`SystemVersionsModule::holdSize()` sets both to the prompt's size and lifts them as it closes. KWin
offers or withdraws maximise every time the bounds change (`handleMinimumSizeChanged()` and
`handleMaximumSizeChanged()` each recompute the capabilities), and a mapped window that loses its
maximise button keeps a dead close glyph in its title bar, so the bounds that withdraw it have to be
on the `QWindow` before it is mapped: Qt's Wayland plugin sends them with the shell surface's first
commit, from `QWaylandWindow::initWindow()` in `setVisible(true)`.
`QQuickPopupPrivate::adjustPopupItemParentAndWindow()` creates the `QQuickPopupWindow` and shows it
in one call, before `aboutToShow` is emitted, so no popup signal comes between the two; the one
hook is the background's `Window.window`, which changes as the popup item is moved into the
window's content item, just before `setVisible`, on every open (`finalizeExitTransition()` takes
the item out again on close). The size then is not the settled one, and the popup's implicit size
is no better, since the content is laid out only in the new window: in the offscreen test the
prompt is 432x16 there on the first open and 432x76 on a reopen, and settles at 432x117. So the
prompt is held again in `onOpened`. That re-hold grows the window, which keeps it unmaximisable:
Qt sends both bounds in one commit, and KWin's `XdgToplevelInterfacePrivate::apply()` applies the
minimum before the maximum, so a new minimum above the old maximum leaves no moment with room to
resize. A re-hold that shrinks a dimension has that moment, and reaches KWin as maximise offered
and withdrawn on a mapped window; the prompt's width is fixed at 24 grid units and its height is
mapped at its frame alone or less than its content needs, so the re-hold does not shrink it. The bounds are hints: neither Qt's
Wayland plugin nor KWin's first configure clamps a size to them. `QWindow` does, though: every
change of a bound fits the window into the bounds unless the minimum exceeds the maximum in both
dimensions. A hold moves the maximum first, which leaves the window at the size it is held at;
the release raises the minimum above every maximum before it lifts both, so neither resizes the
window, and the plugin sends no bounds whose minimum exceeds the maximum.
As its own window the prompt's modal dim is transparent: the page's scene is all a popup can dim,
and a dimmed page beside a bright sidebar reads as a sidebar still in use. KWin's Dialog Parent
effect dims the whole window where effects run; under software rendering KWin reports it
unsupported (`isEffectSupported("dialogparent")` is false on llvmpipe) and nothing is dimmed.
Minimise stays: KWin offers it to every transient, and minimising a modal dialog minimises its main
window too, so the prompt cannot be hidden on its own.

Applying a different update location or install route raises an administrator prompt, which needs the polkit
agent of the Plasma session; a page opened outside a session gets a refusal and says so. A
location is written whole into one `tree =` line of `/etc/kdos/update.conf`, so a path or an
address containing a space or a control character is refused before it is sent.

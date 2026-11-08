## About

xdg-desktop-portal-kde is the Plasma backend behind [xdg-desktop-portal](#port-xdg-desktop-portal):
the process that answers a boxed or sandboxed application's portal requests with KDE's own
dialogs and services. It serves the file chooser, the application chooser, screen casting and
remote desktop through KWin and [kpipewire](#port-kpipewire), screenshots through KWin's
`ScreenShot2`, print, e-mail, inhibit, background, global shortcuts, input capture, the clipboard,
wallpaper, USB access and the Settings portal, which hands an application the colour scheme,
accent and fonts of the Plasma session.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The install puts a systemd user unit, `plasma-xdg-desktop-portal-kde.service`, under
`/usr/lib/systemd`; `build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)). The backend is
started by D-Bus activation and needs no unit: `org.freedesktop.impl.portal.desktop.kde.service`
carries `Exec=/usr/lib/libexec/xdg-desktop-portal-kde` beside its `SystemdService=` line, and a
bus daemon without systemd activation runs the `Exec=` command the first time the front end calls
the name. Removing that line, or moving the binary without the service file, leaves every portal
request with no backend.

The front end picks this backend from `kde-portals.conf`, which
[plasma-workspace](#port-plasma-workspace) installs, when `XDG_CURRENT_DESKTOP` is `KDE`;
`kde.portal` lists the interfaces it serves and `UseIn=KDE`.

The dialogs are QML that import modules of other ports, which the build does not check for and
which are in `depends` for run time: every dialog's frame uses
[qt6-qt5compat](#port-qt6-qt5compat)'s `Qt5Compat.GraphicalEffects`; the screen-cast chooser lists
windows through `org.kde.taskmanager` from plasma-workspace; the global-shortcuts dialog takes its
key sequence editor from [kdeclarative](#port-kdeclarative); and the dialogs use
[kirigami-addons](#port-kirigami-addons), [kcmutils](#port-kcmutils) and
[kitemmodels](#port-kitemmodels). A dialog whose import is missing does not open, and the request
fails.

KWin hands its privileged interfaces only to a program whose desktop entry asks for them: the
hidden `org.freedesktop.impl.portal.desktop.kde.desktop` names `ScreenShot2` in
`X-KDE-DBUS-Restricted-Interfaces` and the fake-input, window-management and screencast
protocols in `X-KDE-Wayland-Interfaces`, and its `Exec=` is the binary's path. A backend run from
any other path gets no screenshots, no screen cast and no remote input.

`FindKIOFuse.cmake` asks the build machine's session bus whether `org.kde.KIOFuse` can be
activated and reports `Could NOT find KIOFuse` when it cannot, which is always the case in a
build root; the result changes nothing in the build. The file chooser asks the same question at
run time. When [kio-fuse](#port-kio-fuse) answers, the dialog offers every KIO location (`sftp:`,
`smb:` and the rest) and hands the application a local path under the FUSE mount; when it does
not, the dialog offers local files only. kio-fuse is in `depends` for that run-time use.

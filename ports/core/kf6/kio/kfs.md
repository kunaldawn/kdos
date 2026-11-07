## About

KIO is KDE's network-transparent file access: one job API for every kind of location, each URL
scheme handled by a worker process. KIO itself carries the workers for local files, FTP, HTTP and
WebDAV, the trash and `help:`; [kio-extras](#port-kio-extras) adds SFTP, SMB, archives and the
rest. It also provides the file open and save dialogs, the file properties dialog and the
"Open With" machinery of KDE applications, the `kiod6` daemon and the `ktrash6` command.
[dolphin](#port-dolphin), [kate](#port-kate), [okular](#port-okular) and many KDE applications in
the tree link it.

## Flags

- `-D WITH_X11=ON`, `-D WITH_WAYLAND=ON` — Build the X11 and Wayland integration. When KIO starts
  an application, or hands files to one over D-Bus from "Open With", it passes on the X11 startup
  identifier or the Wayland `xdg-activation` token, so the application's window may take focus.
  Both are upstream's defaults on Linux; every toolkit in the tree keeps both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the file widgets, which upstream
  builds by default.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch does not reach
  `kdoctools_install()`, which still builds the translated handbooks of the workers; the same rule
  removes them from the package.

## Patches

`kiod6-desktop-exec-path.patch` sets the `Exec=` line of `org.kde.kiod6.desktop` to
`/usr/lib/libexec/kf6/kiod6`. The daemon is installed there, on no `PATH` directory, so a bare
`Exec=kiod6` names a program nothing can start, and the image's
[entrycheck](../../../../testing/entrycheck.py) gate refuses a desktop entry whose program does not
resolve. The patch is KDOS's own; no upstream merge request holds it.

## Notes

Two of KIO's dependencies are optional upstream and used when the configure finds them:
[kdoctools](#port-kdoctools), for the `help:` worker and the handbooks, and [acl](#port-acl), for
access control lists in the copy jobs and the properties dialog. Both are in `depends`, so both
are built in. [kwallet](#port-kwallet), also optional and in `depends`, is where `kpasswdserver`,
the `kiod6` module that answers workers' password requests, keeps the passwords a user asks it to
remember. [switcheroo-control](#port-switcheroo-control) is a run-time dependency: KIO asks it
over D-Bus whether the machine has a second GPU, so that an application whose desktop entry asks
for the discrete one can be started on it.

[kded](#port-kded) is a run-time dependency too, as upstream's configure reports: two of KIO's
modules, `trashdirnotify` and `remotedirnotify`, run inside `kded6` and tell open views when the
trash or `remote:/` changes. `trashdirnotify` watches the trash directories themselves, on the home
filesystem and on every mounted device, so without the daemon a Dolphin window on the trash does
not update when a program outside KIO, a GTK one for instance, moves a file there.

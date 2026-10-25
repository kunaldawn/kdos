## About

KIO is KDE's network-transparent file access: one job API for every kind of location, each URL
scheme handled by a worker process. KIO itself carries the workers for local files, FTP, HTTP and
WebDAV, the trash and `help:`; [kio-extras](#port-kio-extras) adds SFTP, SMB, archives and the
rest. It also provides the file open and save dialogs, the file properties dialog and the
"Open With" machinery of KDE applications, the `kiod6` daemon and the `ktrash6` command. [dolphin](#port-dolphin), [kate](#port-kate), [okular](#port-okular) and many
KDE applications in the tree link it.

## Flags

- `-D WITH_X11=ON`, `-D WITH_WAYLAND=ON` — Build the X11 and Wayland integration. When KIO starts
  an application, or hands files to one over D-Bus from "Open With", it passes on the X11 startup
  identifier or the Wayland `xdg-activation` token, so the application's window may take focus.
  Both are upstream's defaults on Linux; every toolkit in the tree keeps both backends (see
  [Toolkits, Wayland and X11](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the file widgets, which upstream
  builds by default.

## Notes

Two of KIO's dependencies are optional upstream and used when the configure finds them:
[kdoctools](#port-kdoctools), for the `help:` worker and the handbooks, and [acl](#port-acl), for
access control lists in the copy jobs and the properties dialog. Both are in `depends`, so both
are built in. [switcheroo-control](#port-switcheroo-control) is a run-time dependency: KIO asks it
over D-Bus whether the machine has a second GPU, so that an application whose desktop entry asks
for the discrete one can be started on it.

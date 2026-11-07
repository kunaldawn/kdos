## About

kio-fuse mounts KIO locations as an ordinary filesystem, so a program that knows nothing of KIO can
open a file on an `sftp:`, `smb:`, `webdav:` or other KIO URL by path. When a KDE application hands
a remote file to such a program, KIO asks kio-fuse over D-Bus (`org.kde.KIOFuse`) for a local path,
and kio-fuse mounts the location under `$XDG_RUNTIME_DIR/kio-fuse-<random>/` and serves reads and
writes through the matching KIO worker. xdg-desktop-portal-kde and plasma-workspace rely on it to
pass remote files to applications that are not KDE's. The daemon is
`/usr/lib/libexec/kio-fuse`, mounting through [fuse](#port-fuse)'s setuid `fusermount3`.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KDE Frameworks 6. The project asks ECM for a
  version older than Frameworks 6, so ECM's `QtVersionOption` would otherwise pick Qt 5 and
  KF5, which KDOS does not have.

## Notes

The daemon is started by D-Bus activation the first time KIO asks for a mount. The service file
upstream generates names both an `Exec=` line and a `SystemdService=` line; the session bus,
which runs without systemd activation, uses the `Exec=` line. The install also puts a systemd user
unit under `/usr/lib/systemd/user` and a `tmpfiles.d` rule that keeps systemd's cleaner away from
the mount points under `/run/user`; KDOS has neither systemd nor that cleaner (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)), and the package lint
refuses `usr/lib/systemd`, so `build.sh` removes both directories.

The kernel's FUSE driver is built into the KDOS kernel, so no module needs loading before the
first mount.

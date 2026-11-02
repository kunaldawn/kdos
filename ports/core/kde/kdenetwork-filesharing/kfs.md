## About

kdenetwork-filesharing adds two pages to the file properties dialog of Dolphin and every other KIO
program. Share shares a folder over SMB as a Samba usershare, chooses which users may read or
write it, sets the user's Samba password and adds the user to the usershare group. Access Control
edits the Windows ACL of a file on an SMB mount.

## Flags

- `-D USE_SYSTEMD=OFF` — Build no systemd client. Upstream's default is on, and the plugin then
  offers to enable and start `smb.service` through `org.freedesktop.systemd1`, which no KDOS machine
  serves (see [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
- `-D SAMBA_INSTALL=OFF` — With `-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON`: no offer to
  install Samba through PackageKit when it is missing. PackageKit is not a port, and
  [samba](#port-samba) is in `depends`, so it is never missing.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_net=ON` — With `-D CMAKE_REQUIRE_FIND_PACKAGE_testparm=ON`:
  make Samba's `net` and `testparm` commands requirements of the configure. Upstream finds them as
  run-time packages only, but the plugin reads the usershare settings through `testparm` and
  creates every share with `net usershare`, so it does nothing without them.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

[qcoro](#port-qcoro) is a requirement of the configure, with its Core, DBus and Quick parts.

A share exists only where Samba is configured for usershares. The plugin reads `usershare path`
from `smb.conf` through `testparm`, and adding a share fails until that directory exists, is owned
by the usershare group with the sticky bit set, and the user belongs to the group. The package
installs none of that configuration; the image does. It ships `/etc/samba/smb.conf` with the
usershare settings and the `sambashare` group, and [samba](#port-samba)'s service script makes
`/var/lib/samba/usershares` at every boot. `net usershare add` writes a share there whether or not
`smbd` runs, and `smbd` is not running until something is shared: [kded-kdos](#port-kded-kdos)
sees the first share appear and has [kdos-powerd](#port-kdos-powerd) start Samba, which then
serves it. The Share tab adds its user to `sambashare` through the KAuth helper below. Other machines reach a share only through TCP port 445, which the
shipped firewall keeps closed until its `smb` service is opened (see
[The firewall](../../../../docs/kdos/02-user-guide/administration.md#the-firewall)).

Setting the Samba password and joining the group go through a KAuth helper that runs as root:
`smbpasswd` from [samba](#port-samba) and `/usr/sbin/usermod` from [shadow](#port-shadow). Both
actions are `auth_admin`. The page announces new shares over Avahi's D-Bus interface, so
[avahi](#port-avahi) is in `depends` for run time.

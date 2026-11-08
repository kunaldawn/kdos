## About

colord is the system service that tracks colour-managed devices (displays, printers, scanners,
cameras) and the ICC profiles assigned to each, on the system bus as `org.freedesktop.ColorManager`.
The package carries the daemon, `libcolord` and `libcolordprivate`, the `colormgr` and
`cd-create-profile` tools, the colorimeter sensor drivers, the standard display and colour-space
profiles, the `colord-session` helper the session bus starts for calibration, and the udev rules
that tag colour devices and sensors. Its desktop consumer is [colord-kde](#port-colord-kde), the
KCM and the daemon module that register outputs and apply profiles. The system bus of
[dbus](#port-dbus) starts it, so `dbus` is in `depends` for run time.

colord runs as the `colord` account. Nothing starts it at boot: the system bus activates it on the
first call, as the account the activation file names. Whether a caller may create, modify or
delete a device or profile is polkit's answer from the actions in
`/usr/share/polkit-1/actions/org.freedesktop.color.policy`, most of which are `allow_active`, so
they are granted to the active local session elogind reports and refused to everything else; see
[polkit](../../../../docs/kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent).

## Flags

- `-Dsystemd=false` — No systemd integration: no unit, no tmpfiles.d or sysusers.d file, and no
  `sd_pid_get_session()` lookup. Without it a device a client creates carries no seat.
- `-Ddaemon=true` — The daemon itself; upstream's default, stated.
- `-Ddaemon_user=colord` — The account the daemon runs as, compiled into the D-Bus activation file,
  the D-Bus policy and the sensor udev rule's `GROUP=`. Upstream's default is `root`, which it
  warns against. `postinstall.sh` creates the account and gives it `/var/lib/colord`.
- `-Dudev_rules=true` — Install `69-cd-sensors.rules` and `95-cd-devices.rules` into eudev's
  rules directory; upstream's default, stated.
- `-Dbash_completion=true` — `colormgr`'s completion, into the directory
  [bash-completion](#port-bash-completion) names; upstream's default, stated.
- `-Dintrospection=true` — The `Colord-1.0` and `Colorhug-1.0` typelibs; upstream's default,
  stated.
- `-Dvapi=true` — The Vala API, through [vala](#port-vala).
- `-Dsane=false` — No SANE plugin and no `colord-sane` helper, which register scanners as colour
  devices; upstream's default, stated. Building them requires
  [sane-backends](#port-sane-backends), which builds in `43_toolkits`, so colord would build
  there instead of in `41_system`.
- `-Dargyllcms_sensor=false` — No ArgyllCMS sensor driver, which runs ArgyllCMS's `spotread`.
  ArgyllCMS is not a port.
- `-Dprint_profiles=false` — No generated print profiles; building them runs ArgyllCMS's `colprof`.
- `-Dlibcolordcompat=false` — No `libcolordcompat.so`, a shim only ArgyllCMS loads; upstream's
  default, stated.
- `-Dsession_example=false` — No GNOME session example, which needs gnome-desktop and
  colord-gtk; upstream's default, stated.
- `-Dman=true` — The manual pages for `colormgr`, `cd-create-profile`, `cd-fix-profile` and
  `cd-it8`, rendered by `xsltproc` from [libxslt](#port-libxslt) with the namespaced DocBook
  stylesheets of [docbook-xsl](#port-docbook-xsl). The configure fails when the stylesheet is
  missing.
- `-Ddocs=false` — No gtk-doc API reference.
- `-Dtests=false` — No self tests, which need a running bus and udev.
- `-Dinstalled_tests=false` — No installed copy of the tests either; upstream's default, stated.

## Notes

The daemon keeps its databases, `mapping.db` and `storage.db`, and the profiles installed
system-wide, under `/var/lib/colord`. `kpkg` rolls the
package `root:root`, so `postinstall.sh` creates the `colord` group and account (`groupadd -r`,
`useradd -r`, home `/var/lib/colord`, shell `/sbin/nologin`) when the target root has none, and
gives the directory to the account by the ids in the target's `/etc/passwd`. Without that the
daemon cannot write its databases and every device a client registers is lost.

The meson install script `meson_post_install.sh` creates `/var/lib/colord/icc` and, when the build
runs as root, `chown`s the directory to the daemon account by name. Its exit status is that of the
`chmod` after it, so in a build root with no `colord` account the failed `chown` does not fail the
install; the ownership the package needs comes from `postinstall.sh`.

The activation file `org.freedesktop.ColorManager.service` names `SystemdService=colord.service`
as well as `Exec=` and `User=`; the system bus starts the daemon through `Exec=` because there is
no systemd to hand it to.

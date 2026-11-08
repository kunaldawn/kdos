## About

Plasma Setup is the first-boot wizard for a machine handed over without an account (KDOS's
"Let the first person set it up" install). On that boot the login manager signs in a special
`plasma-setup` user with no password, whose session runs only the wizard: it creates the first
real account in the groups KDOS gives its own account, and sets the language, keyboard layout, time zone, host name and
Wi-Fi network, then writes `/etc/plasma-setup-done` so it never runs again and logs the new user
in.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

- `-D KDE_INSTALL_CONFDIR=/usr/share/kdos/xdg` — Install `plasmasetuprc` into
  `/usr/share/kdos/xdg`, where every KDOS default lives, instead of `/etc/xdg`, which is the
  machine owner's and which no package ships into (see
  [Session](../../../../docs/kdos/03-architecture/session.md)). Plasma Setup reads the file by the
  absolute path this directory compiles into `PLASMA_SETUP_CONFIG_PATH`, not through
  `XDG_CONFIG_DIRS`, so the setting moves the file and the path together. Nothing else of this port
  installs into the configuration directory.

## Patches

`plasmasetuprc-kdos-groups.patch` sets `[Accounts] UserGroups` to the groups the image gives its own
account in `fs/etc/group` (`wheel,users,audio,video,render,input,kvm,cdrom,dialout,lpadmin,tty`)
instead of `wheel` alone. An OEM install's first account is made by Plasma Setup's helper with
`useradd -m -U`, which gives it `/etc/default/useradd`'s `GROUPS=audio`, and then `usermod -a -G`
with this list; an account made by the installer keeps the live account's memberships, so the two
would otherwise differ: without `kvm` a box gets no hardware virtualisation, without `lpadmin`
the printer pages ask for an administrator. Every group named must exist in `/etc/group`, or
`useradd` refuses the account; the list follows `fs/etc/group`'s `kdos` lines. It is not proposed
upstream: the file is upstream's documented place for a distribution's choice.

## Notes

Upstream arms the wizard with three systemd pieces, and `build.sh` deletes all three:
`plasma-setup.service` under `/usr/lib/systemd/system`, the `plasma-setup` user in
`/usr/lib/sysusers.d`, and the user's home in `/usr/lib/tmpfiles.d`. KDOS has no systemd and kpkg's
package lint refuses `usr/lib/systemd` (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
Their work is done by a boot script instead, before the login manager starts and only while
`/etc/plasma-setup-done` is absent and the system is not the live medium: it provides the
`plasma-setup` account, builds its home in `/run/plasma-setup` (the autostart entry for the wizard,
the global-shortcut allow-list from `/usr/share/plasma-setup/kglobalaccelrc`, and the settings that
turn off the screen locker, the wallet, the welcome tour and the default panel), and runs
`plasma-setup-bootutil`, which writes the autologin drop-in
`/etc/sddm.conf.d/99-plasma-setup.conf`. The script is `/etc/init.d/62_plasma-setup.sh`; once
`/etc/plasma-setup-done` exists it also deletes a leftover `99-plasma-setup.conf`, so a wizard that
finished without removing its autologin cannot log the `plasma-setup` account in again.

`plasma-setup-bootutil` asks systemd over the system bus whether Plasma Login is enabled; with no
systemd the call fails and it writes SDDM's drop-in, which is the login manager KDOS runs
([sddm](#port-sddm)).

The wizard's privileged steps go through its KAuth helper, `plasma-setup-auth-helper`, which runs
`useradd`, `chpasswd` and `usermod` from [shadow](#port-shadow), found in `/usr/sbin`. Its pages
change the system through the `hostname1`, `locale1` and `timedate1` interfaces KDOS's
`kdos-settingsd` serves, and through NetworkManager. The polkit rule upstream installs,
`/usr/share/polkit-1/rules.d/plasma-setup-polkit.rules`, lets the `plasma-setup` user take exactly
those actions without a password, and nobody else. Accounts it creates join `audio` through `useradd`'s default and the groups in
`/usr/share/kdos/xdg/plasmasetuprc` through `usermod -a`, and it reads the first ordinary UID from
`/etc/login.defs`.

The keyboard page needs `org.kde.plasma.private.kcm_keyboard` from
[plasma-desktop](#port-plasma-desktop) at build time (`ecm_find_qmlmodule(... REQUIRED)`), and the
Wi-Fi page loads [plasma-nm](#port-plasma-nm)'s QML at run time; the wizard's session shows
[plasma-keyboard](#port-plasma-keyboard) on a touch screen.

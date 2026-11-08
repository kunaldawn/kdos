## About

AccountsService is the system service that owns `org.freedesktop.Accounts` on the system bus. It
lists the machine's human accounts and lets a program read and change their properties: real
name, avatar, language, account type and password. Its root daemon, `accounts-daemon`, keeps what
`/etc/passwd` cannot hold in `/var/lib/AccountsService/users/<user>` and each avatar in
`/var/lib/AccountsService/icons/<user>`; `libaccountsservice` is the client library.

On KDOS the reason for it is the desktop, not a link: System Settings' Users page and the portal's
account dialog talk to it over D-Bus, and SDDM shows the avatar it stores at the login screen. It
asks [polkit](#port-polkit) before every change: a user may change their own account from the
active session, and changing another account needs an administrator. The daemon is
`/usr/lib/accounts-daemon`, started at boot by `/etc/init.d/44_accounts.sh` under `ksvc`.

## Flags

- `--libexecdir=/usr/lib` — Install the daemon as `/usr/lib/accounts-daemon`, the path the init
  script starts, instead of under `/usr/libexec`.
- `-Delogind=true` — Link `libaccountsservice` against [elogind](#port-elogind)'s `libelogind`
  instead of `libsystemd`. The client library reads login sessions through `sd-login` to tell
  which listed users are logged in; the daemon does not link it. The build requires one of the
  two, and only elogind is a port.
- `-Dadmin_group=wheel` — The group that makes an account an administrator, both in what the daemon
  reports as the account type and in what it changes when System Settings switches a user between
  Standard and Administrator. `wheel` is the group `sudo` and the polkit rules already treat as
  administrators on KDOS. Left empty, configure guesses from marker files of other distributions
  in the build root.
- `-Dwtmpfile=/var/log/wtmp` — The login history file the daemon watches to update each account's
  last-login time. It is the path [utmps](#port-utmps) writes. Left empty, configure looks for
  `WTMPX_FILENAME` or `_PATH_WTMPX`, finds neither in musl's headers, falls back to FreeBSD's
  `/var/log/utx.log` and stops unless that file exists in the build root. The daemon reads the
  file through the C library's `utmpx` functions, which on musl are stubs, so last-login times
  stay empty.
- `-Dsystemdsystemunitdir=no` — Install no systemd unit. Left empty, configure stops unless it
  finds systemd's `pkg-config` file.
- `-Dlibxcrypt=false` — Hash passwords set through the daemon with the C library's `crypt_r`.
  libxcrypt is not a port; musl's `crypt` writes SHA-512 hashes, the same hashes `passwd` and
  `pam_unix` write here.
- `-Dintrospection=false`, `-Dvapi=false` — Generate no GObject introspection data and no Vala
  bindings for `libaccountsservice`. Every consumer in the tree reaches the service over D-Bus,
  and none loads the GIR or compiles against the VAPI.
- `-Ddocbook=false`, `-Dgtk_doc=false` — Build neither the HTML D-Bus interface reference nor the
  API reference. Upstream has no manual page.
- `-Dtests=false` — Build no tests. They need the `mocklibc` subproject, which meson would try to
  download.

## Patches

`useradd-default-groups.patch` makes the daemon create an administrator in two steps, `useradd -m
-c <name> -- <user>` and then `usermod -a -G wheel -- <user>`, instead of one `useradd -G wheel`.
`useradd`'s `-G` replaces the `GROUPS=` list of `/etc/default/useradd` rather than adding to it,
and that list is how every new account on KDOS joins `audio`, which carries PipeWire's real-time
limits. Without the patch an account System Settings makes as a Standard user would join `audio`
and one made as an Administrator would not. Changing an account's type later keeps it: the daemon
rewrites the membership with `usermod -G`, adding or dropping `wheel` alone, so `audio` stays. The
upstream option `-Dextra_admin_groups=audio` is not the fix, because the daemon takes those groups
away again when an administrator is made Standard. Not proposed upstream: a distribution without
`GROUPS=` sees no difference.

## Notes

The recipe deletes `/usr/share/dbus-1/system-services/org.freedesktop.Accounts.service`, the D-Bus
activation file the build installs with no option to leave it out. `44_accounts.sh` supervises
`accounts-daemon`, and with the file a caller that asked for the name before the supervised daemon
held it would make the bus start a second, unsupervised one; whichever lost the name would exit,
and a supervised one that lost would be restarted every five seconds without ever getting it.

`/var/lib/AccountsService/users` must be mode 0700 and `icons` 0775. The package cannot carry
those modes, because `kpkgadd` creates every directory 0755; `accounts-daemon` creates and
`chmod`s both itself as it starts, and refuses to start if it cannot.

The project's version comes from `generate-version.sh`, which outside a git checkout reads it from
the name of the source directory, `accountsservice-<version>`. `kpkg` unpacks into exactly that
name; a build from a directory named otherwise stops at `meson setup` with "Version unknown".

[gettext](#port-gettext) is in `depends` for `msgfmt`, which merges the polkit policy's
translations; the message catalogues it also installs are removed by the
[language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).

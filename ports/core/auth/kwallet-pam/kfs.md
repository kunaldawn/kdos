## About

kwallet-pam opens the user's KDE wallet with the password typed at login, so the Secret Service
that [kwallet](#port-kwallet)'s `ksecretd` provides is unlocked without a second prompt. It is two
pieces: `pam_kwallet5.so`, a PAM module for the login manager's stacks, and `pam_kwallet_init`,
which an XDG autostart entry runs once the session's environment exists.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The project ships no interface translations, so this changes
  nothing today; every KDE recipe passes it, so a catalogue added upstream is skipped as well (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

In the `auth` phase the module only keeps the typed password for the session phase; it does
nothing for root or for an empty password. In the `session` phase it salts and hashes the
password, forks, starts the wallet daemon as the user with the hash on a pipe, and exports the
socket the daemon listens on as `PAM_KWALLET5_LOGIN`. `pam_kwallet_init` later hands the
session's environment to the waiting daemon over that socket with [socat](#port-socat), which is
in `depends` for run time; the configure only warns when it is missing.

The daemon the module starts is compiled in from `KWALLETD_BIN_PATH`, which the configure reads
from KF6Wallet's CMake package: `/usr/bin/ksecretd`. The first line of `build.sh` stops the build
unless that program exists, because a module built against a kwallet without `ksecretd` would exec
a missing file at every login and unlock nothing.

The module does nothing until a PAM stack names it, and the login manager's stacks carry
`-auth optional pam_kwallet5.so` and `-session optional pam_kwallet5.so`. It reads only the
arguments `kdehome=`, `kwalletd=`, `socketPath=` and `force_run`, and its session phase runs only
for a graphical session, which it recognises from an X display or from `XDG_SESSION_TYPE` set to
`wayland` or `x11`, unless `force_run` is given. With no password at login (autologin) there is
nothing to unlock with, and an unlock of the screen locker runs no session phase, so the wallet
stays locked and asks on first use (see
[The security model](../../../../docs/kdos/03-architecture/security-model.md)).

The install puts a systemd user unit, `plasma-kwallet-pam.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
The XDG autostart entry, `/etc/xdg/autostart/pam_kwallet_init.desktop`, does the same job.

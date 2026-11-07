## About

Linux-PAM, the [Pluggable Authentication Modules]{.idx}, is the library through which SDDM,
util-linux's `login`, `sudo`, `vlock`, the lock screen, polkit and the other programs that link it
hand a password check and the setting up of a login to a stack of modules configured under
`/etc/pam.d`. On KDOS the stacks are short: `pam_unix` against `/etc/passwd` and `/etc/shadow`,
`pam_limits` for resource limits, and, for a login, `pam_turnstile` and `pam_elogind`, which come
from [turnstile](#port-turnstile) and [elogind](#port-elogind); `sudo`'s file adds `pam_fprintd`
for an enrolled finger. Besides the library and modules, this port writes the shared stacks that
other service files include by name. shadow is built without PAM, so `su` and `passwd` read the
shadow file themselves.

## Flags

- `-D pam_unix=enabled` — Build `pam_unix`, the module that checks a password against the shadow
  file, and make it a setup error if it cannot be built; every stack `build.sh` writes except
  `other` names it.
- `-D logind=disabled`, `-D elogind=disabled` — Take no session information from a logind for
  `pam_issue`, `pam_limits` and `pam_timestamp`. elogind links `libpam` and is built after this
  port, so building pam against `libelogind` would be a dependency cycle; and the module that
  registers a login with elogind, `pam_elogind.so`, is built by elogind itself.
- `-D audit=disabled`, `-D selinux=disabled`, `-D econf=disabled`, `-D pwaccess=disabled` — Linux
  audit logging, SELinux contexts, libeconf's configuration parsing and libpwaccess in
  `pam_unix`; none of their libraries is a port.
- `-D nis=disabled` — NIS/YP lookups in `pam_unix`; accounts are local.
- `-D pam_userdb=disabled` — Do not build `pam_userdb`, which checks passwords against a
  Berkeley DB, GDBM or NDBM file instead of the shadow file.
- `-D i18n=disabled` — No translated messages; the English-only result is the
  [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `-D docs=disabled`, `-D examples=false`, `-D xtests=false` — No manuals built from DocBook, no
  example programs and no extended tests.

## Notes

- **`unix_chkpwd` is made setuid root here.** It is how `pam_unix` reads a 0600 `/etc/shadow` on
  behalf of a caller that is not root. meson installs it 0755; left that way it cannot open the
  file, and every check from a program running as the session user — the lock screen or `vlock` —
  is refused. It is one of the entries in
  [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).
- **The shared stacks.** `build.sh` writes `system-auth`, `system-account`, `system-session`,
  `system-password` and `system-login`, which the service files of other ports include by name:
  `sudo`'s, `kbd`'s `vlock`, polkit's `polkit-1`, util-linux's `login` and SDDM's. An `include`
  of a stack that is absent makes every call through that module type fail with
  `PAM_PERM_DENIED`, and only syslog names the file. With `system-account` missing, `sudo` reports
  "PAM account management error: Permission denied" and syslog has `_pam_load_conf_file: unable
  to open config for system-account`.
- **`system-login` is the stack every login includes.** It is the four `system-*` stacks plus two
  session modules: `pam_turnstile`, which creates `/run/user/<uid>` at the user's first login,
  starts the user's services and exports `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS`, and
  `pam_elogind`, which registers the login as a session on its seat, so the login is the active
  session polkit's `allow_active` answers apply to. turnstile comes first, as in Chimera Linux's
  `system-login`. Both lines start with `-`, so a system without either port installed still
  logs in, with no runtime directory or no registered session. A login that skips this stack
  gets neither: no session bus, no audio and no `allow_active` grants.
- **`other` denies.** A service with no file of its own is refused, and `pam_warn` logs it,
  rather than authenticating against nothing.
- **New hashes are SHA-512.** `pam_unix` hashes through musl's `crypt()`, which knows DES, MD5,
  bcrypt, SHA-256 and SHA-512 and nothing else. Naming `yescrypt` in a `password` line makes
  `pam_unix` log "Algo yescrypt not supported by the crypto backend" and refuse every password
  change made through PAM.
- **`/usr/lib/systemd` is removed after the install.** meson installs `pam_namespace.service`
  whatever the options say, and the
  [package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint) refuses
  a package with that directory in it.

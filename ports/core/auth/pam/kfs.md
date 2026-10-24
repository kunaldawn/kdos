## About

Linux-PAM, the [Pluggable Authentication Modules]{.idx}, is the library through which `sudo`,
`vlock`, `wayvnc` and the other programs that link it hand a password check to a stack of modules
configured under `/etc/pam.d`. `shadow` is built without it, so `login`, `su` and `passwd` read
the shadow file themselves. On KDOS the stack is short:
`pam_unix` against `/etc/passwd` and `/etc/shadow`, `pam_limits` for resource limits, and, in `sudo`'s file alone, `pam_fprintd` for an enrolled
finger. Besides the library and modules, this port writes the shared stacks that other service
files include by name.

## Flags

- `-D pam_unix=enabled` — Build `pam_unix`, the module that checks a password against the shadow
  file, and make it a setup error if it cannot be built; every stack `build.sh` writes except
  `other` names it.
- `-D logind=disabled`, `-D elogind=disabled` — The session tracking `pam_issue`, `pam_limits`
  and `pam_timestamp` can take from systemd-logind or elogind. KDOS has
  [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd) and no elogind.
- `-D audit=disabled`, `-D selinux=disabled`, `-D econf=disabled`, `-D pwaccess=disabled` — Linux
  audit logging, SELinux contexts, libeconf's configuration parsing and libpwaccess in
  `pam_unix`; none of their libraries is a port.
- `-D nis=disabled` — NIS/YP lookups in `pam_unix`; accounts are local.
- `-D pam_userdb=disabled` — Do not build `pam_userdb`, which checks passwords against a
  Berkeley DB or GDBM file instead of the shadow file.
- `-D i18n=disabled` — No translated messages; the English-only result is the
  [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `-D docs=disabled`, `-D examples=false`, `-D xtests=false` — No manuals built from DocBook, no
  example programs and no extended tests.

## Notes

- **`unix_chkpwd` is made setuid root here.** It is how `pam_unix` reads a 0600 `/etc/shadow` on
  behalf of a caller that is not root. meson installs it 0755; left that way it cannot open the
  file, and every check from a program running as the session user — `wayvnc` or `vlock` —
  is refused. It is one of the entries in
  [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).
- **The shared stacks.** `build.sh` writes `system-auth`, `system-account`, `system-session` and
  `system-password`, which the service files of other ports include by name: `sudo`'s and
  `kbd`'s `vlock`. A service file naming a stack that is absent falls through to `other`,
  which reports itself as "PAM account management error" and never mentions the missing stack.
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

## About

[sudo]{.idx} runs a command as root or as another user when the policy in `/etc/sudoers` allows
it, asking for the caller's own password and logging each command with its arguments. It is how
KDOS is administered from the session: members of `wheel` may run anything, and the `kdos`
subcommands that need root, such as `kdos clone`, are run through it. It authenticates through PAM, so a finger on an
enrolled reader is accepted before the password is asked.

## Flags

- `--libexecdir=/usr/lib` — Install the helpers and plug-ins (`sesh`, `sudo_noexec.so`,
  `sudoers.so` and the rest) under `/usr/lib/sudo` instead of `/usr/libexec/sudo`. `sudo`'s own
  `configure` puts the configuration in `/etc` whatever the prefix, so no `--sysconfdir` is
  needed.
- `--with-secure-path` — Run every command with a set `PATH` instead of the caller's. The
  compiled-in value starts with `/usr/local/sbin:/usr/local/bin`, and the file `build.sh` writes
  in `/etc/sudoers.d` sets the same list without `/sbin` and `/bin`. `/usr/local` must stay on
  it: `kdos-tools` links `kdos` and its other commands into `/usr/local/bin`, and `sudo kdos
  clone` finds nothing on a path without it.
- `--with-all-insults` — Compile in every insult set. They have an effect only when `insults` is
  enabled in `sudoers`, which nothing here does.
- `--with-env-editor` — Let `visudo` and `sudoedit` take the editor from `SUDO_EDITOR`, `VISUAL`
  or `EDITOR`, which is also upstream's default. The shipped `sudoers.d` file adds the three to `env_keep` so they survive into
  the command's environment.
- `--with-editor=/usr/bin/nvim:/usr/bin/nano` — The editors to fall back on, in order, when none
  of those variables names one; both are ports.
- `--with-rundir=/run/sudo` — Where the time stamps that let a second `sudo` skip the password
  are kept; a tmpfs, so they do not survive a reboot.
- `--with-vardir=/var/lib/sudo` — Where the files that should survive a reboot go, such as the
  record that a user has seen the lecture.
- `--with-passprompt="[sudo] password for %p: "` — The prompt, naming the user whose password is
  wanted.
- `--with-pam` — Authenticate through PAM, using `/etc/pam.d/sudo`.
- `--enable-zlib=system` — Compress I/O logs with the system `zlib` rather than sudo's bundled
  copy.
- `--enable-openssl` — Take TLS and the SHA-2 digests from OpenSSL.
- `--without-sendmail` — Send no mail at all. sudo would otherwise mail the administrator about
  events such as a user who is not in `sudoers` running it.

## Patches

- `eventlog-mailer-gid.patch` — Makes the child that runs the mailer set its group, and its supplementary groups, to the mail user's before it switches to that user, and exit when any of these calls fails instead of running the mailer anyway. `eventlog_set_mailuid()` becomes `eventlog_set_mailuser()`, taking a group ID beside the user ID.

## Notes

- **The PAM file tries the finger first.** `/etc/pam.d/sudo` opens with
  `-auth sufficient pam_fprintd.so`; the leading dash skips the line where `fprintd` is not
  installed, and a refused or absent finger falls through to the password in `system-auth`.
- **`/run` is removed from the package.** `make install` creates `/run/sudo`, and the
  [package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint) refuses
  `run` in a port's payload; sudo creates the directory at run time.
- **The `sudoers.d` file is mode 0440,** the mode of sudo's own `sudoers`: sudo refuses a policy
  file that anyone but root can write.
- `sudo` is one of the [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).

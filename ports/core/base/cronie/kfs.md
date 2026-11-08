## About

cronie is the cron of Fedora and RHEL: `crond`, the daemon that runs each user's scheduled jobs,
and `crontab`, the program a user installs, lists and removes their crontab with, plus
`cronnext`, which prints when a job runs next.

On KDOS it is the scheduler for the crontabs people write, and only for those. System Settings'
Task Scheduler page (kcron) edits a user's jobs through `crontab`. The system's own schedules are
not cron jobs: they stay `snooze` timers in `/etc/kdos/timers.d`, run by `18_timers.sh`, and a
user's timers in `~/.config/kdos/timers.d` are run by their user services. `crond` is started at
boot from an init script under `ksvc`. See
[No systemd init, units or service manager](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager).

## Flags

- `--localstatedir=/var` — Keep the crontabs in `/var/spool/cron/<user>`. Left at its default,
  the spool would be `/usr/var/spool/cron`.
- `--runstatedir=/run` — Write `crond.pid` in `/run`. The default is `<localstatedir>/run`.
- `--disable-anacron` — Build no `anacron`, which catches up daily, weekly and monthly jobs a
  powered-off machine missed. Those are system schedules, which are `snooze`'s. It also leaves
  out the copy of glibc's `obstack` anacron needs on musl.
- `--disable-syscrontab` — `crond` does not read `/etc/crontab`. A system job belongs in
  `/etc/kdos/timers.d`, where `snooze` runs it; with two places for system schedules a job could
  be written to the one nobody reads. kcron's system-crontab page edits `/etc/crontab`, so its
  entries do not run. `/etc/cron.d` is read whatever this says, and no port installs anything
  there.
- `--with-inotify` — `crond` watches the spool and `/etc/cron.d` with inotify and reloads a
  crontab as soon as it changes, instead of checking modification times once a minute.
- `--without-pam` — Run jobs without opening a PAM session. With PAM, `crond` and `crontab` need
  their own service file, because `/etc/pam.d/other` denies everything; a job then registers no
  elogind session, which a job does not need.
- `--without-selinux`, `--without-audit` — No SELinux contexts and no Linux audit records. Neither
  library is a port.
- `--with-editor=/usr/bin/nano` — The editor `crontab -e` runs when neither `VISUAL` nor `EDITOR`
  is set. Left at its default, configure looks for `vi` on the build machine and compiles in
  `/usr/bin/vi`, which the image does not have. [nano](#port-nano) is in `depends` for this.

## Notes

**`crontab` is setuid root.** It writes `/var/spool/cron/<user>` and touches the spool directory,
which is mode 0700 and owned by root; an ordinary user can do neither, and `crontab` drops back to
the caller's identity for everything else (running the editor, reading the new file). `make
install` installs it 0755, so the recipe sets mode 4755 itself. It is setuid, not setgid to a
`crontab` group as some distributions build it, because KDOS ships no setgid binary; see
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries). The
spool directory is created 0700 by `crond` or `crontab` the first time either runs; the package
does not carry it, because `kpkgadd` would create it 0755.

**`/etc/cron.deny` ships empty.** `crontab` refuses every user but root unless `/etc/cron.allow`
or `/etc/cron.deny` exists; an empty deny list lets every account keep a crontab. The recipe also
creates the empty `/etc/cron.d`, which `crond` reads at every reload and logs a failure for when it
is missing.

**Job output.** Without `-s`, `crond` mails a job's output through `/usr/sbin/sendmail`, which on
KDOS is msmtp and has nowhere to deliver a local message. The init script runs `crond -n -s`: in
the foreground for `ksvc`, with output to syslog.

Alpine carries one patch, completing the `error_func` prototype of `load_entry()` for GCC 15's
default C23. The tree compiles C with `-std=gnu11`, under which the original declaration is valid,
so the patch is not carried.

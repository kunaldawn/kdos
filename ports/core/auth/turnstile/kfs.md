## About

turnstile is the part of `systemd --user` that Plasma needs, and nothing else. Its daemon,
`turnstiled`, counts each user's logins across SDDM, the text consoles and anything else that runs
the PAM module `pam_turnstile`: at the first login it creates `/run/user/<uid>` and starts that
user's service manager through a *backend*, and at the last logout it stops the services and
removes the directory, unless the user lingers. The PAM module holds the login until the backend
reports its services ready, then exports `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS` into the
session.

On KDOS the backend is `ksvc`, the script `/usr/lib/turnstile/ksvc`, which runs the scripts in
`/etc/kdos/user-services/` under `ksvc`: the session bus at `$XDG_RUNTIME_DIR/bus`, PipeWire,
WirePlumber, `pipewire-pulse` and the user's timers, for every account from uid 1000; the greeter's
`sddm` account gets the session bus alone, and other system accounts nothing. Both it and
`/etc/turnstile/turnstiled.conf` come from the tree's `fs/` overlay, not from this port.
`turnstiled` itself runs as root under `ksvc` from an init script. See
[No systemd init, units or service manager](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager).

## Flags

- `--libexecdir=/usr/lib` — `turnstiled` runs a backend named `<backend>` as
  `<libexecdir>/turnstile/<backend>`, so the KDOS backend is `/usr/lib/turnstile/ksvc`.
- `-Ddinit=disabled`, `-Drunit=disabled` — Install neither upstream backend nor its
  configuration. KDOS runs neither service manager; its backend is the `ksvc` script.
- `-Dmanage_rundir=true` — Compile in `manage_rundir = yes`: `turnstiled` creates
  `/run/user/<uid>` at a user's first login and removes it at the last, and the PAM module
  exports `XDG_RUNTIME_DIR`. [elogind](#port-elogind) is built not to, so turnstile is the
  directory's one owner; without this, and with elogind's runtime-directory code patched out, a
  login gets no runtime directory and KWin, the greeter's included, does not start. Lingering
  also works only with it on.
- `-Dstatedir=lib/elogind` — The state directory, relative to `/var`, whose `linger`
  subdirectory `turnstiled` reads: a file named after a user there keeps that user's services
  running after the last logout (`linger = maybe`). elogind writes
  `/var/lib/elogind/linger/<user>` when `loginctl enable-linger` runs, so this makes the two read
  the same directory. turnstile keeps nothing else there.
- `-Dlibrary=disabled` — Build no `libturnstile`, the client library for asking `turnstiled`
  about sessions; nothing in the tree links it.
- `-Dman=true` — Build `turnstiled(8)`, `pam_turnstile(8)` and `turnstiled.conf(5)` with
  [scdoc](#port-scdoc).

## Notes

The recipe deletes `/etc/turnstile/turnstiled.conf`, which the build generates from its options. KDOS's own copy is in `fs/` and sets `backend = ksvc`, `manage_rundir = yes`,
`export_dbus_address = yes` and `linger = maybe`; a copy in the package would be installed over
it, because packages are installed after `fs/` is synced. `default_backend` is not passed: it
changes only the generated file, and `turnstiled`'s own built-in backend is `dinit` whatever the
build says, so without KDOS's file every login would try to run a `dinit` backend that does not
exist. The runtime-directory default does reach the daemon, through `-Dmanage_rundir=true`.

`/etc/pam.d/turnstiled` is upstream's: the PAM stack of the session `turnstiled` opens for the
user's service manager. It includes `pam_elogind`, so elogind sees that session, and
`pam_limits`, so the realtime and memory-lock limits in `/etc/security/limits.d` reach PipeWire,
which the backend starts inside it, and not only the login shell.

`turnstiled` waits for the backend's readiness report for at most `login_timeout` seconds and then
ends the user's service manager and closes the user's session connections, so a backend that
never reports ready shows up as a login that hangs for that long and then has no user services.

The last logout's clear of the runtime directory leaves every mount point in it alone
(`rundir-mounts.patch`). KDOS's pack daemon composes each box's root as an overlay under
`/run/user/<uid>/kdos/boxes/<box>/root`, whose upper directory is the box's persistent state in
the user's home, so a clear that descended into it would write a whiteout for every file of the
box.

A login that arrives while the previous manager is stopping waits for that manager to exit and
for the next one to report ready (`stopping-manager-readiness.patch`). Upstream releases it on
the stopping manager's readiness when that manager was still coming up as the last session
closed, and SDDM's automatic login after `loginctl terminate-user` lands there: Plasma starts on
the session bus that is being stopped, `kded6` and `plasmashell` lose it, and the screen stays
black. The same patch closes a stopped manager's unreported readiness pipe when the next one
starts, since an event on it would otherwise make `turnstiled` exit. The patch is to be offered
upstream.

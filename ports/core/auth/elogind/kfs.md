## About

elogind is systemd's logind taken out of systemd and run as a daemon of its own. It owns
`org.freedesktop.login1` on the system bus: it records every login as a session on a seat, tells
which session is the active one, hands the session's compositor its display and input devices
(`TakeControl`), announces a suspend or a power-off and lets programs delay or block one with an
inhibitor. Besides the daemon this port installs `pam_elogind.so`, the PAM module through which a
login registers its session, the client library `libelogind` and `loginctl`.

On KDOS it is the one logind. SDDM records each session through it and switches users through it;
KWin opens the display and the input devices through the session it records, and has no other
session backend that works here; PowerDevil suspends the machine and owns the lid through it; and
the lock screen locks when it announces a sleep. [polkit](#port-polkit) asks it whether a caller
sits in the active local session, which is what the `allow_active` column of an action's policy
means. Why KDOS carries this one piece of systemd's source tree, and what it costs, is in
[No systemd init, units or service manager](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager).

The daemon is `/usr/lib/elogind/elogind`, started at boot by `/etc/init.d/40_elogind.sh` under
`ksvc`, after D-Bus and before polkit and NetworkManager.

## Flags

- `--libexecdir=/usr/lib/elogind` — The daemon, `elogind-cgroups-agent`,
  `elogind-uaccess-command` and the `system-sleep` and `system-shutdown` hook directories go into
  `/usr/lib/elogind`, beside the private `libelogind-shared` library. elogind uses the value as the
  directory itself, not as a parent to add `elogind` to, so `/usr/lib` would make the daemon the
  file `/usr/lib/elogind` and collide with that directory. TLP's sleep hook is installed into
  `/usr/lib/elogind/system-sleep` for the same reason.
- `-Dmode=release` — The build mode. The option has no default value, so meson takes the first
  choice, `developer`, which adds `-fno-omit-frame-pointer` and turns on developer-only behaviour;
  `release` adds `-ftrivial-auto-var-init=zero` instead.
- `-Dcgroup-controller=elogind` — The name elogind gives the cgroup hierarchy it tracks sessions
  in. Left empty, configure runs a script that reads the build machine's `/proc/self/cgroup` and
  takes whatever init that machine runs, so the value would depend on the builder.
- `-Ddefault-hierarchy=unified` — Pure cgroup2, the only hierarchy `/etc/fstab` mounts.
  `DelegateSessionCgroups=yes` in the shipped drop-in needs it. It is upstream's default value,
  stated.
- `-Ddefault-kill-user-processes=false` — Compile in `KillUserProcesses=no`. The drop-in sets the
  same key; the compiled default decides when the drop-in is absent.
- `-Dhalt-path=/sbin/halt`, `-Dpoweroff-path=/sbin/poweroff`, `-Dreboot-path=/sbin/reboot` —
  sysvinit's programs, so Shut Down and Restart from Plasma go through runlevel 0 or 6 and `rcK`
  like any other shutdown. Left empty, each path is whatever the build machine has first in
  `/usr/sbin` or `/sbin`.
- `-Dkexec-path=/usr/sbin/kexec`, `-Dnologin-path=/usr/sbin/nologin` — Stated for the same
  reason: a fixed path instead of one probed on the builder. No port installs `kexec`, so login1's
  kexec reboot is not available.
- `-Dpamlibdir=/usr/lib/security` — Install `pam_elogind.so` where [pam](#port-pam) keeps its
  modules. It is the value the option would derive, stated.
- `-Dpam=enabled` — Build `pam_elogind.so`, and make a missing `libpam` a configure error. Every
  login stack includes it through `/etc/pam.d/system-login`.
- `-Dpolkit=enabled` — Ask polkit before a caller may suspend, power off, reboot, take over a seat
  or set a user's linger, and install `org.freedesktop.login1.policy`. Without it elogind allows
  those calls only to root, and PowerDevil cannot suspend. The option needs no polkit library at
  build time: configure looks up `polkit-gobject-1` only to decide whether to install the
  pre-0.106 `.pkla` files, and the checks go to polkit over D-Bus. So elogind does not depend on
  polkit, and [polkit](#port-polkit), built `-Dsession_tracking=elogind`, depends on elogind with
  no cycle.
- `-Dacl=enabled` — Link [acl](#port-acl). `elogind-uaccess-command`, which the `uaccess` udev rule
  runs, grants the active session's user an ACL on the seat's device nodes (sound, video, DRM,
  input) and takes it back on a session switch.
- `-Dutmp=false` — Write no utmp or wtmp records. SDDM and util-linux's `login` write them through
  [utmps](#port-utmps); elogind would write through musl's `utmpx` functions, which are stubs.
- `-Daudit=disabled`, `-Dselinux=disabled`, `-Dsmack=false`, `-Dxenctrl=disabled` — No Linux audit
  records, no SELinux or SMACK labels and no Xen kexec. None of the libraries is a port.
- `-Ddbus=disabled` — Do not link libdbus. elogind talks D-Bus through its own sd-bus; libdbus is
  used only by its test programs. The policy file goes to `/usr/share/dbus-1/system.d` either way.
- `-Dtranslations=false` — No message catalogues; English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Dman=enabled`, `-Dhtml=disabled` — Build the manual pages (`loginctl(1)`, `logind.conf(5)`,
  `pam_elogind(8)`, the `sd-login` and `sd-bus` API pages) from DocBook with `xsltproc`, through
  [libxslt](#port-libxslt), [docbook-xsl](#port-docbook-xsl) and [docbook-xml](#port-docbook-xml),
  and no HTML copy of them.
- `-Dtests=false` — Build no test programs.

## Patches

Ten of the patches are Alpine's set for elogind 255.24 on musl, each carried as Alpine has it
with a `Subject:` line first: `0001` to `0009` make the systemd-derived code compile and behave
against musl (no `<gshadow.h>`, `sh` instead of `bash` in two generator scripts, the XSI
`strerror_r`, `getdents`, a `struct stat` field the journal code cannot compile against musl,
and three include fixes), and `0010` is a backport from systemd that collects a session when its
leader process dies. `no-runtime-dir.patch` is Chimera Linux's, and is what lets
[turnstile](#port-turnstile) own the runtime directory (see Notes).
`lxml-resolve-entities.patch` passes `resolve_entities=True` to the parser in
`tools/xml_helper.py`: [python3-lxml](#port-python3-lxml) otherwise leaves `custom-entities.ent`
unloaded, and the `elogind.directives` and `elogind.index` pages stop the build with
`Entity 'entities' not defined`. Those two pages are built unconditionally whatever configure
finds, so lxml is a build dependency, not an optional one.
`udevadm-path.patch` writes `/usr/sbin/udevadm` into `71-seat.rules`. The template takes the
directory from `bindir`, which on KDOS holds no `udevadm` (see [eudev](#port-eudev)), and udev logs
the failed run only at debug level.

## Notes

**`/run/user/<uid>` belongs to turnstile.** Unpatched, elogind mounts a tmpfs there at a user's
first login and unmounts and deletes it with everything in it at the last logout, and
`pam_elogind` exports `XDG_RUNTIME_DIR`. KDOS boxes and the user's services keep their sockets in
that directory (the session bus at `/run/user/<uid>/bus`, which every box binds), and a user who
lingers keeps them running after logging out, so a second owner removing the directory under them
breaks every box. `no-runtime-dir.patch` takes both jobs out of elogind; turnstile, built with
`manage_rundir`, creates, exports and removes the directory. With the patch and without turnstile
in the PAM stack a login has no `XDG_RUNTIME_DIR` at all, and KWin, the greeter's included, refuses
to start.

**Lingering is one directory for both.** `loginctl enable-linger` writes
`/var/lib/elogind/linger/<user>`. turnstile is built to read the same directory, so the linger
elogind reports and the one turnstile acts on cannot disagree.

**No D-Bus activation.** The build installs
`/usr/share/dbus-1/system-services/org.freedesktop.login1.service`, which would let the bus start a
second, unsupervised elogind for any caller that asked for `org.freedesktop.login1` before the
supervised one held it. The recipe deletes it, as the polkit recipe deletes polkit's; the init
script waits until elogind owns the name before the scripts after it run.

**Configuration.** Two drop-ins, read after upstream's `10-elogind.conf`, come from the tree's
`fs/` overlay, not from this package, so their comments carry the reasons beside the keys:

- `/etc/elogind/logind.conf.d/50-kdos.conf`. `KillUserProcesses=no` and `RemoveIPC=no` keep a
  user's boxes, their processes and their POSIX shared memory alive across a logout.
  `DelegateSessionCgroups=yes` gives each session the `session` and `delegated` cgroups under
  `/sys/fs/cgroup/<session>`, owned by the user, which rootless podman needs to enforce a box's
  memory and CPU limits. The `Handle*` keys apply only where no session holds the matching
  inhibitor, which in a Plasma session PowerDevil does: at the greeter and at a console login, a
  closed lid suspends unless the machine is docked, and the power key powers off.
  `InhibitDelayMaxSec=5` bounds how long the lock screen and KWin may delay a suspend.
- `/etc/elogind/sleep.conf.d/50-kdos.conf`. Hibernation, suspend-then-hibernate and hybrid sleep
  are refused, so `CanHibernate` and the related calls answer `no` and Plasma hides those actions.
  Swap on KDOS is zram, so there is no disk to resume from. These keys are read from
  `sleep.conf`, not `logind.conf`; a `[Sleep]` section in a `logind.conf` drop-in is ignored.

The package's own `logind.conf`, `sleep.conf` and their `10-elogind.conf` drop-ins set nothing and
sort before 50. A copy of either KDOS file in the package would be installed over the `fs/` one,
because packages are installed after `fs/` is synced, which is why the recipe writes neither.

**Seats come from the udev rules.** The package installs `70-power-switch.rules`,
`70-uaccess.rules`, `71-seat.rules` and `73-seat-late.rules` into `/usr/lib/udev/rules.d`.
`71-seat.rules` tags every DRM card `master-of-seat` and every input device `seat`; without it
seat0 has no graphics device and the greeter's KWin cannot take the seat. `-Dacl=enabled` is what
writes the `elogind-uaccess-command` line into `73-seat-late.rules`.

**No `libsystemd` alias.** Some distributions link `libsystemd.pc` and `/usr/include/systemd` to
elogind's. KDOS does not, so a port that probes for libsystemd still finds none and its
systemd-only code stays off; a port that wants logind names `libelogind`.

`meson setup` runs `realpath --relative-to`, which [coreutils](#port-coreutils)' `realpath` has
and toybox's and busybox's do not, so coreutils is in `depends`.

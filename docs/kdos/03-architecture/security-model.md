# The security model

Who is allowed to do what on a KDOS machine, which mechanisms enforce it, and —
as prominently — what is **not** protected. A security model that lists only
its defences is a security model that will be relied on for things it does not
do, so [What is not protected](#what-is-not-protected) at the end of this page
is part of the specification rather than a caveat.

## What this model is for

KDOS is a single-user workstation. One human account ships, that account is in
`wheel`, and `wheel` is trusted. The threats this design addresses are:

- Software from the outer ring — a browser, an office suite, an application
  from someone else's distribution — doing things a desktop application has no
  business doing.
- A tampered or substituted **artefact**: a package, an index or an application
  image that is not what it claims to be.
- Privilege escalation through the small number of programs that genuinely need
  privilege.
- A local user in `wheel` making a catastrophic mistake through an interface
  that should not have allowed it to be expressed.

## setuid binaries

The shipped system carries nineteen setuid-root binaries. Exactly two are
KDOS's own.

| Binary | Origin | For |
|---|---|---|
| `kdos-checkpass` | **KDOS** | Checking the caller's own password against the shadow file |
| `kdos-resctl` | **KDOS** | Signalling and renicing a process from the resource monitor |
| `sudo` | sudo | Running as another user |
| `su` | shadow | Switching to another account — util-linux's is disabled with `--disable-su` |
| `passwd`, `chage`, `gpasswd`, `chfn`, `chsh`, `newgrp` | shadow | Account management |
| `pkexec`, `polkit-agent-helper-1` | polkit | Authorised privileged actions |
| `ssh-keysign` | OpenSSH | Host-based authentication |
| `dbus-daemon-launch-helper` | dbus | System bus activation — `root:messagebus`, mode `4110`, so only the bus can run it |
| `mount.nfs` | nfs-utils | Mounting an NFS share named in `fstab` as an ordinary user |
| `unix_chkpwd` | pam | How `pam_unix` reads the 0600 shadow file for a caller that is not root — without it every unprivileged PAM check, `wayvnc`'s included, is refused |
| `fusermount3` | libfuse | Mounting a userspace filesystem from a session with no user namespace — sshfs, gocryptfs, fuse-overlayfs, the document portal, `rclone mount`, and `restic mount` through the `fusermount` link beside it |
| `newuidmap`, `newgidmap` | shadow | **Rootless containers** |

The last two are why every application on the machine works. The container
engine runs them to write a process's user-namespace map; the kernel allows
that only from a process already holding the relevant capability, and the
engine checks the binary first and refuses outright if it is not setuid — then
exits with nothing else printed. Lose those bits and every graphical
application stops starting, with nothing saying why.

Kerberos adds none of these. `krb5`'s own `ksu` — `su` to another Kerberos
principal — is installed setuid by upstream and is removed by the recipe. The
accounts on this machine are local and its privilege escalation is `sudo`, so
`ksu` would be an entry nothing uses and nobody audits. What Kerberos is here
for is `kinit`, which is an ordinary program writing your own credential cache.

Count the tree, never this table. `find / -perm /4000 -type f` on a built image
is the authority; a page that disagreed with it would be the one thing in this
chapter nobody can act on.

Mode bits are used rather than file capabilities, deliberately: mode bits
survive all three hops the system makes them take — the compressed system
image, the installer's copy, and the pack image format — with no extended
attribute anywhere in the chain.

`kdos doctor` checks the five critical ones — `kdos-checkpass`, `kdos-resctl`,
`newuidmap`, `newgidmap`, and `dbus-daemon-launch-helper` with its group,
`messagebus` — because losing a setuid bit is the
worst *silent* failure in the system. An archive copy without the right flag is
all it takes.

### No setgid ones, which is why `plocate`'s index is per user

`plocate` upstream ships **setgid** to a `plocate` group with a shared
`/var/lib` database at mode 0640. The index names every path on the machine, an
ordinary user cannot read it, and the binary reads it on their behalf. What
stops that being a full disclosure is a visibility check: for every candidate
path `plocate` walks the parent directories and calls `access(dir, R_OK|X_OK)`
as the calling user's real uid, dropping what they could not have reached.
`--ignore-visibility`, `--debug` and `--flush-cache` all `setresgid` away the
group first, because upstream's own comment says keeping it "would subvert the
entire security model".

KDOS ships it with none of that: no setgid bit, no `plocate` group, no shared
database. The index is built **per user**, into that user's own cache, by that
user's own timer — so it can only ever contain paths that user could already
list, and the visibility check has nothing left to guard. `updatedb -o` and
`--require-visibility no` are upstream's own flags for building it; the second
is not optional, because with the check on `updatedb` insists on the `plocate`
group and writes nothing. `$LOCATE_PATH` names it to the reader, and that is
the one patch: upstream searches its compiled-in `/var/lib` database first and
stops at the first database it cannot open, so on a system with no shared
database every unpatched search fails before it reaches the user's own.
Patched, a set `$LOCATE_PATH` replaces the default rather than following it.

That is the reasoning the rest of this page uses. A mechanism is not made safe
here, it is made unnecessary. The alternative would have been a third
privileged binary and a group to keep in `fs/etc/group`, bought to share one
index between users this machine does not have.

## kdos-checkpass

This is the program that locks you out if it is wrong, so it is worth stating
in full what it does not do.

- **It takes no arguments.** Not even a user name. The account checked is the
  caller's **real user id**, so there is nothing to aim at root and nothing an
  attacker can vary.
- **The password arrives on stdin**, never in the argument vector, because a
  process's command line is world-readable for its whole lifetime.
- **Privilege is dropped as soon as the hash is read.**
- **The comparison is constant time.**
- **A locked, disabled or empty hash always fails.**
- **There are three exit codes** — correct, wrong, error — and the caller must
  distinguish them. Reporting "wrong password" for a machine with a broken
  shadow file sends the user looking in entirely the wrong place.

Without its setuid bit it cannot read the shadow file, so it refuses every
password and locks the user out of their own session.

The shadow file is 0600, and the build has to be told so. git records one
permission bit, so nothing under `fs/` can carry a mode narrower than 644 and
the file-system step hands every non-executable file exactly that. A password
database at 644 is every hash on the machine readable by every account on it —
and it makes the setuid bit above decoration, because the file this program
exists to keep private is already open.
`script/01_phase1/00_file_system.sh` carries an explicit table of the paths 644
is wrong for; `testing/preflight.sh` asserts the result on the **built** tree,
because the source tree cannot express the answer.

## kdos-resctl

The second setuid binary, and its whole security argument is that there is
nothing to aim: three verbs, no paths, no options.

```
kdos-resctl dmi
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

- The hardware-table path is compiled in. Nothing in the argument vector is
  ever opened.
- The caller must be in `wheel` by real user id.
- A process handle is taken **before** the signal is sent, so a signal cannot
  land on a recycled process id.
- Process 1 is refused.
- Privilege is dropped **before** the hardware table is parsed, so the parser
  never runs as root.
- It is never on the sampling path. A setuid fork once a second would be an
  attack surface with a schedule.

## System accounts

Every daemon that drops privilege drops to an account of its own, and none of
them can log in: each has `/sbin/nologin` for a shell and `!` for a password.
The image ships these in `/etc/passwd`, `/etc/group` and `/etc/shadow`:

| Account | uid:gid | Used by |
|---|---|---|
| `dhcpcd` | 999:999 | `dhcpcd`'s privilege-separated children |
| `messagebus` | 997:997 | `dbus-daemon`, the system bus |
| `sshd` | 996:996 | `sshd`'s privilege separation |
| `tss` | 993:993 | The TPM: tpm2-tss's udev rules give `/dev/tpm*` to the user and `/dev/tpmrm*` to the group |
| `lp` | 10:10 | CUPS |
| `nobody` | 99:99 | Anything that asks for an unprivileged account by that name |

`polkitd`, `avahi`, `avahi-autoipd`, `geoclue`, `mosquitto`, `nm-openvpn`, `pcscd`, `postgres`,
`prosody` and `tcpdump` are made by their ports'
`postinstall.sh` with `groupadd -r` and `useradd -r`, which pick a free id.
The group `brlapi` is made the same way by `brltty`'s, with the desktop account
in it: it is the group brltty's polkit rule admits to BrlAPI.
Every id a shipped account uses therefore has a line of its own in both files:
a primary gid with no `/etc/group` line looks free to `groupadd -r`, which
would hand it to another daemon's group. A group a udev rule names has to exist
too, because eudev logs `specified group '<x>' unknown`, carries on with gid 0, and
grants nothing.

The desktop account is not in `tss`, so the TPM is root's to use. The
`tpm2_*` tools run under `sudo`, like any other administration.

## Root daemons

Five daemons run as root and answer a socket in `/run`: `kdos-powerd`,
`kdos-energyd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd`. They share one
authorisation design.

The gate is the peer's credentials, not the socket's mode. Every socket is mode
0666 and anyone may connect; the daemon reads the connecting process's real
user id from the kernel with `SO_PEERCRED` and answers `err not permitted` to
anyone it does not admit. Root and `wheel` reach every verb. `seat`, the group
seatd hands the display to, also reaches what the person at the machine needs
whether or not they administer it: `kdos-powerd`'s suspend, power-off and
reboot, all of `kdos-mountd`, and `kdos-oomd`'s status. The installer keeps the
desktop user in `seat` and takes a non-administrator out of `wheel`, so that
account keeps the lid and the power keys and loses sudo, the polkit admin
actions and every configuration verb. A mode that *looked* like the
authorisation is a mode somebody eventually loosens, and there is nothing in
the message a client can forge.

That test is `libkbase`'s `kb_uid_allowed()` and no daemon keeps a copy of it.
This is the authorisation boundary of the whole system, and five copies are a
rule that gets tightened on one socket and stays loose on the other four, with
nothing in the tree to show which is which. A daemon that needs a different
rule states the difference beside its own call and asks `kb_uid_allowed()` for
the rest — `kdos-mountd`'s `--fixture-serve` mode is the only one that does,
and it grants nothing: its paths are a scratch directory, every child it would
spawn is printed instead of run, and the init script starts the daemon with no
arguments. `kdos-powerd --explain <user>` asks the same function, so what it
prints cannot drift from what the socket decides.

The client never names a path. Every verb takes an identifier out of a list the
daemon itself published a moment earlier, or out of a list compiled into it.
There is nothing to aim: the design where a daemon takes a device and a mount
point ends at mounting a stick over `/etc` from any shell at the seat.
`kdos-powerd accent` is the tightest case — the argument must resolve against
`libkcolor`'s own scheme table, of which there are currently eight, so the
daemon links the palette rather than copying a character class out of it.

The one deliberate exception is installing a pack, which names a **filename in
a staging directory the daemon owns** — the single place an unprivileged write
is allowed, and the daemon publishes that path rather than making clients
derive it.

Per-daemon refusals:

| Daemon | Refuses |
|---|---|
| `kdos-mountd` | Internal disks; filesystems the kernel cannot mount; anything in `fstab`; **every partition of the disk the system booted from**; a verb carrying a token nobody named; an index that is not a number; a filesystem outside the four it will write; a `format` at all unless `format = yes` is set in its configuration, or one not confirmed with the device's own kernel name; a node whose device differs from the one the scan recorded |
| `kdos-packd` | Paths as arguments; a pack whose hash or signature fails; removing a pack that is in use |
| `kdos-oomd` | Any argument at all — killing is its own decision or it does not happen |
| `kdos-energyd` | Republishing the raw counter; a client-chosen sampling interval |
| `kdos-powerd` | Anything but four fixed words — `ping`, `suspend`, `poweroff`, `reboot` — and four that carry an argument validated against a table compiled into it: `firewall`, `autologin`, `accent`, `timezone`; an `accent` that does not name a scheme in `libkcolor` |

`kdos-energyd` deserves its own note. The CPU energy counter is root-only
because fine-grained unprivileged reads can recover cryptographic keys through
a side channel. What leaves this daemon is a per-application percentage over
minutes; the raw counter and the interval are never republished, and the
interval is fixed by the daemon rather than requested by a client, so it cannot
be driven toward being one. There is no write path into the power interface at
all.

`kdos-boxsock` is sometimes counted among these and is not one of them. It runs
as the desktop user, binds one tagged Wayland socket per box, and holds the
security context open for that box's lifetime.

## polkit, and why the desktop has no authentication agent

polkit is installed and `polkitd` is started by the init system. NetworkManager,
ModemManager, `bolt`, `fwupd`, `upower`, `fprintd`, `pcscd` and `brltty`'s
BrlAPI server ask it, as does ModemManager on GeoClue's behalf, and NetworkManager's actions are the only ones a KDOS surface calls.
What follows is measured against the shipped image, not against how polkit
behaves on a distribution that has a session manager.

polkit here can never see an ACTIVE session, so `allow_active` and
`allow_inactive` are dead columns. It resolves a subject's session by calling
`org.freedesktop.ConsoleKit` on the system bus. ConsoleKit is not installed and
elogind is not installed, and neither is coming — logind is refused by
[principles](../01-philosophy/principles.md#no-systemd) and ConsoleKit would be
a session daemon bought for one consumer. The lookup fails, the subject has no
session, and every check falls to the `allow_any` column of the action's
`.policy` file.

An action with no `<allow_any>` element is a flat refusal that no agent can be
asked about. polkit's parser initialises the implicit fields to *not
authorised*, and a not-authorised result is returned without raising a
challenge — an authentication agent is consulted **only** when the result is a
challenge. NetworkManager's shipped policy omits `<allow_any>` for
`enable-disable-wifi`, `enable-disable-network` and both `wifi.share` actions.

That is what decides the choice between an agent and a rules file, and it
decides it on correctness rather than on taste. An agent — one written here, or
the `pkttyagent` polkit already ships — cannot make the wifi toggle work,
because polkit never asks anybody about a flat refusal. It would also have
nothing to register as: with no session, an agent can only register a
**unix-process** subject, and polkit finds that agent by an exact match on pid
and start time, so a session-lifetime agent would never be found for a surface
it did not itself spawn. `polkit-agent-helper-1` is on this machine and is
setuid, so it could check a password, through the same PAM `system-auth` stack
`sudo` uses — but nothing ever asks it to, because a flat refusal raises no
challenge for an agent to answer.

The same reasoning reaches `bolt`, `fwupd` and `upower`, and it is why none of
them has a privileged path for the desktop user. Their actions are
`auth_admin`, which is a challenge, and a challenge with no agent is a refusal;
`bolt` and `fwupd` ship rules of their own that would grant `wheel`, but both
test `subject.active` and `subject.local`, which are never true on this
machine, so neither fires. `fprintd`'s enrol and verify actions are
`allow_active` only. A thunderbolt enrolment, a firmware update or a
fingerprint enrolment is therefore a `sudo` away, not a prompt away — polkit
authorises uid 0 for every action.

Three consumers are granted all the same, each for its own reason. `pcscd` asks
on every PC/SC connection and allows only an active session, so without a
grant no user reaches a smart card; the card is its holder's own device behind
its own PIN, and `50-kdos.rules` grants both of its actions to `wheel`.
ModemManager is built `-Dpolkit=strict`, which ships unlocking a SIM and
reading its text messages `allow_active` only; NetworkManager needs no grant to
drive a modem, being root, so `50-kdos.rules` grants `Device.Control` and
`Messaging` to `wheel` for `mmcli` and nothing else of ModemManager's.
`brltty`'s BrlAPI server asks for `org.a11y.brlapi.write-display`, and the rule
brltty ships grants it to the `brlapi` group with no session test, so it works
here unchanged. GeoClue's rule is the same shape: it grants ModemManager's
`Device.Control` and `Location` to the `geoclue` account alone, with no session
test, so the location service can switch on a modem's GPS; nobody logs in as
that account.

So the answer is a rules file, and there is no agent on this system.
`fs/etc/polkit-1/rules.d/50-kdos.rules` names the actions a shipped surface or user program
calls and grants them to `wheel`. The rules file is also the form that degrades
gracefully: if a session provider is ever added, an explicit grant to `wheel`
stays exactly as narrow as it was written, while a bus-policy grant would have
to be unpicked.

It names actions rather than granting an interface. The alternative — denying
NetworkManager's write interfaces to `context="default"` in a D-Bus policy and
allowing `wheel` — cannot scope a `Properties.Set` to one property and cannot
leave out the hostname or the machine-wide resolver, both of which this file
withholds.

### Why `settings.modify.system` is granted

On this build it is the only permission a saved network can be under.
NetworkManager decides a profile is *visible* by asking its session monitor
whether each user named in the profile's `permissions` list has a session. This
build is compiled `-Dsession_tracking=no` — there is no logind and no
ConsoleKit — so that call is a literal `return FALSE`: a profile naming an
owner is permanently invisible, and an invisible profile has **autoconnect
blocked**. A wifi network joined from `kdos-net` would never come back after a
reboot. So `kdos-net` writes no owner, everything it creates is a system
connection, and forgetting one or reading its passphrase back lands on
`settings.modify.system`.

That grant does let anybody in `wheel` read every stored passphrase. It is a
shortcut rather than a new capability: the shipped sudoers line is
`%wheel ALL=(ALL) ALL`, and the passphrases are files under
`/etc/NetworkManager` that `sudo cat` prints. What the file still withholds is
everything outside networking-as-a-user and smart-card access — the hostname, the machine-wide
resolver, checkpoints, sleep and a daemon reload.

`polkitd` is started by `/etc/init.d/41_polkitd.sh` rather than left to D-Bus
activation. Activation has dbus-daemon, running as `messagebus`, start a
`User=root` service through `dbus-daemon-launch-helper`, which it may execute
only through the helper's group — and kpkg rolls every package `root:root`, so
that group exists only because `dbus`'s `postinstall.sh` puts it back.
Starting polkitd directly keeps the whole of this machine's network
authorisation off that one dependency. When
activation fails it fails silently: the service never starts, every call is
refused, and the only trace is a `Spawn.ExecFailed` in the log. `kdos doctor`
checks the helper's owner, group and mode for that reason. ModemManager is
started by `/etc/init.d/42_modemmanager.sh` for the same reason. The daemons
that are left to activation — `wpa_supplicant`, `fprintd`, `fwupd`, `boltd`,
`upower`, `geoclue` and NetworkManager's dispatcher — are the ones that depend on it.

The rules file and its directory are owned by root, and the build has to say
so, because git records no owner either. polkitd reads every rule it finds with
no ownership or mode check, so a rules directory writable by the desktop user
is that user granting themselves whatever they like.

What this grants, stated honestly: anybody in `wheel` reconfigures networking
with no prompt. That is the same group that may already power the machine off
through `kdos-powerd` and write a filesystem through `kdos-mountd`, so it is
not a new boundary — it is the existing one, applied to a third thing behind
it. What it is *not* is a password prompt: this system cannot produce one, and
a design that pretended otherwise would be a control that fails silently.

The list of action ids is the entire boundary, and the only record of its use
is a log line. NetworkManager is built `-Dlibaudit=no`, so an authorised change
leaves no audit record beyond NetworkManager's own message to syslog. A rules
grant is silent and permanent by construction; that line is the whole of the
trail.

## Sandboxed clients

A client from a box is tagged by `kdos-boxsock` with a security context naming
its box, and the compositor's global filter gives such clients a **fixed
allowlist**.

| Allowed | Denied |
|---|---|
| Surfaces, subsurfaces, shm and the xdg shell | Screen capture, both generations |
| The seat, relative pointer, pointer gestures, pointer constraints | Buffer export |
| Outputs, dmabuf, viewporter, presentation, fractional scale | Data-control (clipboard manipulation), both generations |
| Both decoration managers, activation, tablet, toplevel icon, dialog | Foreign-toplevel management and the toplevel list |
| **Text input** | **Input method and virtual keyboard** |
| The primary selection, and the ordinary data device — the clipboard and drag-and-drop | Output management and output power |
| Colour management, alpha modifier, syncobj, single-pixel buffer | The layer shell |
| Cursor shape, idle inhibit, tearing control, xdg-foreign both ways, xdg-output | The security-context manager itself |

Two entries in that table are the interesting ones.

Text input is deliberately allowed. It is the *application* half of the
input-method protocol, and denying it would deny input methods to exactly the
applications that need one most.

The input-method and virtual-keyboard interfaces are denied, because a client
that can be an input method receives every keystroke on the seat. That is a
keylogger by design, and it is not something an application from someone else's
distribution gets to be.

The security-context manager is denied to anyone already carrying a context, so
a box cannot mint a context of its own.

That denial is what makes the portal the sanctioned route rather than a
convenience. A boxed screen recorder cannot bind the capture interface at all,
so it must ask the portal, which runs on the host and asks you which output to
share. The denial is what gives the question meaning.

### Granting a box past the allowlist

The fixed list is the right default and is deliberately not the only answer: a
screen recorder in a box of its own could not otherwise be given the screen
back, however deliberately. `grant = screencopy, data-control` in
`~/.config/kdos/boxes/<name>.conf` names globals that box may bind.

Eight names are grantable — `screencopy`, `toplevel-capture`, `export-dmabuf`,
`data-control`, `foreign-toplevel`, `layer-shell`, `input-method` and
`output-power` — and a name that is not in that map cannot be granted at all.
The map is the whole policy. Each short name unlocks **both** generations of
its protocol, because granting the old screen-copy and not the new one strands
every client written after 2024.

`input-method` is the one grant that has to be spelled out in full and is kept
conspicuous for the reason above. Granting it hands that box every keystroke on
the seat.

Grants are read once per client, at the first bind that reaches the filter, and
cached against the box name, because the filter runs for every global for every
client. `SIGHUP` drops the cache with the rest of the configuration, so editing
a profile takes effect for the next client; a running one keeps what it already
bound.

## Containers

Boxes are **rootless**. The container engine runs as your user, mapping your
identity into the container, with the setuid mapping helpers above doing the
one privileged step.

Ownership inside a box grants nothing, because packs are mounted `nosuid`.
Running as a mapped non-root user rather than as mapped-root is chosen because
applications refuse to run as root, not because it is a boundary — a process in
your box is a process running as you.

A box is not a security boundary against you. It shares your home directory in
full. What it is, is a boundary against *the desktop's* interfaces — the
compositor globals above — and a packaging mechanism.

Box profiles are honest about this. Every key maps onto something actually
enforced, and the profile says out loud what it cannot enforce: there is no
engine flag that grants a box a speaker and denies it a camera, so the profile
does not pretend to have one. A memory budget is enforced by `kdos-oomd` rather
than by the engine, because rootless containers on a machine with no cgroup
delegation accept a memory limit and ignore it.

A pulled image is not verified. `/etc/containers/policy.json` is upstream's
default, which accepts any image and checks no signature, so a base image is
trusted as far as the registry and the TLS connection to it are.

## Mount options

| Mounted | Options |
|---|---|
| Application packs | `ro,nosuid,nodev` |
| Data packs | `ro,nosuid,nodev,`**`noexec`** |
| Removable media | `nosuid,nodev`, and `noexec` by default |
| `/tmp`, `/run` | `nosuid,nodev` |

`exec = yes` in the removable-media configuration is how somebody says they
meant it. A setuid root binary on a stick from another machine is a local root
hole that predates every other consideration on this page.

`/tmp` is one directory shared by every user and written by every root job — the
init scripts, the timers, a package's hooks. `/etc/sysctl.conf` sets
`fs.protected_symlinks`, `fs.protected_hardlinks` and `fs.protected_fifos` to 1
and `fs.protected_regular` to 2, which the kernel leaves at 0. At 0, a user who
plants a link, a FIFO or a file at a name a root job is about to write in a
sticky world-writable directory turns that write onto any file on the machine.

## Signing and trust

Two keyrings, and their separation is structural rather than conventional:

| Directory | Attests | Used for |
|---|---|---|
| `/etc/kdos/keys` | Who built a host package | The binary host index and package sidecars |
| `/etc/kdos/keys/packs` | Who exported an application pack | The pack index |

The keyring loader reads `*.pub` in a directory and does **not** descend, so
the two are genuinely different policies. A pack-signing key placed in the host
directory would silently become a trusted publisher of *host packages* too — a
widening nobody asked for.

The directory is the policy. There is no revocation list and no online check:
adding a key is copying a file in, removing trust is deleting one. A key id is
not a credential — it is a label on the signature line, and verification tries
every key in the directory and nothing outside it, so what a tool reports is
the key that verified, not the id the line claimed.

The rest of the signing design is in [Packaging](packaging.md).

### TLS trust anchors

A third trust root, and it is not a keyring: the Mozilla CA bundle
`ca-certificates` installs as one file, `/usr/share/ca-certificates/mozilla.pem`,
plus whatever local roots the administrator puts in
`/etc/ca-certificates/trust-source/anchors/`. `/etc/ssl/cert.pem` is the two
together — 121 Mozilla certificates and then each local one — and
`/etc/ssl/certs/ca-certificates.crt` and `/etc/ssl/ca-bundle.crt` are symlinks to
it, so a consumer configured against any of the three reads the same set.

The bundle is built, not carried. The port pins `certdata.txt` at an NSS release
tag — the port's version is that release — and converts it with curl's
`mk-ca-bundle.pl` at a pinned curl release, the converter behind curl's own
`cacert.pem`, with its defaults: the roots NSS trusts to issue server
certificates. The converter also drops any root already expired when it runs,
so a rebuild after a root's expiry ships one certificate fewer.

| Consumer | Reaches the bundle through |
|---|---|
| OpenSSL, and everything linked against it | `--openssldir=/etc/ssl`, which finds `cert.pem` |
| GnuTLS, and everything linked against it | p11-kit's trust module, built `-D trust_paths=/usr/share/ca-certificates/mozilla.pem:/etc/ca-certificates/trust-source` |
| Python code that asks `certifi.where()`, `requests` among it | `python3-certifi`, whose `certifi/cacert.pem` is a symlink to `/etc/ssl/cert.pem` |

GnuTLS is configured `--with-default-trust-store-pkcs11="pkcs11:"`, so p11-kit is
its only source of anchors. p11-kit gives a trust path that is a plain **file** the
anchor flag and marks every certificate parsed out of it `CKA_TRUSTED`; of a
trust path that is a **directory** it trusts only what is in the `anchors/`
subdirectory. GnuTLS therefore sees a local root the moment the file is there.

OpenSSL does not: `/etc/ssl/cert.pem` is a generated file, and
`update-ca-certificates` writes it — the Mozilla bundle, then the certificate
blocks of every PEM file in the anchors directory, renamed into place so no
reader sees half of it. `ca-certificates` runs it from its install hook, so an
upgrade rewrites the bundle with the local roots still in it; the administrator
runs it after adding or removing one. A DER file in the anchors directory has no
certificate block, is skipped with a warning, and is then trusted by GnuTLS
alone. To trust a local CA:

```sh
cp root.crt /etc/ca-certificates/trust-source/anchors/   # PEM
update-ca-certificates
```

`caddy trust` does the same by itself: its trust-store library finds the
anchors directory, writes Caddy's local root there, and runs
`trust extract-compat`, which p11-kit hands to `update-ca-certificates`.
Firefox and other NSS programs keep their own store and are not covered.

GnuTLS reads a system-wide priority policy from `/etc/gnutls/config`. The image
ships none, so every consumer uses the library's `NORMAL` priorities; a file
written there restricts or widens them for every GnuTLS program at once. The path
is compiled in from `--sysconfdir=/etc`, and a policy written anywhere else is
ignored without a warning.

The two halves fail independently, and the failure reads as a remote fault. Point
the trust path at something the image does not ship and p11-kit loads zero tokens
without a word — an absent path is not an error to it —
`gnutls_certificate_set_x509_system_trust()` then returns zero anchors and every
GnuTLS consumer rejects every peer, while `curl` and the rest of the OpenSSL side
keep working. `msmtp`, `openconnect`, `weechat` and chrony's NTS are the four that
show it first.

### An application is verified or it is not, and which one is knowable

The two ways an application arrives have different guarantees, and neither is
hidden.

| | Store install | Import |
|---|---|---|
| Bytes come from | Debian's archive, over the network | A `.ktar` somebody handed you |
| Signed by | nothing in `/etc/kdos/keys` | a key in `/etc/kdos/keys/packs`, when the exporter had one |
| Checked at | nothing checks it | `kdos-packd`, at the mount: payload hash, then signature |
| Needs a network | yes | no |

A store install fetches unsigned content from somebody else's registry.
`kdos-box create` prints exactly that for an OCI base, and it is true of every
application built this way. What apt itself verifies still holds — the
archive's own GPG signature, over a pinned snapshot — but nothing this system
controls attests to the result, and the image is not signed afterwards.

An imported pack is checked where it is mounted, not where it is chosen. A
client that verified a pack and then asked for a mount would have verified
nothing; `kdos-packd` hashes the payload and checks the signature itself, at
the moment it mounts, which is why import is safe over a channel that is not.

An unsigned export is still hash-checked and says it is unsigned. The index
records a payload hash per pack whether or not a key was available, so a
tampered member fails at the mount either way. What the signature adds is
*who*, and an export with no key prints that it added nothing.

## Untrusted image bytes

Anything that can write to a terminal can reach an image decoder. A shell
script, a program inside a box, `cat` on a file somebody sent you — three
escape sequences carry a picture, and behind them are four large C libraries
with long memory-safety histories.

That is why `libkimg` is a library with **one entry point** rather than four
calls scattered through a terminal: one place to audit, one place the budget is
enforced, one place a fifth format would be added.

- **The budget is enforced before any allocation**, from the size the format
  itself declares. A length field is an allocation request from an untrusted
  peer: a decompression bomb is four lines of sixel, and a PNG saying
  65535x65535 is eight bytes on the wire and sixteen gigabytes in memory.
  Refusing after decoding is not refusing.
- **A declared type that disagrees with the bytes is a refusal, not a
  re-sniff.** A peer that says PNG and sends sixel is not making a mistake worth
  accommodating.
- **The payload is capped, and a payload over the cap is dropped entirely
  rather than truncated.** Half an image is not a smaller image; it is a
  malformed file, and handing one to a decoder is handing it exactly what an
  attacker would have sent on purpose.
- **Both decoders are silenced.** libpng and libjpeg write to stderr by
  default, which is untrusted bytes deciding what appears on a terminal.
- **Every failure returns the same nothing.** The caller cannot tell a
  truncated file from an unsupported format from a budget refusal, on purpose:
  there is nothing useful to do differently, and a reason string reaching a log
  is a reason string an attacker chose.
- **The corpus is fuzzed**, every fixture truncated at every length and with
  every byte flipped, under the address and undefined-behaviour sanitizers.

A build without the decoders turns the three protocols off in the parser,
rather than parsing them and dropping the result. Parsing bytes nobody can use
is a buffer somebody can fill.

## A URI a terminal was told about

`OSC 8` marks a run of text as a hyperlink, and following one is the one place
a terminal hands a program that opens things an address it was given by a child
process. The reach is the same as an image payload: a shell script, a program
inside a box, `cat` on a file somebody sent you.

- **Four schemes and nothing else** — `http`, `https`, `file`, `mailto`. The
  set is not "everything a desktop can open"; it is the ones whose worst case is
  a window appearing. A scheme handler is chosen by MIME type from
  `x-scheme-handler/<scheme>`, so an unlisted scheme is a program of the
  attacker's choosing being asked to start.
- **Every byte must be printable ASCII.** A control byte would reach an
  argument vector, and a byte above 126 makes the same address read two ways
  depending on who decodes it — which is how a whitelist gets walked around
  rather than broken.
- **The address is refused at the parser**, not at the click. What is not in
  the table cannot be followed by any path, including one written later.
- **It is executed as an argument vector**, never a command line:
  `kdos-appbox open <uri>` with the URI as one element. There is no shell
  anywhere on the path.
- **The table is capped at 128 addresses per terminal.** A child emitting a
  fresh URI per cell is the shape of the attack, and past the cap the text is
  text with no link offered.
- **A refused link is silent.** The characters draw normally; nothing says
  "refused", because a message naming the address would put the attacker's
  string on the screen.

## What is not protected

Stated plainly, because a reader who assumes otherwise is worse off than one
who knows.

- **There is no mandatory access control.** No SELinux, no AppArmor. A process
  running as you can do anything you can do.
- **There is no verified boot and no signed kernel.** The boot chain is not
  measured or attested. An attacker with physical access and a moment alone with
  the machine owns it.
- **Disk encryption protects data at rest only.** It is a passphrase in the
  initramfs, not a TPM-sealed key, and it does nothing once the machine is
  running.
- **A box is not a jail.** It shares your home directory. A malicious
  application in a box can read and destroy your files exactly as a native one
  could. The sandbox constrains what it can do to the *desktop*, not to your
  data.
- **`wheel` is effectively root.** `sudo` and polkit both grant it — polkit
  through `/etc/polkit-1/rules.d/50-kdos.rules`, unconditionally, because this
  system cannot ask for a password. With no session provider, `subject.active`
  and `subject.local` are always false, so a rule granting `wheel` covers a
  member logged in over SSH and a background process running as them exactly as
  it covers somebody at the console. Adding a session provider later would not
  narrow it; the rule would have to be rewritten. And every root daemon answers
  `wheel` on every verb. There is no separation between "can change the theme"
  and "can reformat the disk".
- **A base naming a container registry fetches unsigned content** from somebody
  else's server. This is an online operation, the strict-signature setting does
  not cover it, and the tool announces it before doing anything rather than
  pretending otherwise.
- **An unsigned pack mounts.** Only a *failed* signature is refused, so a pack
  that is signed and uncheckable is treated more harshly than the same pack with
  no signature block at all.
- **Ports built from source are not signed**, and need not be: a port's
  integrity is the checksum in its recipe. But that means the recipe — and
  therefore this repository — is the trust root for everything on the host.
- **A granted box is as privileged as the grant.** `grant = input-method` hands
  that box every keystroke on the seat, and nothing warns at the moment a key is
  pressed. The profile is the record.
- **There is no automatic update path and no security-advisory service.**
  `kdos cve` tells you what is behind a known fix; acting on it is a rebuild you
  perform.
- **`kdos-oomd` has never fired in anger.** Its victim selection is exercised
  against recorded system state, but a genuine memory-pressure stall is the test
  that matters and has not been run.

## See also

- [Packaging](packaging.md) — signing, the index, and the three equality tests
- [Packs and boxes](packs-and-boxes.md) — verification at mount time, and mount options
- [The daemons](../04-programs/daemons.md) — each daemon's verbs and refusals
- [The session](session.md) — the sandbox allowlist and the portal route
- [Known gaps](../06-reference/known-gaps.md) — everything else that does not exist

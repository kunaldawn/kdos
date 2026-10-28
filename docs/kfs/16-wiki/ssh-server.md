# SSH server

KDOS ships the OpenSSH server, `sshd`, turned off. Two things keep it from answering the network:
a marker file that stops it starting at boot, and the firewall, which drops connections to port 22.
This page covers how to turn both on together, the files involved, logging in with keys instead of
a [password]{.idx}, and why a login is refused.

## Installation

[openssh](#port-openssh), installed by `41_system` on every image, provides `sshd`, the client
`ssh`, `scp`, `sftp`, `ssh-keygen`, `ssh-agent` and `ssh-copy-id`. Its `build.sh`:

<!-- kfs:include ports/core/servers/openssh/build.sh lines=12-39 pin=255eb66d5379 -->

It is built without `--with-pam`, so `sshd` checks passwords against `/etc/shadow` itself and runs
no PAM module. `--with-security-key-builtin` lets it take keys held on a FIDO security key.

The service is `sshd`, started by [`/etc/init.d/70_sshd.sh`]{.idx} under the supervisor. Whether it starts
at boot:

| | Live medium | Installed system |
|:--|:--|:--|
| [`/etc/service.disabled/sshd`]{.idx} | Present: `sshd` does not start | Present unless the installer's [SSH]{.idx} server choice, or the answer file's `services` key, named `sshd` |
| Port 22 in the firewall | Closed | Closed, whatever the installer's choice |
| Passwords | `kdos` and `root` both have the password `kdos` | The ones given to the installer; root is locked unless one was set |
| Host keys | Made when `sshd` first starts in a session, and kept only in a [persistence]{.idx term="persistence store"} store | Made at the first start on the installed disk. The installer does not copy the live session's keys |

The live column is what a booted live medium shows. The installed column is what the installer's
Configure step writes; an installed disk has not been booted to check it.

[mosh](#port-mosh) is installed beside it, and works over an SSH login.

## Configuration

### /etc/ssh/sshd_config

The server's configuration is upstream's file, as `make install` writes it; no file in the tree
replaces it. Every option in it is commented out at its default except `AuthorizedKeysFile` and the
`sftp` subsystem, which `make install` points at `/usr/lib/openssh/sftp-server`. Among the defaults:
password logins are accepted, root may log in only with a key (`PermitRootLogin
prohibit-password`), and X11 forwarding is off. Every option is in `sshd_config(5)`.

`kpkgadd` places every file a package ships, under `/etc` as everywhere else, so installing openssh
again over a root puts upstream's `sshd_config` back over an edited one. `kpkg check openssh` lists
an edited file as a `config` line; see
[Checking an installed root](../../kdos/03-architecture/packaging.md#checking-an-installed-root).

### /etc/init.d/70_sshd.sh

From `fs/etc/init.d/`. It creates the run directory, makes any host key that is missing, and runs
`sshd` in the foreground under the supervisor:

<!-- kfs:include fs/etc/init.d/70_sshd.sh -->

### /etc/service.disabled/sshd

An empty marker, `fs/etc/service.disabled/sshd` in the tree, that makes `rcS` skip `70_sshd.sh` at
boot. It is the one marker the image ships; see
[the service.disabled reference](../../kdos/06-reference/configuration.md#etcservicedisabledname).

### The firewall's ssh service

`kdos-powerd` treats `ssh` as both a port and a daemon. The comment above the code that does it,
from `src/daemons/kdos-powerd/main.c`:

<!-- kfs:include src/daemons/kdos-powerd/main.c lines=463-471 pin=9d44e95d2ac3 -->

The rule it writes goes into [`/etc/nftables.d/50-kdos-services.nft`]{.idx}; see [Firewall](firewall.md).

### The privilege-separation account

`sshd` drops to the `sshd` account for the work it does before a login, with `/var/lib/sshd` as its
empty directory. The account comes from `fs/etc/passwd`:

<!-- kfs:include fs/etc/passwd lines=4-4 pin=ed7f3a055b02 -->

### /etc/profile.d/50-ssh-agent.sh

Written by the openssh `build.sh` (above). Every login shell that has a runtime directory gets one
`ssh-agent`, at `$XDG_RUNTIME_DIR/ssh-agent.socket`, shared by the account's later logins.

## Usage

### Turning the server on

Needs membership of `wheel`.

1. On the live medium, change both known passwords first, since anyone on the network who reaches
   port 22 can otherwise log in:

   ```bash
   passwd
   sudo passwd root
   ```

2. Open the port and start the server, in one step:

   ```bash
   kdos-power firewall ssh on
   ```

   or switch the `ssh` row in Settings → **System** → **Firewall…**. Either removes
   `/etc/service.disabled/sshd`, starts `sshd`, and opens TCP 22.

3. Check:

   ```bash
   service status sshd
   kdos-power firewall list
   ```

   `service status sshd` answers `[ OK ] sshd  (pid N)`, and the `ssh` row reads `on`.

4. From another machine: `ssh kdos@<address>`. The first connection shows the host key's
   fingerprint. Compare it with the fingerprint the KDOS machine itself prints for
   `ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub`, a line of the form
   `256 SHA256:<hash> root@<hostname> (ED25519)`, before answering `yes`.

The first start makes an RSA, an ECDSA, an Ed25519 and an ML-DSA-44/Ed25519 host key in
`/etc/ssh/`.

An installed system whose installer was asked for the SSH server starts `sshd` at boot and still
drops port 22, so step 2 is needed there too.

### Turning it off

```bash
kdos-power firewall ssh off
```

It closes the port, stops `sshd` and puts the marker back, so the server stays off at the next
boot. `sudo service stop sshd` alone stops it for this boot only.

### Logging in with a key

1. On the client, make a key if you have none:

   ```bash
   ssh-keygen -t ed25519
   ```

   or `ssh-keygen -t ed25519-sk` for one held on a security key; see
   [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md).

2. Copy its public half to the KDOS machine:

   ```bash
   ssh-copy-id kdos@<address>
   ```

   It appends the key to `~/.ssh/authorized_keys` on the server.

3. Log in with `ssh kdos@<address>`; no password is asked. `/var/log/auth.log` on the server,
   which only root can read (`sudo tail /var/log/auth.log`), records it as `Accepted publickey for kdos from <address> port <n> ssh2: ED25519 SHA256:<hash>`.

### Refusing password logins

Do this only after a key login works, from a second connection kept open while you change it.

1. As root, edit [`/etc/ssh/sshd_config`]{.idx} and set:

   ```text
   PasswordAuthentication no
   KbdInteractiveAuthentication no
   ```

2. Check the file:

   ```bash
   sudo sshd -t
   ```

   No output means it is valid.

3. Restart the server: `sudo service restart sshd`.

4. Keep a copy of the edited file. Installing openssh again replaces it with upstream's, which
   accepts passwords again.

### mosh

1. Open both services:

   ```bash
   kdos-power firewall ssh on
   kdos-power firewall mosh on
   ```

   `mosh` opens UDP 60000–61000.

2. From the client: `mosh kdos@<address>`. It logs in over SSH, then starts `mosh-server` and
   continues over UDP.

## Tips

- `sudo kdos doctor` warns when the `kdos` account still has the shipped password. Run it before
  opening port 22 on an installed machine whose answer file set no password.
- A login shell reached over SSH finds the desktop's session bus when the desktop is running: the
  profile points `DBUS_SESSION_BUS_ADDRESS` at `$XDG_RUNTIME_DIR/bus` when that socket exists, and
  `SSH_AUTH_SOCK` at the account's shared agent, unless the client forwarded its own agent with
  `ssh -A`, which the profile then leaves in place. A command given on the `ssh` line runs without
  the login profile and gets neither.
- `sftp` and `scp` work as soon as `ssh` does; the `sftp` subsystem is in the shipped file.
- `service status ssh` finds `sshd` too, but `service enable` and `disable` need the exact name
  `sshd`.
- A session over SSH is not moved into the account's control group the way the `tty1` login is,
  so a box started from it runs without the memory limit; see
  [kdos-banner and kdos-getty](../../kdos/04-programs/kdos-command.md#kdos-banner-and-kdos-getty).

## Troubleshooting

**`Connection timed out` from the client.**
The firewall drops port 22. Run `kdos-power firewall list` and turn `ssh` on.

**`Connection refused`.**
The port is open and nothing listens on it. Run `service status sshd`, then read the service's
logs:

```bash
cat /run/kdos-init.sshd.log      # what 70_sshd.sh printed at boot
cat /run/kdos-svc.sshd.log       # the supervisor and sshd's own output
```

The first exists only when `sshd` was started at boot; a `service start` or a firewall toggle
prints to the caller instead. The second starts `[KDOS] (sshd) Starting: /usr/sbin/sshd`.

`[SKIP] sshd: /usr/sbin/sshd not found` means openssh is not installed. A config error shows as
`sshd` exiting at once and the supervisor restarting it every five seconds; `sudo sshd -t` names the
line.

**The server is not started at boot.**
`ls /etc/service.disabled/` lists `sshd`. Remove it with `sudo service enable sshd`, or with
`kdos-power firewall ssh on`, which also opens the port.

**`Permission denied` with the right password.**
Read `/var/log/auth.log`, where `sshd` logs every attempt. Only root can read it, so use
`sudo tail /var/log/auth.log`. Root is refused a password login by
`PermitRootLogin prohibit-password`; log in as your own account and use `sudo`. An account
whose shadow entry is locked has no password to give.

**A key is refused and the password is asked for.**
`sshd` ignores `authorized_keys` that others can write, and logs
`Authentication refused: bad ownership or modes for file /home/<user>/.ssh/authorized_keys` to
`/var/log/auth.log`, which needs `sudo` to read. On the server:
`chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys`, and make sure the home directory is not
group- or world-writable.

**The client warns that the host key has changed, on the live medium.**
Without a persistence store, every boot of the live medium makes new host keys. Remove the old
entry on the client with `ssh-keygen -R <address>`.

**`sudo` over SSH pauses before it asks for the password.**
The account has a finger enrolled, and `pam_fprintd` waits on the machine's reader first; see
[Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md).

## Known gaps

- The live medium's known passwords, among the points in
  [Security](../../kdos/06-reference/known-gaps.md#security)
- [A fingerprint unlocks sudo and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else):
  `sudo` over SSH and the reader
- [Kerberos is built and has never been given a ticket](../../kdos/06-reference/known-gaps.md#kerberos-is-built-and-has-never-been-given-a-ticket):
  openssh is built `--with-kerberos5`, and GSSAPI logins are untried

## See also

- [Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service): the firewall's
  named services
- [firewall](../../kdos/04-programs/daemons.md#firewall): how `ssh on` also starts the server
- [Answer files](../../kdos/04-programs/kinstall.md#answer-files): the installer's `services` key
- [What is not protected](../../kdos/03-architecture/security-model.md#what-is-not-protected)
- [Firewall](firewall.md), [Services](services.md), [Login and autologin](login-and-autologin.md),
  [Users and groups](users-and-groups.md), [Logs](logs.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 385.* Previous: [384. Firewall](firewall.md) · [Contents](../README.md) · Next: [386. Signing keys and trust](signing-keys-and-trust.md)

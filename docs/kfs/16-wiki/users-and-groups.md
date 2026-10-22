# Users and groups

This page is about the accounts on a KDOS machine: the one that ships, how to add, change and remove
others, which groups an account needs for the desktop and the hardware, and which account `tty1`
logs in. What the [wheel]{.idx} and `seat` groups grant, and why each hardware group exists, is in
the KDOS book's [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups)
section, which this page does not repeat.

## Installation

| What | Port |
|---|---|
| [`useradd`]{.idx}, `usermod`, `userdel`, `groupadd`, `gpasswd`, [`passwd`]{.idx}, [`chsh`]{.idx}, [`login`]{.idx}, `su`, `newuidmap`, `newgidmap` | [shadow](#port-shadow) |
| `agetty`, which runs the login prompt on each console | [util-linux](#port-util-linux) |
| [`kdos-users`]{.idx}, the Users window | [kdos-shell](#port-kdos-shell) |
| `kdos-power autologin`, which sets the account `tty1` logs in | [kdos-powerd](#port-kdos-powerd) |
| [`kdos-checkpass`]{.idx}, the [lock screen]{.idx}'s [password]{.idx} check | [kdos-lock](#port-kdos-lock) |

shadow is built without PAM, so `login`, `su` and `passwd` read [`/etc/shadow`]{.idx} themselves; the
port's page says why. The account files themselves are not from a port: [`/etc/passwd`]{.idx},
[`/etc/group`]{.idx}, `/etc/shadow`, [`/etc/subuid`]{.idx}, [`/etc/subgid`]{.idx} and `/etc/skel` come from the tree's
`fs/` overlay, and ports that need a service account add it from their `postinstall.sh`
([System accounts](../../kdos/03-architecture/security-model.md#system-accounts)).

[`/etc/init.d/15_userdirs.sh`]{.idx}, also from `fs/`, makes each account's runtime directory at boot. It
is enabled on the image, on the live medium and on an installed system.

On the live medium the desktop account is [`kdos`]{.idx term="kdos (command)"} with the password `kdos`, and `root` has the same
password. The installer asks for the account's name and password, renames `kdos` to the name given,
and locks `root` unless told otherwise; see
[6. Accounts](../../kdos/02-user-guide/installation.md#6-accounts).

## Configuration

**`/etc/passwd`**, from `fs/etc/passwd`. Every account but `root` and `kdos` is a service account
with `/sbin/nologin`:

<!-- kfs:include fs/etc/passwd -->

**`/etc/group`**, from `fs/etc/group`. The last field lists the members; `kdos` is in `wheel`,
`seat` and the hardware groups:

<!-- kfs:include fs/etc/group -->

**`/etc/shadow`**, from `fs/etc/shadow`, holds the password hashes: the live password of `root`
and `kdos`, and `!` (no password, no login) for every service account. Git cannot record its mode,
so the build sets it to 600; see
[The mode of `/etc/shadow`](../../kdos/03-architecture/security-model.md#the-mode-of-etcshadow).

**`/etc/subuid`** and **`/etc/subgid`**, from `fs/etc/subuid` and `fs/etc/subgid`, give each
account a range of user and group ids for rootless containers, which every boxed application runs
in. The two files are the same:

<!-- kfs:include fs/etc/subuid -->

**[`/etc/shells`]{.idx}**, from `fs/etc/shells`, lists the shells `chsh` accepts:

<!-- kfs:include fs/etc/shells -->

**`/etc/login.defs`** is shadow's own file, installed unchanged from upstream. It sets the ranges
`useradd` picks ids from and the hash `passwd` writes; its `ENCRYPT_METHOD` line is commented out,
so passwords are hashed with SHA-512. See the [shadow](#port-shadow) page.

**`/etc/skel`**, from `fs/etc/skel`, is copied into each new home by `useradd -m`: the shell
profiles that start the desktop on `tty1`, and the desktop's own starting configuration.

**`/etc/kdos/login.conf`**, from `fs/etc/kdos/login.conf`, names the account `tty1` logs in
without a password:

<!-- kfs:include fs/etc/kdos/login.conf -->

The installer comments the key out unless an answer file asked for automatic login, so an
installed machine asks for a password on `tty1`.

**`/etc/init.d/15_userdirs.sh`**, from `fs/etc/init.d/15_userdirs.sh`, makes `/run/user/<uid>` and
a delegated cgroup for every account with a uid from 1000 to 65533 that exists at boot:

<!-- kfs:include fs/etc/init.d/15_userdirs.sh -->

## Usage

### Changing a password

1. Your own: `passwd`.
2. Another account's, as an administrator: `sudo passwd <user>`.

Change the live password before anything else; `sudo kdos doctor` warns while the `kdos` account
still has it.

### Adding a desktop user

1. Create the account with a home, the desktop's groups and bash. For an administrator, as the
   shipped account is:

   ```bash
   sudo useradd -m -s /bin/bash \
       -G wheel,seat,users,audio,video,render,input,kvm,cdrom,dialout,lpadmin,tty alice
   ```

   For an account that runs the desktop but does not administer the machine, leave `wheel` out of
   the list. `seat` is what lets it suspend, power off, reboot and mount removable media.
   `useradd` prints `Creating mailbox file: No such file or directory`, because the image has no
   `/var/spool/mail`; the account is made all the same.

2. Give it a password:

   ```bash
   sudo passwd alice
   ```

3. Check that `useradd` gave it a range of subordinate ids, which boxed applications need:

   ```bash
   grep alice /etc/subuid /etc/subgid
   ```

   The first account added after `kdos` gets `alice:265536:65536` in both, the range after
   `root`'s. If either file has no line for it, see [Troubleshooting](#troubleshooting).

4. Make its runtime directory, which `15_userdirs` otherwise makes only at the next boot:

   ```bash
   sudo service start userdirs
   ```

5. Check what the root daemons will let it do:

   ```bash
   kdos-powerd --explain alice
   ```

   It prints the account's uid and whether it is in `wheel`, in `seat` only, or in neither:
   `alice: uid 1001, primary gid 1001, in wheel — permitted`, or for an account in `seat` only,
   `in seat only — permitted to suspend, power off and reboot; refused the configuration verbs`.
   It is in `/usr/sbin`, on the `PATH` of a login shell.

### Adding an account to a group

```bash
sudo usermod -aG dialout alice
```

`-a` adds to the account's groups; without it, `-G` replaces them. The change applies from the
account's next login: `id alice` shows it at once, while `id` in a session started before it does
not.

To take an account out of a group, here to stop it administering the machine:

```bash
sudo gpasswd -d alice wheel
```

### Choosing the account `tty1` logs in

1. Open the **Users** window, `kdos-users`, which lists the accounts and sets the automatic login;
   or use the command, which needs `wheel`:

   ```bash
   kdos-power autologin alice
   ```

2. To have `tty1` ask for a password instead:

   ```bash
   kdos-power autologin off
   ```

The change applies the next time `tty1`'s login starts: at the next boot, or after ending the
desktop session and then the shell it leaves on `tty1` (`exit`). Both refuse an account that
cannot log in. [Login and autologin](login-and-autologin.md) has the rest of the login path.

### Locking and removing an account

1. Lock it, so no password opens it, and unlock it again:

   ```bash
   sudo usermod -L alice
   sudo usermod -U alice
   ```

2. Remove it with its home directory. If `login.conf` names it, point the automatic login
   elsewhere first:

   ```bash
   sudo userdel -r alice
   ```

3. Check that no subordinate id range is left for it with `grep alice /etc/subuid /etc/subgid`.

### The root account

- Lock it: `sudo passwd -l root`. The installer does this by default.
- Give it a password: `sudo passwd root`.

Keep at least one account in `wheel` while `root` is locked, or nobody can administer the machine.

### Changing a login shell

```bash
chsh -s /bin/zsh
```

The shell must be listed in `/etc/shells` and installed. The skeleton's `~/.zprofile` starts the
desktop on `tty1` as `~/.bash_profile` does.

### On the live medium

Every change on this page is to `/etc` or `/home`, and on the live medium both are in RAM and start
again from the image at every boot. A persistence store keeps them
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

## Tips

- `kdos-users` reads the account files and changes only the automatic login; adding accounts and
  passwords is `useradd` and `passwd`.
- `tty2` is a plain login prompt for any account, whatever `login.conf` says: the way in when the
  desktop account's session will not start. `Ctrl+Alt+F2` reaches it from the desktop.
- `newgrp dialout` starts a shell with a group just added, without logging out.
- The ranges in `/etc/subuid` and `/etc/subgid` must not overlap. Each line is the account, the
  first id and the number of ids; the shipped ranges are in the file shown under
  [Configuration](#configuration).

## Troubleshooting

**A new account's desktop does not start, or its programs complain about `XDG_RUNTIME_DIR`.** Its
runtime directory is missing:

```bash
ls -ld /run/user/$(id -u alice)
sudo service start userdirs
```

**Boxed applications do not start for one account.** `kdos doctor`, run as that account, warns
`no /etc/subuid entry` in its Containers section. Give it a range that overlaps no other line of
`/etc/subuid` or `/etc/subgid`, here 300000 to 365535:

```bash
sudo usermod --add-subuids 300000-365535 --add-subgids 300000-365535 alice
```

**The lock screen refuses the right password.** `kdos-checkpass` has lost its setuid bit, or the
account is locked or has no password, which it refuses outright. `kdos doctor` checks the bit and
prints the repair:

```bash
sudo chown root /usr/bin/kdos-checkpass
sudo chmod 4755 /usr/bin/kdos-checkpass
```

Until then, log in on `tty2`; see
[kdos-checkpass](../../kdos/03-architecture/security-model.md#kdos-checkpass).

**A device answers `Permission denied`.** The account lacks the group that owns it. `ls -l` on
the device node shows the group. For serial adapters, cameras and instruments, `kdos doctor`, run
as that account, lists the nodes it cannot open with their groups in its Hardware section. Add the
group as [above](#adding-an-account-to-a-group).

**`userdel` prints `user alice is currently used by process <pid>` and exits 8.** A process of the
account is still running, such as a shell or a container's pause process, and `userdel` removes
nothing. End them and try again:

```bash
sudo pkill -u alice
sudo userdel -r alice
```

`userdel: alice mail spool (/var/spool/mail/alice) not found` after it is the missing mail
directory again, and the account is gone.

**`kdos-power autologin` prints `err no such account`.** The name is not an account that can log
in: a service account with `/sbin/nologin` is refused.

**`sudo` says the account is not in the sudoers file.** It is not in `wheel`; see
[sudo, PAM and polkit](sudo-pam-polkit.md).

**`tty1` asks for a password on an installed machine.** That is the installed default. Set an
account with `kdos-power autologin <user>` if the machine should log in by itself.

## Known gaps

- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- [Security](../../kdos/06-reference/known-gaps.md#security): the live medium's shared password, its automatic logins, and `wheel` being effectively root
- [A fingerprint unlocks `sudo` and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else)

## See also

- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): what each group grants
- [The session on tty1](../../kdos/02-user-guide/administration.md#the-session-on-tty1): `login.conf` and the automatic login
- [System accounts](../../kdos/03-architecture/security-model.md#system-accounts): the service accounts and how ports add them
- [autologin](../../kdos/04-programs/daemons.md#autologin): what the verb checks and writes
- [6. Accounts](../../kdos/02-user-guide/installation.md#6-accounts): what the installer asks and rewrites
- [sudo, PAM and polkit](sudo-pam-polkit.md): what `wheel` membership lets an account do
- [Login and autologin](login-and-autologin.md): the login path on `tty1`
- [Screen lock](screen-lock.md): the lock screen and `kdos-checkpass`
- [Live persistence](live-persistence.md): keeping accounts on the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 368.* Previous: [367. Locale and console](locale-and-console.md) · [Contents](../README.md) · Next: [369. Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)

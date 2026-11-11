# Users and groups

This page is about the accounts on a KDOS machine: the one that ships, how to add, change and remove
others, which groups an account needs for the hardware, and which account the machine logs in by
itself. What the [wheel]{.idx} group grants, and why each hardware group exists, is in
the KDOS book's [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups)
section, which this page does not repeat.

## Installation

| What | Port |
|---|---|
| [`useradd`]{.idx}, `usermod`, `userdel`, `groupadd`, `gpasswd`, [`passwd`]{.idx}, [`chsh`]{.idx}, `su`, `newuidmap`, `newgidmap` | [shadow](#port-shadow) |
| `agetty`, which runs the login prompt on each console | [util-linux](#port-util-linux) |
| [`login`]{.idx}, built against PAM | [util-linux-login](#port-util-linux-login) |
| The **Users** page of System Settings, and the account pictures on the greeter | [plasma-desktop](#port-plasma-desktop), [accountsservice](#port-accountsservice) |
| The automatic login, on the **Login Screen (SDDM)** page | [sddm-kcm](#port-sddm-kcm) |
| `unix_chkpwd`, the [lock screen]{.idx}'s [password]{.idx} check for a process that is not root | [pam](#port-pam) |

shadow is built without PAM, so `su` and `passwd` read [`/etc/shadow`]{.idx} themselves; the
port's page says why. Every login, the greeter's, the consoles' and the lock screen's, goes
through PAM; see [sudo, PAM and polkit](sudo-pam-polkit.md). The account files themselves are not from a port: [`/etc/passwd`]{.idx},
[`/etc/group`]{.idx}, `/etc/shadow`, [`/etc/subuid`]{.idx}, [`/etc/subgid`]{.idx} and `/etc/skel` come from the tree's
`fs/` overlay, and ports that need a service account add it from their `postinstall.sh`
([System accounts](../../kdos/03-architecture/security-model.md#system-accounts)).

An account's runtime directory, `/run/user/<uid>`, is made by turnstile's PAM module at the
account's first login and removed after its last; see
[Session and environment](session-and-environment.md#the-per-user-services).

On the live medium the desktop account is [`kdos`]{.idx term="kdos (account)"} with the password `kdos`, and `root` has the same
password. The installer asks for the account's name and password, renames `kdos` to the name given,
and locks `root` unless told otherwise; see
[6. Accounts](../../kdos/02-user-guide/installation.md#6-accounts).

## Configuration

**`/etc/passwd`**, from `fs/etc/passwd`. Every account but `root` and `kdos` is a service account
with `/sbin/nologin`:

<!-- kfs:include fs/etc/passwd -->

**`/etc/group`**, from `fs/etc/group`. The last field lists the members; `kdos` is in `wheel`
and the hardware groups. `sambashare` (gid 65) is the group whose members may publish a folder
as a share; see [File sharing](file-sharing.md):

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

**`/etc/login.defs`** is shadow's own file. It sets the ranges `useradd` picks ids from and the
hash `passwd` writes; its `ENCRYPT_METHOD` line is commented out, so passwords are hashed with
SHA-512, and the port comments out its `MAIL_DIR` line, so no account keeps a mailbox. See the
[shadow](#port-shadow) page.

**`/etc/default/useradd`**, from `fs/etc/default/useradd`, holds the defaults `useradd` gives a
new account. It sets two: `GROUPS=audio`, so every ordinary account joins the group that carries
PipeWire's real-time limits ([Audio](audio.md)), whichever tool makes it — System Settings' Users
page through AccountsService, Plasma Setup, or `useradd` by hand — and `CREATE_MAIL_SPOOL=no`.
`useradd -G` replaces the `GROUPS=` list rather than adding to it, so a list given by hand names
`audio` again, and a system account (`useradd -r`) never gets it; AccountsService is patched to add
an administrator's `wheel` after `useradd` for the same reason (see
[accountsservice](#port-accountsservice)). Every other key keeps `useradd`'s compiled default, and
`useradd -D` prints them all:

<!-- kfs:include fs/etc/default/useradd -->

**`/etc/login.defs`**, from [shadow](#port-shadow), sets `HOME_MODE 0700`, so the home `useradd -m`
creates is readable by its owner alone, as the installer makes the first account's. `UMASK` stays
`022`.

**`/etc/skel`**, from `fs/etc/skel`, is copied into each new home by `useradd -m`: the shell
profiles, the pinned applications and the terminal programs' starting configuration. Plasma's own
settings are not there: a new account reads the KDOS defaults in `/usr/share/kdos/xdg` until it
changes something.

**`/etc/sddm.conf.d/kde_settings.conf`** names the account the machine logs in by itself, if any;
see [Login and autologin](login-and-autologin.md#etcsddmconfd).

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
       -G wheel,users,audio,video,render,input,kvm,cdrom,dialout,lpadmin,tty alice
   ```

   For an account that runs the desktop but does not administer the machine, leave `wheel` out of
   the list. No group is needed for the desktop itself: elogind hands the display and the input
   devices to whoever's session is active, and polkit lets that person suspend, power off and
   mount removable media. System Settings → **Users** → **Add New** makes an account in `audio`
   alone, plus `wheel` for the account type **Administrator**; add the hardware groups to it
   afterwards with `sudo usermod -a -G <groups> alice`.

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

4. Make the empty directories the desktop's programs expect. `useradd -m` copies `/etc/skel`,
   which carries only `Desktop` of these; the image build makes the rest in the `kdos` home alone.
   The mail programs need `~/Mail`, and the calendar and address book need their stores under
   `~/.local/share`; none of them creates its directory:

   ```bash
   sudo -u alice mkdir -p /home/alice/{Desktop,Downloads,Documents,Music,Videos,Public,Templates} \
       /home/alice/{Mail,Pictures/Screenshots,.local/bin,.local/share/calendars/personal,.local/share/addressbooks/personal}
   ```

5. Log in as the account at the greeter, which lists it at once. Its runtime directory and
   services start with that first login.

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

### Choosing the account the machine logs in by itself

System Settings → **Login Screen (SDDM)** → **Behavior** → **Automatically log in**, with the
account and **Plasma (Wayland)**; see
[Turning autologin on or off](login-and-autologin.md#turning-autologin-on-or-off).

### Locking and removing an account

1. Lock it, so no password opens it, and unlock it again:

   ```bash
   sudo usermod -L alice
   sudo usermod -U alice
   ```

2. Remove it with its home directory. If the automatic login names it, turn that off first.
   System Settings → **Users** → **Delete User** does the same, and asks whether to keep the
   files. End its processes before `userdel`: its user services, `ssh-agent` among them, run
   until its last logout, and `userdel` refuses an account with a running process (`user alice is
   currently used by process <pid>`, exit 8):

   ```bash
   sudo pkill -u alice
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

The shell must be listed in `/etc/shells` and installed. The session wrapper starts Plasma from a
login shell of whichever of bash or zsh it is, so either one's profile reaches the desktop.

### On the live medium

Every change on this page is to `/etc` or `/home`, and on the live medium both are in RAM and start
again from the image at every boot. A persistence store keeps them
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

## Tips

- `tty2` is a plain login prompt for any account, whatever the automatic login says: the way in
  when the desktop account's session will not start. `Ctrl+Alt+F2` reaches it from the desktop.
- `newgrp dialout` starts a shell with a group just added, without logging out.
- The ranges in `/etc/subuid` and `/etc/subgid` must not overlap. Each line is the account, the
  first id and the number of ids; the shipped ranges are in the file shown under
  [Configuration](#configuration).

## Troubleshooting

**A new account's desktop does not start, or its programs complain about `XDG_RUNTIME_DIR`.** Its
runtime directory is missing, which means turnstile did not run for its login:

```bash
ls -ld /run/user/$(id -u alice)
service status turnstiled
```

**Boxed applications do not start for one account.** `kdos doctor`, run as that account, warns
`no /etc/subuid entry` in its Containers section. Give it a range that overlaps no other line of
`/etc/subuid` or `/etc/subgid`: read both files, and start past the end (first id plus count) of
the highest range in either. With only the shipped lines, that is 265536:

```bash
cat /etc/subuid /etc/subgid
sudo usermod --add-subuids 265536-331071 --add-subgids 265536-331071 alice
```

**The lock screen refuses the right password.** `unix_chkpwd` has lost its setuid bit, or the
account is locked; see [Screen lock](screen-lock.md#troubleshooting).

**A device answers `Permission denied`.** The account lacks the group that owns it. `ls -l` on
the device node shows the group. For serial adapters, cameras and instruments, `kdos doctor`, run
as that account, lists the nodes it cannot open with their groups in its Hardware section. Add the
group as [above](#adding-an-account-to-a-group).

**`userdel` prints `user alice is currently used by process <pid>` and exits 8.** A process of the
account is still running, such as a user service of a login that has not ended, a shell or a
lingering account's container, and `userdel` removes nothing. End them and try again:

```bash
sudo pkill -u alice
sudo userdel -r alice
```

**`sudo` says the account is not in the sudoers file.** It is not in `wheel`; see
[sudo, PAM and polkit](sudo-pam-polkit.md).

**The greeter asks for a password on an installed machine.** That is the installed default. Set
the automatic login if the machine should log in by itself.

## Known gaps

- [Security](../../kdos/06-reference/known-gaps.md#security): the live medium's shared password, its automatic logins, and `wheel` being effectively root

## See also

- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): what each group grants
- [System accounts](../../kdos/03-architecture/security-model.md#system-accounts): the service accounts and how ports add them
- [6. Accounts](../../kdos/02-user-guide/installation.md#6-accounts): what the installer asks and rewrites
- [sudo, PAM and polkit](sudo-pam-polkit.md): what `wheel` membership lets an account do
- [Login and autologin](login-and-autologin.md): the greeter and the automatic login
- [Screen lock](screen-lock.md): the lock screen and its PAM stack
- [Live persistence](live-persistence.md): keeping accounts on the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 392.* Previous: [391. Locale and console](locale-and-console.md) · [Contents](../README.md) · Next: [393. Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)

# sudo, PAM and polkit

An administrator on KDOS is an account in the [`wheel`]{.idx} group, and three pieces of software turn
that membership into rights: [`sudo`]{.idx} runs a command as root after asking for the account's own
[password]{.idx}, [PAM]{.idx} is the library every login, the lock screen and `sudo` check a password through, and
[polkit]{.idx} answers the system daemons that ask whether a user may perform an action, asking for
a password through Plasma's authentication agent where an action's policy says so. This page shows
the files of all three, how to give an account rights and check what it has, how to add a rule of
your own, and what to look at when a request is refused. Who polkit answers and how is in the KDOS
book's
[polkit and the authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-the-authentication-agent).

## Installation

| What | Port |
|---|---|
| `sudo`, [`sudoedit`]{.idx}, [`visudo`]{.idx} | [sudo](#port-sudo) |
| `libpam`, `pam_unix` and the other modules, the shared PAM stacks | [pam](#port-pam) |
| `polkitd`, [`pkexec`]{.idx}, [`pkaction`]{.idx}, [`pkcheck`]{.idx} | [polkit](#port-polkit), built with elogind as its session tracker |
| The authentication agent, the password dialog for polkit in a Plasma session | [polkit-kde-agent-1](#port-polkit-kde-agent-1) |
| `pam_elogind` and `pam_turnstile`, which register a login as a session and start the user's services | [elogind](#port-elogind), [turnstile](#port-turnstile) |
| `pam_fprintd`, the fingerprint module in `sudo`'s stack | [fprintd](#port-fprintd), over [libfprint](#port-libfprint) |

All of them are on the image. `polkitd` is started by [`/etc/init.d/41_polkitd.sh`]{.idx},
from the tree's `fs/` overlay, after the system bus and before NetworkManager; the image ships no
`polkitd` marker in `/etc/service.disabled/`, so it starts on the live medium and on an installed
system. The account it runs as, `polkitd`, is made by the polkit port's `postinstall.sh`:

<!-- kfs:include ports/core/auth/polkit/postinstall.sh lines=19-22 pin=c3d07764dbdf -->

Not every password check goes through PAM. These do, each with the PAM service file named:

| Program | [`/etc/pam.d/`]{.idx term="/etc/pam.d"} file | From |
|---|---|---|
| `sudo`, `sudoedit` | `sudo` | the sudo port |
| polkit's password helper, `polkit-agent-helper-1`, and `pkexec` | `polkit-1` | the polkit port |
| `vlock` | `vlock` | the [kbd](#port-kbd) port |
| CUPS | `cups` | the [cups](#port-cups) port |
| The console `login` on `tty2` and the serial line | `login` | the [util-linux-login](#port-util-linux-login) port |
| SDDM: a password login, an automatic login and the greeter's own session | `sddm`, `sddm-autologin`, `sddm-greeter` | the [sddm](#port-sddm) port |
| The lock screen: a password, and a fingerprint | `kde`, `kde-fingerprint` | the [kscreenlocker](#port-kscreenlocker) port |
| turnstile's own session for the user's services | `turnstiled` | the [turnstile](#port-turnstile) port |

PostgreSQL is built with PAM as well, and uses it only for a `pg_hba.conf` line whose method is
`pam`; it has no file in `/etc/pam.d`, so `other` answers for it and refuses.

`su` and `passwd` are shadow's, built without PAM, and read `/etc/shadow` themselves. A process
that is not root, such as the lock screen, checks a password through `pam_unix`, which asks the
setuid helper `unix_chkpwd`. See [Users and groups](users-and-groups.md).

## Configuration

### sudo

**[`/etc/sudoers`]{.idx}** is the file `sudo`'s own install writes, unchanged; its last line reads every
file in [`/etc/sudoers.d`]{.idx}. **`/etc/sudoers.d/00-sudo`** is the KDOS rule, mode 440, written by
`ports/core/auth/sudo/build.sh`:

<!-- kfs:include ports/core/auth/sudo/build.sh lines=57-62 pin=2c5a24852ff3 -->

`secure_path` is the `PATH` every command run through `sudo` gets. `env_keep` carries the three
editor variables through, so `sudoedit` and `visudo` open the editor the shell names, with `nvim`
and then `nano` as the build's fallbacks. The last line lets every member of `wheel` run any
command as any user after typing their own password. The build's configure switches, among them
the password prompt `[sudo] password for <user>:`, are on the [sudo](#port-sudo) page.

**[`/etc/pam.d/sudo`]{.idx}**, from the same `build.sh`. A fingerprint is tried first, when `fprintd` is
installed and the account has one enrolled; the password follows:

<!-- kfs:include ports/core/auth/sudo/build.sh lines=34-56 pin=3d042633c00e -->

### PAM

**[`/etc/pam.d/system-auth`]{.idx}**, **`system-account`**, **`system-session`**, **`system-password`** and
**`system-login`** and **`other`** are written by `ports/core/auth/pam/build.sh`. The first four are
the shared stacks the service files include; `system-login` is the stack of every login, and adds
`pam_turnstile` and `pam_elogind` to the session; `other` is used by any program that has no file
of its own, and refuses everything:

<!-- kfs:include ports/core/auth/pam/build.sh lines=34-87 pin=dee44e2e5aad -->

[`/etc/pam.d/polkit-1`]{.idx} is installed by polkit's build and includes the four shared stacks.

### polkit

polkit asks elogind about the caller: a process in the active local session at the machine is
`active`, and an action's `allow_active` answer applies to it, so the person at the machine
suspends, joins a network, mounts a stick or sets the time with no password. Where an action asks
for an administrator, Plasma's agent shows the dialog, and the password of any member of `wheel`
answers it.

**`/etc/polkit-1/rules.d/50-kdos.rules`**, from `fs/etc/polkit-1/rules.d/50-kdos.rules`, is what
KDOS changes. It refuses a few actions this system does not do, holds a few at a password for
everyone, and grants a member of `wheel` at the machine a short list without one; for anything
else it returns nothing, and the action's own policy decides. The build sets the file and its
directory to root and mode 644 and 755; see
[The mode of `/etc/shadow`](../../kdos/03-architecture/security-model.md#the-mode-of-etcshadow).

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules -->

Ports install rules of their own in `/usr/share/polkit-1/rules.d`, and polkitd reads that
directory and `/etc/polkit-1/rules.d` as one list, in order of file name. polkit's own
`50-default.rules` names `wheel` as the administrators, whose password answers an
`auth_admin` challenge. `50-libvirt.rules` grants libvirt's group, which no account is in until
one is added to it. The rules of fwupd, bolt and gvfs grant `wheel` in an active local session.

**`/etc/init.d/41_polkitd.sh`**, from `fs/etc/init.d/41_polkitd.sh`:

<!-- kfs:include fs/etc/init.d/41_polkitd.sh -->

## Usage

### Running a command as root

1. Run one command:

   ```bash
   sudo <command>
   ```

2. Edit a root-owned file in your own editor, as yourself, with the result copied back as root:

   ```bash
   sudoedit /etc/chrony/chrony.conf
   ```

3. See what `sudo` lets the account run:

   ```bash
   sudo -l
   ```

   For a member of `wheel` the list ends with the one rule from `00-sudo`:

   ```text
   User kdos may run the following commands on kdos:
       (ALL) ALL
   ```

`sudo` prints only the prompt, the first run included: it is built `--without-lecture`, because
the lecture in front of the prompt is what [KDESu](#port-kdesu)'s password exchange cannot get
past.

### Making an account an administrator

1. Add it to `wheel`:

   ```bash
   sudo usermod -aG wheel alice
   ```

2. The account logs out and in again; its next `sudo` asks for its own password. A new login,
   including `su - alice`, has the group at once. Other groups are added the same way; see
   [Adding an account to a group](users-and-groups.md#adding-an-account-to-a-group).

Taking it out again is `sudo gpasswd -d alice wheel`, which prints
`Removing user alice from group wheel`. Membership of `wheel` also opens every polkit
action in `50-kdos.rules` and the configuration verbs of the KDOS root daemons; see
[Who may ask for what](../../kdos/04-programs/daemons.md#who-may-ask-for-what).

### Adding a sudo rule of your own

Put it in a file of its own: `00-sudo` belongs to the sudo package, and an upgrade of the package
installs it again over an edited copy.

1. Create the file with `visudo`, which refuses to save a file `sudo` could not parse:

   ```bash
   sudo visudo -f /etc/sudoers.d/50-local
   ```

2. Write the rule. For example, to let `alice`, who is not in `wheel`, restart chrony and nothing
   else:

   ```text
   alice ALL=(root) /usr/sbin/service restart chrony
   ```

3. Check every sudoers file at once:

   ```bash
   sudo visudo -c
   ```

   It prints one `parsed OK` line per file, `/etc/sudoers`, `/etc/sudoers.d/00-sudo` and the new
   one.

4. Check from `alice`'s session with `sudo -l`. It ends:

   ```text
   User alice may run the following commands on kdos:
       (root) /usr/sbin/service restart chrony
   ```

`/usr/sbin/service` is the path to name: it is a link to `ksvc`, and the rule matches the
command by that path.

The build includes every set of insults; a `Defaults insults` line in such a file turns them on:
a wrong password is then answered with a line such as `Do you think like you type?` in place
of `Sorry, try again.`

### Unlocking sudo with a fingerprint

On a machine with a reader that libfprint drives; fprintd's own configuration is on
[Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md). On one without, every
`fprintd-*` command
answers `No devices available`, and `fprintd-enroll` prints it as
`Impossible to enroll: GDBus.Error:net.reactivated.Fprint.Error.NoSuchDevice: No devices available`.

1. Enrol a finger, from the desktop on System Settings → **Users** → **Configure Fingerprint
   Authentication**, or from Konsole in the session:

   ```bash
   fprintd-enroll
   ```

2. Check it:

   ```bash
   fprintd-verify
   ```

3. The next `sudo` waits for the finger, and falls back to the password when the finger does not
   match.

`fprintd-delete $USER` removes the enrolled prints. The lock screen accepts the finger as well,
through its `kde-fingerprint` stack.

### Checking what polkit grants

1. Read an action's own policy: its `implicit any`, `implicit inactive` and `implicit active`
   answers, for a caller outside any session, in a session switched away from, and in the active
   session at the machine:

   ```bash
   pkaction --action-id org.freedesktop.NetworkManager.network-control --verbose
   ```

   `pkaction` with no arguments lists every action id installed, about 240 of them on the image.

2. Ask polkit whether the current shell's process would be allowed:

   ```bash
   pkcheck --action-id org.freedesktop.NetworkManager.network-control --process $$; echo $?
   ```

   `0` is allowed, `1` refused, `2` a password challenge. `--allow-user-interaction` lets
   `pkcheck` answer the challenge through the session's agent. Run from Konsole in the session, an
   `allow_active` action answers `0`; run over SSH, the same action prints the policy's answer for
   a caller outside the session and `2`.

### Adding a polkit rule of your own

1. Find the action id the program asks for in the list `pkaction` prints. polkitd logs no allow or
   deny decision at any log level, so the system log does not name the action.

2. Write the rule in a new file that sorts after `50-kdos.rules`, so the shipped list keeps its
   place. Replace `org.example.action` with the id:

   ```bash
   sudo tee /etc/polkit-1/rules.d/60-local.rules > /dev/null << 'EOF'
   polkit.addRule(function (action, subject) {
       if (action.id == "org.example.action" && subject.isInGroup("wheel"))
           return polkit.Result.YES;
   });
   EOF
   ```

3. Check that the file is owned by root and writable by nobody else; polkitd runs every rule it
   finds without looking at the owner:

   ```bash
   ls -l /etc/polkit-1/rules.d/
   ```

4. Test it with `pkcheck`, as above. polkitd watches the directory and reads every rule again when
   a file changes, so no restart is needed: a rule granting
   `org.freedesktop.NetworkManager.settings.modify.hostname` to `wheel` turns that action's
   `auth_admin_keep` into `yes` within a few seconds of the file being written.

polkitd calls the rules in the order of the file names and stops at the first that returns a
result. A grant to `wheel` gives no more than `wheel` already has through `sudo`; a grant to any
other group is a new right.

## Tips

- `sudo -k` forgets the cached password at once, so the next `sudo` asks again.
- `sudo` writes every command it runs to [`/var/log/auth.log`]{.idx}, with the account, the working
  directory, the target user and the command, as in
  `sudo:     kdos : PWD=/home/kdos ; USER=root ; COMMAND=/usr/bin/true`. A refusal is logged on the
  same kind of line, with the reason before `PWD`.
- Over SSH no agent answers, so an action that asks for a password is refused there; run the
  command with `sudo`, as root, whom polkit allows everything. The KDOS book lists the answers
  under [What `50-kdos.rules` changes](../../kdos/03-architecture/security-model.md#what-50-kdosrules-changes).
- `pkexec <command>` runs a command as root after the agent asks for an administrator's
  password, as `sudo` does in a terminal.
- polkitd logs no allow or deny decision. A rule of your own can record one with `polkit.log()`,
  which writes a line to `/var/log/auth.log` tagged `polkitd`, once. Put the rule in a file of its
  own sorting after `50-kdos.rules`, such as `/etc/polkit-1/rules.d/60-log.rules`; polkitd reads
  it as soon as it is written:

  ```javascript
  polkit.addRule(function (action, subject) {
      if (action.id.indexOf("org.freedesktop.NetworkManager.") == 0 && !subject.isInGroup("wheel"))
          polkit.log("not granted: " + action.id + " for " + subject.user);
  });
  ```

  The rule returns no result, so the decision is still the one the other rules and the action's
  policy make; `sudo grep 'not granted' /var/log/auth.log` lists the lines.

## Troubleshooting

**`sudo` says `alice is not in the sudoers file.`** The account is not in `wheel`, or was added in
this session. `id alice` shows the groups it will have at its next login.

**`sudo` refuses with a syntax error, for every account.** A file in `/etc/sudoers.d` does not
parse. Run `sudo visudo -c` while some way to root still works; it names the file and the line.
With `sudo` broken and `root` locked, which is what the installer does by default, there is no
root shell on the running system, and the file has to be repaired from the live medium with the
installed root mounted; see
[Repairing from the live medium](recovering-an-unbootable-system.md#repairing-from-the-live-medium). Single-user mode needs a root password; see
[Single-user and emergency mode](../../kdos/03-architecture/boot-and-init.md#single-user-and-emergency-mode).

**`sudo` over SSH pauses before it asks for the password.** The account has a fingerprint
enrolled, and `pam_fprintd` is waiting on the machine's reader until it times out; see
[Known gaps](#known-gaps). `sudo fprintd-delete <user>` removes the prints.

**The Wi-Fi switch, or connecting to a network, does nothing.** polkitd is not answering
NetworkManager:

```bash
service status polkitd
cat /run/kdos-init.polkitd.log
```

`[SKIP] polkitd: no polkitd account (the port's postinstall makes it)` means the account the port's `postinstall.sh` makes is
missing. Make it with the same two commands, then start the daemon:

```bash
sudo groupadd -r polkitd
sudo useradd -r -g polkitd -d /nonexistent -s /sbin/nologin polkitd
sudo service start polkitd
```

**A program says it is not authorised, and the account is in `wheel`.** Run `pkcheck` for the
action, as [above](#checking-what-polkit-grants). `1` means the policy refuses it; `2` that it asks
for a password and no agent answered, as outside the session. Check that the agent runs in the
session, `pgrep -a polkit-kde-auth`, and that the session is active: `loginctl show-session
$XDG_SESSION_ID -p Active`. polkitd writes an error in a rules file, and every `polkit.log()` line, to
`/var/log/auth.log`, tagged `polkitd`:

```bash
sudo grep polkitd /var/log/auth.log
```

```text
polkitd[880]: Error compiling script /etc/polkit-1/rules.d/61-bad.rules: SyntaxError: unterminated statement (line 1)
```


## Known gaps

- [Security](../../kdos/06-reference/known-gaps.md#security): membership of `wheel` is effectively root

## See also

- [polkit and the authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-the-authentication-agent): who asks polkit, what is granted and what is not
- [How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started): why `polkitd` has a service script
- [setuid binaries](../../kdos/03-architecture/security-model.md#setuid-binaries): `sudo`, `unix_chkpwd` and the rest
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): the `sudo` grant and what `wheel` means
- [Users and groups](users-and-groups.md): adding accounts and groups
- [Time and NTP](time-and-ntp.md): an example of a package-owned file edited with `sudoedit`
- [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md): enrolling a finger and the other devices
- [Recovering an unbootable system](recovering-an-unbootable-system.md): repairing an installed root from the live medium
- [Logs](logs.md): `/var/log/auth.log` and the rest

<!-- book-nav -->
---

*Part XVI. System administration, chapter 404.* Previous: [403. Backups](backups.md) · [Contents](../README.md) · Next: [405. Login and autologin](login-and-autologin.md)

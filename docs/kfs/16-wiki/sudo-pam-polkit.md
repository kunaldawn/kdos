# sudo, PAM and polkit

An administrator on KDOS is an account in the [`wheel`]{.idx} group, and three pieces of software turn
that membership into rights: [`sudo`]{.idx} runs a command as root after asking for the account's own
[password]{.idx}, [PAM]{.idx} is the library `sudo` and a few other programs check that password through, and
[polkit]{.idx} answers the system daemons that ask whether a user may perform an action. This page
shows the files of all three, how to give an account rights and check what it has, how to add a
rule of your own, and what to look at when a request is refused. Why polkit here grants a set
list of actions with no password prompt is in the KDOS book's
[polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent).

## Installation

| What | Port |
|---|---|
| `sudo`, [`sudoedit`]{.idx}, [`visudo`]{.idx} | [sudo](#port-sudo) |
| `libpam`, `pam_unix` and the other modules, the shared PAM stacks | [pam](#port-pam) |
| `polkitd`, [`pkexec`]{.idx}, [`pkaction`]{.idx}, [`pkcheck`]{.idx} | [polkit](#port-polkit) |
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
| polkit's password helper, `polkit-agent-helper-1` | `polkit-1` | the polkit port |
| `vlock` | `vlock` | the [kbd](#port-kbd) port |
| CUPS | `cups` | the [cups](#port-cups) port |
| [`wayvnc`]{.idx} | `wayvnc` | the [wayvnc](#port-wayvnc) port |

PostgreSQL is built with PAM as well, and uses it only for a `pg_hba.conf` line whose method is
`pam`; it has no file in `/etc/pam.d`, so `other` answers for it and refuses.

The console `login`, `su` and `passwd` are shadow's, built without PAM, and the lock screen checks
the password with `kdos-checkpass`; neither reads `/etc/pam.d`. See
[Users and groups](users-and-groups.md).

## Configuration

### sudo

**[`/etc/sudoers`]{.idx}** is the file `sudo`'s own install writes, unchanged; its last line reads every
file in [`/etc/sudoers.d`]{.idx}. **`/etc/sudoers.d/00-sudo`** is the KDOS rule, mode 440, written by
`ports/core/auth/sudo/build.sh`:

<!-- kfs:include ports/core/auth/sudo/build.sh lines=56-61 pin=2c5a24852ff3 -->

`secure_path` is the `PATH` every command run through `sudo` gets. `env_keep` carries the three
editor variables through, so `sudoedit` and `visudo` open the editor the shell names, with `nvim`
and then `nano` as the build's fallbacks. The last line lets every member of `wheel` run any
command as any user after typing their own password. The build's configure switches, among them
the password prompt `[sudo] password for <user>:`, are on the [sudo](#port-sudo) page.

**[`/etc/pam.d/sudo`]{.idx}**, from the same `build.sh`. A fingerprint is tried first, when `fprintd` is
installed and the account has one enrolled; the password follows:

<!-- kfs:include ports/core/auth/sudo/build.sh lines=33-55 pin=3d042633c00e -->

### PAM

**[`/etc/pam.d/system-auth`]{.idx}**, **`system-account`**, **`system-session`**, **`system-password`** and
**`other`** are written by `ports/core/auth/pam/build.sh`. The first four are the shared stacks the
service files include; `other` is used by any program that has no file of its own, and refuses
everything:

<!-- kfs:include ports/core/auth/pam/build.sh lines=34-76 pin=fea3f258648f -->

[`/etc/pam.d/polkit-1`]{.idx} is installed by polkit's build and includes the four shared stacks.

### polkit

**`/etc/polkit-1/rules.d/50-kdos.rules`**, from `fs/etc/polkit-1/rules.d/50-kdos.rules`, is the
whole of what KDOS grants. It answers for members of `wheel` only, and only for the action ids it
names; for anything else it returns nothing, and the action's own policy decides. The build sets
the file and its directory to root and mode 644 and 755; see
[The mode of `/etc/shadow`](../../kdos/03-architecture/security-model.md#the-mode-of-etcshadow).

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules -->

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

The first `sudo` an account runs prints sudo's lecture ("We trust you have received the usual
lecture…") before the prompt; later runs print only the prompt.

### Making an account an administrator

1. Add it to `wheel`:

   ```bash
   sudo usermod -aG wheel alice
   ```

2. The account logs out and in again; its next `sudo` asks for its own password. A new login,
   including `su - alice`, has the group at once.

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

On a machine with a reader that libfprint drives. On one without, every `fprintd-*` command
answers `No devices available`, and `fprintd-enroll` prints it as
`Impossible to enroll: GDBus.Error:net.reactivated.Fprint.Error.NoSuchDevice: No devices available`.

1. Enrol a finger for the account, as root, because fprintd's polkit actions allow enrolment only
   to an active session, which no process here is in:

   ```bash
   sudo fprintd-enroll <user>
   ```

2. Check it:

   ```bash
   sudo fprintd-verify <user>
   ```

3. The next `sudo` waits for the finger, and falls back to the password when the finger does not
   match.

`sudo fprintd-delete <user>` removes the enrolled prints. Nothing else on the machine asks for a
finger ([Known gaps](#known-gaps)).

### Checking what polkit grants

1. Read an action's own policy, including its `implicit any` answer, which is the one that applies
   to every caller here:

   ```bash
   pkaction --action-id org.freedesktop.NetworkManager.network-control --verbose
   ```

   `pkaction` with no arguments lists every action id installed, about 240 of them on the image.

2. Ask polkit whether the current shell's process would be allowed:

   ```bash
   pkcheck --action-id org.freedesktop.NetworkManager.network-control --process $$; echo $?
   ```

   `0` is allowed, `1` refused. `2` means the answer was a password challenge, which nothing on
   this machine can answer, so it is a refusal too. For a member of `wheel` this action prints
   `polkit\56result=yes` and `0`. For an account outside `wheel`, or for an action
   `50-kdos.rules` does not name, it prints the policy's answer and the reason, then `2`:

   ```text
   polkit\56result=auth_admin
   Authorization requires authentication and -u wasn't passed.
   ```

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
- An action left out of `50-kdos.rules`, such as changing the hostname through NetworkManager or a
  firmware update through fwupd, is done with `sudo`, as root, whom polkit allows everything; the
  KDOS book lists them under
  [What is granted instead](../../kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules).
- `50-kdos.rules` does not grant `org.freedesktop.policykit.exec`, the action `pkexec` asks for
  to run an arbitrary command. Use `sudo`.
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

`[SKIP] polkitd: no polkitd account` means the account the port's `postinstall.sh` makes is
missing. Make it with the same two commands, then start the daemon:

```bash
sudo groupadd -r polkitd
sudo useradd -r -g polkitd -d /nonexistent -s /sbin/nologin polkitd
sudo service start polkitd
```

**A program says it is not authorised, and the account is in `wheel`.** Run `pkcheck` for the
action, as [above](#checking-what-polkit-grants). `1` or `2` means the action is not in
`50-kdos.rules` and its own policy refuses it or asks for a password: do it with `sudo`, or add a
rule. polkitd writes an error in a rules file, and every `polkit.log()` line, to
`/var/log/auth.log`, tagged `polkitd`:

```bash
sudo grep polkitd /var/log/auth.log
```

```text
polkitd[880]: Error compiling script /etc/polkit-1/rules.d/61-bad.rules: SyntaxError: unterminated statement (line 1)
```

Each start of polkitd also writes
`polkitd: Error loading /var/run/ConsoleKit/database: ... No such file or directory`. That one is
expected: there is no ConsoleKit here, which is why no subject is ever in an active session.

## Known gaps

- [A fingerprint unlocks `sudo` and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else)
- [Security](../../kdos/06-reference/known-gaps.md#security): membership of `wheel` is effectively root

## See also

- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent): who asks polkit, what is granted and what is not
- [How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started): why `polkitd` has a service script
- [setuid binaries](../../kdos/03-architecture/security-model.md#setuid-binaries): `sudo`, `unix_chkpwd` and the rest
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): the `sudo` grant and what `wheel` and `seat` mean
- [Users and groups](users-and-groups.md): adding accounts and groups
- [Time and NTP](time-and-ntp.md): an example of a package-owned file edited with `sudoedit`
- [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md): enrolling a finger and the other devices
- [Recovering an unbootable system](recovering-an-unbootable-system.md): repairing an installed root from the live medium
- [Logs](logs.md): `/var/log/auth.log` and the rest

<!-- book-nav -->
---

*Part XVI. System administration, chapter 380.* Previous: [379. Backups](backups.md) · [Contents](../README.md) · Next: [381. Login and autologin](login-and-autologin.md)

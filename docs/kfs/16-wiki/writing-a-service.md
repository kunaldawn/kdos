# Writing a service

This page adds a service of your own: a script in [`/etc/init.d`]{.idx} that `rcS` starts at boot and that
`service` manages, with its daemon kept alive by `ksvc`. It covers the two places such a script can
live. On one running machine, you write it into `/etc/init.d` by hand. In the KDOS tree, it goes
into `fs/etc/init.d/` when it belongs to the base system, or into a port's `build.sh` when it
belongs to that port's package. Managing a service once it exists is on [Services](services.md).

## Installation

Nothing extra is needed. The [supervisor]{.idx term="supervisor (ksvc)"} is `ksvc`, from [kdos-tools](#port-kdos-tools), and
`/etc/init.d/service_helper`, from the base filesystem, gives a script the three functions it
calls. A daemon from a package you build yourself is installed with
`kpkg` like any other port; see [kpkg](kpkg.md) and [Anatomy of a kpkg install](../04-chroot/kpkg-install.md).

A script added by hand to a running installed system belongs to no package: `kdos why` reports
that no package owns it, and no package upgrade replaces or removes it. On the live medium it is kept only when a
persistence store exists (`kdos persist`).

## Configuration

### The contract

`rcS`, `rcK` and `service` agree on what a service script is. Each rule below is read from the
code that enforces it:

| Rule | Enforced by | When broken |
|:--|:--|:--|
| The file is `/etc/init.d/NN_<name>.sh`: two digits, an underscore, the name, `.sh` | `rcS`, `rcK`, `ksvc` | The script is never run and `service` does not find it |
| The file is executable | `rcS`, `rcK`, `service list` | The script is skipped without a word, and `service start` fails without a message |
| It takes `start`, `stop` and `status`, and prints a usage line and exits 1 for anything else | the shipped scripts; `service` passes the verb through | `service` commands do nothing useful |
| `start` exits 0 when it started the service or deliberately skipped it, non-zero only on a failure | `rcS` | The splash marks the stage failed and shows the last six lines of output |
| A skip prints `[SKIP] <name>: <reason>` and exits 0 | the shipped scripts | The boot log says nothing about why the service is down |
| A daemon runs in the foreground under `supervise` | `ksvc` | The supervisor sees the parent exit as soon as the daemon forks, and starts it again every five seconds |
| `start` returns once the daemon is launched | `rcS`, which waits for each script | The boot stops at that stage |
| The `<name>` passed to `supervise` is letters, digits, `_`, `-` and `.`, at most 64 characters | `ksvc` | `ksvc` refuses it: `bad service name` |

`NN` places the script in the boot order. Pick a number above every script whose service yours
needs: above `22_syslog` to log through syslog from the first line, above `30_network` and
`42_networkmanager` for the network, above `40_dbus` for the system bus.

### service_helper

<!-- kfs:include fs/etc/init.d/service_helper -->

`supervise` takes an argument vector, not a string: write
`supervise mydaemon /usr/sbin/mydaemon --foreground`, each word its own argument. The options of
`ksvc supervise`, `--final-exit` among them, are in
[ksvc and service](../../kdos/04-programs/kdos-command.md#ksvc-and-service).

### A script in the base system

`fs/etc/init.d/52_smartd.sh` is the base system's pattern with one addition, the exit statuses the
supervisor is not to restart:

<!-- kfs:include fs/etc/init.d/52_smartd.sh -->

### A script in a port

A port writes its script from `build.sh`, in a quoted heredoc whose delimiter is `KDOS_SH`, so the
script is part of the recipe and `testing/preflight.sh` can check it. The [ngircd](#port-ngircd)
port's script skips until the server has a configuration file and an account, and runs the daemon
as that account:

<!-- kfs:include ports/core/servers/ngircd/build.sh lines=22-52 pin=2ce56e74e4f9 -->

The account comes from the port's `postinstall.sh`, which runs on the target at every install and
writes under `PKG_ROOT`:

<!-- kfs:include ports/core/servers/ngircd/postinstall.sh lines=19-22 pin=f7b763078e75 -->

Why a script lives in the recipe and not beside it, and the rules a `postinstall.sh` keeps, are in
[Shipping a script the port carries](../../kdos/05-developer/writing-ports.md#shipping-a-script-the-port-carries)
and [postinstall.sh](../../kdos/05-developer/writing-ports.md#postinstallsh).

## Usage

### Adding a service to one machine

1. Choose the number and the name. List what is there:

   ```bash
   ls /etc/init.d
   ```

2. Write the script as root. This one starts a daemon at `/usr/local/sbin/mydaemon` that stays in
   the foreground when given `--foreground`; replace the three names with yours:

   ```bash
   #!/bin/bash
   . /etc/init.d/service_helper

   NAME="mydaemon"
   DAEMON="/usr/local/sbin/mydaemon"

   case "$1" in
       start)
           if [ ! -x "$DAEMON" ]; then
               echo "[SKIP] $NAME: $DAEMON not found"
               exit 0
           fi
           echo "[KDOS] Starting $NAME..."
           supervise "$NAME" "$DAEMON" --foreground
           ;;
       stop)
           stop_service "$NAME"
           ;;
       status)
           check_status "$NAME"
           ;;
       *)
           echo "Usage: $0 {start|stop|status}"
           exit 1
           ;;
   esac
   ```

   Save it as `/etc/init.d/90_mydaemon.sh`.

3. Make it executable:

   ```bash
   sudo chmod 755 /etc/init.d/90_mydaemon.sh
   ```

4. Start it and check it:

   ```bash
   sudo service start mydaemon
   service status mydaemon
   sudo cat /run/kdos-svc.mydaemon.log
   ```

   On the first start the log's first line is the supervisor's
   `[KDOS] (mydaemon) Starting: /usr/local/sbin/mydaemon`.

5. Stop it, and check that the daemon is gone with the supervisor:

   ```bash
   sudo service stop mydaemon
   pgrep -a mydaemon
   ```

   `pgrep` should print nothing.

6. The service starts at every boot from here on. To keep it installed but off, run
   `sudo service disable mydaemon`.

### Adding a service to the tree

1. **Base system.** Put the script at `fs/etc/init.d/NN_<name>.sh` and make it executable in git
   (mode `100755`). Copy it into the build root and repack the image, on your machine:

   ```bash
   make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"
   ```

   See [The file system](../03-bootstrap/000-file-system.md#running-it-on-its-own).

2. **A port.** Write the script into the port's `build.sh` as above, `install -d "$PKG/etc/init.d"`
   first and `chmod 755` after, and an account, if it needs one, into `postinstall.sh`. Rebuild the
   port and repack:

   ```bash
   make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"
   ```

3. Check the wiring, on your machine:

   ```bash
   testing/preflight.sh
   ```

   It refuses a script under `fs/etc/init.d` that is not executable or not recorded in git as
   `100755`, and a `kdos-*` daemon such a script names that no `build.sh` installs. In a port, it
   reads every heredoc opened with `<<'KDOS_SH'`, refuses one that does not parse under `bash -n`
   and, when a build tree exists, one whose commands are on no image.

4. A service that listens on the network also needs a firewall rule; see
   [Adding your own rules](../../kdos/02-user-guide/administration.md#adding-your-own-rules).

## Tips

- **Keep the daemon in the foreground.** Every shipped script passes the daemon's own switch for
  it: `-D` for `sshd`, `-F` for [`syslogd`]{.idx}, `-n` for `smartd`, `-d` for `chronyd`, `--no-daemon`
  for `thermald`.
- **Do not let the daemon's pid file and the supervisor's share a path.** The supervisor writes its
  own process id to `/run/<name>.pid`, where `<name>` is the first argument of `supervise`, and
  `stop` signals the process group that file names. A daemon that writes its own pid file to the
  same path replaces the supervisor's id with its own, and `stop` then ends the daemon alone, which
  the supervisor restarts five seconds later; a daemon that also removes its pid file when it exits
  leaves `status` reporting the service down. Give `supervise` a name the daemon does not use for
  its pid file, or point the daemon's pid file elsewhere with its own option: `22_syslog.sh` passes
  `syslogd` `-P /run/syslogd/syslogd.pid`, and `70_sshd.sh` passes `sshd` `-o PidFile=none`.
- **Skip in the script, before supervising.** A daemon that cannot run on this machine and exits is
  restarted every five seconds for as long as the machine is up. Test for what it needs (a device,
  a kernel feature, a configuration file) first, and print `[SKIP]` with the reason.
- **Name a final exit.** When only the daemon itself can tell that it has nothing to do, find the
  exit status it uses for that and pass `--final-exit <code>` to `supervise`.
- **Run as an account, not as root,** with `setpriv --reuid=<user> --regid=<group> --init-groups`
  in front of the daemon, as `79_ngircd.sh` does. Test for the account first, with
  `getent passwd <user>` (from [musl-utils](#port-musl-utils)) or, as `79_ngircd.sh` does,
  `grep -q '^<user>:' /etc/passwd`.
- **Start every long-running process through `supervise`, never with `&`.** The supervisor points
  its own output and the daemon's at its log forwarder and its input at `/dev/null`, so
  `sudo service start mydaemon | tee out.txt` returns as soon as the script does. A process the
  script backgrounds itself keeps the script's output open for as long as it lives, so a pipeline
  reading that output waits as long, and nothing restarts the process when it dies. `rcS` writes
  each script's output to `/run/kdos-init.<name>.log` and prints the file afterwards.
- **Name the daemon by its full path**, as every shipped script does. Under `rcS` and `rcK` the
  `PATH` includes `/usr/local`, but a script started by `service` inherits the `PATH` of whatever
  ran it.
- Per-machine state, such as a host key or a database created on first start, belongs in the init
  script, not in the package: whatever a package or a `postinstall.sh` writes while the image is
  built is the same on every machine installed from it. `70_sshd.sh` runs `ssh-keygen -A` for this.

## Troubleshooting

**`service` says `Error: Service '<name>' not found`.** The file name is wrong: it must be two digits, `_`, the name and
`.sh`. **`rcS` never runs the script and `service start` does nothing.** The file is not
executable. `ls -l /etc/init.d/` must show `-rwxr-xr-x`.

**The boot stops at the script's stage.** `start` did not return. Either the daemon was started in
the foreground without `supervise`, or the script reads from a pipe something the supervisor
holds open. Boot with `emergency` (see [Init and runlevels](init-and-runlevels.md#single-user-mode)),
make the script non-executable with `chmod -x`, and reboot.

**The supervisor's log shows `Exited with code 127`.** The supervisor could not execute the command:
the path is wrong or the file is not executable. `supervise` runs the command through `execvp`, so a
bare name is looked up in `PATH`.

**The daemon exits at once and restarts every five seconds.** Most often it forked into the
background and the supervisor saw the parent exit: the log repeats
`[KDOS] (<name>) Exited with code 0. Restarting in 5s...`, a clean exit, after each `Starting:`
line. Add the daemon's foreground switch. Otherwise read its own complaint in
`/run/kdos-svc.<name>.log`.

**`service stop` reports `Stopped`, but the daemon keeps running.** Either the daemon left the
supervisor's process group (a daemon that calls `setsid` itself does), or it rewrote
`/run/<name>.pid`. Check with `ps -o pid,pgid,args -p "$(cat /run/<name>.pid)"`; the process should
be `ksvc supervise …`, and the daemon should share its process group.

**`testing/preflight.sh` reports `KDOS_SH heredoc:` and a syntax error.** The script inside the
port's heredoc does not parse under `bash -n`. Write the delimiter quoted, `<<'KDOS_SH'`: an
unquoted one lets `$1` and `$PATH` expand while the recipe runs, and `preflight.sh` does not read
such a heredoc at all.

## Known gaps

[Known gaps](../../kdos/06-reference/known-gaps.md) lists nothing for this topic. For a script added
to the tree:
[A clone does not check what it pushes unless you enable the hook](../../kdos/06-reference/known-gaps.md#a-clone-does-not-check-what-it-pushes-unless-you-enable-the-hook).

## See also

- [Adding a root daemon](../../kdos/04-programs/daemons.md#adding-a-root-daemon): the rules KDOS's own daemons keep
- [The shape they share](../../kdos/04-programs/daemons.md#the-shape-they-share): supervision, logging and skipping
- [ksvc and service](../../kdos/04-programs/kdos-command.md#ksvc-and-service): `supervise` and its options
- [Shipping a script the port carries](../../kdos/05-developer/writing-ports.md#shipping-a-script-the-port-carries): the `KDOS_SH` heredoc
- [Services](services.md), [Init and runlevels](init-and-runlevels.md), [Logs](logs.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 360.* Previous: [359. Services](services.md) · [Contents](../README.md) · Next: [361. Timers](timers.md)

# Logs

KDOS's [system log]{.idx} is written by [`syslogd`]{.idx}, from sysklogd, into files under `/var/log`.
Beside it, a few files under `/run` keep what the system log cannot: what each [service script]{.idx}
printed at boot, each supervised daemon's recent output, and the boot-time filesystem check. This
page lists where each kind of message goes, how to read and follow it, how to change the routing,
and what to read for a given failure. Every command on this page runs on a running KDOS machine.

## Installation

- [sysklogd](#port-sysklogd) installs `syslogd` and its configuration, [`/etc/syslog.conf`]{.idx}. It is
  built without its own [`logger`]{.idx}.
- [`/etc/init.d/22_syslog.sh`]{.idx}, from the base filesystem, starts `syslogd` under the supervisor as the
  service `syslog`. It runs before the network comes up.
- [`/etc/init.d/20_dmesg.sh`]{.idx} sets the kernel's console log level.
- `ksvc`, from [kdos-tools](#port-kdos-tools), copies every supervised daemon's output into the
  system log and into a file of its own under `/run`.
- [util-linux](#port-util-linux) installs `logger`, which writes a message into the system log, and
  [`dmesg`]{.idx}, which reads the kernel's ring buffer. It also installs [`last`]{.idx}, which reads the login
  records; [utmps](#port-utmps) keeps those.

The services `syslog` and `dmesg` are enabled on every image. On the live medium `/var/log` is in the overlay's upper
layer, which is memory unless a persistence store exists (`kdos persist`), so the log of a live
session is gone at power-off. The installer copies no file under `/var/log` to the disk, so an
installed system's log and its `last` record begin at its first boot. On an installed system the
log is on the root filesystem and survives a reboot. `/run` is a tmpfs on both, so everything under
it is gone at every reboot.

## Configuration

### /etc/syslog.conf

The [sysklogd](#port-sysklogd) port's `build.sh` writes the file:

<!-- kfs:include ports/core/base/sysklogd/build.sh lines=26-45 pin=452c61443245 -->

Each rule is a selector, `facility.level`, and a destination; a `-` before a path writes without
syncing after every message. The routing, the rotation and `secure_mode` are described in
[Logs](../../kdos/02-user-guide/administration.md#logs). `syslog.conf(5)`, installed by the port,
is the reference for every directive, `include` among them.

### /etc/init.d/22_syslog.sh

<!-- kfs:include fs/etc/init.d/22_syslog.sh -->

### /etc/init.d/20_dmesg.sh

<!-- kfs:include fs/etc/init.d/20_dmesg.sh -->

At level 3 the console shows only the kernel's messages more urgent than an error: critical, alert
and emergency. The next script, `22_syslog.sh`, starts `syslogd`, which lowers it further: when
it opens the kernel's log it turns the kernel's console logging off, which leaves the level at 1,
emergencies only, for as long as it runs (`syslogd -l` would keep the level as set).
`cat /proc/sys/kernel/printk` on a booted machine starts `1`. Every message still reaches the ring
buffer and the system log.

### Where each kind of message goes

| Message | File | Kept |
|:--|:--|:--|
| Everything but authentication | [`/var/log/messages`]{.idx} | Rotated by size, as `rotate_size` and `rotate_count` say; older copies gzipped |
| Logins, `sudo`, polkit, other authentication | `/var/log/auth.log` (root only, mode 0600, set by `22_syslog.sh` before `syslogd` starts) | The same |
| The kernel | [`/var/log/kern.log`]{.idx}, and `dmesg` | The same; the ring buffer until it wraps |
| The `cron` facility | `/var/log/cron`; nothing on the image logs there | The same |
| A service script's output at boot | `/run/kdos-init.<service>.log` | Until reboot |
| A supervised daemon's output | `/run/kdos-svc.<name>.log` (root only, mode 0640), and the system log tagged `<name>` | 64 KiB, one `.old` beside it |
| The boot-time check of non-root filesystems | `/run/kdos-fsck.log`, when it reported anything | Until reboot |
| A console's `kdos-getty` | `/run/kdos-getty.<tty>.log` | Until reboot |
| The compositor | `$XDG_RUNTIME_DIR/kdos-comp.log` | Each session start moves the previous one to `kdos-comp.log.old` |
| The installer | [`/var/log/kinstall.log`]{.idx} on the system it ran from, root only; the copy to the installed system leaves it out | Appended to by every run |
| Logins, logouts, boots | [`/var/log/wtmp`]{.idx}, read by `last` | Appended |

`<service>` is the script's name without its number and `.sh`, such as `chrony`; `<name>` is the
name the script gave the supervisor, such as `chronyd`. The two differ for some services; see
[Services](services.md#a-service-script). The `/run` files are listed in
[/run/ — system runtime](../../kdos/06-reference/filesystem-and-ipc.md#run--system-runtime).

A supervised daemon that wrote to the system log itself and copied each line to its standard
error would reach the log twice, so each is set to one of the two. ModemManager writes to standard
output and is logged under `ModemManager`; `dbus-daemon` runs with `--syslog-only`, its lines carry
the tag `dbus-daemon`, and `/run/kdos-svc.dbus.log` holds only what the services the bus starts
print.

## Usage

### Reading the system log

1. Page through it, or follow it as messages arrive:

   ```bash
   less /var/log/messages
   tail -f /var/log/messages
   ```

2. Pick out one daemon. Each supervised daemon's lines carry its name as the tag:

   ```bash
   grep 'kdos-powerd' /var/log/messages
   ```

   A line carries the time, the host name, the tag and the message:
   `Oct 21 19:10:29 kdos kdos-powerd: [KDOS] (kdos-powerd) Starting: /usr/sbin/kdos-powerd`.
   `syslogd` runs with `-8`, so a message in UTF-8, such as `kdos-oomd`'s kill line with its `—`,
   is written as it was sent and the file stays text. ASCII control characters in a message are
   still written as `^x`.

3. Authentication is kept apart:

   ```bash
   sudo tail -n 50 /var/log/auth.log
   ```

4. The older copies sit beside each file: `messages.0` is the most recent, and the ones before it
   are compressed, `messages.1.gz` onwards:

   ```bash
   ls -l /var/log
   zcat /var/log/messages.1.gz | less
   ```

### Reading what a boot printed

1. Find the service scripts' logs from this boot:

   ```bash
   ls /run/kdos-init.*.log
   ```

2. Read one:

   ```bash
   cat /run/kdos-init.networkmanager.log
   ```

3. Read a supervised daemon's own output, which stays readable before `syslogd` starts and while
   it restarts:

   ```bash
   sudo cat /run/kdos-svc.NetworkManager.log
   ```

### Reading the kernel's messages

```bash
sudo dmesg
sudo dmesg --level=err,warn
```

The ring buffer holds messages from before `syslogd` started. Reading it takes root, because the
kernel is built with `CONFIG_SECURITY_DMESG_RESTRICT`, which sets `kernel.dmesg_restrict` to 1;
`/etc/sysctl.conf` leaves it there. The kernel's messages are also in `/var/log/kern.log`.

### Writing a message

From a script or a shell, with a tag of your choosing:

```bash
logger -t backup "finished, 4 GB written"
```

The line lands in `/var/log/messages` with the tag `backup`.

### Reading the login records

```bash
last
who -b
who -r
```

`last` reads `/var/log/wtmp`, which the `wtmpd` service appends to, and `who -b` and `who -r`, the
last boot and the current runlevel, ask `utmpd`; see
[Login records](../../kdos/03-architecture/boot-and-init.md#login-records).

### Changing where messages go

1. Edit `/etc/syslog.conf` as root. To keep a daemon's messages in a file of their own as well,
   add a program filter, a `!` line naming the tag, at the end of the file, followed by the rules
   for that program:

   ```text
   !kdos-powerd
   *.*                                     -/var/log/powerd.log
   ```

   The filter applies to the rules after it, so the lines above it still send the same messages
   to `/var/log/messages`. `syslog.conf(5)` describes program, host and property filters.
2. Restart the logger:

   ```bash
   sudo service restart syslog
   ```

3. Check the result with `logger` as above.

## Tips

- `kpkg check sysklogd` reports an edited `/etc/syslog.conf` as a `config` line, which does not
  count as a fault; see [Checking an installed root](../../kdos/03-architecture/packaging.md#checking-an-installed-root).
- `syslogd` rotates its files itself, by the `rotate_size` and `rotate_count` lines. There is no
  `logrotate` job to configure.
- `syslogd` listens on no network socket (`secure_mode 2`). To forward to another host, set
  `secure_mode 1` and add an `@host` destination; it still accepts nothing from the network.
- The periodic jobs do not log to the `cron` facility, so their output is not in `/var/log/cron`:
  it is under the tag `kdos-timer-NAME`. See [Timers](timers.md).

## Troubleshooting

**A service failed at boot.** Read `/run/kdos-init.<service>.log` first, then
`/run/kdos-svc.<name>.log` for a supervised daemon.
[When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops) maps every boot
stage to the file that explains it.

**`/var/log/messages` is missing or stops growing.** `syslogd` is not running:

```bash
service status syslog
sudo cat /run/kdos-svc.syslogd.log
```

Start it with `sudo service start syslog`. Messages sent while it was down are not in the log, but
each supervised daemon's `/run/kdos-svc.<name>.log` still holds its recent output.

**The console shows kernel messages over the login prompt.** The console level was raised. Lower it
again to the level a running `syslogd` keeps:

```bash
sudo dmesg -n 1
```

**The splash shows `fsck: errors left uncorrected, see /run/kdos-fsck.log`.** A filesystem in
`/etc/fstab`, other than the root, has errors the automatic check did not repair, and it was mounted anyway. Read the
file, then check the filesystem unmounted; see
[Checking a filesystem by hand](filesystems-and-fstab.md#checking-a-filesystem-by-hand).

**The desktop crashed or never appeared.** Log in on `tty2` and read the compositor's log, which
lives in the desktop user's runtime directory:

```bash
tail -n 40 /run/user/1000/kdos-comp.log
```

`kdos-comp.log.old` is the previous session's. See [Debugging](../../kdos/04-programs/kdos-comp.md#debugging)
in the compositor's chapter.

## Known gaps

[Known gaps](../../kdos/06-reference/known-gaps.md) lists nothing for this topic.

## See also

- [Logs](../../kdos/02-user-guide/administration.md#logs): the routing and the rotation
- [Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging): how a daemon's output reaches syslog and `/run`
- [When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops): which file to read for each boot stage
- [/run/ — system runtime](../../kdos/06-reference/filesystem-and-ipc.md#run--system-runtime): every runtime file
- [Services](services.md), [Timers](timers.md), [Init and runlevels](init-and-runlevels.md),
  [The initramfs and the rescue shell](initramfs-and-rescue.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 362.* Previous: [361. Timers](timers.md) · [Contents](../README.md) · Next: [363. Modules and sysctl](modules-and-sysctl.md)

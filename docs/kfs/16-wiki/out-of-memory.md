# Out of memory

When a KDOS machine runs [out of memory]{.idx}, [kdos-oomd]{.idx} ends the program that is starving it
before the desktop stops responding, and the kernel's own out-of-memory killer remains behind it
for the case the daemon does not catch. This page shows how to check that the daemon is running,
how to find out what it killed and why, how to steer its choice, and how to turn it off. How it
picks a victim is in the KDOS book's [kdos-oomd](../../kdos/04-programs/daemons.md#kdos-oomd)
section; this page does not restate it.

## Installation

The daemon is the [kdos-oomd](#port-kdos-oomd) port, a program of KDOS's own installed by
`50_desktop` as `/usr/sbin/kdos-oomd`. Its service script, [`/etc/init.d/57_oomd.sh`]{.idx}, comes from the
tree's `fs/` overlay. The image ships no `oomd` marker in `/etc/service.disabled/`, so it starts at
every boot, on the live medium and on an installed system alike, under the supervisor `ksvc`.

The script skips the daemon, with a `[SKIP]` line, when the binary is missing or when the kernel's
[pressure stall]{.idx term="pressure stall information"} information (PSI) is off; see
[Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves).
The kernel is built with PSI on, in `ports/core/boot/linux/kdos.config`:

<!-- kfs:include ports/core/boot/linux/kdos.config lines=32-32 pin=c46bc96311a6 -->

Its socket is read in the procedures below with [`socat`]{.idx}, from the [socat](#port-socat) port, which
the image carries.

## Configuration

The daemon has no configuration file. What it watches for and what it never kills are compiled
into `src/daemons/kdos-oomd/main.c`. `KO_TRIGGER` is written into `/proc/pressure/memory` as a
stall amount in microseconds and the window it is measured over, `KO_COOLDOWN` is the shortest
time between two scans, and `KO_AVAIL_PCT` is the share of `MemTotal` that `MemAvailable` in
`/proc/meminfo` must be at or under before a stall kills anything:

<!-- kfs:include src/daemons/kdos-oomd/main.c lines=64-70 pin=80fc432106d5 -->

The programs it never chooses, matched by process name; every process whose name starts with
`pipewire` is spared as well:

<!-- kfs:include src/daemons/kdos-oomd/main.c lines=240-253 pin=86589480a7f6 -->

**`/etc/init.d/57_oomd.sh`**, from `fs/etc/init.d/57_oomd.sh`:

<!-- kfs:include fs/etc/init.d/57_oomd.sh -->

Two settings outside the daemon change its choice:

- **A box's memory budget.** The `memory` key of a box profile,
  `~/.config/kdos/boxes/<box>.conf`, such as `memory = 4G`. A box whose processes together hold
  more than its budget is the first to lose one. The daemon reads the profile each time it chooses,
  from every `/home/<user>/.config/kdos/boxes/`, so an edited budget counts from the next choice
  with nothing restarted. The key is described in
  [`~/.config/kdos/boxes/<name>.conf`](../../kdos/06-reference/configuration.md#configkdosboxesnameconf).
- **A process's `oom_score_adj`.** The daemon never chooses a process whose
  `/proc/<pid>/oom_score_adj` is -500 or lower. Lowering the value needs root, and a child
  inherits it, so a program that lowers its own shields everything it starts afterwards.

## Usage

### Checking that it is running

1. Ask the supervisor:

   ```bash
   service status oomd
   ```

2. Or run the KDOS check, which looks for the daemon's socket:

   ```bash
   kdos doctor
   ```

   It prints `[ ok ] kdos-oomd watching /proc/pressure/memory` when the daemon is up, and a
   `[warn]` line when it is not.

### Finding out what it has done

1. Ask the daemon. The socket answers root and members of `seat` and `wheel`, which the desktop
   account is in:

   ```bash
   echo status | socat - UNIX-CONNECT:/run/kdos-oomd.sock
   ```

   The answer is `ok trigger 'full 150000 1000000', kills 0`, and after the first kill it ends
   with `last:` and the victim, its box when it has one, its process id and its size, such as
   `kills 1, last: tail (pid 3801, 3344 MB)`. The count starts again at every start of the
   daemon.

2. Read every kill since the system log started, each with the pressure that caused it:

   ```bash
   grep kdos-oomd /var/log/messages
   ```

   A kill is logged as
   `kdos-oomd: killed <name> [(appbox <box>)] (pid <pid>, <size> MB) — memory pressure avg10 full=<f> some=<s>, <avail> of <total> MB available`.
   For example `kdos-oomd: killed python3 (pid 3485, 1122 MB) — memory pressure avg10 full=1.17 some=1.23, 227 of 3892 MB available`:
   a stall that fills a one-second window can be over before the ten-second average moves.
   The same lines are in `/run/kdos-svc.kdos-oomd.log`, which also holds them from before `syslogd`
   started; see [Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging).

3. Check whether the kernel's own killer acted as well:

   ```bash
   sudo dmesg | grep -i 'out of memory'
   ```

### Watching memory pressure

```bash
cat /proc/pressure/memory
```

The `full` line is the share of time every runnable task was stalled on memory, averaged over 10,
60 and 300 seconds. The daemon acts when the `full` stall within one window of `KO_TRIGGER`
reaches its amount and `MemAvailable` is at most `KO_AVAIL_PCT` percent of `MemTotal`:

```bash
grep -E 'MemTotal|MemAvailable' /proc/meminfo
```

A single busy program, such as a large copy that recycles the page cache, can stall every task that
wants to run while most of memory is still available; the daemon leaves it alone. `avg10` is
averaged over ten seconds, so a short stall that fires the trigger can read lower there.

### Watching it act

A program that does nothing but take memory shows the daemon at work without risking anything
else. `tail` keeps a whole line in memory, and `/dev/zero` never ends one:

```bash
timeout 180 tail /dev/zero
```

On a 4 GB machine with zram the shell reports `Killed`, exit status 137, long before `timeout`'s
three minutes are up, after `tail` has grown to about 3.3 GB; `dmesg` shows no kill by the kernel,
and the `status` query above then counts the kill. Save your work first: the
desktop stalls while the memory fills.

### Keeping one process out of its reach

1. Start the program with a lowered score:

   ```bash
   sudo choom -n -500 -- <command>
   ```

   or lower the score of one already running:

   ```bash
   sudo choom -n -500 -p <pid>
   ```

   `choom` is from [util-linux](#port-util-linux). Run `choom -p <pid>` to read a score back.

2. Prefer a box budget for a boxed application: a budget makes the box the first candidate when it
   is over it, and leaves the rest of the session alone.

### Turning it off

```bash
sudo service stop oomd
sudo service disable oomd
```

The machine then has only the kernel's own killer. `sudo service enable oomd` and
`sudo service start oomd` bring it back.

## Tips

- A box's budget is checked against the resident memory of all its processes added up, so a
  browser with many processes is measured as one.
- The daemon scans at most once per cool-down (`KO_COOLDOWN` above), counted from the moment the
  trigger fires, whether or not that scan killed anything. A second program that keeps the machine
  stalled goes at the next scan, not at once.
- The live medium keeps `/var/log` in RAM; a kill logged there is gone after power-off. Note it, or
  copy `/var/log/messages`, before shutting down.
- Swap to zram gives the machine more room before the trigger fires; see
  [zram and swap](zram-and-swap.md).

## Troubleshooting

**`kdos doctor` warns `kdos-oomd not running`.** Read why it did not start:

```bash
cat /run/kdos-init.oomd.log
```

- `[SKIP] kdos-oomd: no writable /proc/pressure/memory (PSI off)`: PSI is switched off. Look for
  `psi=0` in `/proc/cmdline` and remove it from the boot entry.
- `[SKIP] kdos-oomd: /usr/sbin/kdos-oomd not found`: the `kdos-oomd` package is not installed.
- No log: the service is disabled; `ls /etc/service.disabled/` shows an `oomd` marker.

Start it with `sudo service start oomd`, the command the doctor's warning names.

**`socat` prints `err not permitted`.** The account is in neither `seat` nor `wheel`. Run it with
`sudo`, or see [Users and groups](users-and-groups.md).

**The desktop froze and no kill was logged.** Check the count with the `status` query above, then
the pressure: a machine whose `full` stall stays under `KO_TRIGGER` does not fire it, and one whose
`MemAvailable` stays above `KO_AVAIL_PCT` percent of `MemTotal` kills nothing when it does. Read
`dmesg` for a kill by the kernel. If a protected program is the one using the memory, the daemon
finds nothing to kill; `ps -eo rss,comm --sort=-rss | head` shows the largest.

**The wrong program was killed.** The log line names it and its box. Give the box that should go
first a `memory` budget, or lower the score of the process that should stay, as above. Which of
several large processes the daemon picks is tested only against recorded process tables (see
[Known gaps](#known-gaps)).

**A large copy fires the trigger.** The installer's Copy system step, and any copy of many
gigabytes, streams through the page cache and can stall the machine long enough to fire
`KO_TRIGGER`, most of all with little swap: the installer's Prepare step turns off every swap area
on a disk and leaves only [zram](zram-and-swap.md). The page cache counts as available memory, so
`MemAvailable` stays high and the daemon kills nothing; the `status` query's count stays where it
was. The installer's copy is out of reach even when memory does run
short: `kinstall` sets its own score to -900 when an install starts, and `rsync` and every other
program the install runs inherit it (`cat /proc/$(pgrep -x rsync | head -1)/oom_score_adj` during
the Copy system step prints `-900`). Any other copy that must not be killed is started with a
lowered score, as *Keeping one process out of its reach* under [Usage](#usage) shows.

## Known gaps

- [Security](../../kdos/06-reference/known-gaps.md#security): the daemon's choice between several
  large processes is tested only against recorded process tables.
- [Status](../../kdos/06-reference/status.md#daemons) rates the daemon Beta and gives the one run in
  which it fired on a live machine.

## See also

- [kdos-oomd](../../kdos/04-programs/daemons.md#kdos-oomd): how it decides, the socket and how it is tested
- [Who may ask for what](../../kdos/04-programs/daemons.md#who-may-ask-for-what): which groups reach each root daemon
- [Root daemons](../../kdos/03-architecture/security-model.md#root-daemons): the daemons that run as root
- [`~/.config/kdos/boxes/<name>.conf`](../../kdos/06-reference/configuration.md#configkdosboxesnameconf): the `memory` key
- [zram and swap](zram-and-swap.md): the compressed swap that comes before it
- [Services](services.md): starting, stopping and disabling a service
- [Logs](logs.md): the system log and the `/run` logs

<!-- book-nav -->
---

*Part XVI. System administration, chapter 365.* Previous: [364. zram and swap](zram-and-swap.md) · [Contents](../README.md) · Next: [366. Time and NTP](time-and-ntp.md)

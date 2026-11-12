# Timers

A periodic job on KDOS is one line in a `.timer` file, and [`snooze`]{.idx}, a program that sleeps
until the clock matches its options and then runs one command, does the waiting. There are three
tables: the system's, [`/etc/kdos/timers.d/`]{.idx term="/etc/kdos/timers.d"}, whose jobs run as
root under the service supervisor from boot; `/etc/kdos/user-services/timers.d/`, whose jobs run
for every user, as that user; and each user's own, `~/.config/kdos/timers.d/`. The two user tables
run under the user's services from the user's first login to the last. Users' crontabs, written
with `crontab -e` or System Settings' Task Scheduler, are [cronie]{.idx}'s, which reads no
`/etc/crontab`, so no job is in both. This page covers the tables: reading and checking a
schedule, adding and removing a job, running one now, and finding out why one did not fire.

## Installation

- [snooze](#port-snooze) installs `/usr/bin/snooze` and its manual page, `snooze(1)`, which is the
  reference for every time option.
- The base filesystem supplies [`/etc/init.d/18_timers.sh`]{.idx}, the service `timers`, which starts one
  supervised `snooze` per line of the system table at boot; the system table itself with its
  shipped jobs; the user service `/etc/kdos/user-services/30_timers.sh`, which runs
  `/usr/local/lib/kdos/user-timers` for the two user tables; the every-user table with its one
  job; and the user table's skeleton, `/etc/skel/.config/kdos/timers.d/`, copied into each new
  home directory. `/usr/local/lib/kdos/timers.sh` is the one parser all three share.
- [cronie](#port-cronie) installs `crond` and the setuid `crontab`, started by
  `/etc/init.d/85_crond.sh`, for users' crontabs; [kcron](#port-kcron) is the Task Scheduler page.
- `kdos remind`, from [kdos-tools](#port-kdos-tools), writes reminders into the user table.

The services `timers` and `crond` are enabled on the live medium and on an installed system alike.
On the live medium a job added to any table is kept only when a persistence store exists
(`kdos persist`).

## Configuration

### The line

Every line of every table has the same shape:

```text
NAME  TIMESPEC...  --  COMMAND...
```

`NAME` is letters, digits, `_` and `-`. `TIMESPEC` is `snooze`'s own options, unchanged. `--` ends
the timespec, and everything after it is the command: an argument vector, split on blanks, never
passed to a shell, with no quoting, so an argument cannot contain a space. Blank lines and lines
starting with `#` are ignored. The rules are in
[/etc/kdos/timers.d/ and ~/.config/kdos/timers.d/](../../kdos/06-reference/configuration.md#etckdostimersd-and-configkdostimersd).

### /etc/kdos/timers.d/

The system table's own description, `fs/etc/kdos/timers.d/README`:

<!-- kfs:include fs/etc/kdos/timers.d/README -->

The two jobs the base system ships:

<!-- kfs:include fs/etc/kdos/timers.d/10-update-check.timer -->

<!-- kfs:include fs/etc/kdos/timers.d/20-fstrim.timer -->

No port adds a system job.

### /etc/init.d/18_timers.sh

How a line becomes a supervised job. A command that is not installed is skipped with a `[SKIP]`
line; everything else is handed to `supervise` as the service `kdos-timer-NAME`. A status
query fails when any one job is down, not only the last one read:

<!-- kfs:include fs/etc/init.d/18_timers.sh lines=36-61 pin=51b52355efa2 -->

### /etc/kdos/user-services/timers.d/

Rows here run for every user, as that user, beside the user's own table. The one KDOS ships gives
idle boxes back, from `fs/etc/kdos/user-services/timers.d/40-box-gc.timer`:

<!-- kfs:include fs/etc/kdos/user-services/timers.d/40-box-gc.timer -->

### The user service

From `fs/etc/kdos/user-services/30_timers.sh`, how the two user tables are run:

<!-- kfs:include fs/etc/kdos/user-services/30_timers.sh lines=4-17 pin=76121c3308aa -->

### ~/.config/kdos/timers.d/

The user table's description, from the skeleton:

<!-- kfs:include fs/etc/skel/.config/kdos/timers.d/README -->

The `-t` path in that example does not work as written. A row is never given to a shell, so `~`
and `$HOME` reach `snooze` as those characters, and a `-t` path has to be written out in full.
The file it names must also be one the job touches after each run, which a `.timer` file is not;
see [Tips](#tips).

The two jobs in the skeleton. The backup job is commented out:

<!-- kfs:include fs/etc/skel/.config/kdos/timers.d/20-updatedb.timer -->

<!-- kfs:include fs/etc/skel/.config/kdos/timers.d/10-backup.timer -->

## Usage

### Seeing what is scheduled

1. List every system job and whether its `snooze` is waiting:

   ```bash
   service status timers
   ```

   Each job prints `[ OK ] kdos-timer-NAME  (pid N)` or `[DOWN] kdos-timer-NAME`. For the five
   seconds after a run whose command failed, before `snooze` starts again, it prints
   `[FAIL] kdos-timer-NAME  (exited with code N; restarting)`; a run that succeeded leaves it
   `[ OK ]`. The command exits 0 only when every job is `[ OK ]`; one `[DOWN]` or `[FAIL]` job
   anywhere in the list makes it exit 1, so a script can test it with `if service status timers`.

2. Ask about one system job:

   ```bash
   ksvc check kdos-timer-update-check
   ```

3. Print the next five times a timespec matches, without running anything:

   ```bash
   snooze -n -H 4 -M 17
   ```

   Each line is a time, its weekday and how far away it is, such as
   `2026-10-22T04:17:00+0000 Thu  0d  9h  4m 12s`. `snooze -n` exits non-zero for a timespec it
   does not accept: `snooze -n -H 99` prints `number outside 0 <= n < 24` and exits 1.

### Adding a system job

1. Check the timespec with `snooze -n`, as above.
2. Write the line into a new file under `/etc/kdos/timers.d/`, as root. The file name must end in
   `.timer`; the job's name is the first word of the line, not the file name. For a run every day
   at 06:30, up to two hours late after a sleep:

   ```text
   mybackup  -H 6 -M 30 -s 2h  --  /usr/local/sbin/mybackup --quiet
   ```

3. Restart the service, which stops every system job and starts the table again:

   ```bash
   sudo service restart timers
   ```

4. Check that the job is waiting:

   ```bash
   ksvc check kdos-timer-mybackup
   ```

### Removing or changing a system job

`service stop timers` stops only the jobs it finds in the table as it is when it runs, so stop a
job before removing its line:

1. Stop the job:

   ```bash
   sudo ksvc stop-supervised kdos-timer-mybackup
   ```

2. Delete or edit the line.
3. If you edited it, start the table again with `sudo service restart timers`.

### Running a job now

A job's command is an ordinary command. To run it once now, type the part after `--` as root, for
example:

```bash
sudo fstrim -a
```

To stop every system job until the next boot, run `sudo service stop timers`; to stop them at
every boot, `sudo service disable timers`.

### Adding a job of your own

1. Write the line into a file ending `.timer` under `~/.config/kdos/timers.d/`.
2. Check it, since a line the runner cannot use is skipped without a word:

   ```bash
   snooze -n -H 7 -M 30
   command -v <the command's first word>
   ```

3. Read the tables again:

   ```bash
   service --user restart timers
   ```

   This stops every user job running at that moment. The loops run from the user's first login
   to the last, whether that is the desktop, a console or both.

A reminder needs none of this: `kdos remind in 20m tea` writes a row of its own and also starts a
`snooze` for it at once; see [kdos remind](../../kdos/04-programs/kdos-command.md#kdos-remind).

## Tips

- **A job that must run even when the machine was off across its slot** needs `-t FILE` as well as
  `-s`. `snooze` then counts from the file's modification time, and the job must update that file
  itself, since `snooze` never touches it. A command on the line cannot run `touch` as well, so
  such a job is a script that does its work and then touches the file. If nothing touches it,
  every restart of `snooze` finds the slot just run still inside the slack and runs the job again,
  until the slack has passed.
- **No `~` and no `$HOME`.** Nothing on a line is expanded, so a path in a timespec or a command
  is written in full, `/home/you/...`; a program that needs the home directory finds it itself,
  as `kdos-updatedb` does.
- **Give each job a minute of its own.** A laptop that wakes after several slots have passed runs
  every job whose slack covers the gap at the same moment.
- **`*` is safe in a timespec.** `18_timers.sh` and the user runner both split the line with pathname
  expansion off, so `-M *` reaches `snooze` as written.
- **A command that needs a pipe or a redirection** has to be a script of its own, named on the line.
- **The times are local times.** `snooze` reads the clock through the machine's time zone,
  `/etc/localtime`, which a time-zone change through timedate1 sets.
- `kdos why /etc/kdos/timers.d/10-update-check.timer` reports that no package owns the file; it is
  part of the base filesystem.

## Troubleshooting

**A system job never fires.** Read what `18_timers.sh` printed at boot:

```bash
cat /run/kdos-init.timers.log
```

Each complaint names the file and the line:

```text
[KDOS] (kdos-timers) /etc/kdos/timers.d/91-test.timer:2: bad timer name 'bad.name'
[KDOS] (kdos-timers) /etc/kdos/timers.d/91-test.timer:3: no '--' or no command; skipped
[SKIP] kdos-timers: missing needs /usr/bin/nonexistent-x, which is not on this image
```

`bad timer name` means the name holds a character other than letters, digits, `_` and `-`; a line
ending `skipped` has no `--` or nothing after it; the `[SKIP]` line means the command's first word
is not installed, and that job is then listed `[DOWN]` by `service status timers`, which
therefore exits 1 until the line is corrected or removed.
Correct the line and run `sudo service restart timers`. The file holds the boot's output only: a
restart prints the same lines to the terminal and leaves the file as the boot wrote it.

**A system job fired but did nothing useful.** Its output went to the supervisor's log for the job
and to the system log, tagged `kdos-timer-NAME`:

```bash
sudo cat /run/kdos-svc.kdos-timer-NAME.log
sudo grep kdos-timer-NAME /var/log/messages
```

**A user job never fires.** `service --user status timers` must say the service runs; its log is
`$XDG_RUNTIME_DIR/ksvc/kdos-svc.timers.log`. Check the timespec with `snooze -n` and the
command with `command -v`, as in [Adding a job of your own](#adding-a-job-of-your-own), and check
that the file name ends in `.timer`. Its standard output is discarded and its standard error goes
to that log and to syslog, so a job that must leave a trace
writes it to standard error.

**A job missed its slot while the machine was off.** That is how `-s` behaves; add `-t FILE`, as in
[Tips](#tips).

## Known gaps

- Nothing installs updates on a timer: the update-check job only fetches the ports tree and reports
  what is behind. See [Security](../../kdos/06-reference/known-gaps.md#security).
- The user tables' loops run only while the user has a login, unless the user lingers.

## See also

- [Periodic jobs](../../kdos/02-user-guide/administration.md#periodic-jobs): the shipped jobs and why each runs when it does
- [/etc/kdos/timers.d/ and ~/.config/kdos/timers.d/](../../kdos/06-reference/configuration.md#etckdostimersd-and-configkdostimersd): the format and the two tiers
- [kdos remind](../../kdos/04-programs/kdos-command.md#kdos-remind): one-off reminders through the user table
- [Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging): how each system job is supervised
- [Services](services.md), [Logs](logs.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 385.* Previous: [384. Writing a service](writing-a-service.md) · [Contents](../README.md) · Next: [386. Logs](logs.md)

## About

snooze is a single small program that sleeps until the clock matches the time given in its
options, runs one command, and exits. KDOS has no cron daemon: [periodic jobs]{.idx} are one
`snooze` each, started by a supervisor that restarts it after every run, so it sleeps again until
the next slot. That fits the supervisor KDOS already has (see
[No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).

At boot `/etc/init.d/18_timers.sh` starts one supervised `snooze` per line of
`/etc/kdos/timers.d/*.timer`; the shipped ones run the update check and `fstrim -a`. Each user
session runs the lines of `~/.config/kdos/timers.d` in a loop of its own around `snooze`, and
`kdos remind` arms a `snooze` per reminder. The format of a timer line, and how a missed slot is
caught up with `-s` and `-t`, is in
[Periodic jobs](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#periodic-jobs).

## Flags

- `PREFIX=/usr` — The Makefile has no configure step and installs under `/usr/local` by default;
  this puts `snooze` in `/usr/bin` and its manual page in `/usr/share/man/man1`.
- `CFLAGS="$CFLAGS"` — Replaces the Makefile's own `CFLAGS = -g -O2 -Wall -Wextra
  -Wwrite-strings` with the tree's release flags. A variable on the `make` command line overrides
  the Makefile's assignment; the environment alone would not.

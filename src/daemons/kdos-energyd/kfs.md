## About

`kdos-energyd` attributes the processor's energy use to applications: it samples the kernel's
RAPL energy counters under `/sys/class/powercap`, which only root may read, and divides the
energy among the running programs, naming a boxed application by the container its processes run
in. Every boxed application is its own rootless container under a known name, so the
attribution starts from an identity rather than a guess. It reports relative shares, never
watt-hours. `kdos-energy` prints its report, and System Monitor's KDOS energy and box pages read
`kdos-energy --json`, whose rows carry the app and its box as separate fields beside the display
name. What it measures and how it splits the figure is in
[kdos-energyd](../../../docs/kdos/04-programs/daemons.md#kdos-energyd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles the daemon's own files with
the sources of `libkbase` and `libkproc` and nothing else. The daemon walks a process's parent
chain itself; `libkproc` recognises podman's per-container supervisor, `conmon`, where the walk
stops, and reads the box's name from its command line. See
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built) and
[libkproc](../../../docs/kdos/05-developer/c-libraries.md#libkproc).

## Notes

One binary has two names: `/usr/sbin/kdos-energyd` serves `/run/kdos-energyd.sock`, and the
link `/usr/bin/kdos-energy` runs it as the client. The name it was started under is the whole
switch.

`/etc/init.d/56_energyd.sh` skips the daemon at boot when no `energy_uj` counter is readable at
all. The daemon's own test is narrower: it refuses to start when the first domain it keeps (the
platform's `psys` domain, where there is one) cannot be read. A machine with one readable counter
but an unreadable first domain passes the script's check, and the supervisor then restarts the
refusing daemon every five seconds; see [Supervision and logging](../../../docs/kdos/04-programs/daemons.md#supervision-and-logging).

A ten-second window during which the machine was suspended is left out of the ledger and counted
in the report instead: the daemon compares `CLOCK_BOOTTIME` with `CLOCK_MONOTONIC` across each
window, because the RAPL counter after a resume runs on from wherever the firmware left it, and
the difference across the suspend would otherwise be charged to whatever ran around the lid.

# Build times

This appendix lists every build time the book's snapshot of a built tree holds, phase by phase and
in build order, so that the cost of a part of the build can be judged before it is started. It says
first what the unit is, which tree the figures were measured on, which machine measured the unit,
and which package phases have measured ports.

## The unit

A time is given in [SBUs]{.idx term="SBU"}, standard build units. One SBU is the time the first step of the
whole build took, the cross binutils, `00_cross/binutils`, and every other figure is a step's own
time divided by it. A figure is therefore a proportion of one machine's speed, not a time: a port
shown at 2.0 SBU took twice as long as the cross binutils did on the machine that built both. What
moves a figure, the job count, the compiler cache, whether the step did any work and which run
measured it, is explained
in [Build times and the SBU](../00-front/conventions.md#build-times-and-the-sbu).

kdosbuild records each step's seconds in `build/snapshots/timings.json`, keyed by phase and step;
see [Step timings](../../kdos/05-developer/build-system.md#step-timings). `make book-data` divides
each port's figure by the SBU and writes it into `docs/kfs/data/packages.tsv`, the file this page is
generated from. A step that did no work, because its port was installed and current or came from
the package store, keeps the figure of the last run that did, so a port missing from the tables is
one whose build no run of that tree measured. A figure below 0.1 SBU is shown as 0.1.

The scripts of `00_cross` and `10_bootstrap` are steps, not ports, and their times are not part of
the snapshot's package table; their chapters in Parts II and III give each one's time.

## Figures from different runs

`timings.json` is merged, not replaced, so the figures of one snapshot come from every run that
built a port of that tree: a `--continue-from` or `--rebuild` run adds the ports it built and
leaves the others' figures as an earlier run measured them. A phase whose ports no run measured
has no table, and [The measured times](#the-measured-times) names the phases that do before its
tables and the phases that do not after them; a phase only part of whose ports were measured says
how many, and its sum is then less than the phase costs. A port is looked
up under the phase its list now names, and, when its list has moved since it was measured, under
the one other package phase that holds a figure for it.

kdosbuild files each figure under the machine of the run that measured it: the processor, the CPUs
the build could run on, the memory and `KDOS_JOBS`. The stamp names the SBU's machine, and a port
figure from another machine, or one that names none, is marked `*` in the tables, and its package
page says so, because it is divided by an SBU it was not shown to be measured beside. A figure that names no machine came
from a kdosbuild that filed none; when the SBU's own figure is one of those, no figure can be shown
to share its machine, and the opening of [The measured times](#the-measured-times) says so instead
of marking them.

Nothing but `timings.json` measures a port's build. An install log
(`build/logs/<phase>/<n>_<port>.install.log`) carries no time, `build/logs/snapshots.log` stamps
each of kdosbuild's notices (a phase's start, a snapshot, a restore, a retry, a failure) with the
moment it was written but records no duration, and a log's file times are not a
duration, because a later run rewrites the log in place: its birth time stays that of the first
run, its modification time is that of the last, which may only have found the port already built.

## The measured times

<!-- kfs:sbu -->

## See also

- [Typographical conventions](../00-front/conventions.md#build-times-and-the-sbu): the SBU, and what moves it
- [kdosbuild](../01-preparing/kdosbuild.md#step-timings): where the seconds are recorded
- [What a build costs](../../kdos/05-developer/how-kdos-is-built.md#what-a-build-costs): the whole build in hours and disk
- [The package store](../../kdos/03-architecture/packaging.md#the-package-store): when a port is installed from a stored package instead of built

<!-- book-nav -->
---

*Part XVII. Appendices, chapter 424.* Previous: [423. Flag glossary](flag-glossary.md) · [Contents](../README.md) · Next: [425. Licences](licences.md)

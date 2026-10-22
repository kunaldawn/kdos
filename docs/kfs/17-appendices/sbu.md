# Build times

This appendix lists every build time the book's snapshot of a built tree holds, phase by phase and
in build order, so that the cost of a part of the build can be judged before it is started. It says
first what the unit is and which tree the figures were measured on.

## The unit

A time is given in [SBUs]{.idx term="SBU"}, standard build units. One SBU is the time the first step of the
whole build took, the cross binutils, `00_cross/binutils`, and every other figure is a step's own
time divided by it. A figure is therefore a proportion of one machine's speed, not a time: a port
shown at 2.0 SBU took twice as long as the cross binutils did on the machine that built both. What
moves a figure, the job count, the compiler cache and whether the step did any work, is explained
in [Build times and the SBU](../00-front/conventions.md#build-times-and-the-sbu).

kdosbuild records each step's seconds in `build/snapshots/timings.json`, keyed by phase and step;
see [Step timings](../../kdos/05-developer/build-system.md#step-timings). `make book-data` divides
each port's figure by the SBU and writes it into `docs/kfs/data/packages.tsv`, the file this page is
generated from. A step that did no work, because its port was installed and current or came from
the package store, keeps the figure of the last run that did, so a port missing from the tables is
one whose build no run of that tree measured. A figure below 0.1 SBU is shown as 0.1.

The snapshot's figures cover the ports of `20_selfhost`, `30_foundation` and `31_compilers` and
none of a later phase: the `timings.json` of the tree it was taken from holds no port of `40_lang`
or after, and nothing else that build left measures one. An install log
(`build/logs/<phase>/<n>_<port>.install.log`) carries no time, `build/logs/snapshots.log` stamps
only events of a whole phase (its start, a retry, a failure), and a log's file times are not a duration, because a
later run rewrites the log in place: its birth time stays that of the first run, its modification
time is that of the last, which may only have found the port already built. The tables below fill
in for those phases from the next `make book-data` over a tree whose `timings.json` holds them.

The scripts of `00_cross` and `10_bootstrap` are steps, not ports, and their times are not part of
the snapshot's package table; their chapters in Parts II and III give each one's time.

## The measured times

<!-- kfs:sbu -->

## See also

- [Typographical conventions](../00-front/conventions.md#build-times-and-the-sbu): the SBU, and what moves it
- [kdosbuild](../01-preparing/kdosbuild.md#step-timings): where the seconds are recorded
- [What a build costs](../../kdos/05-developer/how-kdos-is-built.md#what-a-build-costs): the whole build in hours and disk
- [The package store](../../kdos/03-architecture/packaging.md#the-package-store): why a port installed from it records no time

<!-- book-nav -->
---

*Part XVII. Appendices, chapter 424.* Previous: [423. Flag glossary](flag-glossary.md) · [Contents](../README.md) · Next: [425. Licences](licences.md)

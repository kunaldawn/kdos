## About

`kdos-oomd` kills the process that is starving the machine of memory before the desktop freezes.
It writes a trigger into the kernel's memory-pressure file, `/proc/pressure/memory`, sleeps until
the kernel reports a stall, and then kills one process: the largest in a box over its declared
memory budget first, otherwise preferably a boxed one, and never the compositor, the panel, the
audio session or the other programs the desktop is made of. It frees the victim's memory at once
with `process_mrelease()`. It acts at most once every ten seconds. The kernel's own out-of-memory
killer fires only when an allocation fails, which on a machine with swap can be long after the
session stopped responding. How it chooses, and the socket `kdos doctor` asks, are in
[kdos-oomd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/daemons.md#kdos-oomd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and `libkproc`, the library that reads the process table and names the box a
process belongs to, and nothing else; see
[How the libraries are built](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

The daemon needs PSI in the kernel. `/etc/init.d/57_oomd.sh` skips it at boot when
`/proc/pressure/memory` is not writable, and the machine then has only the kernel's own killer.
It is installed under one name only, with no client link. Its socket takes no argument and
answers `ping` and `status`; `kdos doctor` reports whether the socket is present.

The protected programs are matched by process name, a list compiled into `main.c`. A desktop
program renamed, or a new one added to the session, is a candidate victim until the list names it.

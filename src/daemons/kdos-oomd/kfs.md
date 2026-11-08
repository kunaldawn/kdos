## About

`kdos-oomd` kills the process that is starving the machine of memory before the desktop freezes.
It writes a trigger into the kernel's memory-pressure file, `/proc/pressure/memory`, sleeps until
the kernel reports a stall, and then, when `MemAvailable` is also at most a tenth of `MemTotal`,
kills one process: the largest in a box over its declared memory budget first, otherwise
preferably a boxed one, and never KWin, plasmashell, the login, the audio session or the other
programs the session is made of. It frees the victim's memory at once
with `process_mrelease()`. It acts at most once every ten seconds. The kernel's own out-of-memory
killer fires only when an allocation fails, which on a machine with swap can be long after the
session stopped responding. How it chooses, and its socket, are in
[kdos-oomd](../../../docs/kdos/04-programs/daemons.md#kdos-oomd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and `libkproc` and nothing else. The daemon reads the process table from `/proc`
itself; `libkproc` names the box a process belongs to, by walking its parent chain up to podman's
`conmon`, and moves the `/proc` root for the `--fixture` mode. See
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

The daemon needs PSI in the kernel. `/etc/init.d/57_oomd.sh` skips it at boot when
`/proc/pressure/memory` is not writable, and the machine then has only the kernel's own killer.
It is installed under one name only, with no client link. Its socket, `/run/kdos-oomd.sock`,
answers `ping`, `status` and `protected` from root, from members of `wheel` and from a user with
an active session at the machine, and nothing in the protocol names a process;
`kdos-oomd --explain <user>` says which of the three admits a user.

The active session is elogind's answer, `sd_uid_get_state()`, and `libelogind.so.0` from
[elogind](#port-elogind) is opened when the first caller outside root and `wheel` connects rather
than linked. elogind is therefore not a dependency: without it the daemon still starts and kills,
and only that third tier is closed.

The protected programs are matched by process name, a list compiled into `main.c`, and any
name that begins with `pipewire` is protected as well. A process name is the executable's name cut
at 15 characters, so each entry is the cut form (`kwin_wayland_wr`, `kscreenlocker_g`,
`polkit-kde-auth`); `kdos-oomd --protected` prints the list. A session program renamed, or a new
one added to the session, is a candidate victim until the list names it. Any other process is
spared only when its `oom_score_adj` is -500 or lower, a value only root can set and which its
children inherit.

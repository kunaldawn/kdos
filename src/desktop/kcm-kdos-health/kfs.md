## About

`kcm-kdos-health` is the Health page, listed in Info Center beside About this System and under
System in System Settings, and the six KDOS rows of About this System itself. The page runs
`kdos doctor --json` and shows one row per check under doctor's own section headings: passed,
not checked on this machine, or needing attention, the last in the negative colour and named
"Needs attention" to a screen reader. Check Again runs doctor again, Copy to Clipboard copies
the report as `kdos doctor` prints it, and a filter narrows the rows. It changes nothing, so it
has no Apply.

The rows of About this System are six POSIX shell scripts installed in `/usr/libexec/kdos`:
`about-slot` (the A/B slot, its version and state, from `/run/kdos/slot.json`, or the slot alone
from the boot entry's `kdos_slot=` when that file is absent, or `Live medium` when neither names a
slot and the root is an overlay), `about-update` (the key that
verified the last update offered and when, from `/var/lib/kdos/update.json`), `about-tree` (the
commit and date of `/ports`, from its `TREE` stamp), `about-accent` (the KDOS accent, and the
login screen's when it differs), `about-libc` (musl's version, from kpkg's record) and
`about-boxes` (how many box profiles this user has). Each prints its label ending in a colon,
since Info Center shows the label verbatim beside its own rows, which end in one. The list that names them is
`/usr/share/kdos/xdg/kcm-about-distrorc`, shipped under `fs/`. The port is KDOS's own code
with no upstream source, built with CMake against KF6 KCMUtils.

## Flags

- `-D KDE_INSTALL_LIBEXECDIR=libexec` — Put the about scripts in `/usr/libexec/kdos`, the
  paths `kcm-about-distrorc` names. ECM's default for this tree's layout is `lib/libexec`, where
  About this System would find none of them.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. `testing/plasmatest.sh` runs
  `healthtest` against `tests/fake-doctor`, and `testing/selftest.sh` runs
  `tests/aboutrows.sh` on a bare host.

## Notes

kinfocenter runs every script `ExtraSoftwareData` names twice, on the GUI thread, and ends
System Settings when one prints fewer than two lines. So each script prints a label and a value
on every machine and on every error, `Unknown` when it knows nothing, reads only small state
files and never asks podman, the ESP or the network, and sets its own `PATH`. A script named in
`kcm-about-distrorc` that this package does not install would print nothing, so the two change
together. `tests/aboutrows.sh` checks every row with no environment, in the English pass, on a
root with no state at all, with a read-only `/var` and with state files written to break it,
and checks that the shipped list names exactly the six scripts.

`about-update` reads `update.json` as the one line of JSON `kdos update check` writes and takes
the key and the time from the top-level `signed_by` and `verified` fields only. A quote inside a
JSON string is always escaped, so release notes or a signer's name holding the same words cannot
stand in for them; a key that is not 16 hex digits is not shown.

kinfocenter's own command-output pages use `CommandOutputContext`, which is kinfocenter's
private library: it installs no header and says it may not be linked to, and its plain text
cannot mark a failed check as one. The page keeps its shape (a program and its arguments, ready,
error and explanation, a filter, refresh) and reads doctor's JSON records instead. Doctor exits
1 when a check warned, which is a report like any other; only another exit status, output that
is not a report, or a `kdos` that cannot be started is shown as an error.

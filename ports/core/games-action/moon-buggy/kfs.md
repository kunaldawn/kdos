## About

Moon-buggy is a curses game for the terminal: a buggy drives across the moon, and the player jumps
its craters and shoots the meteors in its way. It runs in any terminal; the desktop entry the
recipe writes opens it in one.

## Notes

`hangup-restore.patch` is what keeps the game from crashing when its terminal goes away, as it
does when the terminal window is closed with the game running. `fatal()` calls
`prepare_for_exit()`, which calls `term_restore()`, and upstream's `term_restore()` calls `fatal()`
when `tcgetattr()` fails. On a terminal that has hung up every `tcgetattr()` fails, so those calls
recurse until the stack runs out and the game dies of `SIGSEGV`. The patched `term_restore()`
clears each flag before it acts on it and ignores the result, so the game exits with
`Fatal error: Cannot read keyboard input` and status 1, or of the `SIGHUP` it was sent, whichever
it reaches first.

`build.sh` installs through `install-exec install-data-local install-man install-info-am` rather
than `make install`, which also runs upstream's `install-data-hook`. That hook runs the
just-installed binary with `-c` to create the shared score file, and the binary writes to the
directory compiled into it, `/var/lib/moon-buggy`, which `DESTDIR` does not reach: the file would
be written on the build machine, or as the builder's own `~/.mbscore` where that directory is
missing, and never into the package. Without the hook the package ships the empty
`/var/lib/moon-buggy` alone, which a user cannot write to, so the game keeps each user's high
scores in `~/.mbscore`.

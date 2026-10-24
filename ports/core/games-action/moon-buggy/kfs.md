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

## About

aha reads text carrying ANSI colour and attribute escapes on standard input and writes it as
HTML, keeping the colours, bold, underline and blink. [kinfocenter](#port-kinfocenter) runs it at
run time to turn the coloured output of the command-line tools behind several of its pages into
HTML it can show.

## Notes

The `Makefile` appends its own `-std=c99 -Wall -Wextra` to the exported `CFLAGS` and links with
`LDFLAGS`, so the tree's release and hardening flags reach the program with nothing passed.
`PREFIX=/usr` on the install line replaces upstream's `/usr/local`, which the package lint
refuses. The install puts `aha.1` in `/usr/share/man/man1` as well as the program.

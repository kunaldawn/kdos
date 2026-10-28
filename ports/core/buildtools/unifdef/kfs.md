## About
[unifdef]{.idx} removes C preprocessor conditionals from a file, keeping or dropping each
`#if`/`#ifdef` block according to the symbols it is told are defined or undefined, and leaves
the rest of the file as it was. KDOS carries it for [webkitgtk](#port-webkitgtk) and
[webkitgtk6](#port-webkitgtk6), which name it in `depends`.

## Notes
unifdef's makefile installs under `${HOME}` unless told otherwise, so `prefix=/usr` on the
install line is what puts `unifdef` and the `unifdefall` script in `/usr/bin` and their manual
pages in `/usr/share/man/man1`.

The patch is upstream's own (its `Patch-Source:` line names the pull request). It renames a
variable called `constexpr`, which is a keyword in C23, GCC's default dialect. The makefile
compiles with the tree's `CFLAGS`, whose `-std=gnu11` does not make it a keyword, so the patch
matters to any compile of `unifdef.c` that does not carry those flags; see
[The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags).

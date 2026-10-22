## About
bat is `cat` with syntax highlighting, line numbers and a Git change gutter. On KDOS it is also
the reader of manual pages: `/etc/bash.bashrc` aliases `cat` to `bat --paging=never
--style=plain` and, where the port's pager script exists, sets `MANPAGER` to it. The syntax and
theme sets bat highlights with are compiled during this build, and installed under
`/usr/share/bat/assets` as well, where [delta](#port-delta) and [presenterm](#port-presenterm)
take theirs from.

## Flags
- `BAT_ASSETS_GEN_DIR="$SRC/gen"` — Where bat's build script writes the manual page and the
  shell completions it renders from templates; the default is cargo's hashed `OUT_DIR` under
  `target/`, which no install line can name. The page and the completions are installed from
  `gen/assets/`.
- `--source=assets`, `--target=assets` — Compile the syntax and theme sets from the unpacked
  grammars into `assets/`, the directory bat's `include_bytes!` reads, with `--blank` so nothing
  from bat's own embedded sets is merged in and `--acknowledgements` so the licence texts are
  collected. `--source` stays relative: a syntax set records each grammar's path as given, and an
  absolute one would put the build directory into the set's bytes.

## Notes
Three archives make the build. The release tarball carries bat's highlighting sources as empty git
submodules; `bat-assets-<version>.tar.xz` is those submodules at the tag's pinned commits, cut
down to the grammars, themes and licence files the set compiler reads, and no tool writes it, so it
is remade by hand on a version bump with the commands in
[A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
`bat-vendor-<version>.tar.xz` holds the crates, from `cargo vendor`. `build.sh` unpacks the assets
over the empty directories and applies upstream's own `assets/patches` to them.

The build runs cargo twice. The first build makes a `bat` that can compile sets; `bat cache
--build` then writes `syntaxes.bin`, `themes.bin` and `acknowledgements.bin`, and the second build
embeds them, recompiling only the bat crate.

The tree's environment makes `git2` and `onig_sys` link the system [libgit2](#port-libgit2) and
[oniguruma](#port-oniguruma). `build.sh` checks the binary NEEDs both libraries, so a crate that
falls back to compiling its bundled copy stops the build.

`/usr/libexec/bat/man-pager` strips the bold and underline that `man` writes as escape sequences
and overstrikes, and hands the plain page to `bat -l man -p`, which highlights it. It is a script
rather than a pipeline in `MANPAGER` because mandoc's `man` splits `MANPAGER` on spaces and runs
it with no shell.

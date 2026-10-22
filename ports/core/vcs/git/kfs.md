## About

Git is the distributed version control system. On KDOS it is a command of its own and the
program that `git-lfs`, `tig`, `lazygit`, `git-cola`, `jujutsu`, `pass` (which keeps its password
store in a Git repository), `notmuch`, `helix` and the other ports that name it in `depends` run.

Git has no `configure` step in this recipe: its `Makefile` reads every choice as a make variable,
so the whole configuration is the `_mk` array, passed unchanged to the main build and to
`contrib/subtree`.

## Flags

- `gitexecdir=/usr/lib/git-core` — where the helper programs go: the `git-<command>` executables
  and scripts that `git <command>` runs, `git-remote-https` among them. Upstream's default is
  `$(prefix)/libexec/git-core`. `git --exec-path` prints the directory a given `git` uses.
- `perllibdir` — the directory for Git's Perl modules (`Git.pm` and the rest), which the Perl
  commands such as `git send-email` load. It is set to what Perl reports as its own
  `installvendorlib`, `/usr/share/perl5/vendor_perl`. Git writes the directory into a `use lib`
  line at the top of each of its Perl commands, so they find the modules wherever they are; this
  location is also on Perl's own `@INC`, so any other Perl program can `use Git`. Upstream's
  default, `$(prefix)/share/perl5`, is not on `@INC`.
- `PYTHON_PATH=/usr/bin/python3` — the interpreter written into the `#!` line of `git-p4`, Git's
  one Python command. Upstream's default is `/usr/bin/python`.
- `NO_REGEX=NeedsStartEnd` — build Git's own copy of the GNU regex engine, in `compat/regex`,
  instead of using the C library's. Git needs the `REG_STARTEND` flag, which musl's `<regex.h>`
  does not define; without this switch `git-compat-util.h` stops the compile with "Git requires
  REG_STARTEND support. Compile with NO_REGEX=NeedsStartEnd". Any non-empty value turns the switch
  on; `NeedsStartEnd` is the value upstream's `config.mak.uname` uses for the systems that need
  it.
- `NO_TCLTK=Yes` — do not install `gitk` or `git gui`, which are Tcl/Tk programs. Tk is built in
  `42_graphics`, and Git in `30_foundation`.
- `NO_RUST=Yes` — build without the parts of Git written in Rust, using their C versions. Rust is
  built in `31_compilers`, the phase after this one, so there is no `cargo` in the chroot when Git
  is built.
- `USE_LIBPCRE2=Yes` — link PCRE2 for Perl-compatible regular expressions: `git grep -P`,
  `git log --perl-regexp` and `grep.patternType=perl` need it.
- `install-man` — the target that builds and installs the manual pages, with `asciidoc` and
  `xmlto`, both in `depends`. `make install` alone installs none.

## Notes

The second `make`, in `contrib/subtree`, builds and installs `git subtree` and its manual page:
it is shipped in Git's source but not built by the top-level Makefile.

`build.sh` installs two shell files from `contrib/completion`. `git-completion.bash` goes to
`/usr/share/bash-completion/completions/git`, where bash-completion loads it the first time `git`
is completed. `git-prompt.sh` goes to `/usr/share/git`, for a prompt to source; nothing sources it
by default. The zsh completion in the same directory is not installed: as
`site-functions/_git` it would come first in zsh's `fpath` and shadow zsh's own, fuller `_git`
with a wrapper around the bash one.

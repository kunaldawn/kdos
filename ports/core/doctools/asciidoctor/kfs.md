## About

Asciidoctor converts AsciiDoc text into HTML, DocBook and manual pages. It is a Ruby program,
installed as a gem. In KDOS its use is at build time: ports whose manual pages are written in
AsciiDoc generate them with it, and the ports that name it in `depends` include `perf`,
`bpftrace`, `wireshark`, `weechat`, `newsboat`, `git-lfs`, `keepassxc` and `ccache-manual`. A
build that looks for `asciidoctor` on `PATH` uses it whenever it is there, so
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages) has every port
whose pages come from it name it in `depends`.

## Flags

- `GIT_DIR` — set to a directory that does not exist while `gem build` runs. The gemspec lists the
  gem's files with `git ls-files` and falls back to every file in the tree only when that prints
  nothing. Run from inside a Git checkout, `git ls-files` answers for that checkout rather than for
  the unpacked source; with `GIT_DIR` pointing nowhere it prints nothing wherever the build
  directory sits, and the gem's file list comes from the unpacked tree.
- `--local` — install the `.gem` file just built, and never contact a gem server.
- `--ignore-dependencies` — install without resolving any other gem; asciidoctor declares no
  runtime dependencies, so this skips a lookup that would find none.
- `--no-user-install` — install into the given directory, not a per-user gem directory under
  `$HOME`.
- `--no-document` — generate no rdoc or ri documentation for the gem.
- `--install-dir` — the gem repository to install into: `$PKG` followed by Ruby's default gem
  directory, which `ruby -e 'print Gem.default_dir'` prints, so the installed gem lands where
  the system's `ruby` looks for gems.
- `--bindir` — where the `asciidoctor` command goes: `$PKG/usr/bin`. rubygems writes a small Ruby
  stub there that loads the gem.

## Notes

The gem is built from the source tree and installed from that file, rather than by copying `lib/`
into place, so that it is a real installed gem: `gem list` shows it and the `asciidoctor` command
is rubygems' own stub. `build.sh` then deletes the gem repository's `cache/` directory, which would
hold a second copy of the gem as a `.gem` archive, and installs the manual page from `man/`,
which `gem install` leaves inside the gem.

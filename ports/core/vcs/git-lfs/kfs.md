## About
[Git LFS]{.idx} stores large files outside a git repository: the repository holds small pointer
files, and the `lfs` filter swaps them for the real content on checkout and back on commit.
Without it a clone of such a repository still succeeds, and checks out the pointer files in place
of the files. It is a Go program built with cgo off from its vendor bundle, the second
`sha256 =` line, which `ports/fetch` generates with `go mod vendor` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `-X github.com/git-lfs/git-lfs/v3/config.Vendor=KDOS` — the vendor name in git-lfs's version
  string, `git-lfs/<version> (KDOS; linux amd64; go …; git v<version>)`, which `git lfs version` prints and which
  is sent as the HTTP user agent. Unset, it is `GitHub`. Upstream's Makefile sets the same
  variable from its `VENDOR`.
- `-X github.com/git-lfs/git-lfs/v3/config.GitCommit=v$version` — the commit in the same string.
  Upstream's Makefile takes it from `git rev-parse`, which a release archive cannot answer, so the
  tag stands in for it.
- `VERSION=v$version` — with `GIT_LFS_SHA`, given to every `make` call. The Makefile otherwise
  asks `git describe` and `git rev-parse` of whatever repository encloses the build directory.

## Notes
`go generate ./commands` must come before `go build`. It runs `docs/man/mangen.go`, which writes
`commands/mancontent_gen.go`, the text of `git lfs help <command>`, from the pages under
`docs/man`; the release archive does not carry that file.

The manual pages are built with [asciidoctor](#port-asciidoctor) from upstream's own list,
`MAN_ROFF_TARGETS`, which names each page under its section's directory, `man/man1`, `man/man5`
or `man/man7`. `make man` would
also write an HTML copy of every page, so `build.sh` asks the Makefile for the list and builds only
those targets. The list is printed by the recipe of a rule given with `--eval`: a recipe is
expanded when it runs, after the Makefile has been read, while an `--eval` string is read before
any makefile, so a prerequisite list written there would expand to nothing.

The package owns `/etc/gitconfig`, which registers the `lfs` filter for every user, the block
`git lfs install --system` would write. Git hands file content to git-lfs only through that filter, so
the program alone changes no checkout. Owned by the package, the filter is removed with it; left behind, a filter
marked `required` would fail every checkout of an LFS repository on a missing program. Another
port that installs `/etc/gitconfig` stops at the
[conflict scan](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file) unless its
recipe names git-lfs in `replaces`.

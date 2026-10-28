## About

pandoc converts between document formats: Markdown in its several dialects, HTML, LaTeX, DOCX,
ODT, EPUB, man pages, typst and many more. KDOS needs it to build this book: the `kdos-kfs` port,
which ships *KDOS from Scratch*, depends on it, and so does `openjdk`; `ripgrep-all` runs it to
search inside documents.

It is a Haskell program, built with the `ghc` and `cabal-install` ports of the same phase. The
source is Hackage's `pandoc-cli` package, which holds only the `pandoc` executable and its three
manual pages; the converter itself is the `pandoc` library, and the Lua engine and the HTTP server
are `pandoc-lua-engine` and `pandoc-server`. Those, and every other Haskell package they need,
come from the vendor bundle `pandoc-vendor-<version>.tar.xz`, which `build.sh` serves to cabal as
a local repository; see
[The Haskell bundle](../../../../docs/kdos/05-developer/writing-ports.md#the-haskell-bundle).

## Flags

- `--flags="+lua +server"` — build `pandoc-cli` with the Lua engine (`--lua-filter`, custom
  readers and writers, the `pandoc-lua` interpreter) and the HTTP server (`pandoc-server`). Both
  are on by default in `pandoc-cli.cabal`; the Notes say why they are named.
- `--constraint="pandoc +embed_data_files"` — compile pandoc's data files (templates, reference
  DOCX and ODT documents, abbreviation lists) into the executable. Without it the program reads
  them at run time from the cabal store of the build, a path under the build's work directory that
  does not exist on an installed system, so every DOCX, ODT and EPUB conversion would fail.
- `--enable-split-sections` — compile every Haskell function into a section of its own, so the
  linker can drop the unused code of the many libraries pandoc links.
- `--enable-executable-stripping` — strip the installed executable, which also leaves out any
  debug information.
- `--installdir="$PKG/usr/bin"`, `--install-method=copy` — install the built `pandoc` as a file in
  `$PKG/usr/bin`. cabal's default is a symbolic link into its store, which is under the work
  directory and is deleted with it.
- `-j"$KDOS_JOBS"` — build that many Haskell packages at once, the tree's job count.

## Notes

The port directory carries `cabal.project.freeze`, a freeze pinned by hand, and the vendor bundle
is built from it. An unpinned `cabal freeze` is free to turn flags off to reach a solution, and it
settles on `pandoc-cli -lua -server` and `pandoc -embed_data_files`: a smaller program, and one
whose document conversions fail once installed. The freeze pins those flags on, and the
`--flags` and `--constraint` above repeat them, so a freeze that disagrees makes the offline solve
fail instead of quietly building the smaller program. The freeze also pins the `pandoc` library to
the release this recipe builds, so a version bump regenerates it first, in the new source, with the
`ghc` and `cabal` the recipes name:

```sh
cabal freeze --constraint="pandoc-cli +lua +server" --constraint="pandoc +embed_data_files"
```

and then `ports/fetch pandoc`. Lua is the copy of Lua compiled into the `lua` Haskell
package (`-system-lua` in the freeze), so filters see the Lua pandoc was written against whatever
the system's `lua` port is.

One binary is all three programs. `pandoc-cli` decides by the name it was started under: as
`pandoc-server` it serves conversions over HTTP, and as `pandoc-lua` it is a Lua interpreter with
the pandoc modules loaded. `build.sh` installs the two names as symbolic links to `pandoc`.

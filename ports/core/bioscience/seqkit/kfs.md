## About

[SeqKit]{.idx} is a toolkit for FASTA and FASTQ files, in one Go program: it counts, filters,
samples, deduplicates, converts, locates and rewrites sequences, fast enough to be the tool a
sequence pipeline starts with, before anything is aligned or searched.

## Flags

- `CGO_ENABLED=0` — Build a pure-Go program, with no C compiler and no C libraries involved;
  seqkit binds none.
- `-mod=vendor` — Build from the `vendor/` tree the second tarball unpacks, with no module
  download.
- `-ldflags "-s -w"` — Leave out the symbol table and the DWARF data, which the Go linker
  otherwise writes and `kpkg` does not strip; see [go](../../../../docs/kdos/05-developer/writing-ports.md#go).
- `-o out/seqkit` — Write the binary into a directory of its own. The program's source is the
  package directory `./seqkit`, of the same name as the binary, and `go build -o seqkit` would
  write the binary inside that directory rather than over it, leaving `install` a directory where
  it expects the program.

## Notes

- **The modules are vendored.** The recipe sets `vendoring = go`, so `ports/fetch` runs
  `go mod vendor` against the source and packs the result as `seqkit-vendor-<version>.tar.xz`,
  the second `sha256 =` line; a generated bundle is held to that hash like any download. The
  first line of `build.sh` unpacks it into the source tree. See
  [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
- **The bundle carries fonts.** Among the vendored modules are Go packages that embed the
  Liberation faces, built as upstream publishes them; they are one of the exemptions in
  [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

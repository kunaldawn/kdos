## About

go-pmtiles is the reference tool for PMTiles, a single-file archive format for map tiles. It
installs one program, `pmtiles`, whose subcommands include ones that inspect an archive (`show`, `verify`), read a
tile out of it (`tile`), convert an MBTiles database into one (`convert`), cut a region or a range
of zoom levels out of a larger one (`extract`), and serve the tiles over HTTP (`serve`). KDOS
carries it for what it does with a file already on the disk; `extract` against a hosted planet
archive is its network use, and not why it is here.

The build is the plain Go shape of
[writing-ports.md](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go), with every switch
canonical.

## Notes

The Go modules are not downloaded at build time. `ports/fetch` runs `go mod vendor` against the
release and packs the module `vendor/` tree into `go-pmtiles-vendor-<version>.tar.xz`, whose
`sha256 =` line in the recipe holds it like any other source; `build.sh` unpacks it into the source
tree and builds with `-mod=vendor` (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

The program is built as `out/pmtiles`, not `pmtiles`, because the source tree has a `pmtiles/`
package directory, and `go build -o pmtiles` would write the binary inside it.

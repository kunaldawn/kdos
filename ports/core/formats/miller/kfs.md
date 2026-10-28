## About
Miller (`mlr`) is like awk, sed, cut, join and sort for name-indexed data: CSV, TSV, JSON, and
other record formats. A field is addressed by its name instead of its column number, so
`mlr --icsv --opprint sort -nr price then head -n 5 shop.csv` does in one line what awk needs a
script for, and it converts between the formats as it goes. It is a single Go binary, built without
cgo.

## Notes
`go build` is pointed at `./cmd/mlr`, the main package; the top of the source tree holds only the
regression test. The manual page is upstream's pre-generated `man/mlr.1`, installed as it is.

The Go modules come from the vendor bundle, `miller-vendor-<version>.tar.xz`, made by
`go mod vendor` and pinned by its own `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## About
fx is an interactive terminal viewer for JSON and YAML: it opens a document as a tree to fold,
expand and search, and it also takes JavaScript expressions on the command line to transform the
data. Where [jq](#port-jq) answers a question about a
document whose shape is known, fx is for exploring one that is not. It is a single Go binary, built
without cgo; the recipe writes a terminal desktop entry, "JSON Viewer", that opens a file in it. The entry
claims `application/json` and is `NoDisplay=true`: with no file fx prints its usage and exits, so
it is offered under Open With and has no Start row. Kate stays the type's default in
`kde-mimeapps.list`.

## Notes
The Go modules come from the vendor bundle, `fx-vendor-<version>.tar.xz`, made by `go mod vendor`
and pinned by its own `sha256 =` line; `go build -mod=vendor` builds from it offline (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

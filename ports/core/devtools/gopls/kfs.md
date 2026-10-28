## About
[gopls]{.idx} is the Go language server, which editors run for completion, navigation,
diagnostics and refactoring of Go code. It is built with cgo off from its vendor bundle; see
[go](../../../../docs/kdos/05-developer/writing-ports.md#go).
It works against the installed [go](#port-go) toolchain.

## Notes
The source is the `gopls/v<version>` tag of `golang.org/x/tools`, and gopls is not the module at
the top of that tree: it is the `gopls` subdirectory, with a `go.mod` of its own. The recipe's
`vendordir = gopls` makes `ports/fetch` run `go mod vendor` there (see
[Keys other tools read](../../../../docs/kdos/05-developer/writing-ports.md#keys-other-tools-read)),
and `build.sh` unpacks the bundle into the same directory and builds from it. A bundle made at
the top of the tree would vendor the parent `x/tools` module instead.

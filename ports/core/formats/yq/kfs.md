## About
yq is Mike Farah's command-line processor for YAML, JSON, TOML, XML, HCL, INI, CSV, TSV and
properties files, with a jq-like expression language: `yq '.services.web.image' compose.yaml`
reads a value, and `-i` edits a file in place, keeping its comments. It is a single Go binary,
built without cgo.
[jq](#port-jq) is the JSON tool; yq is the one for the formats jq cannot read.

## Flags
- `-M title=YQ` — With the other `-M` metadata (`section=1`, `author=Mike Farah`), sets the
  manual page's title, section and author when `lowdown` renders it. These are the title, section
  and author upstream's own `scripts/generate-man-page.sh` passes to pandoc as `--variable`s; the
  recipe runs the same step with [lowdown](#port-lowdown) instead.

## Notes
The manual page is built in two steps: upstream's `scripts/generate-man-page-md.sh` concatenates
`pkg/yqlib/doc/operators/headers/Main.md`, the top-level `how-it-works.md`, and the operator and
usage pages from `pkg/yqlib/doc` into one `man.md`, and `lowdown -s -Tman` turns it into `yq.1`.

The shell completions are written by running the freshly built `./yq completion bash|zsh|fish`,
so they always match the binary. The Go modules come from the vendor bundle,
`yq-vendor-<version>.tar.xz`, made by `go mod vendor` and pinned by its own `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

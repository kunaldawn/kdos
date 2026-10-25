## About
glow renders Markdown in the terminal: headings, lists, tables, code blocks with highlighting and
links, in a style that suits a light or dark background. Given a file it prints it rendered; run
with no argument it opens a browser of the Markdown files under the current directory. The port
installs `glow`, a manual page and completions generated from the binary, and a desktop entry that
opens a Markdown file in a terminal as *Markdown Reader*.

## Flags
- `-X main.Version=$version` — Stamp the release into the binary. glow's `main.Version` is
  empty in the source, set only by upstream's release tooling, and `glow --version` otherwise
  reports `unknown (built from source)`.
- `HOME="$PWD/.home"`, `XDG_CACHE_HOME="$PWD/.home"`, `XDG_CONFIG_HOME="$PWD/.home"` — Run the
  freshly built `glow` with a home inside the build tree when it writes its manual page and
  completions. Every start of glow creates a log file in its cache directory and a default
  `glow.yml` in its configuration directory, and these keep both inside the source tree.

## Notes
glow is a pure Go program, built with cgo off from a vendor bundle, `glow-vendor-<version>.tar.xz`,
which `ports/fetch` makes with `go mod vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

`glow man` writes the manual page with mango, which dates the page's header with the day it runs,
so two builds of the port on different days install different `glow.1` files. The `sed` after it
deletes any terminal colour sequence from the page.

## About
[aerc]{.idx} is a terminal mail client that reads IMAP, JMAP, Maildir, mbox and
[notmuch](#port-notmuch) accounts and sends over SMTP. KDOS builds it with its notmuch backend, so
mail already indexed on the machine is searched from the client through that one index. The
shipped `~/.config/aerc/aerc.conf` names the filters that render each kind of message part; see
[Mail, calendar and contacts](../../../../docs/kdos/06-reference/configuration.md#mail-calendar-and-contacts).

## Flags
- `CGO_ENABLED=1` — let the Go compiler build cgo code. aerc's notmuch worker is a cgo binding to
  `libnotmuch`, so without cgo the backend cannot be compiled. Most Go ports in the tree build with
  cgo off; see [go](../../../../docs/kdos/05-developer/writing-ports.md#go).
- `-mod=vendor` — build from the module `vendor/` tree unpacked from the vendor bundle. It is added
  to the exported `GOFLAGS` rather than replacing them, which keeps the tree's `-trimpath`,
  `-buildvcs=false` and `-buildmode=pie`.
- `-tags=notmuch` — compile the notmuch backend. Upstream's makefile sets
  `GOFLAGS ?= $(shell contrib/goflags.sh)`, a probe that adds the tag only after a test compile
  against `notmuch.h` succeeds. `?=` runs it only when `GOFLAGS` is unset, and the tree exports
  `GOFLAGS`, so the probe never runs here: without the tag on this line the client is built with no
  notmuch backend. Named outright, a missing `libnotmuch` stops the build instead.
- `GO_EXTRA_LDFLAGS` — upstream's makefile writes its own `-ldflags` (the version, date and the
  share and libexec directories) and appends this variable to it; `-s -w` there leaves out the
  symbol table and DWARF, which kpkg does not strip.

## Notes
The vendor bundle, `aerc-vendor-<version>.tar.xz`, is `go mod vendor` run at the top of the
source by `ports/fetch` (`vendoring = go`) and held to its own `sha256 =` line in the recipe like
the upstream tarball; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
`build.sh` unpacks it into the source tree, beside `go.mod`.

`build.sh` writes one filter of KDOS's own, `kdos-part`, into aerc's filter directory,
`/usr/libexec/aerc/filters`. aerc puts that directory on the `PATH` it runs a filter with, so
`aerc.conf` names the script by bare word. Its argument picks a renderer: `pdf`
(`mutool` from [mupdf](#port-mupdf)), `image` ([chafa](#port-chafa)), `docx`
([docx2txt](#port-docx2txt)), `html` ([w3m](#port-w3m)) and `ics` (aerc's own `calendar` awk
script, run through [gawk](#port-gawk) by name). The part is spooled to a temporary file first,
because `mutool` opens a document by path.

- chafa runs with `--animate off`. chafa animates a GIF even into a pipe, sleeping each frame's
  delay, and nothing on aerc's side times a filter out, so the sender of an animated picture would
  otherwise decide how long the message takes to open.
- w3m always runs with `-dump`, so the filter never takes the terminal and the output goes to
  aerc's pager. It runs inside a new network namespace (`unshare --map-root-user --net` from
  [util-linux](#port-util-linux)) when the kernel grants one, and otherwise with every proxy set to
  `127.0.0.1:1`, so a remote image or tracking link in the HTML cannot be fetched either way.
- A filter's error output is shown as the message body, so a renderer missing from the image shows
  as `command not found` in place of the message. Each program the script calls is in `depends`.

The desktop entry is KDOS's own, written over upstream's `contrib/aerc.desktop`. It sets
`Terminal=true` and passes `%u`, so a `mailto:` link opened through the entry reaches aerc as its
argument. It claims no MIME type. Every row of `/etc/xdg/kdos-mimeapps.list` names an application
that draws its own window, so `mailto:` and `message/rfc822` go to
[thunderbird](#port-thunderbird) there, and aerc, which needs a terminal around it, is run by name.

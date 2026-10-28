## About
gocryptfs is an encrypted overlay filesystem: `gocryptfs -init ~/vault.enc` creates an encrypted
directory, and `gocryptfs ~/vault.enc ~/vault` mounts its plaintext view through FUSE. Each file
in the plaintext view is stored as one ciphertext file with an encrypted name, so the encrypted
directory can be synced or backed up file by file, and only the files that changed are sent
again. `gocryptfs-xray` inspects the ciphertext files and the master key. The mount goes through
`fusermount3`, from [fuse](#port-fuse).

## Flags
- `CGO_ENABLED=1` — Build with cgo, unlike most of the tree's Go ports, so the OpenSSL crypto
  backend is compiled in and the binary links [openssl](#port-openssl)'s `libcrypto`. With
  `-openssl auto`, its default, gocryptfs picks whichever backend is faster on the processor it
  runs on: OpenSSL is about four times faster than Go's built-in AES-GCM on a processor without AES
  instructions, and Go's is chosen where they exist (`gocryptfs -speed` shows the figures). Built
  with `-tags without_openssl` instead, it is a pure-Go binary with only the Go backend.
- `-X main.GitVersion=v$version` — Stamps the version that `gocryptfs -version` prints. Upstream's
  `build.bash` takes it from `git describe`, or from a `VERSION` file the GitHub tarball does not
  carry; without a stamp, a build from a source tree prints a placeholder asking to be built with
  that script.
- `-X main.GitVersionFuse=vendored` — Stamps the go-fuse version; `build.bash` writes `[vendored]`
  when go-fuse comes from `vendor/`, as it does here.
- `-X main.BuildDate=offline` — Stamps a fixed string as the build date. Upstream's `build.bash`
  stamps the day of the build, or the day `SOURCE_DATE_EPOCH` names; left unstamped, the binary
  prints `0000-00-00`.

## Notes
The binaries are built into `out/`, not the top of the tree: `gocryptfs-xray` is also the name of a
package directory there, and `go build -o gocryptfs-xray` would write the program inside that
directory instead.

The manual pages are written from upstream's Markdown, `Documentation/MANPAGE.md` and
`MANPAGE-XRAY.md`, with [lowdown](#port-lowdown).

The Go modules, go-fuse among them, come from the vendor bundle, `gocryptfs-vendor-<version>.tar.xz`,
made by `go mod vendor` and pinned by its own `sha256 =` line (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

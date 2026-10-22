## About
This port is the system's trust store for TLS: the root certificates of the certificate
authorities Mozilla trusts to issue server certificates, as one PEM bundle. The bundle is
generated at build time rather than downloaded ready-made. The port pins
`certdata.txt` from an NSS release tag (the port's version is that NSS release) and converts it
with curl's `mk-ca-bundle.pl` from a pinned curl release; the two helpers `_tag` and `_curl` in
`kpkgbuild` hold the tag and the curl version.

It installs the Mozilla bundle as `/usr/share/ca-certificates/mozilla.pem`, an empty
`/etc/ca-certificates/trust-source/anchors/` for local roots, the `update-ca-certificates` command,
and the links `/etc/ssl/certs/ca-certificates.crt` and `/etc/ssl/ca-bundle.crt`. The file every
OpenSSL program actually reads, `/etc/ssl/cert.pem`, is not in the package: `postinstall.sh` writes
it on the installed system. How OpenSSL, GnuTLS, NSS and Python each find these anchors, and how to
add a root of your own, is in
[TLS trust anchors](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#tls-trust-anchors).

## Flags
- `-n` — `mk-ca-bundle.pl`: do not download `certdata.txt`, use the one in the current directory.
  The build has no network, and the file is a verified source of the recipe; `build.sh` first
  renames it from `certdata-<version>.txt` to the `certdata.txt` the script expects.
- `-f` — Rebuild the bundle even if `certdata.txt` matches the previous run's.
- `-q` — Print no progress.
- `-d` — The URL `certdata.txt` would be fetched from. With `-n` nothing is fetched; the URL, the
  pinned NSS tag's, is only written into the bundle's header, so the header names the exact source.

## Notes
`mk-ca-bundle.pl` heads its output with a line `## Certificate data from Mozilla as of: <date>`.
With `-n` that date is `certdata.txt`'s modification time on the build machine, not anything
recorded in the source, so `build.sh` deletes the line; left in, it would make two builds of the
same sources differ. `grep -q 'BEGIN CERTIFICATE'` then stops the build if the conversion produced no
certificate at all.

The converter drops every root that has already expired when it runs, so a rebuild after a root
expires ships one fewer even though the sources have not changed.

`/etc/ssl/cert.pem` is written by `postinstall.sh` rather than shipped, because a file the package
owned would be replaced on every upgrade and lose the local roots appended to it.
`update-ca-certificates` builds it in a temporary file and renames it into place, so no reader sees
half a bundle, and it skips with a warning any file in the anchors directory that holds no PEM
certificate, a DER file among them. Run it again after adding or removing a local root.

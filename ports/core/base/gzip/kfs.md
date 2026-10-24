## About
GNU gzip compresses and decompresses the `.gz` format. Besides `gzip` itself, the package installs
`gunzip`, `zcat` and the `z` tools (`zgrep`, `zdiff`, `zless` and the rest), most of them shell
scripts that run `gzip`. `tar` runs `gzip` to read a `.tar.gz`, and many upstream sources ship as
one, so the bootstrap builds gzip before the first port (see [Gzip](#03-bootstrap-060-gzip)). This
port replaces that copy in `30_foundation` and adopts its files; apart from the switches of the
cross build, the two builds are the same.

## Notes
The build writes the path of the `grep` configure found into `zgrep`. The installed script runs
`/usr/bin/grep` unless the `GREP` environment variable names another, and tests that grep for GNU
behaviour before passing it GNU options such as `--label`.

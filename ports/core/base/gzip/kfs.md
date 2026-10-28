## About
GNU gzip compresses and decompresses the `.gz` format. Besides `gzip` itself, the package installs
`gunzip`, `zcat` and the `z` tools (`zgrep`, `zdiff`, `zless` and the rest), most of them shell
scripts that run `gzip`. `tar` runs `gzip` to read a `.tar.gz`, and many upstream sources ship as
one, so the bootstrap builds gzip before the first port (see [Gzip](#03-bootstrap-060-gzip)). This
port replaces that copy in `30_foundation` and adopts its files. Apart from the switches of the
cross build, the two builds run the same commands, but they do not install the same files: see
the note on `zless`.

## Notes
The build writes the path of the `grep` configure found into `zgrep`. The installed script runs
`/usr/bin/grep` unless the `GREP` environment variable names another, and tests that grep for GNU
behaviour before passing it GNU options such as `--label`.

`configure` builds and installs `zless` and `zless.1` only when it finds `less` on the `PATH`,
and `zmore` likewise only when it finds `more`. The bootstrap runs on the build host, which has
both, so it installs `zless`. The port does not name `less` in `depends`, and `30_foundation`
builds it before [less](#port-less) is installed, so its configure finds no `less`: the package
carries no `zless` and does not claim the bootstrap's copy, which stays in the tree as a file no
package owns.

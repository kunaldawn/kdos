## About
tealdeer is a fast client for the tldr pages, the community's short, example-first help pages:
`tldr tar` shows the handful of `tar` command lines people actually need. The program is `tldr`.
A plain tealdeer has no pages until `tldr --update` downloads them, so this port ships them: the
recipe's second source is the tldr-pages release archive, and its English pages are installed
with the program, so `tldr` answers with no network.

## Notes
The pages are installed in the layout of a downloaded cache,
`/usr/share/tldr/tldr-pages/pages.en/`, because tealdeer looks pages up only under
`<cache_dir>/tldr-pages/`. The configuration that points it there is
`/etc/skel/.config/tealdeer/config.toml`, which a new account's home is copied from:

- `cache_dir = "/usr/share/tldr"` — the shipped pages, instead of `~/.cache/tealdeer`.
- `auto_update = false` — tealdeer's own default, written out: an update would rewrite
  `/usr/share/tldr`, which an ordinary user cannot write.
- `warn_cache_age = "never"` — tealdeer judges the cache's age by the modification time of its
  pages directory, and a package's files carry the pinned timestamp packages are rolled with, so
  the warning would print on every lookup.

tealdeer reads no system-wide configuration: only `~/.config/tealdeer/config.toml`, a `config.toml`
in the directory `TEALDEER_CONFIG_DIR` names, or the file `--config-path` names. An account whose home has no
`~/.config/tealdeer/config.toml` falls back to `~/.cache/tealdeer`, finds no pages there, and is
told to run `tldr --update`.

kpkg unpacks the pages archive into `$SRC_ROOT`, beside the program's source, and `build.sh`
copies from `$SRC_ROOT/tldr-<pages version>/pages` by its literal directory name, so a new pages
release changes `build.sh` as well as the `source` line. tealdeer itself is built from a vendor
bundle, `tealdeer-vendor-<version>.tar.xz`, made by `ports/fetch` with `cargo vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

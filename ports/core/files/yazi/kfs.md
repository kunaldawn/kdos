## About

yazi is a terminal file manager written in Rust, with asynchronous I/O and previews of the file
under the cursor, pictures among them. It is installed for those pictures: in
[kdos-term](../../../../docs/kdos/04-programs/kdos-term.md), which draws images in its cell grid,
yazi shows real thumbnails in a window that is otherwise characters. The package installs `yazi`,
its command-line helper `ya`, and their shell completions.

## Flags

- `VERGEN_GIT_SHA="$version"` — yazi's build script stamps the binary with the commit it came
  from through vergen, and its version string reads `env!("VERGEN_GIT_SHA")`. A release tarball
  has no `.git`, and vergen takes the value from this environment variable when it is set, so the
  release tag stands in for the commit the tarball was cut from.
- `VERGEN_IDEMPOTENT=1` — vergen writes the fixed string `VERGEN_IDEMPOTENT_OUTPUT` instead of the
  build date and other values that change from one build to the next, so two builds of the same
  source give the same binary.
- `YAZI_GEN_COMPLETIONS=1` — Makes the build scripts of `yazi-boot` and `yazi-cli` write shell
  completions for `yazi` and `ya` into each crate's `completions/` directory; without it they write
  none. They write them for bash, zsh, fish, Elvish, PowerShell, Nushell and Fig, and `build.sh`
  installs the bash, zsh and fish ones.

## Notes

The crates are vendored: `ports/fetch` runs `cargo vendor` over the source and packs the result
into `yazi-vendor-<version>.tar.xz`, pinned by its `sha256` in `kpkgbuild`, and the build runs
`--frozen --offline` (see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

The `readelf` line stops the build unless `yazi` links `libonig.so.5` from the
[oniguruma](#port-oniguruma) port. The `onig_sys` crate compiles its own copy of oniguruma, with no
message, unless `RUSTONIG_SYSTEM_LIBONIG` is set, which the tree's build environment exports from
`40_lang` on (see [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)); the check
catches the private copy if that export is ever lost.

The previews are other programs, which is why `depends` names them: yazi runs `file` for MIME
types, ffmpeg for video, `pdftoppm` from [poppler](#port-poppler) for PDF, [resvg](#port-resvg)
for SVG, ImageMagick's `magick` for other images, `jq` for JSON, `fd`, `rg`, `fzf` and `zoxide`
for search and jumping, `7zz` from [7zip](#port-7zip) for archives and disk images, and
[chafa](#port-chafa) where the terminal has no image protocol. A missing one is a blank preview,
not an error.

The desktop entry is `Terminal=true` with `X-KDOS-Term=kdos-term`, so the launcher opens yazi in
kdos-term, which speaks sixel and the kitty graphics protocol, rather than in the session's own
terminal (see [Terminal entries](../../../../docs/kdos/04-programs/kdos-appbox.md#terminal-entries)).

## About
dust shows what is filling a disk as a tree: the largest directories under a path, each with a bar
for its share of the whole, sorted so the biggest entries sit next to the prompt. It answers the
question `du -sh * | sort -h` asks, in one command. The interactive shell aliases it as `dud`
(`/etc/bash.bashrc`), and the recipe writes a terminal desktop entry for it.

## Notes
The Rust crates it builds against come from the vendor bundle, `dust-vendor-<version>.tar.xz`,
which `ports/fetch` writes with `cargo vendor` and pins with its own `sha256 =` line; `build.sh`
unpacks it at the top of the tree and cargo builds `--frozen --offline` from it (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

The manual page and the bash, fish and zsh completions are upstream's pre-generated files in
`man-page/` and `completions/`, installed as they are.

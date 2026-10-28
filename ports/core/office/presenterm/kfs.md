## About
presenterm presents a Markdown file as slides in the terminal, with syntax-highlighted code and
pictures. It is built from its
[Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)
and highlights code with the syntax and theme sets the [bat](#port-bat) port compiles. The
package adds a desktop entry, "Slides", which opens a file in a terminal. It is `NoDisplay=true`:
presenterm given no file exits at once, so Start does not list it.

## Flags
- `X-KDOS-Term=kdos-term` — In the desktop entry `build.sh` writes: open the program in
  [kdos-term](../../../../docs/kdos/04-programs/kdos-term.md)
  whatever terminal the session uses, because presenterm draws its pictures with the sixel and
  kitty graphics protocols kdos-term speaks. See
  [Terminal entries](../../../../docs/kdos/04-programs/kdos-appbox.md#terminal-entries).

## Notes
presenterm embeds bat's serialized syntax and theme sets, `bat/syntaxes.bin` and
`bat/themes.bin`, with `include_bytes!`, and upstream ships them prebuilt from a bat checkout.
`build.sh` copies the sets the `bat` port built from source, and its acknowledgements, from
`/usr/share/bat/assets` over those files before cargo runs, so `bat` is in `depends`. The sets
are bincode dumps of syntect's types and carry no version tag: sets written by a `bat` that links
another syntect major version abort presenterm at the first code block, so a bump of either port
has to keep both `Cargo.lock` files on the same syntect. See
[A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
presenterm's own stock themes still come from the dump inside the vendored syntect crate; see
[Known gaps](../../../../docs/kdos/06-reference/known-gaps.md#presenterms-seven-stock-syntax-themes-are-not-compiled-here).

The `readelf` check after the build stops it unless the binary needs `libonig.so.5`: syntect's
`regex-onig` feature links [oniguruma](#port-oniguruma), and the check catches a build that
compiled the crate's bundled copy instead.

## About
Symbols Nerd Font carries the icon code points that "Nerd Font" patched fonts add (file-type icons,
version-control and powerline glyphs, and the rest) as a face of its own, so they can be drawn as a
fallback behind any font instead of from a patched copy of it. Prompt themes, file managers' type
columns and status bars use them. The port installs both upstream faces: `Symbols Nerd Font`, whose
icons may be two cells wide, and `Symbols Nerd Font Mono`, whose icons fit one cell.

## Notes
The release tarball is flat, with the fonts at its top level, and kpkg's `--strip-components=1`
leaves `$SRC` without them, so `build.sh` unpacks the tarball again itself.

Upstream's `10-nerd-font-symbols.conf` is not installed: it puts the proportional face first in
`monospace`, and a terminal then draws a double-width icon in a single cell. The recipe's
`66-nerd-font-symbols.conf` instead `<accept>`s the Mono face behind `monospace`, `Terminus` and
`Terminus (TTF)`, and the proportional face behind `sans-serif` and `serif`, so a symbol face is
used only for code points the requested face lacks. The two Terminus names are listed one by one,
because a request for a concrete family carries no generic family for the `monospace` rule to
match.

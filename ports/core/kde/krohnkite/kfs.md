## About

Kröhnkite is a dynamic tiling script for KWin: new windows take their place in a layout (tile,
monocle, three-column, spiral, quarter, stacked, columns, spread, stair, binary tree, cascade or
floating), and keyboard shortcuts move, resize and swap them. The port is the maintained fork on
Codeberg. It installs as a KWin/Script package under `/usr/share/kwin/scripts/krohnkite/`, and
KWin loads it only while `kwinrc [Plugins] krohnkiteEnabled` is true; its metadata does not enable
it by default, so it ships off. It is meaningful only under KWin, so the port is built in
`50_desktop`.

## Notes

The script is written in TypeScript. `build.sh` compiles it with the TypeScript compiler from a
[node vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring), which holds
that one package at the version upstream's `package-lock.json` pins, and runs it from
`node_modules/.bin` with [nodejs](#port-nodejs); nothing reaches the network. Upstream's
`Makefile` then lays out the package: `make pkg` copies the QML, the compiled `krohnkite.js` as
`contents/code/script.js` and the configuration schema into `pkg/`, and writes `metadata.json`
from `res/metadata.json` with the version and the short commit id filled in. The Makefile reads
both from `git`, which a release tarball has none of, so the recipe passes them as
`PROJECT_VER` and `PROJECT_REV`, the second from the `_rev` helper. The `.kwinscript` attached
to upstream's release is the same tree zipped, plus the Russian and Chinese catalogues, which the
English-only rule leaves out (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
A bump updates `_rev` to the new tag's commit.

When the script loads, it runs its own `contents/code/install-icons.py` through Plasma5Support's
executable engine, with [python3](#port-python3): the script copies the application icon and the
twelve layout icons into the user's `~/.local/share/icons/hicolor` and refreshes the cache with
`xdg-icon-resource` from [xdg-utils](#port-xdg-utils). The port installs the same SVGs in the
system hicolor theme as well, under the names the script gives them, so the icons resolve
before that copy and for a user whose copy fails.

The QML imports `org.kde.kwin` from [kwin](#port-kwin), `org.kde.plasma.core` and
`org.kde.plasma.components` from [libplasma](#port-libplasma), `org.kde.taskmanager` from
[plasma-workspace](#port-plasma-workspace) and `org.kde.plasma.plasma5support` from
[plasma5support](#port-plasma5support); none is checked at build time, and a missing one fails
only when KWin loads the script.

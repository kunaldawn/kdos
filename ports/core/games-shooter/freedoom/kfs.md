## About

Freedoom is free game data for Doom engines: `freedoom1.wad` (Phase 1, four nine-level episodes in
the form of the original Doom), `freedoom2.wad` (Phase 2, a 32-level campaign in the form of Doom
II) and `freedm.wad`, a deathmatch set. Each IWAD is assembled here from the project's PNG
graphics, sound and map lumps by [deutex](#port-deutex); nothing in the package is a prebuilt WAD.
It is the game data for the KDOS Doom engines: [chocolate-doom](#port-chocolate-doom),
[crispy-doom](#port-crispy-doom) and [dsda-doom](#port-dsda-doom) each name this port in their
`depends`.

## Flags

- `VERSION=v$version` — The version string the build writes into the `freedoom.lmp` and
  `freedm.lmp` lumps and draws, through `graphics/text/create_caption`, into the title-screen
  caption. Upstream's makefile takes it from `git describe`, and a release tarball has no git
  history, so without it every WAD would carry the fallback string upstream writes into its
  makefile, which ends in `-unknown`.
- `XML_CATALOG_FILES` — Names `/etc/xml/catalog`, the catalog [libxml2](#port-libxml2) reads
  when the variable is unset, so the export states the default. `a2x` turns `dist/freedoom.adoc`
  into `freedoom1.6`, `freedoom2.6` and `freedm.6` through DocBook, and its `manpage.xsl` imports
  [docbook-xsl](#port-docbook-xsl) by URL. That URL resolves to the local copy only through the
  catalog; with no catalog, xsltproc tries the network and the build fails.

## Notes

`make` builds only the three WADs. `make -C dist` builds the three manual pages and the three
64×64 icons, which `dist/pillow-resize` and `dist/pillow-compose` cut from the game's own graphics
with [python3-pillow](#port-python3-pillow). The PDF manuals are not built: they need
asciidoctor-pdf, which is not a port.

The WADs go to `/usr/share/games/doom`, where every Doom engine in KDOS looks for `freedoom1.wad`
and `freedoom2.wad`.

The three commands `freedoom1`, `freedoom2` and `freedm` are one script, upstream's
`dist/freedoom`, installed under three names. It picks the IWAD from the name it was run by, sets
`DOOMWADPATH` (unless it is already set) to `$XDG_DATA_HOME/games/doom`
(`~/.local/share/games/doom` when `XDG_DATA_HOME` is unset), `/usr/local/share/games/doom`,
`/usr/local/share/doom`, `/usr/share/games/doom` and `/usr/share/doom`, and runs the first engine it finds on `PATH` from its own list: `doom`,
`odamex`, `gzdoom`, `crispy-doom`, `chocolate-doom`, `prboom-plus`. Of those, KDOS has
`crispy-doom` and `chocolate-doom`, so Crispy Doom is the one that starts whenever it is installed,
and the desktop entries say `StartupWMClass=crispy-doom` to match it. dsda-doom is not on the list;
`~/.doomport`, a symbolic link to an engine, or `freedoom2 -p dsda-doom` picks it instead.

`build.sh` writes its own desktop entries, for Phase 1 and Phase 2 only. FreeDM has an icon and a
command but no desktop entry, although upstream ships one in `dist/`.

## About

xdg-utils is the freedesktop.org set of shell scripts that desktop-independent programs call to
reach the desktop: `xdg-open`, `xdg-mime`, `xdg-email`, `xdg-settings`, `xdg-desktop-menu`,
`xdg-icon-resource` and the others. In KDOS the `xdg-open` a user runs is `kdos-appbox`'s, a link
in `/usr/local/bin`, which comes first on the shipped `PATH`; it opens a file in a native or a
boxed handler and falls back to this package's `/usr/bin/xdg-open` when it finds none (see
[kdos-appbox](../../../../docs/kdos/04-programs/kdos-appbox.md#where-it-is-installed)).

## Flags

- `XML_CATALOG_FILES=/etc/xml/catalog` — Make `xmlto`, which renders the scripts' DocBook
  descriptions into manual pages and into the text of each script's `--help`, resolve the DocBook
  DTD and stylesheets through the system catalog rather than by fetching their URLs.

## Notes

The build needs a text-mode web browser: each script's `--help` text is made with `xmlto txt`,
which renders the DocBook to HTML and then runs a text browser over it, and the result is pasted
into the script. [lynx](#port-lynx) is that browser, which is why it is in `depends`; at run time it
is only one of the text browsers `xdg-open`'s generic fallback tries.

`qtpaths6.patch` changes how `xdg-mime default` finds the KDE configuration directory in a Plasma
session. Upstream runs the unsuffixed `qtpaths`, which this tree does not put on `PATH` (Qt 6's is
`/usr/lib/qt6/bin/qtpaths`, Qt 5's `/usr/bin/qtpaths-qt5`), so the unpatched script prints
`qtpaths: command not found` on every call. The patched lookup tries `qtpaths6`, then `qtpaths`, with their
errors discarded, and falls back to `$XDG_CONFIG_HOME` or `~/.config`, the directory Qt names on
Linux; `mimeapps.list` there is the file the generic path writes as well. The patch is to be
offered upstream as a merge request to
[xdg/xdg-utils](https://gitlab.freedesktop.org/xdg/xdg-utils).

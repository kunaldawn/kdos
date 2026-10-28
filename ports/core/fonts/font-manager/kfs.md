## About

Font Manager is a GTK 4 program, written in Vala, for browsing, comparing and previewing the
installed fonts and enabling or disabling them for the user through fontconfig. The package
carries two programs: Font Manager itself, and Font Viewer, which previews one font file and
installs it for the current user.

## Flags

- `-Dmanager=true` — With `-Dviewer=true`: both programs. Upstream's defaults, named.
- `-Dsearch-provider=false` — No GNOME Shell search provider, a `.ini` registration file under
  `share/gnome-shell/search-providers` that only GNOME Shell reads; there is no GNOME Shell here.
- `-Dadwaita=false` — Upstream's own stylesheet rather than libadwaita's Adwaita one. Upstream's
  default, named.
- `-Dwebkit=false` — No Google Fonts integration, the one part of the program that works over the
  network: it browses the Google Fonts catalogue in a WebKitGTK 6 view, and with it off the
  program links neither WebKitGTK nor libsoup.
- `-Dlibarchive=true` — Fonts can be installed from archive files, through
  [libarchive](#port-libarchive). Upstream's default, named.
- `-Dyelp-doc=false` — No help documents, which need yelp-tools to build and a help viewer to
  read; neither is a port.
- `-Dunihan=true` — Compiles in the Unicode Han unification data for the character map.
  Upstream's default, named.
- `-Dnautilus=false` — With `-Dnemo=false` and `-Dthunar=false`: no extensions for those file
  managers, none of which is a port. Upstream's defaults, named.
- `-Dgtk-doc=false` — No API documentation. Upstream's default, named.
- `-Dreproducible=true` — Compiles the C sources with `-DREPRODUCIBLE` instead of
  `-DNOT_REPRODUCIBLE`. With the latter, the build directory's `data/` is compiled into the
  library as a further place to look for the GSettings schemas, so the binary would carry the path
  it was built in.
- `-Dapp-armor=false` — No AppArmor profile. Upstream's default, named.

## Notes

Upstream installs one SVG icon for the two programs. `build.sh` renders it with `rsvg-convert` from
[librsvg](#port-librsvg) into hicolor PNGs, in four sizes, under both application ids.

Upstream's two desktop entries are replaced. Each new entry carries its program's GApplication id,
`com.github.FontManager.FontManager` or `com.github.FontManager.FontViewer`, as its
`StartupWMClass`, since that id is the window's Wayland app_id; upstream's entries name
`font-manager` and `font-viewer`. Only the viewer claims the font types, so a font file has one
handler rather than a choice between two windows of the same package, and the types it claims are
the MIME database's canonical names (`font/ttf`, `font/otf`, `font/collection`): the desktop
resolves a file to one of those from its name, so an alias or an unknown type never matches.

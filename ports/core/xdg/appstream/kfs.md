## About

AppStream is the freedesktop.org standard for describing software: each application's metainfo
file (name, summary, description, screenshots, licence, what it provides) and the catalogues a
software centre reads them from. This port installs `libappstream`, `appstreamcli`, which
validates metainfo files and queries and builds catalogues, the Qt binding `libAppStreamQt` and
`libappstream-compose`. Discover requires `AppStreamQt`, and lists ports, packs and boxes from
catalogues that `appstreamcli compose` writes from the built tree.

## Flags

- `-Dsystemd=false` — No systemd support; KDOS has no systemd.
- `-Dstemming=true` — Stem search terms with [snowball](#port-snowball)'s `libstemmer`, so a
  search for "editing" finds "editor".
- `-Dqt=true` — Build `libAppStreamQt`, the Qt 6 binding Discover and Plasma's application
  launchers read software metadata through. It makes [qt6-qtbase](#port-qt6-qtbase) a dependency,
  which places the port in `43_toolkits`, after the toolkits.
- `-Dcompose=true` — Build `libappstream-compose`, the `appstreamcli compose` subcommand and its
  media worker, `asc-mediaworker`, which renders icons, screenshots and font samples. Compose turns
  the metainfo and desktop entries of a directory tree into a catalogue with its icon cache; the
  image build runs it to produce the catalogue of the ports the image ships. The media worker
  requires [libvips](#port-libvips), [cairo](#port-cairo), [pango](#port-pango),
  [fontconfig](#port-fontconfig) and [freetype2](#port-freetype2).
- `-Dvapi=false` — No Vala bindings.
- `-Dgir=true` — Build the GObject introspection data, so Python and other bindings can load the
  library; this is why [gobject-introspection](#port-gobject-introspection) and
  [glib-introspection](#port-glib-introspection) are in `depends`.
- `-Dtools=true` — Build `appstreamcli`.
- `-Dzstd-support=true` — Read and write zstd-compressed catalogues with [zstd](#port-zstd).
- `-Dblake3-support=false` — Hash compose's caches with GLib's SHA-256 instead of BLAKE3, whose
  library is not a port.
- `-Dapt-support=false` — No Debian APT integration.
- `-Ddisplay-detection=wayland` — Measure the screen through Wayland when a component declares a
  display-size requirement, and never through X11.
- `-Dbash-completion=true` — Install `appstreamcli`'s bash completion.
- `-Ddocs=false` — With `-Dapidocs=false` and `-Dinstall-docs=false`: build and install neither the
  specification nor the API reference.
- `-Dman=true` — Build `appstreamcli(1)` from its DocBook 5 source with `xsltproc` from
  [libxslt](#port-libxslt). The configure runs `xsltproc --nonet` on the namespaced manual-page
  stylesheet's canonical URL and stops when it does not resolve; [docbook-xsl](#port-docbook-xsl)
  installs that stylesheet and maps the URL to it in the XML catalogue.

## Notes

The release tarball wraps its tree as `./AppStream-<version>/`, so `kpkg`'s strip of one component
leaves it one level down, and `build.sh` changes into that directory first.

`build.sh` removes `/usr/share/installed-tests`, test definitions the build installs whatever
the test options say.

`libvips` sits on the same phase boundary as this port: compose needs it at build time, so it is
built no later than this port's phase.

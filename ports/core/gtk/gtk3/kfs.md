## About

GTK 3 is the widget toolkit that many of the tree's GTK applications are written for, among
them [gimp](#port-gimp), [inkscape](#port-inkscape), [firefox-esr](#port-firefox-esr),
[libreoffice](#port-libreoffice) and [virt-manager](#port-virt-manager). It installs beside
[gtk4](#port-gtk4); each has its own library, its own series and its own bindings. Like every
toolkit in the tree it is for applications only, and no part of the KDOS desktop links it (see
[Toolkits are for applications](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).

## Flags

- `XML_CATALOG_FILES=/etc/xml/catalog` — Names the XML catalogue libxml2 reads. With `-Dman=true`
  the build runs `xsltproc --nonet` over the tools' DocBook pages with the manual-page stylesheet
  at `http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl`, and the catalogue
  maps that address onto the installed [docbook-xsl](#port-docbook-xsl); without a mapping,
  `--nonet` refuses the download and the pages fail. The file named is the one libxml2 reads when
  the variable is unset, and the export makes it the one read whatever the build environment
  carries.
- `-Dwayland_backend=true`, `-Dx11_backend=true` — Build both GDK backends, upstream's defaults on
  Linux. `GDK_BACKEND` is left unset in the session, so an application tries Wayland first and the
  X11 backend serves the ones that force it, under Xwayland (see
  [A graphical application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
  GTK 3's AT-SPI accessibility bridge is compiled only beside its X11 backend, so turning X11 off
  would also take away accessibility from every GTK 3 program.
- `-Dbroadway_backend=false` — Build no Broadway backend, which draws a GTK program into a web
  browser over HTML5. It is upstream's default.
- `-Dxinerama=yes` — Make the Xinerama extension a requirement of the X11 backend, through
  [libXinerama](#port-libXinerama). Upstream's `auto` uses it only if found.
- `-Dcloudproviders=false`, `-Dtracker3=false`, `-Dprofiler=false` — No cloud-provider entries in
  the file chooser's sidebar (libcloudproviders), no Tracker search in the file chooser, and no
  sysprof profiler hooks. None of libcloudproviders, Tracker or sysprof is a port; all three are
  upstream's defaults.
- `-Dprint_backends=file,cups` — Build the "Print to File" backend and the [cups](#port-cups) one,
  and fail if CUPS cannot be found. Upstream's `auto` also builds the `lpr` backend, and builds
  without the CUPS one when its library is missing.
- `-Dcolord=no` — No colord colour-management support in the CUPS backend. colord is not a port;
  upstream's `auto` would use it if found.
- `-Dbuiltin_immodules=all` — Compile every input method module (the Wayland text-input module,
  XIM, multipress, and the simple Amharic, Cedilla, Cyrillic, Inuktitut, IPA, Thai, Tigrigna
  and Vietnamese ones) into `libgtk-3` instead of installing each as a loadable module under
  `/usr/lib/gtk-3.0/3.0.0/immodules`. GTK 3 finds a loadable module only through the
  `immodules.cache` that `gtk-query-immodules-3.0` writes, and upstream's install step skips writing
  it under `DESTDIR`; built in, the modules need no cache.
- `-Dinstalled_tests=false`, `-Ddemos=false`, `-Dexamples=false` — Install no test programs and
  build neither `gtk3-demo`, `gtk3-widget-factory` nor the examples; the demos and examples are on
  by default upstream.

## About

NetSurf is a small web browser with its own layout engine, built for slow machines. Its parts
are libraries of the NetSurf project, each a port of its own: [libhubbub](#port-libhubbub)
parses HTML, [libcss](#port-libcss) parses and selects CSS, [libdom](#port-libdom) holds the
document, and [libnsbmp](#port-libnsbmp), [libnsgif](#port-libnsgif) and
[libsvgtiny](#port-libsvgtiny) decode images. JavaScript runs on the copy of Duktape in its
source. This port builds the GTK 3 front end, `netsurf-gtk3`.

## Flags

- `-fcommon` — Added to `CFLAGS`. Places a global variable defined without an initialiser
  in a common block, so the same definition in more than one file is merged into one symbol at
  link time. The compiler's default, `-fno-common`, makes that a multiple-definition link error.
- `-g0` — Added to `CFLAGS`. NetSurf's GTK makefile puts `-g` in its own flags, and it places the
  environment's `CFLAGS` after its own on every compile line, so `-g0` cancels it and the binary
  carries no debugging information.
- `-I$SRC_ROOT/shim` — `utils/idna.c` includes `<libutf8proc/utf8proc.h>`, and
  [utf8proc](#port-utf8proc) installs its header as `/usr/include/utf8proc.h`. `build.sh` makes
  `$SRC_ROOT/shim/libutf8proc/utf8proc.h` a link to the real header and puts the shim directory
  on the include path.
- `TARGET=gtk3` — Build the GTK front end against GTK 3. It is also what an empty `TARGET`
  builds; named, the build does not rest on that default.
- `LIBDIR=lib`, `INCLUDEDIR=include` — The install directories the NetSurf libraries' makefiles
  read. The browser's own makefiles read neither, so they change nothing here.
- `NETSURF_USE_CURL=YES`, `NETSURF_USE_OPENSSL=YES`, `NETSURF_USE_UTF8PROC=YES`,
  `NETSURF_USE_DUKTAPE=YES`, `NETSURF_USE_JPEG=YES`, `NETSURF_USE_PNG=YES`,
  `NETSURF_USE_WEBP=YES`, `NETSURF_USE_JPEGXL=YES`, `NETSURF_USE_BMP=YES`,
  `NETSURF_USE_GIF=YES`, `NETSURF_USE_RSVG=YES`, `NETSURF_USE_NSSVG=YES`,
  `NETSURF_USE_NSPSL=YES`, `NETSURF_USE_NSLOG=YES`, `NETSURF_USE_GRESOURCE=YES` — Fetching
  through curl with OpenSSL certificates, internationalised domain names through utf8proc,
  JavaScript, the image formats, SVG through both librsvg and libsvgtiny, the public suffix list
  through [libnspsl](#port-libnspsl), filtered logging through [libnslog](#port-libnslog), and the
  interface's resources compiled in as a GResource. Many of these default to `AUTO`, which
  enables the feature only when pkg-config finds its library; `YES` stops the build with "Unable
  to find library" instead.
- `NETSURF_USE_ROSPRITE=NO` — No RISC OS sprite images; librosprite is not a port.
- `NETSURF_USE_HARU_PDF=NO` — No PDF export through libharu; upstream's default. The switch has
  no `AUTO` detection, and `YES` links `-lhpdf` directly, which would need
  [libharu](#port-libharu) in `depends`.
- `NETSURF_USE_VIDEO=NO` — No video; upstream's default. The GTK front end looks for
  `gstreamer-0.10`, and the tree's [gstreamer](#port-gstreamer) is not that series.
- `GTK_TRANSLATIONS_HTML=en` — Install the English resource tree only. The default list installs
  German, French, Italian, Japanese, Dutch and Chinese pages and messages as well. The system is
  English only (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  and these trees hold plain `Messages` files and HTML pages rather than message catalogues, so
  kpkg's prune would leave them in the package.

## Notes

Upstream's `-src` tarball contains the browser alone; each NetSurf library in `depends` is built
from its own port first, and the browser finds them through pkg-config.

`make install` installs the binary as `netsurf-gtk3` and its resources under
`/usr/share/netsurf/`, but no manual page, icon or desktop entry. `build.sh` installs
`docs/netsurf-gtk.1` under the binary's name, `netsurf-gtk3.1`, and makes a 128-pixel hicolor icon
from `frontends/gtk/res/netsurf.xpm`. That picture is 132 by 135 pixels; it is first centred on a
transparent 136-pixel square, so the scaled icon keeps its proportions. The desktop entry is
written last.

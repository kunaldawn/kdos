#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# :ccopy and :cpaste go through wl-copy and wl-paste. Upstream bakes xclip into
# the default config, and there is no X clipboard on this image for it to
# reach; the patch is the only way in, because the Makefile appends its own
# -D after anything exported.
patch -p1 -i $PORT_SRC/wayland-clipboard.patch

cd src

# A SPREADSHEET IS A GRID OF CELLS, WHICH IS WHAT THIS WHOLE DESKTOP IS. That
# is not a joke about the styling guide — it is why sc-im fits here where a
# GUI spreadsheet needs a container: the data model and the display model are
# the same thing, so it runs on tty1 at the console font with no compromise.
#
# libxml2 and libzip are what let it READ .xlsx, and libxlsxwriter is what lets
# it WRITE one — the format a spreadsheet actually arrives in, in both
# directions. Without the readers it opens its own format and CSV, and the
# LibreOffice pack in a box becomes the only way to read a file somebody sent;
# without the writer it opens that file and answers `XLSX export support not
# compiled in.` to a save, so it cannot be handed back.
#
# ALL THREE ARE FOUND BY pkg-config AND BY NO FLAG. The Makefile probes for
# `libxml-2.0 libzip` and separately for `xlsxwriter`, and a probe that fails
# is a feature quietly left out of a build that otherwise succeeds — which is
# why they are `depends` and why there is nothing to pass here.
#
# LUA_PKGNAME IS NAMED because the Makefile otherwise takes the highest
# versioned luaX.Y it can list, which leaves the scripting Lua to whichever of
# lua and lua54 happens to be installed. Plain `lua` is the lua port's own
# .pc, the one lua54 does not ship. Plotting is the same kind of probe:
# -DGNUPLOT is set only when `which gnuplot` answers at build time, which is
# why gnuplot is a dependency. Legacy .xls import needs libxls, which is not a
# port, so its probe finds nothing.
#
# CFLAGS IS EXPORTED, NEVER PASSED ON THE COMMAND LINE. The Makefile builds its
# whole configuration with `CFLAGS +=` — HELP_PATH, CONFIG_DIR, HISTORY_FILE,
# the ncurses and colour switches, every -D the source reads — and a command-line
# assignment beats all of it. The build then fails on `'CONFIG_DIR' undeclared`,
# which reads as a missing header rather than as a lost flag. Exported, ours is
# the base and upstream appends to it.
export CFLAGS="$CFLAGS -Wno-error"

make prefix=/usr LUA_PKGNAME=lua
make prefix=/usr LUA_PKGNAME=lua DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sc-im.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Spreadsheet
GenericName=Spreadsheet
Comment=A spreadsheet on the grid
Exec=sc-im %f
Icon=x-office-spreadsheet
Terminal=true
Categories=Office;Spreadsheet;
Keywords=spreadsheet;cells;calc;sc-im;
EOF
chmod 644 "$PKG/usr/share/applications/sc-im.desktop"

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

cd src

# A SPREADSHEET IS A GRID OF CELLS, WHICH IS WHAT THIS WHOLE DESKTOP IS. That
# is not a joke about the styling guide — it is why sc-im fits here where a
# GUI spreadsheet needs a container: the data model and the display model are
# the same thing, so it runs on tty1 at the console font with no compromise.
#
# libxml2 and libzip are what let it read and write .xlsx, which is the format
# a spreadsheet actually arrives in. Without them it opens its own format and
# CSV, and the LibreOffice pack in a box becomes the only way to read a file
# somebody sent.
# CFLAGS IS EXPORTED, NEVER PASSED ON THE COMMAND LINE. The Makefile builds its
# whole configuration with `CFLAGS +=` — HELP_PATH, CONFIG_DIR, HISTORY_FILE,
# the ncurses and colour switches, every -D the source reads — and a command-line
# assignment beats all of it. The build then fails on `'CONFIG_DIR' undeclared`,
# which reads as a missing header rather than as a lost flag. Exported, ours is
# the base and upstream appends to it.
export CFLAGS="$CFLAGS -Wno-error"

make prefix=/usr
make prefix=/usr DESTDIR=$PKG install

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

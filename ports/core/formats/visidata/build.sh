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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor \
	--root=$PKG --prefix=/usr standard-mailcap .

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/visidata.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Data Sheets
GenericName=Data Explorer
Comment=Open CSV, JSON and spreadsheets as a sheet
Exec=vd %F
Icon=x-office-spreadsheet
Terminal=true
Categories=Office;Spreadsheet;Utility;
MimeType=text/csv;
Keywords=csv;json;data;sheet;table;visidata;
EOF
chmod 644 "$PKG/usr/share/applications/visidata.desktop"

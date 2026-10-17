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
pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr goocalendar .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
find "$_site/tryton/data/locale" -mindepth 1 -maxdepth 1 ! -name 'tryton.pot' -exec rm -rf {} +

install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
rsvg-convert -w 128 -h 128 tryton/data/pixmaps/tryton/tryton.svg \
	-o "$PKG/usr/share/icons/hicolor/128x128/apps/tryton.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/tryton.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Tryton
GenericName=Business Client
Comment=Accounting, stock, sales and GNU Health, from a Tryton server
Exec=tryton %u
Icon=tryton
Terminal=false
StartupWMClass=tryton
Categories=Office;Finance;GTK;
MimeType=x-scheme-handler/tryton;
Keywords=erp;accounting;invoice;stock;business;health;gnuhealth;tryton;
EOF2
chmod 644 "$PKG/usr/share/applications/tryton.desktop"

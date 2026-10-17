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
	--root=$PKG --prefix=/usr \
	icalendar tzdata tzlocal .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
(cd doc/source && PYTHONPATH=$_site sphinx-build -b man -q \
	-D extensions=sphinx.ext.autodoc,sphinx.ext.todo \
	-d "$SRC_ROOT/khal-doctrees" . "$SRC_ROOT/khal-man")
install -Dm644 "$SRC_ROOT/khal-man/khal.1" -t "$PKG/usr/share/man/man1"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ikhal.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Calendar
GenericName=Calendar
Comment=Appointments, over a local vdir
Exec=ikhal
Icon=x-office-calendar
Terminal=true
Categories=Office;Calendar;
Keywords=calendar;appointment;event;ical;khal;
EOF
chmod 644 "$PKG/usr/share/applications/ikhal.desktop"

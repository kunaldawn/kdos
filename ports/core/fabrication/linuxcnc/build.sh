# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/musl.patch"
patch -p1 -i "$PORT_SRC/tcl9.patch"
patch -p1 -i "$PORT_SRC/no-preconv.patch"
patch -p1 -i "$PORT_SRC/link-ldflags.patch"
patch -p1 -i "$PORT_SRC/pygi-glib.patch"

cd src
./autogen.sh

_py=$(python3 -c 'import sys; print(f"{sys.version_info[0]}{sys.version_info[1]}")')

export CFLAGS="$CFLAGS -g0" CXXFLAGS="$CXXFLAGS -g0"
export LDFLAGS="$LDFLAGS -Wl,--push-state,--no-as-needed -lintl -Wl,--pop-state"
./configure \
	PYTHON_BIN="python$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')" \
	PYTHON=/usr/bin/python3 \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--with-realtime=uspace \
	--with-tclConfig=/usr/lib/tclConfig.sh \
	--with-tkConfig=/usr/lib/tkConfig.sh \
	--with-boost-python=boost_python$_py \
	--enable-gtk \
	--disable-gtk2 \
	--disable-check-runtime-deps \
	--disable-build-documentation

_ulflags='-Wall -I. -I$(RTDIR)/include -DULAPI -D_GNU_SOURCE -DLOCALE_DIR=\"$(localedir)\" -DPACKAGE=\"$(package)\"'
_mkflags=(EXTRA_DEBUG=-g0 ULFLAGS="$_ulflags" XHC_WHB04B6_DEBUG=-funwind-tables)
make "${_mkflags[@]}"
make "${_mkflags[@]}" DESTDIR="$PKG" install

find "$PKG" -perm -4000 -exec chmod u-s {} +

mv "$PKG/etc/X11/app-defaults/TkLinuxCNC" "$PKG/usr/lib/tcltk/linuxcnc/TkLinuxCNC"
rm -rf "$PKG/etc/X11"
rmdir "$PKG/lib/linuxcnc" "$PKG/lib"

_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')
if [ -d "$PKG/usr/lib/python3/dist-packages" ]; then
	install -d "$PKG$_site"
	cp -a "$PKG/usr/lib/python3/dist-packages/." "$PKG$_site/"
	rm -rf "$PKG/usr/lib/python3"
fi

find "$PKG/usr/lib/tcltk/linuxcnc/msgs" -name '*.msg' ! -name 'en*.msg' -delete 2>/dev/null || true

find "$PKG/usr/share/doc/linuxcnc" -type l ! -exec test -e {} \; -delete

install -Dm644 ../linuxcncicon.png "$PKG/usr/share/icons/hicolor/48x48/apps/linuxcncicon.png"

rm -f "$PKG/usr/share/applications/linuxcnc-latency-histogram.desktop"
cat > "$PKG/usr/share/applications/linuxcnc.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LinuxCNC
GenericName=CNC Machine Controller
Comment=Run a mill, lathe, router or plasma cutter from G-code
Exec=linuxcnc
Icon=linuxcncicon
Terminal=false
StartupWMClass=Pickconfig
Categories=Science;Engineering;
Keywords=cnc;linuxcnc;gcode;g-code;mill;lathe;router;plasma;axis;hal;
DESKTOP
cat > "$PKG/usr/share/applications/linuxcnc-latency.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LinuxCNC Latency Test
Comment=Measure how steadily this machine keeps realtime deadlines
Exec=latency-test
Icon=linuxcncicon
Terminal=false
NoDisplay=true
StartupWMClass=Latency-test
Categories=Science;Engineering;
Keywords=cnc;linuxcnc;latency;jitter;benchmark;realtime;
DESKTOP
cat > "$PKG/usr/share/applications/linuxcnc-stepconf.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LinuxCNC StepConf Wizard
Comment=Write a LinuxCNC configuration for a stepper machine
Exec=stepconf
Icon=linuxcncicon
Terminal=false
StartupWMClass=stepconf
Categories=Science;Engineering;
Keywords=cnc;linuxcnc;stepper;configuration;wizard;parallel;
DESKTOP
cat > "$PKG/usr/share/applications/linuxcnc-pncconf.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LinuxCNC PnCConf Wizard
Comment=Write a LinuxCNC configuration for Mesa I/O cards
Exec=pncconf
Icon=linuxcncicon
Terminal=false
StartupWMClass=pncconf
Categories=Science;Engineering;
Keywords=cnc;linuxcnc;mesa;configuration;wizard;servo;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/linuxcnc*.desktop

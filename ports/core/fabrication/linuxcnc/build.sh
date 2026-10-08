# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# musl has no mallopt(), no pthread_attr_setaffinity_np() and only the
# POSIX basename(); its stderr is not assignable; its socket calls take a
# plain struct sockaddr pointer; and its headers declare struct timeval,
# u_short and the byte-order helpers only where they are included by name.
# The patch pins the realtime thread with pthread_setaffinity_np() once it
# exists, reopens stderr in place, and gives the interpreter a GNU basename().
patch -p1 -i "$PORT_SRC/musl.patch"
# The 2.9 releases build against Tcl/Tk 8 only, and Tcl/Tk 9 is the only one
# here: this is upstream's Tcl/Tk 9 port of the C extensions, Togl, the Tcl
# GUIs and AXIS, taken from its master branch.
patch -p1 -i "$PORT_SRC/tcl9.patch"
# The component manual pages are written as UTF-8 and read by mandoc, which
# takes UTF-8 as it is. groff's preconv pass, which escapes the non-ASCII
# characters, is dropped, so the build does not need groff.
patch -p1 -i "$PORT_SRC/no-preconv.patch"
# Two component pages are installed with GNU install's --mode=, and the
# libraries' links copied with GNU cp's --no-dereference; toybox's install and
# cp take neither long option. toybox-options gives them -m and -P.
patch -p1 -i "$PORT_SRC/toybox-options.patch"

cd src
./autogen.sh

_py=$(python3 -c 'import sys; print(f"{sys.version_info[0]}{sys.version_info[1]}")')

# uspace: the realtime layer runs as POSIX threads in userspace, on the
# stock kernel. The configure-time probes for run-time Tcl packages are
# skipped (they need a display), and classicladder, the only GTK 2 part,
# is off. gettext's libintl.h renames gettext() to libintl_gettext(), which
# only libintl defines; configure probes for plain gettext() and finds it in
# the C library, so libintl is named on every link line through LDFLAGS,
# the one variable all of LinuxCNC's link rules use. The shared-library rules
# put LDFLAGS before the objects, where --as-needed would drop it, so it is
# linked unconditionally.
#
# The makefile puts its own -Os and -g ahead of CFLAGS and CXXFLAGS, so the
# exported level wins and a trailing -g0 takes the debug information back.
export CFLAGS="$CFLAGS -g0" CXXFLAGS="$CXXFLAGS -g0"
export LDFLAGS="$LDFLAGS -Wl,--push-state,--no-as-needed -lintl -Wl,--pop-state"
#
# The Python check tries a fixed list of python3.N names that ends at 3.13,
# then plain python; PYTHON_BIN names the interpreter it looks for and the
# library it links, PYTHON the one the development check runs.
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

# Three make variables put -g where the -g0 above cannot follow it, and are
# given here. ULFLAGS, which the Tcl, Togl and Python-embedding objects add
# after CFLAGS, is its own value without -g and -Os; its paths and defines stay
# make references. The realtime components compile with upstream's -Os and
# none of the exported flags; EXTRA_DEBUG=-g0 follows their -g.
# XHC_WHB04B6_DEBUG is that pendant driver's -g -funwind-tables, kept without
# the -g.
_ulflags='-Wall -I. -I$(RTDIR)/include -DULAPI -D_GNU_SOURCE -DLOCALE_DIR=\"$(localedir)\" -DPACKAGE=\"$(package)\"'
_mkflags=(EXTRA_DEBUG=-g0 ULFLAGS="$_ulflags" XHC_WHB04B6_DEBUG=-funwind-tables)
make "${_mkflags[@]}"
make "${_mkflags[@]}" DESTDIR="$PKG" install

# The install marks rtapi_app and linuxcnc_module_helper setuid root. The
# package ships no setuid file: without the bit rtapi_app runs its threads
# without realtime priority, which is the simulator.
find "$PKG" -perm -4000 -exec chmod u-s {} +

# No package puts anything under /etc/X11; the TkLinuxCNC resource file is
# read from beside the Tcl sources instead.
mv "$PKG/etc/X11/app-defaults/TkLinuxCNC" "$PKG/usr/lib/tcltk/linuxcnc/TkLinuxCNC"
rm -rf "$PKG/etc/X11"
# The install also makes an empty /lib/linuxcnc, named outright rather than
# under the prefix; /lib is a link to usr/lib here, and nothing is put in it.
rmdir "$PKG/lib/linuxcnc" "$PKG/lib"

# configure falls back to Debian's dist-packages when the interpreter has
# no Debian scheme; this python imports from its own site-packages.
_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')
if [ -d "$PKG/usr/lib/python3/dist-packages" ]; then
	install -d "$PKG$_site"
	cp -a "$PKG/usr/lib/python3/dist-packages/." "$PKG$_site/"
	rm -rf "$PKG/usr/lib/python3"
fi

# English only.
find "$PKG/usr/share/locale" -mindepth 1 -maxdepth 1 ! -name 'en*' -exec rm -rf {} + 2>/dev/null || true
find "$PKG/usr/lib/tcltk/linuxcnc/msgs" -name '*.msg' ! -name 'en*.msg' -delete 2>/dev/null || true

install -Dm644 ../linuxcncicon.png "$PKG/usr/share/icons/hicolor/48x48/apps/linuxcncicon.png"

# The launcher and the latency test are Tk, so X11 windows under Xwayland;
# the wizards are GTK 3 and take their app_id from the program name. The
# latency histogram draws with BLT, which has no port, so it has no entry.
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

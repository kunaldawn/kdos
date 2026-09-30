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

# Alpine's musl patch: libstdc++ on musl has no abs() overloads for the
# unsigned and narrow integer types pr-output.cc prints, so the build stops
# there without it.
patch -p1 -i "$PORT_SRC/abs.patch"

# The Qt 6 GUI with its QScintilla editor, the "qt" graphics toolkit (a
# QOpenGLWidget, so no GLX) and gl2ps for printing figures to PDF, EPS and
# SVG; gnuplot is the second graphics toolkit, run as a program. FLTK and the
# X11 display probe are off: the Qt toolkit replaces both. Java is off.
# Octave's configure answers a missing library with a warning and builds
# without the functions it backs, so config.h is checked after the configure
# for every one this port depends on: OpenBLAS (BLAS and LAPACK),
# SuiteSparse (sparse algebra), ARPACK (eigs), qrupdate, FFTW in both
# precisions, GLPK (glpk), QHull (convhulln, delaunayn), SUNDIALS with KLU
# (ode15s, ode15i), HDF5 (save -hdf5), GraphicsMagick's Magick++ (imread,
# imwrite), libsndfile and PortAudio (audioread, audioplayer), cURL (urlread,
# webread), RapidJSON (jsondecode). The manual ships built in the tarball as
# Info, HTML, PDF and a Qt help collection, and is installed as it is.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--infodir=/usr/share/info --mandir=/usr/share/man \
	--localstatedir=/var \
	--enable-shared \
	--disable-static \
	--disable-rpath \
	--disable-java \
	--enable-openmp \
	--enable-fftw-threads \
	--enable-rapidjson \
	--enable-readline \
	--with-qt=6 \
	--with-blas=-lopenblas \
	--with-lapack=-lopenblas \
	--with-magick=GraphicsMagick++ \
	--with-openssl \
	--without-fltk \
	--without-x \
	--with-wayland-client
for m in HAVE_UMFPACK HAVE_CHOLMOD HAVE_CXSPARSE HAVE_KLU HAVE_SPQR \
	HAVE_ARPACK HAVE_QRUPDATE HAVE_FFTW3 HAVE_FFTW3F HAVE_GLPK HAVE_QHULL \
	HAVE_SUNDIALS HAVE_SUNDIALS_SUNLINSOL_KLU HAVE_HDF5 HAVE_MAGICK \
	HAVE_SNDFILE HAVE_PORTAUDIO HAVE_CURL HAVE_RAPIDJSON HAVE_QSCINTILLA \
	HAVE_GL2PS_H HAVE_QT6 HAVE_OPENGL HAVE_FREETYPE HAVE_FONTCONFIG \
	HAVE_READLINE; do
	grep -q "^#define $m 1" config.h || { echo "octave: configure left out $m" >&2; exit 1; }
done
make
make DESTDIR=$PKG install

ls "$PKG"/usr/libexec/octave/$version/exec/*/octave-gui >/dev/null
test -e "$PKG"/usr/share/info/octave.info
rm -rf "$PKG"/usr/share/octave/$version/etc/tests

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the GUI sets its desktop
# file name, and so its Wayland app_id, to org.octave.Octave. The upstream
# entry also carries translated comments; bundled data is English only.
cat > "$PKG/usr/share/applications/org.octave.Octave.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GNU Octave
GenericName=Numerical Computing
Comment=Interactive programming environment for numerical computations
TryExec=octave
Exec=octave --gui %f
Icon=octave
Terminal=false
StartupNotify=false
StartupWMClass=org.octave.Octave
Categories=Education;Science;Math;
MimeType=text/x-octave;text/x-matlab;
Keywords=science;math;matrix;numerical;computation;plotting;matlab;octave;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.octave.Octave.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/octave.png

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

patch -p1 -i "$PORT_SRC/abs.patch"

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
	HAVE_SNDFILE HAVE_PORTAUDIO HAVE_CURL HAVE_RAPIDJSON HAVE_RAPIDJSON_PRETTYWRITER HAVE_QSCINTILLA \
	HAVE_GL2PS_H HAVE_QT6 HAVE_OPENGL HAVE_FREETYPE HAVE_FONTCONFIG \
	HAVE_READLINE; do
	grep -q "^#define $m 1" config.h || { echo "octave: configure left out $m" >&2; exit 1; }
done
make
make DESTDIR=$PKG install

ls "$PKG"/usr/libexec/octave/$version/exec/*/octave-gui >/dev/null
test -e "$PKG"/usr/share/info/octave.info
rm -rf "$PKG"/usr/share/octave/$version/etc/tests

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

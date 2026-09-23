# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# WITHOUT FILTERS, CUPS ACCEPTS A JOB AND PRINTS A BLANK PAGE. cups itself has
# shipped no filters since 2.x split them out, so the queue exists, the job is
# spooled, the printer runs the paper through, and nothing is on it.
#
# The filter code lives in libcupsfilters and the PPD code in libppd; this
# package is the CUPS-facing half — the filter executables, their mime.convs,
# foomatic-rip, the driverless PPD generator and the beh backend. The network
# browsing daemon is cups-browsed, its own port.
#
# EVERY FILTER IS NAMED. Each of these switches answers a missing dependency by
# turning the filter off rather than by failing, so leaving them at their
# default builds a cups-filters that installs and silently cannot render a PDF,
# an image or a PostScript job. The universal filter is one executable that
# picks the conversion chain itself; the individual ones are the older layout
# and would install a second set of mime.convs rules competing with it.
# --disable-mutool: mupdf is a port but its path duplicates the poppler one.
#
# foomatic-rip declares start_process() with an empty parameter list and passes
# it a three-argument callback. In C23 `()` means `(void)`, so the declaration
# and the definition conflict and the build stops; the code is C17.
export CFLAGS="$CFLAGS -std=gnu17"
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--enable-universal-cups-filter --disable-individual-cups-filters \
	--enable-imagefilters --enable-poppler --enable-ghostscript \
	--disable-mutool --enable-foomatic --enable-driverless \
	--with-shell=/bin/bash
make
# -j1 ON THE INSTALL: MKDIR_P is upstream's bundled install-sh, which tests
# each path component and then creates it with a bare mkdir. Parallel install
# targets create the same directories at once, and the loser stops the install
# on EEXIST.
make -j1 DESTDIR=$PKG install

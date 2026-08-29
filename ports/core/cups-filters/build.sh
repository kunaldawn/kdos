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
# 1.28.x, NOT 2.x. The 2.x series is built against CUPS 3's libcupsfilters/
# libppd split, and ports/core/cups is 2.4.16.
#
# EVERY FILTER IS NAMED. Each of these configure switches answers a missing
# dependency by turning the filter off rather than by failing, so leaving them
# at their default builds a cups-filters that installs and silently cannot
# render a PDF, an image or a PostScript job. --disable-mutool is deliberate:
# mupdf is a port but its filter duplicates the poppler one.
#
# --without-php: there is no PHP here and the only thing it builds is a web
# front end this system does not serve.
# A RULE-7 PATCH: an API that no longer exists is not a warning to switch off.
# 1.28.x is the last series that works with CUPS 2.4, and it predates two qpdf
# removals. Both replacements are the ones qpdf's own headers name:
#
#   replaceOrRemoveKey  ->  replaceKey, which now carries exactly its
#                           semantics ("if value is null, remove the key")
#   PointerHolder<T>    ->  std::shared_ptr<T>, qpdf's own smart pointer
#                           having been replaced by the standard one
#   ph.getPointer()     ->  ph.get()
#   ph = new T(...)     ->  ph.reset(new T(...)), PointerHolder having had an
#                           assignment operator taking a raw pointer
#
# Five files, and no behaviour changes: every one is the standard spelling of
# what the qpdf type already did.
patch -p1 -i "$PORT_SRC/qpdf12-api.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --with-rcdir=no --disable-static \
	--disable-mutool --without-php --enable-avahi \
	--enable-ghostscript --enable-poppler --enable-imagefilters \
	--with-test-font-path=/usr/share/fonts/TTF/DejaVuSans.ttf
make
make DESTDIR=$PKG install

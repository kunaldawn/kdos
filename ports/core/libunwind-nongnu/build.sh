# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# libunwind-ptrace is what htop comes for: its backtrace screen walks another
# process's stack with it. GStreamer, libcamera and samba link libunwind for
# their own crash and debug backtraces. ptrace, coredump and
# setjmp are autodetected, so each is named and a missing header stops
# configure. --disable-unwind-header leaves unwind.h to the compilers, which
# each carry their own for the C++ exception runtime.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-tests \
	--disable-unwind-header \
	--enable-ptrace \
	--enable-coredump \
	--enable-setjmp \
	--enable-minidebuginfo \
	--enable-zlibdebuginfo
make
make DESTDIR=$PKG install

# The manual pages are generated from LaTeX by latex2man, which is not a
# port, and configure then skips the doc directory. The tarball carries them
# already rendered.
for _m in doc/*.man; do
	_p=${_m##*/}
	install -Dm644 "$_m" "$PKG/usr/share/man/man3/${_p%.man}.3"
done

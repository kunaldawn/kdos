# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

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

for _m in doc/*.man; do
	_p=${_m##*/}
	install -Dm644 "$_m" "$PKG/usr/share/man/man3/${_p%.man}.3"
done

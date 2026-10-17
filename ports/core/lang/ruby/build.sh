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

patch -p1 -i "$PORT_SRC/musl-main-stack.patch"
patch -p1 -i "$PORT_SRC/pc-libs-no-ldflags.patch"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--mandir=/usr/share/man \
	--enable-shared \
	--disable-rpath \
	--disable-install-doc \
	--enable-yjit \
	--disable-zjit \
	--with-gmp \
	--with-mantype=man
make
make DESTDIR=$PKG install
strip --strip-debug "$PKG/usr/lib/libruby.so.$version"

for ext in openssl psych zlib fiddle; do
	if ! find "$PKG/usr/lib/ruby" -name "$ext.so" | grep -q .; then
		echo "ruby: extension '$ext' was not built" >&2
		exit 1
	fi
done

find "$PKG/usr/share/man" -name '*.gz' -exec gunzip {} +
for page in "$PKG"/usr/lib/ruby/*/bundler/man/*.[1-8] \
	"$PKG"/usr/lib/ruby/gems/*/gems/irb-*/man/*.[1-8] \
	"$PKG"/usr/lib/ruby/gems/*/gems/rdoc-*/man/*.[1-8]; do
	install -Dm644 "$page" -t "$PKG/usr/share/man/man${page##*.}"
done

rm -rf "$PKG"/usr/lib/ruby/gems/*/cache

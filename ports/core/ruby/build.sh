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

# YJIT AND ZJIT ARE OFF. Both are Rust, and configure turns each on by itself
# when it finds a new enough rustc, so leaving them unset makes the interpreter
# depend on whether the rust port happens to be installed in the chroot.
#
# --disable-install-doc skips the rdoc and ri indexes, which install generates
# by running rdoc over the whole standard library; the manual pages under man/
# still install.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--mandir=/usr/share/man \
	--enable-shared \
	--disable-rpath \
	--disable-install-doc \
	--disable-yjit \
	--disable-zjit \
	--with-gmp \
	--with-mantype=man
make
make DESTDIR=$PKG install

# An extension whose library is missing is a warning in extmk's output, not a
# failed build, and a LoadError on the target: without yaml.h there is no
# psych, without ffi.h no fiddle. Each one the depends line pays for must exist.
for ext in openssl psych zlib fiddle; do
	if ! find "$PKG/usr/lib/ruby" -name "$ext.so" | grep -q .; then
		echo "ruby: extension '$ext' was not built" >&2
		exit 1
	fi
done

# The gem cache holds a second copy, as a .gem archive, of every bundled gem
# already unpacked beside it; only `gem pristine` reads it.
rm -rf "$PKG"/usr/lib/ruby/gems/*/cache

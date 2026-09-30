# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The TPM 1.2 and 2.0 reference implementations as a library, with their
# cryptography done by OpenSSL. swtpm is the program around it, and the one
# consumer here. A tag archive carries no configure, so autoreconf writes it.
autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-openssl \
	--with-tpm2
make
make DESTDIR=$PKG install

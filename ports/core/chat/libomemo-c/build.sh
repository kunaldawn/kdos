# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The .proto files are compiled at build time with protoc and protoc-gen-c,
# which is why protobuf is a build dependency here and not only protobuf-c.
# nofallback: the tarball carries a protobuf-c wrap, and a missing protobuf-c
# must fail setup rather than download one.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback -Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

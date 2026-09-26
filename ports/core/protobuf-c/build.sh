# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# protoc-gen-c is the plugin a consumer's build hands to protoc to turn its
# .proto files into C. It links libprotobuf, so --enable-protoc makes a
# protobuf without its development files a configure error rather than a
# package carrying the runtime and no generator.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --enable-protoc
make
make DESTDIR=$PKG install

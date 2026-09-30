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
#
# protobuf-34-field-label.patch is protobuf-c/protobuf-c#797: protobuf 34
# removed FieldDescriptor::label(), which every field generator calls. The
# patch derives the same label from is_repeated() and is_required().
patch -p1 -i "$PORT_SRC/protobuf-34-field-label.patch"
./configure --prefix=/usr --libdir=/usr/lib --disable-static --enable-protoc
make
make DESTDIR=$PKG install

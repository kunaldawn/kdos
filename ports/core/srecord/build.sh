# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# EVERY BOOTLOADER ARGUMENT IS A FILE-FORMAT ARGUMENT. Converting between HEX,
# S-record and TI-TXT, filling gaps, fixing a CRC and shifting an offset is
# what stands between a linker's output and a part that boots.
# A RULE-7 PATCH, two hunks and no flag for either. `find_package(Doxygen
# REQUIRED doxygen dot)` has no option behind it — `dot` is graphviz, wanted
# only to draw call graphs into docs that are never packaged — and hp64k.h uses
# uint16_t while including nothing that declares it, which GCC 15 rejects
# rather than warns about.
patch -p1 -i "$PORT_SRC/build-fixes.patch"

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
# THE THREE PROGRAMS BY NAME, NOT `all`. The `doc` subdirectory is added
# unconditionally and its targets shell out to groff, a2ps and a TeX chain none
# of which is on this host, so `all` dies at `srec_cat.1.html` with exit 127 —
# after everything that matters has already been built.
make srec_cat srec_cmp srec_info srecord

install -Dm755 srec_cat/srec_cat   $PKG/usr/bin/srec_cat
install -Dm755 srec_cmp/srec_cmp   $PKG/usr/bin/srec_cmp
install -Dm755 srec_info/srec_info $PKG/usr/bin/srec_info
install -d $PKG/usr/share/man/man1
install -m644 ../doc/srec_cat.1 ../doc/srec_cmp.1 ../doc/srec_info.1 \
        $PKG/usr/share/man/man1/ 2>/dev/null || true

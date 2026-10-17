# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/build-fixes.patch"

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DHAVE_LIBGCRYPT=ON -DHAVE_GCRY_MD_HD_T=ON
make srec_cat srec_cmp srec_info srecord

install -Dm755 srec_cat/srec_cat   $PKG/usr/bin/srec_cat
install -Dm755 srec_cmp/srec_cmp   $PKG/usr/bin/srec_cmp
install -Dm755 srec_info/srec_info $PKG/usr/bin/srec_info
srec_man() {
	while IFS= read -r line || [ -n "$line" ]; do
		case $line in
		'.so '*)
			inc=${line#.so }
			if [ -f "doc/$inc" ]; then srec_man "doc/$inc"; else srec_man "../doc/$inc"; fi
			;;
		*)
			printf '%s\n' "$line"
			;;
		esac
	done < "$1"
}
for s in 1 5; do
	install -d $PKG/usr/share/man/man$s
	for m in ../doc/man$s/*.$s; do
		srec_man "$m" > $PKG/usr/share/man/man$s/${m##*/}
	done
done

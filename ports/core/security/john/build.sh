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

patch -p1 -i "$PORT_SRC/blake2-align.patch"

patch -p1 -i "$PORT_SRC/opencl-topology.patch"

patch -p1 -i "$PORT_SRC/cpu-fallback-first.patch"
patch -p1 -i "$PORT_SRC/cprepair-ldflags.patch"

export CFLAGS="$CFLAGS -fcommon"

cd src
_opts=(--prefix=/usr --disable-native-tests --disable-mpi --enable-pcap --enable-opencl)

_base_cflags="$CFLAGS"
for _lvl in avx512bw:john avx2:john-non-avx512bw xop:john-non-avx2 avx:john-non-xop; do
	CFLAGS="$_base_cflags -DCPU_FALLBACK" ./configure "${_opts[@]}" \
		--enable-simd="${_lvl%%:*}"
	make clean
	make
	mv ../run/john ../run/.simd-"${_lvl#*:}"
done
CFLAGS="$_base_cflags" ./configure "${_opts[@]}"
make clean
make
mv ../run/john ../run/john-non-avx
for _f in ../run/.simd-*; do
	mv "$_f" ../run/"${_f#../run/.simd-}"
done

install -dm755 $PKG/usr/share/john $PKG/usr/bin
cd ../run
rm -f $(grep -l '^#!.*python2' *.py)

for f in *; do
	if [ -L "$f" ]; then
		ln -s "$(readlink "$f")" $PKG/usr/bin/"$f"
	elif [ -f "$f" ] && [ "$(head -c4 "$f")" = $'\x7fELF' ]; then
		install -m755 "$f" $PKG/usr/bin/"$f"
	else
		cp -a "$f" $PKG/usr/share/john/
	fi
done

rm -rf $PKG/usr/share/john/ztex

for f in *2john*; do
	[ -e $PKG/usr/bin/"$f" ] && continue
	[ "$(head -c2 "$f")" = '#!' ] || continue
	chmod 755 $PKG/usr/share/john/"$f"
	ln -s ../share/john/"$f" $PKG/usr/bin/"$f"
done

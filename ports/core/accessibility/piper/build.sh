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

patch -p1 -i "$PORT_SRC/espeak-ng-local-archive.patch"

patch -p1 -i "$PORT_SRC/pw-play-playback.patch"
_espeak_tar=espeak-ng-$_espeak.tar.gz
for d in "$PORT_SRC" "$SOURCE_DIR"; do
	[ -f "$d/$_espeak_tar" ] && break
done
[ -f "$d/$_espeak_tar" ] || { echo "piper: $_espeak_tar is missing" >&2; exit 1; }

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DPIPER_ESPEAKNG_ARCHIVE="$d/$_espeak_tar"
cmake --build build

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
install -d "$_site"
cp -r src/piper "$_site/piper"
rm -rf "$_site/piper/train" "$_site/piper/espeakbridge.c"
cmake --install build --prefix "$_site/piper"
[ -f "$_site/piper/espeak-ng-data/phontab" ] ||
	{ echo "piper: espeak-ng-data was not built" >&2; exit 1; }
ls "$_site"/piper/espeakbridge*.so >/dev/null ||
	{ echo "piper: the espeakbridge module was not built" >&2; exit 1; }
python3 -m compileall -q -s "$PKG" -p / "$_site/piper"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/piper" <<'SCRIPT'
#!/usr/bin/python3
import sys
from piper.__main__ import main
sys.exit(main())
SCRIPT
chmod 755 "$PKG/usr/bin/piper"

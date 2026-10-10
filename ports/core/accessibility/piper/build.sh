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

# PIPER PHONEMISES THROUGH A NEWER ESPEAK-NG THAN ANY RELEASE. Its bridge
# calls espeak_TextToPhonemesWithTerminator, which espeak-ng 1.52.0 does not
# have, so upstream builds a static espeak-ng from a pinned commit with
# ExternalProject and a git clone. The commit is a later source here; the
# patch hands ExternalProject the archive in place of the clone and puts the
# tree's CFLAGS and CXXFLAGS in front of the compiler flags upstream sets
# there, which otherwise replace them and drop the reproducibility maps from
# the static library. Every other setting (no audio output, no MBROLA, no
# Klatt, static, position independent) is upstream's. espeak-ng clones sonic
# whenever it finds no installed copy, even with USE_LIBSONIC off; the patch
# also names a placeholder SONIC_LIB and SONIC_INC, which skips the clone and
# which nothing reads while USE_LIBSONIC is off. espeak-ng's compiled
# espeak-ng-data is copied into the package beside the module, which is where
# piper looks for it.
patch -p1 -i "$PORT_SRC/espeak-ng-local-archive.patch"

# Without -f, piper speaks by piping raw samples to a player. Upstream's player
# is ffplay, which the ffmpeg port does not build (it is built without SDL), so
# every plain invocation would write output.wav into the current directory
# instead of speaking. The patch pipes the same s16 mono stream to pipewire's
# pw-play and makes the availability check look for pw-play.
patch -p1 -i "$PORT_SRC/pw-play-playback.patch"
_espeak_tar=espeak-ng-$_espeak.tar.gz
for d in "$PORT_SRC" "$SOURCE_DIR"; do
	[ -f "$d/$_espeak_tar" ] && break
done
[ -f "$d/$_espeak_tar" ] || { echo "piper: $_espeak_tar is missing" >&2; exit 1; }

# setup.py is scikit-build, which is not a port, and all it does here is run
# this CMake project and copy the package; the two steps are done directly.
# The project builds the espeakbridge extension (abi3) and copies
# espeak-ng-data into src/piper.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DPython_EXECUTABLE=/usr/bin/python3 \
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

# The console script setup.py's entry point would write. Voices are .onnx
# files with a .onnx.json beside them, passed with -m; piper.download_voices
# fetches them from the network and is the only part that does.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/piper" <<'SCRIPT'
#!/usr/bin/python3
import sys
from piper.__main__ import main
sys.exit(main())
SCRIPT
chmod 755 "$PKG/usr/bin/piper"

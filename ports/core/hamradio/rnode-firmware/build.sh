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


# CODE FOR ANOTHER PROCESSOR: the images RNode radios run, on an ESP32 or an
# nRF52. They are installed under the names rnodeconf asks for, beside the
# `<file>.version` record ("<version> <sha256>") it otherwise writes from the
# release.json it downloads, so rnodeconf verifies each image against the
# hash upstream published for it. rnodeconf flashes an ESP32 with the esptool
# it carries and an nRF52 by running adafruit-nrfutil, which it cannot do
# without.
_dest=$PKG/usr/share/rnode-firmware/$version
install -dm755 "$_dest"
install -m644 "$name-$version.json" "$_dest/release.json"
install -m644 "$name-console-$version.bin" "$_dest/console_image.bin"
for _zip in "$name"-*-"$version".zip; do
	_board=${_zip#"$name"-}
	_board=${_board%-"$version".zip}
	install -m644 "$_zip" "$_dest/rnode_firmware_$_board.zip"
done
python3 - "$_dest" <<'PY'
import json, os, sys
dest = sys.argv[1]
with open(os.path.join(dest, "release.json")) as f:
    release = json.load(f)
for fw, info in release.items():
    if not os.path.exists(os.path.join(dest, fw)):
        continue
    with open(os.path.join(dest, fw + ".version"), "w") as out:
        out.write(f"{info['version']} {info['hash']}")
PY
printf '%s\n' "$version" > "$PKG/usr/share/rnode-firmware/version"

# rnodeconf keeps its images in ~/.config/rnodeconf/update/<version>/ and
# downloads whatever is missing there. This links the packaged images into
# that cache and runs it with the version named and the online check off,
# so flashing and updating an RNode reach no network. The links, not
# copies: the cache then follows the package when it is upgraded.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/rnodeconf-local" <<'KDOS_SH'
#!/bin/sh
share=/usr/share/rnode-firmware
read -r ver < "$share/version" || exit 1
cache=$HOME/.config/rnodeconf/update/$ver
mkdir -p "$cache" || exit 1
for f in "$share/$ver"/*; do
	[ -e "$cache/${f##*/}" ] || ln -s "$f" "$cache/${f##*/}"
done
exec rnodeconf --nocheck --fw-version "$ver" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/rnodeconf-local"

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# OBD-II IS THE ONE MANUFACTURER-INDEPENDENT DIAGNOSTIC INTERFACE, and the
# adapter that speaks it is a serial port — so the whole of this is pyserial
# plus a protocol. Reaching the adapter needs the `dialout` group, which
# fs/etc/group already grants the console user, and the udev rules under
# fs/etc/udev/rules.d that give that group the CH341 and CP210x bridges these
# cables are built from.
#
# EVERY DECODED VALUE IS A pint QUANTITY, so pint is imported the moment `obd`
# is. pint and its two helpers, flexcache and flexparser, have no other
# consumer here and are vendored and installed into this package; pint is
# held to the 0.24 series obd declares. Their own runtime dependencies,
# platformdirs and typing-extensions, and every build backend they need are
# ports, which is why the bundle is installed --no-deps with isolation off.
#
# The bundle is unpacked beside the source, not inside it: obd is a flat
# layout with no package list, and setuptools refuses to guess between `obd`
# and a `vendor` directory sitting next to it.
mkdir -p "$SRC_ROOT/vendor"
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C "$SRC_ROOT/vendor"
pip3 install --no-deps --no-index --find-links="$SRC_ROOT/vendor" --no-build-isolation \
	--root=$PKG --prefix=/usr \
	pint flexcache flexparser .

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CHARDEV ABI, NOT sysfs. /sys/class/gpio was deprecated and removed; this
# is the interface a current kernel actually offers, and `gpiomon` on it is a
# poor man's logic analyzer that needs no hardware beyond the header pins.
#
# EVERY FEATURE IS NAMED because they are `feature`s and `auto` answers a
# missing dependency by turning the thing off. `tools` is the whole
# user-facing half — gpiodetect, gpioinfo, gpioget, gpioset, gpiomon — and a
# package without it is a library with no consumer here. The rest are off for
# reasons rather than by omission: the bindings each want another toolchain,
# `dbus` and `systemd` want a session daemon and a unit file this distro does
# not have, and `introspection` is gobject-introspection for those bindings.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dtools=enabled -Dgpioset-interactive=disabled \
	-Dtests=disabled -Dexamples=disabled \
	-Dbindings-cxx=disabled -Dbindings-python=disabled \
	-Dbindings-rust=disabled -Dbindings-glib=disabled \
	-Ddbus=disabled -Dintrospection=disabled -Dsystemd=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

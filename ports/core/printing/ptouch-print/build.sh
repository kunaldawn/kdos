# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Upstream's cgit serves no snapshots, so the source is
# `git archive --prefix=ptouch-print-$version/ $_commit | gzip -n -9` of the
# v$version tag, and the source archive is the only place it is fetched from.
#
# gitversion.cmake asks git for a hash and finds no checkout, so the version
# the program prints is "N/A"; cmake refuses to configure without git on the
# path.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib
cmake --build build
DESTDIR=$PKG cmake --install build

# The rule upstream installs when the build root has /etc/udev/rules.d grants
# access through TAG+="uaccess", which only logind acts on. Printer access is
# granted by group under fs/etc/udev/rules.d instead.
rm -rf "${PKG:?}/etc/udev"
find "${PKG:?}" -depth -type d -name etc -empty -delete

# German is the one other catalogue; the image is English only.
rm -rf "${PKG:?}/usr/share/locale/de"

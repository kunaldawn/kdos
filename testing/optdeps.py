#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   optdeps — features a recipe switches off although a port provides them
# ---------------------------------
#
# Run from the repository root. Every build.sh under ports/core and src/ is read
# for the switches that turn an optional dependency off:
#
#   --without-X   --disable-X                         autotools
#   -D…X…=OFF     -DCMAKE_DISABLE_FIND_PACKAGE_X=ON   cmake (OFF, false or no)
#   -DX=disabled  -DX=false                           meson
#
# X is mapped to a port by name: X itself, lib<X>, X without its lib prefix,
# python3-<X>, each with _ read as -, and then through ALIAS, the table of
# upstream spellings that differ from the port's name. A switch whose X is a
# port is a MATCH: either a deliberate choice or a feature lost to a claim that
# the port does not exist. The report cannot tell the two apart; a reader can.
#
# A match is reported, never refused. A recipe turns a present library off for
# reasons a switch does not carry (a cycle, a licence, a feature nothing here
# uses), so a gate on the bare match would fail on the deliberate ones.
#
#   python3 testing/optdeps.py            every match, one line each
#   python3 testing/optdeps.py --count    the number of matches only
#   python3 testing/optdeps.py --claims   comment lines that say something is
#                                         not a port, each name in them that
#                                         is one marked with a *
#
# The name a switch carries is upstream's, not the port's: `--without-numa`
# finds `numactl` only through ALIAS. An upstream spelling that maps to no port
# is silent, so a new port whose upstream name differs from its own needs its
# row here for the switches that name it to be seen.

import os
import re
import sys

ALIAS = {
    "numa": "numactl",
    "libnuma": "numactl",
    "pygobject": "python3-gobject",
    "gobject-introspection-python": "python3-gobject",
    "pyside6": "python3-pyside6",
    "pyside": "python3-pyside6",
    "libstemmer": "snowball",
    "stemmer": "snowball",
    "stemming": "snowball",
    "nanobind": "python3-nanobind",
    "xscreensaver": "libXScrnSaver",
    "xss": "libXScrnSaver",
    "xscrnsaver": "libXScrnSaver",
    "tdb": "tdb",
    "talloc": "talloc",
    "glew": "glew",
    "gtest": "gtest",
    "googletest": "gtest",
    "jemalloc": "jemalloc",
    "openh264": "openh264",
    "blosc": "c-blosc",
    "c-blosc": "c-blosc",
    "sphinx": "python3-sphinx",
    "pyqt5": "python3-pyqt5",
    "pyqt6": "python3-pyqt6",
    "pybind11": "pybind11",
    "lcms": "lcms2",
    "lcms2": "lcms2",
    "jpeg": "libjpeg-turbo",
    "libjpeg": "libjpeg-turbo",
    "turbojpeg": "libjpeg-turbo",
    "png": "libpng",
    "tiff": "libtiff",
    "webp": "libwebp",
    "openjpeg": "openjpeg2",
    "openjp2": "openjpeg2",
    "freetype": "freetype2",
    "ssl": "openssl",
    "openssl": "openssl",
    "gnutls": "gnutls",
    "z": "zlib",
    "bz2": "bzip2",
    "bzip": "bzip2",
    "lzma": "xz",
    "xz": "xz",
    "zstd": "zstd",
    "curl": "curl",
    "libcurl": "curl",
    "sqlite": "sqlite",
    "sqlite3": "sqlite",
    "ffmpeg": "ffmpeg",
    "avcodec": "ffmpeg",
    "libav": "ffmpeg",
    "pulse": "pulseaudio",
    "pulseaudio": "pulseaudio",
    "libpulse": "pulseaudio",
    "alsa": "alsa-lib",
    "asound": "alsa-lib",
    "jack": "pipewire-jack",
    "gstreamer": "gstreamer",
    "gst": "gstreamer",
    "x264": "x264",
    "x265": "x265",
    "vpx": "libvpx",
    "aom": "aom",
    "dav1d": "dav1d",
    "opus": "opus",
    "vorbis": "libvorbis",
    "ogg": "libogg",
    "flac": "flac",
    "sndfile": "libsndfile",
    "samplerate": "libsamplerate",
    "fftw": "fftw",
    "fftw3": "fftw",
    "usb": "libusb",
    "usb1": "libusb",
    "libusb1": "libusb",
    "udev": "eudev",
    "libudev": "eudev",
    "dbus": "dbus",
    "glib": "glib",
    "gtk": "gtk3",
    "gtk3": "gtk3",
    "gtk4": "gtk4",
    "qt5": "qt5-qtbase",
    "qt6": "qt6-qtbase",
    "x11": "libX11",
    "xlib": "libX11",
    "xext": "libXext",
    "xrandr": "libXrandr",
    "xinerama": "libXinerama",
    "xft": "libXft",
    "xpm": "libXpm",
    "xi": "libXi",
    "xcb": "libxcb",
    "egl": "mesa",
    "gl": "mesa",
    "opengl": "mesa",
    "gles": "mesa",
    "gbm": "mesa",
    "vulkan": "vulkan-loader",
    "vdpau": "libvdpau",
    "vaapi": "libva",
    "va": "libva",
    "drm": "libdrm",
    "wayland": "wayland",
    "xkbcommon": "libxkbcommon",
    "lua": "lua",
    "luajit": "luajit",
    "python": "python3",
    "python3": "python3",
    "perl": "perl",
    "ruby": "ruby",
    "tcl": "tcl",
    "tk": "tk",
    "readline": "readline",
    "ncurses": "ncurses",
    "curses": "ncurses",
    "pcre": "pcre2",
    "pcre2": "pcre2",
    "icu": "icu",
    "xml2": "libxml2",
    "libxml": "libxml2",
    "xslt": "libxslt",
    "expat": "expat",
    "yaml": "libyaml",
    "jansson": "jansson",
    "json-c": "json-c",
    "jsonc": "json-c",
    "uuid": "util-linux",
    "blkid": "util-linux",
    "mount": "util-linux",
    "pam": "pam",
    "linux-pam": "pam",
    "cap": "libcap",
    "seccomp": "libseccomp",
    "acl": "acl",
    "attr": "attr",
    "krb5": "krb5",
    "kerberos": "krb5",
    "gssapi": "krb5",
    "ldap": "openldap",
    "sasl": "cyrus-sasl",
    "sasl2": "cyrus-sasl",
    "avahi": "avahi",
    "cups": "cups",
    "polkit": "polkit",
    "nm": "networkmanager",
    "networkmanager": "networkmanager",
    "bluez": "bluez",
    "bluetooth": "bluez",
    "smbclient": "samba",
    "samba": "samba",
    "nfs": "libnfs",
    "fuse": "fuse",
    "fuse3": "fuse",
    "gpgme": "gpgme",
    "gcrypt": "libgcrypt",
    "sodium": "libsodium",
    "magic": "file",
    "libmagic": "file",
    "archive": "libarchive",
    "unwind": "libunwind",
    "elf": "elfutils",
    "dw": "elfutils",
    "libelf": "elfutils",
    "bpf": "libbpf",
    "hwloc": "hwloc",
    "boost": "boost",
    "eigen": "eigen",
    "eigen3": "eigen",
    "tbb": "onetbb",
    "onetbb": "onetbb",
    "openmp": "gcc",
    "hdf5": "hdf5",
    "netcdf": "netcdf-c",
    "gdal": "gdal",
    "proj": "proj",
    "geos": "geos",
    "poppler": "poppler",
    "cairo": "cairo",
    "pango": "pango",
    "harfbuzz": "harfbuzz",
    "fontconfig": "fontconfig",
    "rsvg": "librsvg",
    "svg": "librsvg",
    "heif": "libheif",
    "avif": "libavif",
    "jxl": "libjxl",
    "exr": "openexr",
    "openexr": "openexr",
    "raw": "libraw",
    "gif": "giflib",
    "exif": "libexif",
    "exiv2": "exiv2",
    "spice": "spice-gtk",
    "sdl": "sdl2",
    "sdl2": "sdl2",
    "sdl3": "sdl3",
    "openal": "openal-soft",
    "libsrt": "libsrt",
    "srt": "libsrt",
    "rist": "librist",
    "librist": "librist",
    "oidn": "oidn",
    "openimagedenoise": "oidn",
    "embree": "embree",
    "meshoptimizer": "meshoptimizer",
    "kate": "libkate",
}

# Switches that name a port and are not a lost feature: documentation and test
# tooling, which the book's size policy keeps out of every package, and
# switches whose name happens to be a port's.
NOT_FEATURE = {
    "gtk-doc", "doxygen", "docs", "doc", "documentation", "man", "manpages",
    "tests", "test", "testing", "examples", "benchmarks", "werror", "static",
    "shared", "nls", "rpath", "debug", "silent-rules", "maintainer-mode",
    "dependency-tracking", "option-checking", "valgrind", "coverage",
    "ccache", "xmlto", "asciidoc", "asciidoctor", "git", "packaging",
    "man-pages", "gettext",
}

# BUILD_<X>=OFF is left out: in cmake it most often refuses a bundled copy of X
# in favour of the system one, which is the opposite of a lost feature.
DISABLE_PREFIXES = ("with_", "enable_", "use_", "have_", "want_", "support_")


def ports():
    names = {}
    for top in ("ports/core", "src"):
        if not os.path.isdir(top):
            continue
        for g in sorted(os.listdir(top)):
            d = os.path.join(top, g)
            if not os.path.isdir(d):
                continue
            for n in sorted(os.listdir(d)):
                if os.path.isfile(os.path.join(d, n, "kpkgbuild")):
                    names.setdefault(n, os.path.join(d, n))
    return names


def resolve(x, names, lower, guess=True):
    x = x.strip("'\"").strip()
    if not x:
        return None
    k = x.lower().replace("_", "-")
    if k in NOT_FEATURE:
        return None
    if k in ALIAS:
        t = ALIAS[k]
        return t if t in names else None
    if not guess:
        return lower.get(k)
    for c in (k, "lib" + k, k[3:] if k.startswith("lib") else None,
              "python3-" + k):
        if c and c in lower:
            return lower[c]
    return None


def cmake_key(key):
    """The feature a cmake -D…=OFF names, with its project prefix removed."""
    k = key.lower()
    m = re.match(r"cmake_disable_find_package_(.+)", k)
    if m:
        return [m.group(1)]
    out = []
    parts = k.split("_")
    for i, p in enumerate(parts):
        if p + "_" in DISABLE_PREFIXES and i + 1 < len(parts):
            out.append("_".join(parts[i + 1:]))
            break
    out.append(k)
    return out


SW = re.compile(
    r"--(?:without|disable)-([A-Za-z0-9_.+-]+)"
    r"|-D([A-Za-z0-9_]+)=(?:OFF|off|Off|disabled|false|FALSE|NO|no)\b"
    r"|-D(CMAKE_DISABLE_FIND_PACKAGE_[A-Za-z0-9_]+)=(?:ON|on|TRUE|true|1)\b")


def switches(path):
    out = []
    with open(path, errors="replace") as f:
        for no, line in enumerate(f, 1):
            s = line.split("#", 1)[0] if not line.lstrip().startswith("#") else ""
            for m in SW.finditer(s):
                if m.group(1):
                    out.append((no, m.group(0), [m.group(1)]))
                else:
                    out.append((no, m.group(0), cmake_key(m.group(2) or m.group(3))))
    return out


# A claim is prose, so its words are matched by exact name and ALIAS only, never
# through the lib<X> and python3-<X> guesses that turn "tool" into libtool, and
# the English words that are also port names are skipped.
CLAIM_STOP = {"which", "file", "time", "less", "make", "find", "patch"}

CLAIM = re.compile(r"not (?:a |yet a )?port|no port|not ported|"
                   r"isn't a port|ports? that do(?:es)? not exist", re.I)


def main():
    names = ports()
    lower = {n.lower(): n for n in names}
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    matches = []
    claims = []
    for n, d in names.items():
        b = os.path.join(d, "build.sh")
        if not os.path.isfile(b):
            continue
        for no, sw, keys in switches(b):
            for k in keys:
                p = resolve(k, names, lower)
                if p and p != n:
                    matches.append((d, no, sw, p))
                    break
        if mode == "--claims":
            with open(b, errors="replace") as f:
                for no, line in enumerate(f, 1):
                    if line.lstrip().startswith("#") and CLAIM.search(line):
                        words = re.findall(r"[A-Za-z][A-Za-z0-9_.+-]*[A-Za-z0-9]",
                                           line)
                        hit = sorted({resolve(w, names, lower, False)
                                      for w in words if len(w) > 2
                                      and w.lower() not in CLAIM_STOP}
                                     - {None, n})
                        claims.append((d, no, line.strip(), hit))
    if mode == "--count":
        print(len(matches))
        return 0
    if mode == "--claims":
        for d, no, text, hit in sorted(claims):
            mark = (" *" + ",".join(hit)) if hit else ""
            print("%s/build.sh:%d: %s%s" % (d, no, text, mark))
        return 0
    for d, no, sw, p in sorted(matches):
        print("%s/build.sh:%d: %s -> %s" % (d, no, sw, p))
    return 0


if __name__ == "__main__":
    sys.exit(main())

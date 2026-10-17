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

for _p in fix-musl-incompability gcc13 lfs64 gcc15-musl dyn-pagesize; do
	patch -p1 -i "$PORT_SRC/$_p.patch"
done

patch -p1 -i "$PORT_SRC/fmt-imported-configs.patch"

patch -p1 -i "$PORT_SRC/texturepacker-giflib6.patch"

patch -p1 -i "$PORT_SRC/pcre-cmp0026.patch"

patch -p1 -i "$PORT_SRC/platformdefs-limits.patch"

_src() {
	if [ -f "$PORT_SRC/$1" ]; then echo "$PORT_SRC/$1"; else echo "$SOURCE_DIR/$1"; fi
}

python3 -m zipfile -e "apache-groovy-binary-$_groovy.zip" "$SRC_ROOT/groovy"
_groovydir="$SRC_ROOT/groovy/groovy-$_groovy"

export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=2097152"
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
export CMAKE_POLICY_VERSION_MINIMUM=3.5
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCORE_PLATFORM_NAME=wayland \
	-DAPP_RENDER_SYSTEM=gles \
	-DENABLE_VAAPI=ON \
	-DENABLE_VDPAU=OFF \
	-DENABLE_GOLD=OFF \
	-DENABLE_LLD=OFF \
	-DENABLE_MOLD=OFF \
	-DUSE_LTO=OFF \
	-DENABLE_SSE=ON \
	-DENABLE_SSE2=ON \
	-DENABLE_SSE3=OFF \
	-DENABLE_SSSE3=OFF \
	-DENABLE_SSE4_1=OFF \
	-DENABLE_SSE4_2=OFF \
	-DENABLE_AVX=OFF \
	-DENABLE_AVX2=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_EVENTCLIENTS=OFF \
	-DENABLE_PYTHON=ON \
	-DENABLE_UPNP=ON \
	-DENABLE_OPTICAL=ON \
	-DENABLE_DVDCSS=ON \
	-DENABLE_AIRTUNES=ON \
	-DENABLE_ALSA=ON \
	-DENABLE_PULSEAUDIO=ON \
	-DENABLE_PIPEWIRE=ON \
	-DENABLE_AVAHI=ON \
	-DENABLE_BLUETOOTH=ON \
	-DENABLE_BLURAY=ON \
	-DENABLE_CAP=ON \
	-DENABLE_DAV1D=ON \
	-DENABLE_DBUS=ON \
	-DENABLE_LCMS2=ON \
	-DENABLE_MICROHTTPD=ON \
	-DENABLE_PLIST=ON \
	-DENABLE_SMBCLIENT=ON \
	-DENABLE_UDEV=ON \
	-DENABLE_UDFREAD=ON \
	-DENABLE_XSLT=ON \
	-DENABLE_CEC=ON \
	-DENABLE_NFS=ON \
	-DENABLE_ISO9660PP=ON \
	-DENABLE_LIRCCLIENT=ON \
	-DENABLE_MDNS=OFF \
	-DENABLE_SNDIO=OFF \
	-DENABLE_MARIADBCLIENT=OFF \
	-DENABLE_MYSQLCLIENT=OFF \
	-DENABLE_INTERNAL_FFMPEG=ON \
	-DENABLE_INTERNAL_DAV1D=OFF \
	-DENABLE_INTERNAL_CROSSGUID=ON \
	-DENABLE_INTERNAL_CEC=OFF \
	-DENABLE_INTERNAL_NFS=OFF \
	-DENABLE_INTERNAL_FLATBUFFERS=ON \
	-DENABLE_INTERNAL_FMT=ON \
	-DENABLE_INTERNAL_SPDLOG=ON \
	-DENABLE_INTERNAL_FSTRCMP=ON \
	-DENABLE_INTERNAL_PCRE=ON \
	-DENABLE_INTERNAL_RapidJSON=OFF \
	-DENABLE_INTERNAL_TAGLIB=OFF \
	-DENABLE_INTERNAL_UDFREAD=OFF \
	-DENABLE_INTERNAL_KISSFFT=ON \
	-DENABLE_INTERNAL_GTEST=OFF \
	-DCROSSGUID_URL="$(_src "crossguid-$_crossguid.tar.gz")" \
	-DLIBDVDCSS_URL="$(_src "libdvdcss-$_dvdcss.tar.gz")" \
	-DLIBDVDNAV_URL="$(_src "libdvdnav-$_dvdnav.tar.gz")" \
	-DLIBDVDREAD_URL="$(_src "libdvdread-$_dvdread.tar.gz")" \
	-DFFMPEG_URL="$(_src "ffmpeg-$_ffmpeg.tar.gz")" \
	-DFMT_URL="$(_src "fmt-$_fmt.tar.gz")" \
	-DSPDLOG_URL="$(_src "spdlog-$_spdlog.tar.gz")" \
	-DFLATBUFFERS_URL="$(_src "flatbuffers-$_flatbuf.tar.gz")" \
	-DFSTRCMP_URL="$(_src "fstrcmp-$_fstrcmp.tar.gz")" \
	-DPCRE_URL="$(_src "pcre-$_pcre.tar.bz2")" \
	-Dgroovy_SOURCE_DIR="$_groovydir" \
	-Dapache-commons-lang_SOURCE_DIR="$SRC_ROOT/commons-lang3-$_langver" \
	-Dapache-commons-text_SOURCE_DIR="$SRC_ROOT/commons-text-$_textver" \
	-Wno-dev
ninja crossguid
ninja
DESTDIR=$PKG ninja install
cd ..
rm -f "$PKG/usr/bin/TexturePacker"

rm -rf "$PKG/usr/share/xsessions" "$PKG/usr/share/wayland-sessions" \
	"$PKG/usr/lib/firewalld"

install -Dm644 /dev/stdin "$PKG/usr/share/kodi/system/settings/appliance.xml" <<'XML'
<?xml version="1.0" encoding="utf-8" ?>
<settings version="1">
  <section id="system">
    <category id="addons">
      <group id="1">
        <setting id="general.addonupdates">
          <default>2</default>
        </setting>
      </group>
    </category>
  </section>
</settings>
XML

cat > "$PKG/usr/share/applications/kodi.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kodi
GenericName=Media Center
Comment=Watch and listen to your videos, music and pictures
TryExec=kodi
Exec=kodi
Icon=kodi
Terminal=false
StartupWMClass=Kodi
Categories=AudioVideo;Video;Player;TV;
Keywords=kodi;xbmc;media;center;tv;movies;music;player;
Actions=Fullscreen;

[Desktop Action Fullscreen]
Name=Open in Full Screen
Exec=kodi -fs
DESKTOP
chmod 644 "$PKG/usr/share/applications/kodi.desktop"

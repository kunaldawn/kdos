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

# AUDIO IS PIPEWIRE'S, NAMED. Both backends are `auto` and pulse wins the
# default whenever libpulse-simple is installed, so left alone the recorder's
# audio path follows whatever the chroot happens to hold. Enabled, a missing
# libpipewire stops configure instead of producing a recorder with no audio.
#
# libavdevice has no switch — it is a `required: false` probe — and is found
# because ffmpeg builds it; it is what writes to a v4l2loopback device.
# ffmpeg-supported-config.patch is ammen99/wf-recorder#352: FFmpeg 9 removed
# AVCodec's pix_fmts, sample_fmts and ch_layouts, which the frame writer reads
# to pick formats. The patch asks avcodec_get_supported_config() for the same
# lists, and keeps the fields for a libavcodec older than 61.13.100.
patch -p1 -i "$PORT_SRC/ffmpeg-supported-config.patch"
meson setup build --prefix=/usr --libdir=lib --buildtype=release -Db_ndebug=if-release \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Ddefault_audio_backend=pipewire
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# meson.build names the fish completion directory share/fish/fish/, which fish
# never reads; vendor_completions.d under share/fish is where it looks.
install -d "$PKG/usr/share/fish/vendor_completions.d"
mv "$PKG/usr/share/fish/fish/vendor_completions.d/wf-recorder.fish" \
	"$PKG/usr/share/fish/vendor_completions.d/"
rm -r "$PKG/usr/share/fish/fish"

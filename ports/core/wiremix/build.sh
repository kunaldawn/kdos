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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# pipewire-sys GENERATES ITS BINDINGS, so this build runs bindgen, which
# dlopens libclang — and a static-musl rust binary cannot dlopen anything at
# all. Turning crt-static off is what gives the build script a dynamic loader;
# LIBCLANG_PATH saves it a search. Both are build-time only.
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline
install -Dm755 target/release/wiremix $PKG/usr/bin/wiremix

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/wiremix.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Volume Mixer
GenericName=Audio Mixer
Comment=Volumes, devices and routing for PipeWire
Exec=wiremix
Icon=audio-x-generic
Terminal=true
Categories=AudioVideo;Mixer;Settings;
Keywords=audio;volume;mixer;pipewire;route;wiremix;
ENTRY
chmod 644 "$PKG/usr/share/applications/wiremix.desktop"

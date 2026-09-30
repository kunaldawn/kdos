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

# X-KDOS-Term NAMES THE EMULATOR THIS ENTRY NEEDS. The previews are the whole
# reason this is on the image, and they are drawn by the terminal rather than by
# yazi: `kdos-term` links the decoders and speaks sixel and the kitty protocol,
# and the launcher opens this entry in it. The key accepts
# only an emulator this image ships; without it the session's own terminal is
# used, which is lighter.

# DO NOT DISABLE THE IMAGE PREVIEW. In kdos-term yazi's previewer draws real
# thumbnails in a window that is otherwise a character grid — the one place on
# this system where a picture beats a filename, and the reason to have this
# beside mc and kdos-pick rather than instead of them.
#
# THE PREVIEWERS ARE OTHER PROGRAMS, which is why `depends` names them: yazi
# execs file(1) for mime types, ffmpeg for video, pdftoppm (poppler) for PDF,
# resvg for SVG, magick for the rest, jq for JSON, fd/rg/fzf/zoxide for search
# and jump, 7zz (7zip) to list and extract archives, ISO and disk images, and
# chafa where the terminal has no image protocol. A missing one is a blank
# preview, not an error.

# VERGEN_GIT_SHA IS SUPPLIED BECAUSE A TARBALL IS NOT A REPOSITORY. yazi's
# build script uses vergen to stamp the binary with the commit it came from,
# and with no .git present the crate emits nothing while the source still reads
# `env!("VERGEN_GIT_SHA")` — a compile error naming an environment variable
# rather than a missing tool. The release tag is the honest answer to "which
# commit is this": it is exactly what the tarball was cut from.
export VERGEN_GIT_SHA="$version"
export VERGEN_IDEMPOTENT=1

export YAZI_GEN_COMPLETIONS=1
export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
install -Dm755 target/release/yazi $PKG/usr/bin/yazi
install -Dm755 target/release/ya   $PKG/usr/bin/ya

# YAZI_GEN_COMPLETIONS makes each crate's build script write its completions
# into that crate's own completions/ directory.
for b in yazi-boot/completions/yazi yazi-cli/completions/ya; do
	n=${b##*/}
	install -Dm644 "$b.bash" "$PKG/usr/share/bash-completion/completions/$n"
	install -Dm644 "${b%/*}/_$n" "$PKG/usr/share/zsh/site-functions/_$n"
	install -Dm644 "$b.fish" "$PKG/usr/share/fish/vendor_completions.d/$n.fish"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/yazi.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Files (yazi)
GenericName=File Manager
Comment=Browse files with yazi
Exec=yazi %f
Icon=folder
Terminal=true
X-KDOS-Term=kdos-term
Categories=System;FileTools;FileManager;
Keywords=file;manager;browser;yazi;preview;
EOF
chmod 644 "$PKG/usr/share/applications/yazi.desktop"

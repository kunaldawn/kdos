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
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
export VERGEN_GIT_DESCRIBE="v$version"
export VERGEN_GIT_SHA="v$version"

cargo build --release --frozen --offline

install -Dm755 target/release/rmpc $PKG/usr/bin/rmpc

install -Dm644 target/man/rmpc.1 $PKG/usr/share/man/man1/rmpc.1
for f in target/completions/*; do
	[ -e "$f" ] || continue
	case "$f" in
	*.bash|*rmpc.bash)
		install -Dm644 "$f" \
			$PKG/usr/share/bash-completion/completions/rmpc ;;
	*.fish)
		install -Dm644 "$f" \
			$PKG/usr/share/fish/vendor_completions.d/rmpc.fish ;;
	_rmpc|*.zsh)
		install -Dm644 "$f" $PKG/usr/share/zsh/site-functions/_rmpc ;;
	esac
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/rmpc.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=Music Player
GenericName=MPD Client
Comment=Play the music library mpd is serving
Exec=rmpc
Icon=folder-music
Terminal=true
Categories=AudioVideo;Player;
Keywords=music;audio;player;mpd;rmpc;
X-KDOS-Term=kdos-term
DESK
chmod 644 "$PKG/usr/share/applications/rmpc.desktop"

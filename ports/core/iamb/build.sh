# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE TWO LINK DECISIONS, MADE HERE RATHER THAN INHERITED.
#
# SQLITE IS BUNDLED, and that is not laziness: every Rust port in this tree
# builds a static-pie musl binary — `readelf -d` on atuin or pizauth prints no
# NEEDED at all — so none of them can reach the shared libsqlite3 the image
# carries for its C programs. `bundled` compiles matrix-sdk's own copy, which
# is the only shape that works for a statically linked client.
#
# TLS IS rustls AND NOT openssl. `bundled` selects it; the `native-tls` feature
# is the other road and would put OpenSSL under a chat client that has no
# reason to need the system's certificate stack rather than its own.
#
# THE `desktop` FEATURE IS KEPT, AND IT COSTS NOTHING TO LINK. It is
# notify-rust plus modalkit's clipboard, which is arboard — and measured on the
# built binary, arboard reaches X11 through x11rb, which speaks the PROTOCOL in
# Rust: the result is static-pie with no NEEDED at all and not one libX11,
# libxcb or libwayland-client string in it. What it buys is notifications over
# zbus, which reach kdos-notifyd like any other program's.
#
# ITS CLIPBOARD HALF IS INERT HERE and that is worth knowing rather than
# discovering: there is no X server on this image, and `wayland-data-control`
# is absent from the binary, so a yank inside iamb has nowhere to go. The
# desktop's clipboard is kdos-clip.
# THE TRAIT SOLVER RUNS OUT OF DEPTH BEFORE THE CODE RUNS OUT OF SENSE.
# matrix-sdk's sync path is async functions nested deep enough that proving the
# future is `Send` exceeds rustc's default recursion limit of 128, and the
# build stops with `E0275: overflow evaluating the requirement … Send`. The
# limit is a CRATE ATTRIBUTE and there is no flag for it on a stable compiler —
# `-Z recursion-limit` is nightly's — so this is one of the few places a patch
# is the only road. rustc's own diagnostic names the fix.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
patch -p1 -i $PORT_SRC/recursion-limit.patch

cargo build --release --frozen --offline

install -Dm755 target/release/iamb $PKG/usr/bin/iamb

# Terminal=true and a bare Exec: the launcher supplies the emulator, which is
# the only way one entry serves both desktops. No X-KDOS-Term: that key names
# the emulator an entry needs, and it is for the programs that draw pictures in
# the cell grid. iamb's image preview is `image_preview` in its own config and
# is absent unless somebody sets it, so this entry takes the session's terminal,
# which is lighter.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/iamb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Matrix
GenericName=Chat
Comment=Matrix chat with vim's keys
Exec=iamb
Icon=mail-message
Terminal=true
Categories=Network;InstantMessaging;
Keywords=chat;matrix;im;message;iamb;
EOF
chmod 644 "$PKG/usr/share/applications/iamb.desktop"

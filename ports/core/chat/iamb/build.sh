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
# notify-rust plus modalkit's clipboard, and both halves are pure Rust: the
# result is static-pie with no NEEDED at all and not one libX11, libxcb or
# libwayland-client string in it. Notifications go over zbus and reach
# kdos-notifyd like any other program's.
#
# AND ITS CLIPBOARD REACHES kdos-clip, WHICH IS WHY THE FEATURE IS NOT NARROWED.
# modalkit asks arboard for `wayland-data-control`, so the binary carries
# wl-clipboard-rs speaking `zwlr_data_control_v1` — 214 of those symbols are in
# it — and kdos-comp creates both data-control managers. That path stays pure
# Rust only while wayland-backend's `client_system` feature is off: turning it
# on links libwayland-client and a static-pie musl binary cannot have it.
# A login with no Wayland socket falls back to arboard's X11 path, and there is
# no X server here — so a yank from a bare terminal has nowhere to go and the
# terminal's own selection is the answer.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline

install -Dm755 target/release/iamb $PKG/usr/bin/iamb
install -Dm644 docs/iamb.1 -t "$PKG/usr/share/man/man1"
install -Dm644 docs/iamb.5 -t "$PKG/usr/share/man/man5"

# Terminal=true and a bare Exec: the launcher supplies the emulator, which is
# what keeps the entry free of one. No X-KDOS-Term: that key names
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

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
cargo build --release --frozen --offline
install -Dm755 target/release/virtiofsd "$PKG/usr/bin/virtiofsd"
install -d "$PKG/usr/share/qemu/vhost-user"
cat > "$PKG/usr/share/qemu/vhost-user/50-virtiofsd.json" <<'JSON'
{
  "description": "virtiofsd vhost-user-fs",
  "type": "fs",
  "binary": "/usr/bin/virtiofsd",
  "features": [
      "migrate-precopy",
      "posix-acl-negotiation-mode",
      "security-label-negotiation-mode",
      "separate-options"
  ]
}
JSON
chmod 644 "$PKG/usr/share/qemu/vhost-user/50-virtiofsd.json"

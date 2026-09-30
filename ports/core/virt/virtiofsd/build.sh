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

# THE SHARE A LINUX GUEST EXPECTS. qemu's 9p export (--enable-virtfs) works
# and is slow; virtio-fs maps the host directory into the guest through a
# vhost-user device served by this daemon; qemu ships none of its own,
# so this port is the only one. One daemon per shared directory, started
# before the guest:
#
#   virtiofsd --socket-path=/tmp/vfs.sock --shared-dir=DIR &
#   qemu-system-x86_64 -object memory-backend-memfd,id=mem,size=4G,share=on \
#       -numa node,memdev=mem -chardev socket,id=vfs,path=/tmp/vfs.sock \
#       -device vhost-user-fs-pci,chardev=vfs,tag=host ...
#
# It sandboxes itself with seccomp and drops capabilities with libcap-ng, so
# both are linked, not optional.
#
# libvirt finds virtiofsd only through a vhost-user descriptor of type fs in
# /usr/share/qemu/vhost-user, and refuses to start a guest that has a virtiofs
# <filesystem> share when none names a binary that exists. Upstream's
# 50-virtiofsd.json names /usr/libexec/virtiofsd; this one names where the
# binary is installed.
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

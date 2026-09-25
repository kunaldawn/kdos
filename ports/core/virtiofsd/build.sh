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
# both are linked, not optional. 50-virtiofsd.json is libvirt's descriptor
# for the binary, and libvirt is not on this system.
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
install -Dm755 target/release/virtiofsd "$PKG/usr/bin/virtiofsd"

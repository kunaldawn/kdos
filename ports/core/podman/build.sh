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

# Disable systemd integration; KDOS has no systemd.
export BUILDTAGS="seccomp exclude_graphdriver_btrfs"

# `binaries` builds podman, podman-remote, rootlessport, quadlet, etc.
make BUILDTAGS="$BUILDTAGS" \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	binaries docs

make DESTDIR=$PKG \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	install.bin install.remote install.man install.completions

# install.bin always installs quadlet's generator links under lib/systemd and a
# tmpfiles.d entry: the directory variables only move them, and nothing here
# reads either.
rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/tmpfiles.d"

mkdir -p $PKG/etc/containers

cat > $PKG/etc/containers/containers.conf <<'EOF'
[engine]
cgroup_manager = "cgroupfs"
events_logger = "file"
runtime = "crun"

[network]
network_backend = "netavark"
firewall_driver = "nftables"
default_rootless_network_cmd = "pasta"
EOF

cat > $PKG/etc/containers/storage.conf <<'EOF'
[storage]
driver = "overlay"
runroot = "/run/containers/storage"
graphroot = "/var/lib/containers/storage"

[storage.options.overlay]
mount_program = "/usr/bin/fuse-overlayfs"
EOF

cat > $PKG/etc/containers/registries.conf <<'EOF'
unqualified-search-registries = ["docker.io"]
EOF

chmod 0644 $PKG/etc/containers/containers.conf \
	$PKG/etc/containers/storage.conf \
	$PKG/etc/containers/registries.conf

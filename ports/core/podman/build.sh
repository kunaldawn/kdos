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
export BUILDTAGS="seccomp exclude_graphdriver_btrfs exclude_graphdriver_devicemapper"

# `binaries` builds podman, podman-remote, rootlessport, quadlet, etc.
# Skip install.man — man pages need go-md2man (not packaged in kdos).
make BUILDTAGS="$BUILDTAGS" \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	binaries

make DESTDIR=$PKG \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	install.bin install.remote install.completions

mkdir -p $PKG/etc/containers

cat > $PKG/etc/containers/containers.conf <<'EOF'
[engine]
cgroup_manager = "cgroupfs"
events_logger = "file"
runtime = "crun"

[network]
network_backend = "netavark"
firewall_driver = "nftables"
default_rootless_network_cmd = "slirp4netns"
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

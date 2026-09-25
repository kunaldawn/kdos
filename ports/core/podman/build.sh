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

# BUILDTAGS is set whole, which replaces upstream's probes: no systemd tag
# (KDOS has no systemd), no apparmor, no libsubid, no btrfs driver, and
# libsqlite3 so the database links the sqlite port instead of the copy bundled
# with the Go binding.
export BUILDTAGS="seccomp libsqlite3 exclude_graphdriver_btrfs"

# `binaries` builds podman, podman-remote, rootlessport, quadlet, etc.
make BUILDTAGS="$BUILDTAGS" \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	GOMD2MAN=/usr/bin/go-md2man \
	binaries docs

make DESTDIR=$PKG \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	install.bin install.remote install.man install.completions install.docker

# install.bin and install.docker always install quadlet's generator links under
# lib/systemd and tmpfiles.d entries: the directory variables only move them,
# and nothing here reads any of them. The csh half of the DOCKER_HOST profile
# goes too; no csh is installed.
rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/tmpfiles.d" "$PKG/usr/share/user-tmpfiles.d"
rm -f "$PKG/etc/profile.d/podman-docker.csh"

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

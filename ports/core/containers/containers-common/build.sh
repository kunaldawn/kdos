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

# ONE OWNER FOR /etc/containers, because every engine reads all of it. podman,
# buildah and skopeo each open policy.json before the first pull and refuse
# the pull without it; each reads storage.conf to find the store and
# registries.conf to expand a short name. A file that came with only one of
# them would leave the others failing whenever that one was not installed.
#
# cgroupfs, because there is no systemd to delegate a cgroup. The file event
# logger, because journald does not exist. storage.conf names fuse-overlayfs
# as the mount program, which this port depends on, since every reader of the
# store mounts through it. crun, netavark and pasta are named here but only a
# port that runs containers needs them, and each such port depends on them.
install -d "$PKG/etc/containers"

cat > "$PKG/etc/containers/containers.conf" <<'EOF2'
[engine]
cgroup_manager = "cgroupfs"
events_logger = "file"
runtime = "crun"

[network]
network_backend = "netavark"
firewall_driver = "nftables"
default_rootless_network_cmd = "pasta"
EOF2

cat > "$PKG/etc/containers/storage.conf" <<'EOF2'
[storage]
driver = "overlay"
runroot = "/run/containers/storage"
graphroot = "/var/lib/containers/storage"

[storage.options.overlay]
mount_program = "/usr/bin/fuse-overlayfs"
EOF2

cat > "$PKG/etc/containers/registries.conf" <<'EOF2'
unqualified-search-registries = ["docker.io"]
EOF2

# Image signatures are not checked: this is upstream's default policy, which
# accepts any image from any transport.
cat > "$PKG/etc/containers/policy.json" <<'EOF2'
{
    "default": [
        {
            "type": "insecureAcceptAnything"
        }
    ],
    "transports": {
        "docker-daemon": {
            "": [{"type": "insecureAcceptAnything"}]
        }
    }
}
EOF2

chmod 0644 "$PKG/etc/containers/containers.conf" \
	"$PKG/etc/containers/storage.conf" \
	"$PKG/etc/containers/registries.conf" \
	"$PKG/etc/containers/policy.json"

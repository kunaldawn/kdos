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

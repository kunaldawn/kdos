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

make PREFIX=/usr bin/skopeo
install -Dm755 bin/skopeo $PKG/usr/bin/skopeo
make PREFIX=/usr DESTDIR=$PKG install-docs
make PREFIX=/usr DESTDIR=$PKG install-completions
install -Dm644 default-policy.json $PKG/etc/containers/policy.json
install -Dm644 default.yaml $PKG/etc/containers/registries.d/default.yaml

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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# THE CLOUD BACKENDS ARE DEAD WEIGHT AND THE LOCAL VERBS ARE NOT. What earns
# this a place on an offline machine is `rclone check`, which answers "did
# every byte reach the second stick" in one command over two local trees —
# nothing else here compares two directories by CONTENT and reports what
# differs. `rclone mount` additionally reads an archive as a filesystem.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X github.com/rclone/rclone/fs.Version=v$version" -o rclone
install -Dm755 rclone $PKG/usr/bin/rclone
install -Dm644 rclone.1 $PKG/usr/share/man/man1/rclone.1

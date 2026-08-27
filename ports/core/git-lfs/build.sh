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

# THE KDOS REPO ITSELF NO LONGER USES LFS — its tarballs travel as release
# packfiles — but `kdos rebuild` promises this stick can rebuild this stick,
# and that promise extends to any OTHER repository somebody clones on it. A
# clone of an LFS repo without this SUCCEEDS and checks out pointer files: a
# hundred-byte text file where a binary should be, which fails later and
# somewhere else.
export CGO_ENABLED=0
go build -mod=vendor \
	-ldflags "-s -w -X github.com/git-lfs/git-lfs/v3/config.Vendor=KDOS -X github.com/git-lfs/git-lfs/v3/config.GitCommit=v$version" \
	-o git-lfs
install -Dm755 git-lfs $PKG/usr/bin/git-lfs

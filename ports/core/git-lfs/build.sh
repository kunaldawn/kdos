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
go generate ./commands
go build -mod=vendor \
	-ldflags "-s -w -X github.com/git-lfs/git-lfs/v3/config.Vendor=KDOS -X github.com/git-lfs/git-lfs/v3/config.GitCommit=v$version" \
	-o git-lfs
install -Dm755 git-lfs $PKG/usr/bin/git-lfs
for sh in bash zsh fish; do
	HOME="$PWD/.home" ./git-lfs completion $sh > git-lfs.$sh
done
install -Dm644 git-lfs.bash $PKG/usr/share/bash-completion/completions/git-lfs
install -Dm644 git-lfs.zsh  $PKG/usr/share/zsh/site-functions/_git-lfs
install -Dm644 git-lfs.fish $PKG/usr/share/fish/vendor_completions.d/git-lfs.fish

# The manual pages only, from upstream's own list: `make man` also writes an
# HTML copy of every page. MAN_ROFF_TARGETS names each page under the section
# its .adoc declares (git-lfs-config is 5, git-lfs-faq is 7). The list is read
# by a recipe because a recipe expands when it runs, after the Makefile is
# read; an --eval'd prerequisite list expands before, and is empty.
# VERSION and GIT_LFS_SHA are given because the Makefile otherwise asks
# `git describe` of whatever repository encloses the build directory.
_mk="VERSION=v$version GIT_LFS_SHA=v$version"
make $_mk $(make -s $_mk --eval='kdos-man-list: ; @echo $(MAN_ROFF_TARGETS)' kdos-man-list)
for page in man/man*/*.[1-8]; do
	install -Dm644 "$page" -t "$PKG/usr/share/man/man${page##*.}"
done

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

_mk="VERSION=v$version GIT_LFS_SHA=v$version"
make $_mk $(make -s $_mk --eval='kdos-man-list: ; @echo $(MAN_ROFF_TARGETS)' kdos-man-list)
for page in man/man*/*.[1-8]; do
	install -Dm644 "$page" -t "$PKG/usr/share/man/man${page##*.}"
done

install -Dm644 /dev/stdin "$PKG/etc/gitconfig" <<'GITCONFIG'
[filter "lfs"]
	clean = git-lfs clean -- %f
	smudge = git-lfs smudge -- %f
	process = git-lfs filter-process
	required = true
GITCONFIG

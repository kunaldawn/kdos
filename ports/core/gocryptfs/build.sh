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

# IT ENCRYPTS FILE BY FILE, WHICH IS WHY IT IS THE ONE THAT WORKS INSIDE
# SYNCTHING. A container-format volume (LUKS, veracrypt) is one enormous file
# that changes on every write, so syncing it means resending the whole thing
# and two machines editing it at once corrupt it; gocryptfs keeps a ciphertext
# file per plaintext file, so a sync moves only what changed and merges the way
# ordinary files do.
#
# CGO IS ON HERE AND OFF EVERYWHERE ELSE IN THIS TREE. gocryptfs uses OpenSSL
# for AES-GCM because Go's own implementation is several times slower on a
# filesystem's hot path, and that is a cgo binding — so unlike every other Go
# port here this one links libcrypto and is not static. The alternative is
# `-tags without_openssl`, which builds a pure-Go binary that is correct and
# noticeably slower.
export CGO_ENABLED=1
# -o INTO a directory rather than beside one: `./gocryptfs-xray` is a package
# directory of that same name, and `go build -o gocryptfs-xray` writes the
# binary inside it instead of over it.
mkdir -p out
go build -mod=vendor -ldflags "-X main.GitVersion=v$version -X main.GitVersionFuse=vendored -X main.BuildDate=offline" -o out/gocryptfs
go build -mod=vendor -o out/gocryptfs-xray ./gocryptfs-xray
install -Dm755 out/gocryptfs       $PKG/usr/bin/gocryptfs
install -Dm755 out/gocryptfs-xray  $PKG/usr/bin/gocryptfs-xray
# The manual is markdown in the tarball and upstream renders it with pandoc,
# which is not a port. The source is installed rather than nothing, so `-h` is
# not the only description of the flags on the machine.
install -Dm644 Documentation/MANPAGE.md       $PKG/usr/share/doc/gocryptfs/MANPAGE.md
install -Dm644 Documentation/MANPAGE-XRAY.md  $PKG/usr/share/doc/gocryptfs/MANPAGE-XRAY.md
install -Dm644 Documentation/MANPAGE-STATFS.md $PKG/usr/share/doc/gocryptfs/MANPAGE-STATFS.md

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

# The source is Hackage's pandoc-cli: the `pandoc` executable and its three
# manual pages. The converter itself is the `pandoc` library, and the Lua
# engine and the HTTP server are `pandoc-lua-engine` and `pandoc-server`; all
# three come from the vendor bundle with the rest of the 229 packages.
#
# The port directory carries the freeze, because an unpinned `cabal freeze`
# settles on flags that build a smaller program. pandoc-cli comes out
# `-lua -server`: no `--lua-filter`, no server, and neither library in the
# bundle. The pandoc library comes out `-embed_data_files`, and its templates,
# reference documents and abbreviations then stay in the build's cabal store,
# so every DOCX, ODT and EPUB conversion on the installed system fails naming
# a path under $SRC_ROOT. The freeze pins all three flags on, and the flags
# below make a freeze that disagrees fail the solve instead. It also pins the
# `pandoc` library to this version, so a version bump rewrites it first, with
# the ghc and cabal the recipes name, in the new source:
#   cabal freeze --constraint="pandoc-cli +lua +server" \
#                --constraint="pandoc +embed_data_files"
# then `ports/fetch pandoc`.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
cp "$SRC_ROOT/vendor/cabal.project.freeze" .
export CABAL_DIR="$SRC_ROOT/cabal"
mkdir -p "$CABAL_DIR"
printf 'repository hackage.haskell.org\n  url: file+noindex://%s\n' "$SRC_ROOT/vendor" \
	> "$CABAL_DIR/config"

# Lua is the copy of Lua 5.4 the `lua` package compiles in (`-system-lua`),
# so filters see the Lua version pandoc was written against whatever the
# system's lua port is. zlib is the system library, found by pkg-config.
# Split sections let the link drop the unused code of 229 libraries, and the
# stripped binary carries no debug information.
cabal v2-install -j \
	--flags="+lua +server" \
	--constraint="pandoc +embed_data_files" \
	--enable-split-sections \
	--enable-executable-stripping \
	--installdir="$PKG/usr/bin" --install-method=copy \
	exe:pandoc

# One binary is all three programs: invoked as `pandoc-server` it serves
# conversions over HTTP, and as `pandoc-lua` it is a Lua interpreter with
# the pandoc modules loaded.
ln -s pandoc "$PKG/usr/bin/pandoc-server"
ln -s pandoc "$PKG/usr/bin/pandoc-lua"
install -Dm644 -t "$PKG/usr/share/man/man1" \
	man/pandoc.1 man/pandoc-server.1 man/pandoc-lua.1

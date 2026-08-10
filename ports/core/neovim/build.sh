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

# Pre-stage vendored dep tarballs into CMake's ExternalProject DOWNLOAD_DIR.
# CMake URL_HASH-checks each file before fetching, so when a tarball is
# already present with a matching SHA256 it skips the network entirely.
dl=".deps/build/downloads"
mkdir -p "$dl"/{libuv,luajit,unibilium,luv,lpeg,lua_compat53,utf8proc,treesitter} \
         "$dl"/{treesitter_c,treesitter_lua,treesitter_vim,treesitter_vimdoc,treesitter_query,treesitter_markdown}

cp "$PORT_SRC/libuv-v1.52.1.tar.gz"               "$dl/libuv/v1.52.1.tar.gz"
cp "$PORT_SRC/luajit-fbb36bb6.tar.gz"             "$dl/luajit/fbb36bb6bfa88716a47c58bcf9ce9f2ef752abac.tar.gz"
cp "$PORT_SRC/unibilium-v2.1.2.tar.gz"            "$dl/unibilium/v2.1.2.tar.gz"
cp "$PORT_SRC/luv-1.52.1-0.tar.gz"                "$dl/luv/1.52.1-0.tar.gz"
cp "$PORT_SRC/lpeg-1.1.0.tar.gz"                  "$dl/lpeg/lpeg-1.1.0.tar.gz"
cp "$PORT_SRC/lua-compat-5.3-v0.13.tar.gz"        "$dl/lua_compat53/v0.13.tar.gz"
cp "$PORT_SRC/utf8proc-v2.11.3.tar.gz"            "$dl/utf8proc/v2.11.3.tar.gz"
cp "$PORT_SRC/tree-sitter-v0.26.7.tar.gz"         "$dl/treesitter/v0.26.7.tar.gz"
cp "$PORT_SRC/tree-sitter-c-v0.24.1.tar.gz"       "$dl/treesitter_c/v0.24.1.tar.gz"
cp "$PORT_SRC/tree-sitter-lua-v0.5.0.tar.gz"      "$dl/treesitter_lua/v0.5.0.tar.gz"
cp "$PORT_SRC/tree-sitter-vim-v0.8.1.tar.gz"      "$dl/treesitter_vim/v0.8.1.tar.gz"
cp "$PORT_SRC/tree-sitter-vimdoc-v4.1.0.tar.gz"   "$dl/treesitter_vimdoc/v4.1.0.tar.gz"
cp "$PORT_SRC/tree-sitter-query-v0.8.0.tar.gz"    "$dl/treesitter_query/v0.8.0.tar.gz"
cp "$PORT_SRC/tree-sitter-markdown-v0.5.3.tar.gz" "$dl/treesitter_markdown/v0.5.3.tar.gz"

make CMAKE_BUILD_TYPE=Release \
	CMAKE_INSTALL_PREFIX=/usr \
	CMAKE_EXTRA_FLAGS="-DCMAKE_INSTALL_LIBDIR=lib"
make DESTDIR=$PKG install

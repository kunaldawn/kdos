#!/bin/bash
set -e
source script/env.sh

echo ">>> Fetching sources..."
echo ">>> Using GNU Mirror: $GNU_MIRROR"

download() {
    local url=$1
    local file=$2
    if [ ! -f "$SRC_DIR/$file" ]; then
        echo "Downloading $file..."
        wget -O "$SRC_DIR/$file" "$url"
    else
        echo "Found $file"
    fi
}

# --- Base System ---
download "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$LINUX_VER.tar.xz" "linux-$LINUX_VER.tar.xz"
download "https://musl.libc.org/releases/musl-$MUSL_VER.tar.gz" "musl-$MUSL_VER.tar.gz"
download "http://landley.net/toybox/downloads/toybox-$TOYBOX_VER.tar.gz" "toybox-$TOYBOX_VER.tar.gz"
download "https://github.com/void-linux/musl-fts/archive/v$MUSL_FTS_VER.tar.gz" "musl-fts-$MUSL_FTS_VER.tar.gz"
download "$GNU_MIRROR/bash/bash-$BASH_VER.tar.gz" "bash-$BASH_VER.tar.gz"

# --- Toolchain (Cross & Native) ---
download "$GNU_MIRROR/binutils/binutils-$BINUTILS_VER.tar.xz" "binutils-$BINUTILS_VER.tar.xz"
download "$GNU_MIRROR/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz" "gcc-$GCC_VER.tar.xz"
download "$GNU_MIRROR/make/make-$MAKE_VER.tar.gz" "make-$MAKE_VER.tar.gz"
download "$GNU_MIRROR/gmp/gmp-$GMP_VER.tar.xz" "gmp-$GMP_VER.tar.xz"
download "$GNU_MIRROR/mpfr/mpfr-$MPFR_VER.tar.xz" "mpfr-$MPFR_VER.tar.xz"
download "$GNU_MIRROR/mpc/mpc-$MPC_VER.tar.gz" "mpc-$MPC_VER.tar.gz"

# --- Libraries ---
download "https://zlib.net/zlib-$ZLIB_VER.tar.gz" "zlib-$ZLIB_VER.tar.gz"
download "https://www.openssl.org/source/openssl-$OPENSSL_VER.tar.gz" "openssl-$OPENSSL_VER.tar.gz"
download "$GNU_MIRROR/ncurses/ncurses-$NCURSES_VER.tar.gz" "ncurses-$NCURSES_VER.tar.gz"
download "$GNU_MIRROR/readline/readline-$READLINE_VER.tar.gz" "readline-$READLINE_VER.tar.gz"
download "https://github.com/libevent/libevent/releases/download/release-$LIBEVENT_VER/libevent-$LIBEVENT_VER.tar.gz" "libevent-$LIBEVENT_VER.tar.gz"
download "https://www.tcpdump.org/release/libpcap-$LIBPCAP_VER.tar.gz" "libpcap-$LIBPCAP_VER.tar.gz"

# --- Tools ---
download "https://matt.ucc.asn.au/dropbear/releases/dropbear-$DROPBEAR_VER.tar.bz2" "dropbear-$DROPBEAR_VER.tar.bz2"
download "https://curl.se/download/curl-$CURL_VER.tar.gz" "curl-$CURL_VER.tar.gz"
download "http://www.dest-unreach.org/socat/download/socat-$SOCAT_VER.tar.gz" "socat-$SOCAT_VER.tar.gz"
download "https://www.tcpdump.org/release/tcpdump-$TCPDUMP_VER.tar.gz" "tcpdump-$TCPDUMP_VER.tar.gz"
download "http://links.twibright.com/download/links-$LINKS_VER.tar.gz" "links-$LINKS_VER.tar.gz"
download "https://github.com/htop-dev/htop/releases/download/$HTOP_VER/htop-$HTOP_VER.tar.xz" "htop-$HTOP_VER.tar.xz"
download "$GNU_MIRROR/bc/bc-$BC_VER.tar.gz" "bc-$BC_VER.tar.gz"
download "https://github.com/jqlang/jq/releases/download/jq-$JQ_VER/jq-$JQ_VER.tar.gz" "jq-$JQ_VER.tar.gz"
download "https://github.com/vim/vim/archive/refs/tags/v$VIM_VER.0000.tar.gz" "vim-$VIM_VER.tar.gz"
download "https://github.com/tmux/tmux/releases/download/$TMUX_VER/tmux-$TMUX_VER.tar.gz" "tmux-$TMUX_VER.tar.gz"
download "$GNU_MIRROR/nano/nano-$NANO_VER.tar.xz" "nano-$NANO_VER.tar.xz"
download "$GNU_MIRROR/gawk/gawk-$GAWK_VER.tar.xz" "gawk-$GAWK_VER.tar.xz"
download "$GNU_MIRROR/diffutils/diffutils-$DIFFUTILS_VER.tar.xz" "diffutils-$DIFFUTILS_VER.tar.xz"
download "$GNU_MIRROR/gzip/gzip-$GZIP_VER.tar.xz" "gzip-$GZIP_VER.tar.xz"
download "http://ftp.astron.com/pub/file/file-$FILE_VER.tar.gz" "file-$FILE_VER.tar.gz"
download "https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v${UTIL_LINUX_VER%.*}/util-linux-$UTIL_LINUX_VER.tar.xz" "util-linux-$UTIL_LINUX_VER.tar.xz"
download "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v$E2FSPROGS_VER/e2fsprogs-$E2FSPROGS_VER.tar.xz" "e2fsprogs-$E2FSPROGS_VER.tar.xz"
download "https://github.com/dosfstools/dosfstools/releases/download/v$DOSFSTOOLS_VER/dosfstools-$DOSFSTOOLS_VER.tar.gz" "dosfstools-$DOSFSTOOLS_VER.tar.gz"
download "https://gitlab.com/OldManProgrammer/unix-tree/-/archive/$TREE_VER/unix-tree-$TREE_VER.tar.gz" "tree-$TREE_VER.tar.gz"
download "https://github.com/libffi/libffi/releases/download/v$LIBFFI_VER/libffi-$LIBFFI_VER.tar.gz" "libffi-$LIBFFI_VER.tar.gz"
download "https://github.com/thom311/libnl/releases/download/libnl${LIBNL_VER//./_}/libnl-$LIBNL_VER.tar.gz" "libnl-$LIBNL_VER.tar.gz"
download "https://mirrors.edge.kernel.org/pub/linux/utils/net/iproute2/iproute2-$IPROUTE2_VER.tar.xz" "iproute2-$IPROUTE2_VER.tar.xz"
download "https://w1.fi/releases/wpa_supplicant-$WPA_SUPPLICANT_VER.tar.gz" "wpa_supplicant-$WPA_SUPPLICANT_VER.tar.gz"
download "https://www.kernel.org/pub/software/scm/git/git-$GIT_VER.tar.xz" "git-$GIT_VER.tar.xz"
download "https://www.python.org/ftp/python/$PYTHON_VER/Python-$PYTHON_VER.tar.xz" "Python-$PYTHON_VER.tar.xz"
download "https://go.dev/dl/go$GO_VER.src.tar.gz" "go$GO_VER.src.tar.gz"

# --- Bootloaders ---
download "https://downloads.sourceforge.net/project/gnu-efi/gnu-efi-$GNU_EFI_VER.tar.bz2" "gnu-efi-$GNU_EFI_VER.tar.bz2"
download "https://downloads.sourceforge.net/project/refind/$REFIND_VER/refind-src-$REFIND_VER.tar.gz" "refind-src-$REFIND_VER.tar.gz"

echo "All sources fetched."

#!/bin/bash
set -e
source script/env.sh

echo ">>> [Check] Verifying offline assets..."
REQUIRED=(
    "linux-$LINUX_VER.tar.xz"
    "musl-$MUSL_VER.tar.gz"
    "toybox-$TOYBOX_VER.tar.gz"
    "musl-fts-$MUSL_FTS_VER.tar.gz"
    "bash-$BASH_VER.tar.gz"
    "binutils-$BINUTILS_VER.tar.xz"
    "gcc-$GCC_VER.tar.xz"
    "make-$MAKE_VER.tar.gz"
    "gmp-$GMP_VER.tar.xz"
    "mpfr-$MPFR_VER.tar.xz"
    "mpc-$MPC_VER.tar.gz"
    "zlib-$ZLIB_VER.tar.gz"
    "openssl-$OPENSSL_VER.tar.gz"
    "ncurses-$NCURSES_VER.tar.gz"
    "readline-$READLINE_VER.tar.gz"
    "libevent-$LIBEVENT_VER.tar.gz"
    "libpcap-$LIBPCAP_VER.tar.gz"
    "dropbear-$DROPBEAR_VER.tar.bz2"
    "curl-$CURL_VER.tar.gz"
    "socat-$SOCAT_VER.tar.gz"
    "tcpdump-$TCPDUMP_VER.tar.gz"
    "links-$LINKS_VER.tar.gz"
    "htop-$HTOP_VER.tar.xz"
    "bc-$BC_VER.tar.gz"
    "jq-$JQ_VER.tar.gz"
    "vim-$VIM_VER.tar.gz"
    "tmux-$TMUX_VER.tar.gz"
    "nano-$NANO_VER.tar.xz"
)

MISSING=0
for file in "${REQUIRED[@]}"; do
    if [ ! -f "$SRC_DIR/$file" ]; then
        echo "ERROR: Missing $file in src/"
        MISSING=1
    fi
done

if [ "$MISSING" -eq 1 ]; then
    echo "Build failed due to missing sources. Please run 'make fetch' while online."
    exit 1
fi

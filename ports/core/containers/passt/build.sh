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

patch -p1 -i "$PORT_SRC/musl-close-range.patch"

# The Makefile reads VERSION from `git describe`, and a release tarball is not
# a repository: without this every binary reports `unknown version`, which is
# what a caller checking for a pasta new enough to use gets told.
export VERSION="$version"

# seccomp.sh prints the syscalls each profile allows through `fmt -t`, and
# toybox's fmt takes only -w: the unknown option exits 1, the script runs under
# `sh -e`, and one cosmetic line takes every generated seccomp header with it.
# A pass-through fmt ahead of the real one on PATH keeps both the text and a
# zero exit. It exists for the build only and is never installed.
install -d "$SRC_ROOT/fmt-shim"
printf '#!/bin/sh\nexec cat\n' > "$SRC_ROOT/fmt-shim/fmt"
chmod 755 "$SRC_ROOT/fmt-shim/fmt"
export PATH="$SRC_ROOT/fmt-shim:$PATH"

# `all` also builds passt.avx2 and pasta.avx2 on x86_64, and they are not
# optional decoration: arch_avx2_exec() re-execs /proc/self/exe with `.avx2`
# appended whenever __builtin_cpu_supports("avx2") is true, so a tree missing
# them takes an execv() failure and a warning on every start on such a machine.
make

make DESTDIR="$PKG" prefix=/usr install

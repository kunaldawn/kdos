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

export ZIG_GLOBAL_CACHE_DIR="$SRC_ROOT/zig-cache"
mkdir -p "$ZIG_GLOBAL_CACHE_DIR/p"

unpack_zig_dep() {
	mkdir -p "$ZIG_GLOBAL_CACHE_DIR/p/$2"
	tar xf "$PORT_SRC/zlsdep-$1.tar.gz" --strip-components=1 \
		-C "$ZIG_GLOBAL_CACHE_DIR/p/$2"
}
unpack_zig_dep known_folders known_folders-0.0.0-Fy-PJk3KAACzUg2us_0JvQQmod1ZA8jBt7MuoKCihq88
unpack_zig_dep diffz         diffz-0.0.1-G2tlISzNAQCldmOcINavGmF1zdt20NFPXeM8d07jp_68
unpack_zig_dep lsp_kit       lsp_kit-0.1.0-bi_PL3IyDACfp1xdTnkiOHEok2YpPCCCJHuuOcNzjl1D
unpack_zig_dep tracy         N-V-__8AAOncKwEm1F9c5LrT7HMNmRMYX8-fAoqpc6YyTu9X

patch -p1 -i "$PORT_SRC/pie.patch"
zig build --system "$ZIG_GLOBAL_CACHE_DIR/p" -Doptimize=ReleaseSafe -Dcpu=baseline -Dpie=true --prefix "$PKG/usr"
strip --strip-debug "$PKG/usr/bin/zls"

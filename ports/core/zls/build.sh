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

# THE VERSIONS ARE PINNED TO EACH OTHER AND THAT IS NOT A COINCIDENCE. zls
# parses Zig with the compiler's own AST, so a zls built against a different
# Zig than the one installed either fails to build or reports syntax errors on
# correct code. `ports/core/zig` is 0.16.0 and this is 0.16.0; bumping one
# without the other is the way this breaks, and it breaks in the editor rather
# than in the build.
#
# ZLS DOES NOT VENDOR ITS DEPENDENCIES — build.zig.zon names four remote
# tarballs by URL and hash, and zig fetches them at build time. Under
# `--network none` that is four `unable to connect to server:
# NameServerFailure` lines and nothing else.
#
# --system IS THE LEVER, not the cache directory. Unpacking into the global
# cache alone is not enough: zig re-derives each package's hash from the tree
# and fetches again when it does not match. `--system <dir>` names a directory
# of already-provided packages and turns remote fetching OFF entirely — zig's
# own message is "remote package fetching disabled due to --system mode",
# which is the guarantee a build under --network none wants.
#
# The layout is one directory per dependency, named by the hash in
# build.zig.zon. Those hashes are copied here and must move with the .zon on a
# version bump.
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

zig build --system "$ZIG_GLOBAL_CACHE_DIR/p" -Doptimize=ReleaseSafe --prefix "$PKG/usr"

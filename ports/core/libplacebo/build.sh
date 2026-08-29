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

# mpv 0.41 DOES NOT MAKE THIS OPTIONAL. Its meson.build has a bare
# `dependency('libplacebo', version: '>=6.338.2')` with no `required: false`,
# so without this port mpv fails at setup — and the fallback in that same line
# is a subproject wrap, which is a download.
#
# EVERY GPU BACKEND IS OFF, and the reason is the archive rather than a
# preference: libplacebo's Vulkan-Headers, glad and jinja live in `3rdparty/`
# as git submodules, and a release ARCHIVE carries those directories empty.
# So `vulkan` has no headers and `opengl` has no generator. What remains is the
# colour, scaling and tone-mapping pipeline, which is what mpv links it for;
# `--vo=gpu-next` needs a backend and is therefore unavailable, while
# `--vo=gpu` — mpv's own GL renderer, and what it shipped for years — is not.
#
# glslang and shaderc compile shaders at runtime and are neither of them ports;
# demos want SDL and nuklear, another empty submodule.
#
# python3-jinja2 IS A BUILD DEPENDENCY AND NOT AN OPTIONAL ONE: every file
# under src/shaders/ is GENERATED from a template at build time. libplacebo
# vendors jinja and markupsafe in 3rdparty/, which a release archive carries
# empty, so the system one is what answers `import jinja2`.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dvulkan=disabled -Dopengl=disabled -Dd3d11=disabled \
	-Dglslang=disabled -Dshaderc=disabled \
	-Dlcms=enabled -Dxxhash=enabled \
	-Ddemos=false -Dtests=false -Dbench=false -Dfuzz=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

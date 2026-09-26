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
# VULKAN AND OPENGL ARE BOTH BUILT. libplacebo's Vulkan-Headers, glad and
# jinja live in `3rdparty/` as git submodules, and a release ARCHIVE carries
# those directories empty. An empty Vulkan-Headers falls through to the system
# headers, and `-Dvulkan-registry` hands the code generator the `vk.xml` that
# the vulkan-headers port installs beside them: the two must be the same
# release, or the generated tables name structures the headers do not have.
# glslang compiles its shaders to SPIR-V at run time. shaderc is off: it is
# glslang again behind another interface, a second compiler for the same job.
#
# `opengl` is the backend mpv's `--vo=gpu-next` runs on where no Vulkan device
# answers. Its GL and EGL loader is generated at build time by glad2, which the
# empty `3rdparty/glad` would have supplied; python3-glad2 answers
# `import glad` instead. `gl-proc-addr` lets libplacebo find the loader's entry
# points itself through libdl rather than being handed them. `libdovi` is not a
# port, so Dolby Vision RPUs go through the built-in `dovi` path only.
# `unwind` is off because libunwind would serve only stack traces on error.
# Demos want SDL and nuklear, another empty submodule.
#
# python3-jinja2 IS A BUILD DEPENDENCY AND NOT AN OPTIONAL ONE: every file
# under src/shaders/ is GENERATED from a template at build time. libplacebo
# vendors jinja and markupsafe in 3rdparty/, which a release archive carries
# empty, so the system one is what answers `import jinja2`.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dvulkan=enabled -Dvk-proc-addr=enabled \
	-Dvulkan-registry=/usr/share/vulkan/registry/vk.xml \
	-Dglslang=enabled -Dshaderc=disabled \
	-Dopengl=enabled -Dgl-proc-addr=enabled -Dd3d11=disabled \
	-Ddovi=enabled -Dlibdovi=disabled -Dunwind=disabled \
	-Dlcms=enabled -Dxxhash=enabled \
	-Ddemos=false -Dtests=false -Dbench=false -Dfuzz=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

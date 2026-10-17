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

patch -p1 -i "$PORT_SRC/perf-docs.patch"
export LDFLAGS="$LDFLAGS -Wl,--strip-debug"

perf_make=(
	make -C tools/perf
	prefix=/usr
	WERROR=0
	EXTRA_CFLAGS="$CFLAGS $LDFLAGS"
	LIBBPF_DYNAMIC=1
	BUILD_BPF_SKEL=1
	NO_LIBPFM4=1
	NO_BABELTRACE2=1
	NO_LIBDEBUGINFOD=1
	NO_JVMTI=1
	NO_SDT=1
	NO_LIBLLVM=1
	NO_PERF_READ_VDSO32=1
	NO_PERF_READ_VDSOX32=1
	NO_BACKTRACE=1
	NO_RUST=1
	LIBPERL=1
	PYTHON_SETUPTOOLS_INSTALLED=no
)

"${perf_make[@]}"

"${perf_make[@]}" DESTDIR="$PKG" install-bin
make -C tools/perf/Documentation prefix=/usr DESTDIR="$PKG" \
	USE_ASCIIDOCTOR=1 PERF_VERSION="$version" \
	KBUILD_BUILD_TIMESTAMP="@$SOURCE_DATE_EPOCH" \
	ASCIIDOC_EXTRA="-a compat-mode -I. -rasciidoctor-extensions -a mansource=perf -a manmanual=perf\\ Manual" \
	install-man

rm -f "$PKG"/usr/libexec/perf-core/tests/pe-file.exe*

install -Dm644 "$PKG"/etc/bash_completion.d/perf "$PKG"/usr/share/bash-completion/completions/perf
rm -r "$PKG"/etc/bash_completion.d
rmdir --ignore-fail-on-non-empty "$PKG"/etc

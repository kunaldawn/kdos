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

# perf is not a project of its own: it is tools/perf inside the kernel tree, so
# this port's tarball is the linux port's tarball under another name and the
# two versions must be bumped together. A perf built from a different release
# than the running kernel does not fail — it reports unknown record types and
# misses the new tracepoints, which is the quiet kind of wrong.

# `prefix` is compiled in — perf resolves its helper programs, scripts and
# tests through $prefix/libexec/perf-core at run time — and it also derives
# every install path, so it belongs on both make lines. Unset, the build step
# takes $HOME while the install step, which defines DESTDIR, takes the empty
# string: the package stages into $PKG/bin and $PKG/libexec instead of under
# /usr, around a binary that looks for its helpers in the build user's home.
#
# perf's Makefile.config ASSIGNS CFLAGS from EXTRA_CFLAGS rather than appending
# to the environment's, so the tree's flags reach the compiler only as a make
# argument here. That is the documented knob, not a variable being overridden.
#
# WERROR is on unless it is switched off, and -Wall -Wextra -std=gnu11 over a
# kernel tree with gcc 15 and musl headers stops the build on a warning that
# cannot be fixed in a recipe.
#
# LIBUNWIND is opt-in upstream and stays off. DWARF call graphs come from libdw
# instead — perf's own preferred unwinder, and elfutils is already a dependency
# for libelf — and perf unwinds with one of the two, so linking
# libunwind-nongnu as well would add a library and no call graph.
#
# LIBBPF_DYNAMIC links the installed libbpf. tools/lib/bpf in this tarball is
# the same 1.7 series, so building a second copy statically buys nothing and
# hides a future version skew.
#
# BUILD_BPF_SKEL is what `perf lock contention`, `perf record --off-cpu` and
# `perf stat --bpf-counters` are made of: it compiles BPF skeletons with clang
# and generates them with a bpftool built here, which needs libelf, zlib,
# libbpf and openssl. Lose any one of those and perf still builds — without
# those verbs and with no error.
#
# LIBPERL is opt-in upstream and is on: it is the Perl engine behind
# `perf script -s foo.pl`, and it links perl's libperl.so, which exists only
# because the perl port configures -Duseshrplib. Its feature check is fatal, so
# a perl without the shared library stops this build rather than dropping the
# engine.
#
# slang, capstone, python3, zlib, xz and zstd have no switch to demand them —
# only a NO_* to refuse them — so their place in depends is what keeps the
# report/top TUI, the capstone disassembler, `perf script` Python, and
# compressed modules and `record -z`.
#
# The Python extension module (python/perf.so) is built whenever setuptools
# imports, and `install` never installs it. PYTHON_SETUPTOOLS_INSTALLED=no
# answers that probe, so the build does not depend on whether another port
# happened to put setuptools in the chroot. The embedded Python engine is
# unaffected.
#
# Each NO_* names a feature this image does not carry, most of them for want
# of a library. perf answers a missing dependency with a warning and a
# narrower binary, so the absent ones are stated rather than probed: a
# detection that silently flips is a perf that loses a feature on an unrelated
# version bump.
#   NO_LIBNUMA           numactl is not ported; drops `perf bench numa mem`
#   NO_LIBPFM4           libpfm is not ported; drops its raw event names
#   NO_BABELTRACE2       babeltrace2 is not ported; drops `perf data` CTF
#   NO_LIBDEBUGINFOD     elfutils is configured --disable-debuginfod
#   NO_JVMTI             no JDK, so no Java JIT agent
#   NO_SDT               no sys/sdt.h; systemtap is not ported
#   NO_LIBLLVM           LLVM here is BUILD_SHARED_LIBS, so `llvm-config
#                        --libs all` is two hundred shared objects mapped at
#                        every perf start to resolve source lines libdw
#                        already resolves
#   NO_PERF_READ_VDSO32  no 32-bit libc to compile the vdso readers against
#   NO_PERF_READ_VDSOX32 likewise for x32
#   NO_BACKTRACE         musl has no execinfo.h
#   NO_RUST              the Rust feature is `perf test` workloads only
perf_make=(
	make -C tools/perf
	prefix=/usr
	WERROR=0
	EXTRA_CFLAGS="$CFLAGS"
	LIBBPF_DYNAMIC=1
	BUILD_BPF_SKEL=1
	NO_LIBNUMA=1
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

# The same argument list installs, or make re-runs every feature probe with a
# different answer and relinks the whole tool at install time.
#
# `install` chains try-install-man, which renders the AsciiDoc manuals with
# asciidoc and xmlto and derives their include graph with perl's
# build-docdep.perl. All three are in depends because the documentation build
# only WARNS for a tool it cannot find: drop one and the port still builds,
# with `perf help record` opening nothing and 41 manuals gone with no error
# anywhere.
"${perf_make[@]}" DESTDIR="$PKG" install

# install-tests copies Windows PE binaries in beside the shell tests, for a
# `perf test` case about reading PE build ids. Nothing on this system runs
# them.
rm -f "$PKG"/usr/libexec/perf-core/tests/pe-file.exe*

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

# THE PATCH IS THE ONE CASE THE HARD RULE ALLOWS, because there is no flag.
# blake2.h writes `JTR_ALIGN(64) typedef struct __blake2b_state {...}
# blake2b_state;` — the attribute is BEFORE the typedef, so it attaches to the
# declaration rather than to the type, the struct's SIZE is never padded to its
# alignment, and `blake2b_state S[4][1]` inside blake2bp_state is then an array
# whose element size is not a multiple of its element alignment. GCC made that
# a hard error, with no -Wno- to answer it. Moving the attribute after the
# closing brace applies it to the type, which is what upstream meant.
#
# Only the two `_state` structs are touched. The `_sp_state` pair has the same
# misplacement and is NOT in an array, so it does not error — and its
# attribute sits inside an `#if defined(JOHN_NO_SIMD)` where "fixing" it would
# change the alignment of the non-SIMD build.
patch -p1 -i "$PORT_SRC/blake2-align.patch"

# opencl-topology.patch has no flag either. opencl_common.h supplies AMD's
# cl_device_topology_amd union only when CL_DEVICE_TOPOLOGY_AMD is undefined,
# and current OpenCL headers define that macro without the union, so the GPU
# build stops at an unknown type. The patch guards the union on
# CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD, which those headers do not define.
patch -p1 -i "$PORT_SRC/opencl-topology.patch"

# cpu-fallback-first.patch has no flag either. main() runs sig_preinit() and
# path_init() before it tests the CPU, and both are compiled for the build's
# SIMD level, so the AVX-512 build dies of SIGILL on an AVX2 machine before
# it can hand over to the next build down. The patch runs the test first,
# only in a build with a fallback and a fixed exec directory, where the test
# needs nothing those two set up.
patch -p1 -i "$PORT_SRC/cpu-fallback-first.patch"

# -fcommon: the OpenCL formats declare file-scope globals (psalt, insize,
# keyfiles_data, ...) under the same names as their CPU twins without `static`,
# and GCC's -fno-common default turns every such pair into a multiple-definition
# link error. -fcommon merges them the way the source was written for.
export CFLAGS="$CFLAGS -fcommon"

cd src
_opts=(--prefix=/usr --disable-native-tests --disable-mpi --enable-pcap --enable-opencl)

# --disable-native-tests IS WHAT MAKES THIS REPRODUCIBLE. john's configure
# otherwise probes THIS CPU's instruction set and bakes the best it finds into
# the binary, so a package built on a machine with AVX-512 crashes on one
# without it — and the failure is SIGILL at run time, not a link error.
#
# ONE BUILD PER LEVEL, CHOSEN AT RUN TIME. john selects its SIMD code only at
# compile time, and a build for one level is either slow everywhere or an
# illegal instruction on an older machine. --enable-simd=<level> compiles
# every file with -m<level> except john.c, which is told the level by a
# define and tests the processor for it first; the level must not go into
# CFLAGS, or john.c itself is compiled for it and dies of SIGILL before the
# test. -DCPU_FALLBACK makes a failed test execv() the next build down under
# the name x86-64.h gives it, from JOHN_SYSTEMWIDE_EXEC, which --prefix=/usr
# sets to /usr/bin:
#   john (AVX-512BW) -> john-non-avx512bw (AVX2) -> john-non-avx2 (XOP)
#   -> john-non-xop (AVX) -> john-non-avx (SSE2, the x86-64 baseline)
# argv is passed on unchanged, so unshadow, zip2john and the other links to
# john keep their meaning through the chain. The baseline build is made last:
# the helper programs it leaves in run/ (calc_stat, SIPdump, the *2john
# converters written in C) are the only copies shipped, and they run on any
# x86-64. A new level must keep the chain's names; a name x86-64.h does not
# expect is never exec'd.
#
# --enable-pcap fails configure without libpcap rather than dropping the
# vncpcap2john / SIPdump / eapmd5tojohn helpers. --enable-opencl only asks:
# the GPU formats come in when CL/cl.h and -lOpenCL link, which is why
# opencl-headers and ocl-icd are `depends`; configure's "OpenCL support"
# summary line is the one to read. The formats then run on the ICD
# /etc/OpenCL/vendors names, Mesa's rusticl, which offers a device only for the
# drivers RUSTICL_ENABLE lists (/etc/profile.d/50-opencl.sh sets it), and
# report no device when there is none.
_base_cflags="$CFLAGS"
for _lvl in avx512bw:john avx2:john-non-avx512bw xop:john-non-avx2 avx:john-non-xop; do
	CFLAGS="$_base_cflags -DCPU_FALLBACK" ./configure "${_opts[@]}" \
		--enable-simd="${_lvl%%:*}"
	make clean
	make
	mv ../run/john ../run/.simd-"${_lvl#*:}"
done
CFLAGS="$_base_cflags" ./configure "${_opts[@]}"
make clean
make
mv ../run/john ../run/john-non-avx
for _f in ../run/.simd-*; do
	mv "$_f" ../run/"${_f#../run/.simd-}"
done

# WHAT IT IS FOR: reading a hash out of a LUKS header, a KeePass database, an
# encrypted PDF or ZIP, or /etc/shadow, and answering "how long would this
# take" — about your own material. NO WORDLIST IS SHIPPED. The bundled
# password.lst is upstream's small default and stays; breach corpora of unclear
# provenance are not something this distro redistributes, and SecLists is MIT
# and one clone away for anyone who wants it.
install -dm755 $PKG/usr/share/john $PKG/usr/bin
cd ../run

# --prefix=/usr builds john system-wide: it reads john.conf, the rules and the
# .chr files from /usr/share/john and keeps its pot, log and session files in
# ~/.john. Without it john looks for its configuration beside argv[0], which a
# command found on $PATH does not have. Compiled programs and the links to
# john go to /usr/bin; everything else in run/ — scripts, the python modules
# they import, data — goes to /usr/share/john.
for f in *; do
	if [ -L "$f" ]; then
		ln -s "$(readlink "$f")" $PKG/usr/bin/"$f"
	elif [ -f "$f" ] && [ "$(head -c4 "$f")" = $'\x7fELF' ]; then
		install -m755 "$f" $PKG/usr/bin/"$f"
	else
		cp -a "$f" $PKG/usr/share/john/
	fi
done

# run/ztex holds prebuilt FPGA bitstreams and a USB controller image for the
# ZTEX 1.15y board. --enable-ztex is not passed, so no program here can load
# them and they are not shipped.
rm -rf $PKG/usr/share/john/ztex

# The *2john converters are the half people actually reach for — they read a
# container and print the hash john takes. The scripted ones get a symlink so
# `ssh2john.py` is a command rather than a path somebody has to remember.
for f in *2john*; do
	[ -e $PKG/usr/bin/"$f" ] && continue
	[ "$(head -c2 "$f")" = '#!' ] || continue
	chmod 755 $PKG/usr/share/john/"$f"
	ln -s ../share/john/"$f" $PKG/usr/bin/"$f"
done

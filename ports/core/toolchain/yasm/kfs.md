## About
Yasm is an assembler for the x86 and AMD64 instruction sets, a rewrite of NASM that accepts
both NASM and GAS syntax and writes ELF, COFF, Mach-O, Win32/64 and other object formats.

In KDOS one port needs it: [veracrypt](#port-veracrypt), whose makefile sets `AS := yasm` to
assemble the x86 implementations of its ciphers and hashes.

The source is the tag archive from GitHub, which holds the tree as it is in git: no
`configure`, no `Makefile.in`, and no generated x86 instruction tables. `build.sh` therefore
runs upstream's `autogen.sh`, which runs `aclocal`, `autoheader`, `automake` and `autoconf`
and then `configure --enable-maintainer-mode` with the arguments given to it, which is why
[autoconf](#port-autoconf) and [automake](#port-automake) are in `depends`.

## Flags
- `ac_cv_header_stdc=yes` — Answer configure's "ANSI C header files" check in advance.
  yasm's `configure.ac` calls `AC_HEADER_STDC` and stops with "Standard (ANSI/ISO C89) header
  files are required" unless `ac_cv_header_stdc` is `yes`. The tree's autoconf treats
  `AC_HEADER_STDC` as obsolete and never sets that variable, so without the answer the
  configure that `autogen.sh` generates refuses to run.
- `--enable-python` — Use Python in the build, and stop configure when no Python is found. The
  build runs `modules/arch/x86/gen_x86_insn.py` to generate `x86insn_nasm.gperf`,
  `x86insn_gas.gperf` and `x86insns.c`; without Python the makefile copies the two `.gperf`
  files from the source tree instead and has no rule for `x86insns.c`, and the tag archive has
  none of the three. [python3](#port-python3) is in
  `depends` for this.
- `--disable-python-bindings` — Build no Python module for libyasm. This is configure's
  default, so the switch only states it; the bindings would also need Cython and the Python
  headers.
- `XMLTO=:` — Set the `xmlto` command to `:`, which turns off the rules that regenerate the
  manual pages from their DocBook sources. The archive carries the finished pages, `yasm.1` and
  the `yasm_*.7` pages, and they are installed as they are, whether or not the
  [xmlto](#port-xmlto) port is in the build root.
- `-j1` — Build and install with one job. The build environment exports `MAKEFLAGS=-j$KDOS_JOBS`
  (`script/env/common.env`), and a `-j` on the command line overrides it, for both `make` and `make install` here.

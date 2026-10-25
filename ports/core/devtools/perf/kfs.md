## About

perf is the kernel's own [profiler]{.idx}: it samples where the processors spend their time, counts
hardware and software events, records tracepoints and runs BPF programs that summarise in the
kernel, then reports what it found (`perf top`, `perf record` and `perf report`, `perf stat`, `perf
trace`). It is not a project of its own but `tools/perf` inside the kernel source, so the port's
tarball is the [linux](#port-linux) port's tarball under another name, and the two carry the same
version and hash. `testing/preflight.sh` fails the tree when they disagree, because a perf built
from another kernel's source does not fail: it reports unknown record types and misses the newer
tracepoints.

## Flags

- `LDFLAGS` — The tree's link flags with `-Wl,--strip-debug` added. Several of the libraries perf
  builds and links in, `tools/lib/api`, `tools/lib/subcmd`, `tools/lib/symbol` and `tools/lib/perf`,
  add their own `-g` or `-ggdb3` after the tree's flags, and the linker drops that debug information
  from the programs it writes.
- `prefix=/usr` — perf compiles the prefix in: it finds its helper programs, scripts and tests under
  `$prefix/libexec/perf-core` at run time. It also derives every install path from it, so it is on
  both the build and the install command line. Unset, the build takes `$HOME` and the install, which
  sets `DESTDIR`, takes the empty string, so the package would stage into `$PKG/bin` around a binary
  that looks for its helpers in the build user's home.
- `WERROR=0` — perf compiles with `-Werror` unless this is set to 0. Over a kernel tree with the
  tree's GCC and musl's headers, any new warning would stop the build.
- `EXTRA_CFLAGS` — perf's `Makefile.config` sets `CFLAGS` from `EXTRA_CFLAGS` instead of adding to
  the environment's, so this is where the tree's `$CFLAGS` go. `$LDFLAGS` go there as well, because
  the rule that links the `dlfilters` shared objects reads `EXTRA_CFLAGS` and not `LDFLAGS`; without
  them those objects would be linked without `-z now`. See [The release
  flags](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-release-flags).
- `LIBBPF_DYNAMIC=1` — Links the shared `libbpf` from the [libbpf](#port-libbpf) port, instead of
  building the copy in `tools/lib/bpf` and linking it statically.
- `BUILD_BPF_SKEL=1` — Builds the BPF skeletons behind `perf lock contention`, `perf record
  --off-cpu`, `perf stat --bpf-counters` and others: BPF programs compiled with [clang](#port-clang)
  and wrapped by a bpftool perf builds for itself. The default is on, but `Makefile.config` turns it
  off with only a warning when libelf, zlib, libbpf, OpenSSL or clang is missing; the ports that
  provide them are all in `depends`.
- `NO_LIBPFM4=1` — Leaves out libpfm4's named raw hardware events. The tree has no libpfm port.
- `NO_BABELTRACE2=1` — Leaves out `perf data convert --to-ctf`, the export to the CTF trace format,
  which needs babeltrace2's CTF writer. The tree has no babeltrace2 port.
- `NO_LIBDEBUGINFOD=1` — Leaves out fetching debug information from a debuginfod server. The
  [elfutils](#port-elfutils) port is configured with `--disable-libdebuginfod`, so there is no
  `libdebuginfod` to link.
- `NO_JVMTI=1` — Does not build the JVMTI agent that lets perf name the methods of Java code the JIT
  compiled. It compiles against a JDK's headers, and [openjdk](#port-openjdk) is not one of this
  port's dependencies.
- `NO_SDT=1` — perf defines no SDT markers (static probes) in its own code; this needs `sys/sdt.h`
  from systemtap, which the tree does not carry. As upstream notes, perf still finds and uses the
  SDT markers in other programs.
- `NO_LIBLLVM=1` — Without it, perf finds LLVM and links every library `llvm-config --libs all`
  names, for source-line lookup and for `perf annotate`'s disassembler. This tree's LLVM is one
  shared library per component, so every one of them would load at each perf start, for work libdw
  (source lines) and capstone (disassembly) already do.
- `NO_PERF_READ_VDSO32=1`, `NO_PERF_READ_VDSOX32=1` — Do not build the two helpers that read the
  32-bit and x32 compatibility vDSOs. Each is compiled with `-m32` or `-mx32`, which needs a C
  library for that ABI, and the tree has only the 64-bit one.
- `NO_BACKTRACE=1` — Leaves out perf printing its own stack when it crashes, which uses
  `backtrace()` from `execinfo.h`. musl has no `execinfo.h`.
- `NO_RUST=1` — Upstream uses Rust only for test workloads of `perf test`; the switch skips the
  probe for a Rust compiler.
- `LIBPERL=1` — Off by default upstream. It builds the Perl engine behind `perf script -s foo.pl`,
  which links `libperl.so`; the [perl](#port-perl) port is configured with `-Duseshrplib` so that
  library exists. With `LIBPERL` set, a perl without it stops the build.
- `PYTHON_SETUPTOOLS_INSTALLED=no` — perf builds its Python extension module, `python/perf.so`, when
  Python can import setuptools, and its `install` never installs the module. Answering the probe
  keeps the build the same whether or not some other port has put setuptools in the chroot. The
  embedded Python engine of `perf script` does not depend on it.
- `install-bin` — perf's `install` target is `install-bin` followed by `try-install-man`, which
  would render the manuals the AsciiDoc and DocBook way. The recipe installs the programs with
  `install-bin` from the same argument list as the build, so make does not repeat every feature
  probe with different answers and relink the tool, and renders the manuals on its own line.
- `USE_ASCIIDOCTOR=1` — Renders each manual straight to a man page with
  [asciidoctor](#port-asciidoctor), instead of going through DocBook, xmlto and xsltproc. On that
  route the `perf-c2c` page nests the stylesheet's `string.subst` template past libxslt's limit of
  3000, which xmlto has no way to raise.
- `ASCIIDOC_EXTRA` — The Documentation makefile adds asciidoc's `--unsafe -f asciidoc.conf` to this
  variable on the Asciidoctor route too, and asciidoctor refuses them, so the recipe gives the whole
  value itself: the options upstream's Asciidoctor route sets (`-a compat-mode`,
  `-rasciidoctor-extensions`, `mansource` and `manmanual`). `tools/perf/Documentation` is run
  directly because `Makefile.perf` passes the variable on unquoted and would split the value at its
  spaces.
- `PERF_VERSION`, `KBUILD_BUILD_TIMESTAMP` — Run directly, the Documentation makefile does not
  inherit the version `Makefile.perf` would pass it, so the recipe names it. The timestamp,
  `SOURCE_DATE_EPOCH`, gives the date printed in every page, which the makefile otherwise takes from
  `git log`.
- `install-man` — Unlike `try-install-man`, which `install` runs, it stops when asciidoctor is
  missing instead of warning and installing no manuals.

## Patches

- `perf-docs.patch` — Shortens two AsciiDoc heading underlines to the length of the heading above them: the `perf-check(1)` title of `perf-check.txt` and the `OPTIONS for 'perf kwork top'` section heading of `perf-kwork.txt`.

## Notes

- **Some features depend only on `depends`.** perf has no switch that demands slang, capstone,
  numactl, Python, zlib, xz or zstd, only a `NO_…` that refuses each one, and it builds without any
  it does not find. Their place in `depends` is what keeps the `perf report` and `perf top` text
  interface, the capstone disassembler, `perf bench numa mem`, Python in `perf script`, and
  compressed kernel modules and `perf record -z`.
- **No libunwind.** `LIBUNWIND` is opt-in upstream and is not set. perf unwinds DWARF call graphs
  with libdw from [elfutils](#port-elfutils); built with libunwind as well, it only gains a second
  unwinder that it tries when libdw returns nothing.
- **The Windows test files are removed.** `install-tests` copies `pe-file.exe` and its debug file in
  beside the shell tests, for a `perf test` case that reads build IDs from PE binaries. `build.sh`
  deletes them.
- **The bash completion moves.** perf installs it as `/etc/bash_completion.d/perf`; the recipe moves
  it to `/usr/share/bash-completion/completions/perf`, where bash-completion loads it on demand, and
  removes the empty `/etc`.

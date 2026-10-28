## About

bpftrace is a tracing language for the kernel's [eBPF]{.idx} machinery: a one-line script names a
probe (a kprobe, a uprobe, a tracepoint, a timer) and what to record when it fires, and bpftrace
compiles it with LLVM into BPF bytecode, loads it, and prints the maps it fills. It is how to ask
why something is slow when the answer lies inside the kernel. It needs a kernel with the `bpf()` system
call, the BPF JIT, kprobes, uprobes and BTF type information. The kernel the [linux](#port-linux)
port builds from its `kdos.config` has all of them, and that port's build stops if
`CONFIG_DEBUG_INFO_BTF` is lost, so this port needs no kernel change of its own.

## Flags

- `-DUSE_SYSTEM_LIBBPF=ON` — By default bpftrace builds the libbpf in its `libbpf/` git submodule. A
  GitHub tag archive carries that directory empty, so the default cannot build. With the switch,
  bpftrace takes the headers and the static `libbpf.a` that the [libbpf](#port-libbpf) port
  installs. It insists on the static archive, because it embeds libbpf's headers into the code it
  generates.
- `-DENABLE_MAN=ON`, `-DASCIIDOCTOR=/usr/bin/asciidoctor` — `ENABLE_MAN` (on by default) renders
  `bpftrace(8)` from `man/adoc/bpftrace.adoc`. `man/adoc/CMakeLists.txt` looks for `asciidoctor`
  with `find_program`, and when the search fails it only skips the page; naming the program's path
  makes the man target fail instead if [asciidoctor](#port-asciidoctor) is missing.
- `-DENABLE_SKB_OUTPUT=ON` — On by default. It looks for libpcap and, when it is found, builds the
  PCAP writer behind the `skboutput()` builtin, which writes the data of a socket buffer to a PCAP
  file.
- `-DENABLE_SYSTEMD=OFF` — Off by default; when on, it links `libsystemd`. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DCMAKE_REQUIRE_FIND_PACKAGE_LibBfd=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_LibOpcodes=ON`,
  `-DCMAKE_REQUIRE_FIND_PACKAGE_LibDw=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_LibPcap=ON` — bpftrace
  calls `find_package()` for these four libraries without `REQUIRED`, and builds without any it does
  not find. This CMake switch makes each `find_package` of that name fatal. libbfd and libopcodes,
  from [binutils](#port-binutils), give bpftrace its disassembler, which it uses to refuse a uprobe
  placed in the middle of an instruction. libdw, from [elfutils](#port-elfutils), lets it read a
  program's function arguments and types from DWARF. libpcap, from [libpcap](#port-libpcap), is
  `skboutput()`.
- `-DCMAKE_DISABLE_FIND_PACKAGE_LibBlazesym=ON` — bpftrace looks for blazesym, a Rust symbolisation
  library, and resolves kernel and user-space addresses to symbols with it when it finds it. The tree has no blazesym port.
  Disabling the search keeps the build the same whatever else is installed.
- `-DSTATIC_LINKING=OFF` — Off by default. On, it links bpftrace against the static clang and LLVM
  libraries; the [llvm](#port-llvm) port builds LLVM as shared libraries.
- `-DCMAKE_CXX_STANDARD_LIBRARIES` — CMake appends this to the end of every C++ link line. The value
  is the output of `llvm-config --libs`, every LLVM component library. This tree's LLVM is built
  with `BUILD_SHARED_LIBS=ON`, one shared library per component, and CMake puts on a link line only
  the few components bpftrace names. A target that calls none of them, such as `bpftrace-aotrt`, has
  each one dropped by the tree's `--as-needed`, and the libraries those components need
  (`LLVMSupport`, `LLVMObject`, `LLVMDebugInfoDWARF` among them) are then never searched: the link
  fails on `llvm::raw_ostream` and `llvm::object::createBinary`. With every component named, each
  library a binary uses is found, and `--as-needed` still records only those.

## Patches

- `llvm-definitions.patch` — `LLVMConfig.cmake` exports `LLVM_DEFINITIONS` as one space-separated string, and `add_definitions()` takes it as one definition whose value is the rest of the string. The patch splits the string into a list before handing it on. LLVM built against libstdc++ exports `-D_GLIBCXX_USE_CXX11_ABI=1` first, so unpatched, the whole string becomes the value of that one macro and every compile fails under `-Werror=undef`.

## About

John the Ripper, in its community "jumbo" edition, is a password hash cracker. With its `*2john`
converters it reads the hash out of a LUKS header, a KeePass database, an encrypted PDF, ZIP or
7-Zip archive, an SSH key or `/etc/shadow`, and then tests candidate passwords against it, which
answers how long a password of your own would hold. The package ships upstream's small default
`password.lst` and no other wordlist. Its GPU formats run through OpenCL when a device is present.

John chooses its SIMD code only when it is compiled, so `build.sh` builds it five times, once per
instruction-set level, and installs the five as a chain that hands over at start-up to the best
one the processor can run. This is the per-level form of the rule in
[Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).

## Flags

- `--prefix=/usr` — For john this does more than place files. It makes the build system-wide:
  john reads `john.conf`, the rules and the `.chr` files from `/usr/share/john` and keeps its pot,
  log and session files in `~/.john`, instead of looking for everything in the directory its
  binary is in, which for a command in `/usr/bin` holds none of it. It also sets `JOHN_SYSTEMWIDE_EXEC` to
  `/usr/bin`, the directory the fallback chain below `exec`s from.
- `--disable-native-tests` — Do not probe the build machine's processor. Without it configure
  compiles in the best instruction set the builder has, and the package dies of `SIGILL` on any
  processor that lacks it.
- `--enable-simd` — In the loop, `--enable-simd=avx512bw`, `avx2`, `xop` and `avx` in turn. Each
  compiles every file with that level except `john.c`, which is told the level by a define and
  tests the processor for it first. The level must not go into `CFLAGS`, or `john.c` itself would
  be compiled for it and die before the test. The fifth build, with no `--enable-simd`, is the
  SSE2 baseline every x86-64 processor runs.
- `-DCPU_FALLBACK` — Added to the four SIMD builds. When the processor fails the test, john
  `execv()`s the next build down by the name `x86-64.h` gives it, from `/usr/bin`, with `argv`
  unchanged: `john` (AVX-512BW) to `john-non-avx512bw` (AVX2) to `john-non-avx2` (XOP) to
  `john-non-xop` (AVX) to `john-non-avx` (SSE2). Because `argv` is passed on, `unshadow`,
  `zip2john` and the other links to `john` keep their meaning through the chain.
- `export CFLAGS="$CFLAGS -fcommon"` — Several format files declare file-scope globals without
  `static` under names another format file also uses: `psalt` and `keyfiles_data` in both the CPU
  and the OpenCL TrueCrypt formats, `insize` across many of the OpenCL formats, and others. With
  GCC's default `-fno-common` every such name is a multiple-definition link error; `-fcommon`
  merges them, as the code was written for.
- `--disable-mpi` — Build without OpenMPI support, which is off unless asked for; there is no
  OpenMPI port.
- `--enable-pcap` — Make configure stop with "PCAP not installed/usable" when libpcap is missing,
  instead of quietly leaving out the helpers that need it (`vncpcap2john`, `SIPdump`,
  `eapmd5tojohn`). [libpcap](#port-libpcap) is in `depends`.
- `--enable-opencl` — Build the GPU formats. configure includes them when `CL/cl.h` compiles and
  `-lOpenCL` links, which is what [opencl-headers](#port-opencl-headers) and
  [ocl-icd](#port-ocl-icd) in `depends` provide; the "OpenCL support" line of configure's summary
  says whether it did.

## Patches

- `blake2-align.patch` — Moves `JTR_ALIGN(64)` in `src/blake2.h` from before `typedef struct __blake2s_state` and `__blake2b_state` to after their closing braces. Placed before the typedef, the attribute attaches to the declaration and not to the type, so the struct's size is never padded to its alignment, and `blake2b_state S[4][1]` inside `blake2bp_state` becomes an array whose element size is not a multiple of its alignment, which GCC rejects as an error with no `-Wno-` switch to answer it. Only the two `_state` structs are moved: `blake2sp_state` and `blake2bp_state` have the same placement but are in no array, and their attribute sits inside an `#if` where moving it would change the alignment of the non-SIMD build.
- `opencl-topology.patch` — In `src/opencl_common.h`, guards AMD's `cl_device_topology_amd` union on `CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD` instead of on `CL_DEVICE_TOPOLOGY_AMD`. Current OpenCL headers define the second macro without the union, so with upstream's guard the union is never declared and the OpenCL build stops at an unknown type.
- `cpu-fallback-first.patch` — Calls `CPU_detect_or_fallback()` at the very top of `main()` in `src/john.c`, before `sig_preinit()` and `path_init()`. Those two are compiled for the build's SIMD level, so without the patch the AVX-512 build dies of `SIGILL` on an AVX2 machine before it reaches the fallback. The early call is made only in a build with `CPU_FALLBACK` and a fixed `JOHN_SYSTEMWIDE_EXEC`, where the test needs nothing those two functions set up.
- `cprepair-ldflags.patch` — Adds `@LDFLAGS@` to the link line of `cprepair` in `src/Makefile.in`. Upstream links it with no `LDFLAGS` at all, so it would miss the tree's linker flags, among them `-z now`, whose absence the package lint warns about.

## Notes

The baseline build is the last one made. The helper programs it leaves in `run/`
(`calc_stat`, `SIPdump`, the `*2john` converters written in C) are the only copies shipped, and
being SSE2 builds they run on any x86-64. The loop parks each SIMD `john` as `.simd-<name>` so the
next `make clean` does not delete it, and renames the five into place at the end. A new level
must use a name `x86-64.h` expects, or nothing ever `exec`s it.

The install sorts `run/` by kind: ELF programs and the links to `john` go to `/usr/bin`, and
everything else (scripts, the Python modules they import, data) to `/usr/share/john`. The
Python scripts that declare `python2` are deleted first, since the tree has no Python 2.
Each scripted `*2john` converter then gets a link in `/usr/bin`, so `ssh2john.py` is a command.

`run/ztex` holds prebuilt FPGA bitstreams and a USB controller image for the ZTEX 1.15y board.
`--enable-ztex` is not passed, so nothing in the package can load them, and `build.sh` removes
them.

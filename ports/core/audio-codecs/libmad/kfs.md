## About
[libmad]{.idx} is an MPEG audio decoder library, for MPEG-1 and MPEG-2 layers I, II and III, that
computes in fixed-point arithmetic. [sox](#port-sox), [cmus](#port-cmus),
[scummvm](#port-scummvm), [vlc](#port-vlc), [k3b](#port-k3b) and the other ports that name it in
`depends` decode MP3 with it.

The source is the fork maintained by the Tenacity project on Codeberg, which builds with CMake.

## Flags
- `-D BUILD_SHARED_LIBS=True` — build `libmad.so`, upstream's default, named.
- `-D FPM_64BIT=ON` — fixed-point arithmetic on 64-bit integers. On a 64-bit target the
  `CMakeLists.txt` chooses it by itself; named, it is the recipe's own choice.
- `-D ASO=ON` — architecture-specific optimisations, upstream's default. Its `ASO_*` definitions
  are set only in the x86 (32-bit), ARM and MIPS branches, so on x86-64 it changes nothing.
- `-D EXAMPLE=OFF` — skip `mad_example`, a small decoder built from `minimad.c`.

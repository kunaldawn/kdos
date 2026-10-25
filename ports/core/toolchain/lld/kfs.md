## About
[LLD]{.idx}, LLVM's linker, in all its flavours: `ld.lld` for ELF, `lld-link` for Windows COFF,
`ld64.lld` for Mach-O and `wasm-ld` for WebAssembly, with `lld` itself choosing among them by the
name it is run as. The system linker stays binutils' `ld`; nothing makes lld the default, and a
port that wants it names it in its own recipe. The browsers ([chromium](#port-chromium),
[firefox-esr](#port-firefox-esr), [librewolf](#port-librewolf)), [thunderbird](#port-thunderbird),
[webkitgtk](#port-webkitgtk) and [webkitgtk6](#port-webkitgtk6), [wine](#port-wine) (for its
Windows-side code), [qemu](#port-qemu) and [limine](#port-limine) depend on it.

The build is standalone, against the installed [llvm](#port-llvm). lld's own libraries
(`liblldELF.a`, `liblldCOFF.a` and the rest) are installed as static archives with their CMake
package, `/usr/lib/cmake/lld`, for a program that embeds the linker.

## Flags
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++.
- `LLVM_TARGETS_TO_BUILD` — Names five targets, but the installed `LLVMConfig.cmake` that this
  standalone build loads sets `LLVM_TARGETS_TO_BUILD` to the list the llvm port was built with,
  every target, and that takes the place of the command line's. The installed `ld.lld` links the
  libraries of every LLVM target, `libLLVMAArch64CodeGen.so` among them, which it needs for
  link-time optimisation of objects for any of them.
- `LLVM_LINK_LLVM_DYLIB=OFF` — Links the linkers against LLVM's component libraries. The llvm port
  builds no `libLLVM.so`, so there is nothing else to link.

## Notes
The manual page `ld.lld.1` is in the source tree, but upstream's install does not copy it, so the
recipe installs it by hand.

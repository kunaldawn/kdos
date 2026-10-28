## About

zlib is the reference implementation of the deflate compression method and of the zlib and gzip
stream formats built on it, packaged as one library, `libz`, with the header `zlib.h`. PNG
images, `.gz` files and HTTP compression are all deflate, so programs that handle any of them
link it.

[binutils](#port-binutils) and [gcc](#port-gcc) each carry a copy of zlib in their own source
trees, and both are configured `--with-system-zlib` to link this port instead, so `20_selfhost`
builds zlib before either of them. The assembler and the linker read and write zlib-compressed
debug sections through it, and the compiler compresses its link-time-optimisation bytecode with
it, because it is built without zstd. Several hundred other ports name it in `depends`.

zlib's `configure` is a shell script of its own, not one autoconf generated. It takes `--prefix`
and reads `CC`, `CFLAGS` and `LDFLAGS` from the environment, so the exported
[release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags) reach it with
nothing passed on the command line. It builds the shared and the static library together, and the
package carries both, as every package does with the static archives its build installs (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

zlib is the only package that installs `libz.so` and `zlib.h`. The [zlib-ng](#port-zlib-ng) port
is built without its zlib-compatible mode, which would install both names over this port's.

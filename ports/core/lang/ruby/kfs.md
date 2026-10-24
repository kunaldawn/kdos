## About

The Ruby interpreter, `ruby`, with `gem`, `irb`, `erb`, `rdoc`, `bundler` and the standard
library. In KDOS it is mostly a build tool: `asciidoctor`, which many ports generate their manual
pages with, is a Ruby program, and `mkvtoolnix` builds with Ruby's `rake` (its `drake` wrapper).
`weechat` embeds it for Ruby scripts, and `webkitgtk`, `webkitgtk6` and `ocl-icd` name it in
`depends`. It is built in `31_compilers`, after `rust`, which it needs for YJIT.

## Flags

- `--enable-shared` — build `libruby.so` and link `ruby` against it, so programs that embed Ruby,
  `weechat`'s plugin among them, share the interpreter's library. On Linux upstream's default is a
  static `libruby`, linked into `ruby` itself.
- `--disable-rpath` — do not embed a run path in `ruby` and the extension libraries; they find
  `libruby.so` in `/usr/lib` like any other library.
- `--disable-install-doc` — do not generate the rdoc and ri indexes, which `make install` would
  build by running rdoc over the whole standard library. The manual pages under `man/` still
  install.
- `--enable-yjit` — build [YJIT]{.idx}, Ruby's just-in-time compiler, written in Rust. `configure`
  turns it on by itself whenever it finds a recent enough `rustc`, so naming it makes the
  interpreter the same whether or not `rust` happens to be in the chroot; `rust` is in `depends`,
  so it always is. YJIT is off at run time until `ruby --yjit` or `RUBY_YJIT_ENABLE=1` turns it on
  for a process.
- `--disable-zjit` — leave out ZJIT, Ruby's experimental method JIT, also written in Rust.
  `configure` would otherwise build it whenever the `rustc` it finds compiles Rust's 2024 edition
  and `make` is GNU make.
- `--with-gmp` — use GMP to speed up arithmetic on large integers (`Integer` values beyond a
  machine word). This is the default, and `configure` uses GMP only if it finds `gmp.h`; without
  the header it builds Ruby's own bignum code with no warning, so the `gmp` port is in `depends`.
  The build log shows `checking for library containing __gmpz_init... -lgmp`.
- `--with-mantype=man` — write the manual pages as `man(7)` macros, converted from the `mdoc`
  sources by Ruby's installer. Without it, `configure` chooses between `man` and `doc` by probing
  the `nroff` it finds.

## Patches

- `pc-libs-no-ldflags.patch` — removes `${DLDFLAGS}` from the `Libs:` line of `ruby.pc`, so `pkg-config --libs ruby` gives a program that embeds Ruby the library and its dependencies only, not the linker flags Ruby itself was built with (the tree's `LDFLAGS` and `--compress-debug-sections=zlib`).
- `musl-main-stack.patch` — on a Linux C library that is not glibc, measures the main thread's stack from the `[stack]` line of `/proc/self/maps`, bounded by `RLIMIT_STACK` (at most 100 MB) and by the mapping below it, instead of asking `pthread_getattr_np()`. musl answers that call with the part of the stack mapped so far, a few pages at start-up, and Ruby takes it as the whole stack, so a recursion that re-enters the interpreter from C, such as a block called by `map` or a nested parse, raises `SystemStackError` short of a thousand levels under an 8 MB limit.

## Notes

`build.sh` checks, after the install, that the `openssl`, `psych`, `zlib` and `fiddle` extensions
were built. An extension whose library is missing is only a warning in the extension build's
output, and a `LoadError` on the installed system: without `yaml.h` there is no `psych` (and so
no YAML), without `ffi.h` no `fiddle`. Each extension the `depends` line pays for must exist, or
the build stops.

Ruby compiles its own code with `-ggdb3`, which `configure` adds as `debugflags` after the tree's
flags, so `libruby.so` is passed through `strip --strip-debug` to keep the tree's rule that no
package ships debug information.

The installer compresses `ruby.1` and `erb.1` with `gzip` whenever `gzip` is on `PATH`, so the
package's file names would depend on the build host; `build.sh` decompresses them, matching every
other page on the image. The pages `bundler`, `irb` and `rdoc` carry sit inside their library
trees, where `man` never looks, and are copied into `/usr/share/man`. The gem cache, a second copy
of every bundled gem as a `.gem` archive that only `gem pristine` reads, is deleted.

## About
[tree-sitter]{.idx} is a parser generator and an incremental parsing library. The port installs
two things: the `tree-sitter` command, which generates a grammar's C parser from its
`grammar.js` and compiles it, and `libtree-sitter`, the C runtime every generated parser runs
against, with its header and `tree-sitter.pc`. [lldb](#port-lldb) names it in `depends` for
its tree-sitter syntax highlighting (`LLDB_ENABLE_TREESITTER`). [neovim](#port-neovim) does not:
it builds its own copy of the library from a tree-sitter release its recipe pins.

The phase list's reason is the offline case: an editor plugin that downloads and compiles a
grammar the first time a file of that language is opened cannot do so with no network, and the
CLI is what compiles one locally.

## Flags
- `LIBCLANG_PATH=/usr/lib` — Where the `clang-sys` crate looks for `libclang.so`. The CLI's
  default `qjs-rt` feature builds the QuickJS runtime that evaluates `grammar.js`, through the
  `rquickjs` crate with its `bindgen` feature, so `rquickjs-sys`'s build script runs bindgen,
  which loads libclang at build time. That is why [clang](#port-clang) is in `depends`.
- `${CFLAGS/-O2/-O3}` — Raise the tree's `-O2` to `-O3` for the C library. The top-level
  `Makefile` compiles with `-O3` when `CFLAGS` is unset (`CFLAGS ?= -O3 …`), and the exported
  `CFLAGS` replaces that whole default, so the substitution keeps upstream's level and the rest
  of the tree's flags.
- `PREFIX=/usr`, `LIBDIR=/usr/lib` — The Makefile's install layout. Its own defaults are
  `/usr/local` and `$(PREFIX)/lib`; `LIBDIR` also decides the directory written into
  `tree-sitter.pc`.

## Notes
The CLI and the library come from two build systems in one tree: `cargo build` makes the
`tree-sitter` binary, and the top-level `Makefile`, not one in `lib/`, builds and installs
`libtree-sitter.so` and `libtree-sitter.a` from the sources under `lib/src`. Both come from the
same release, so a parser the CLI generates declares an ABI version this library accepts.

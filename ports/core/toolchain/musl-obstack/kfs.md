## About

An [obstack]{.idx} is glibc's stack-shaped memory pool: objects are allocated by growing the top
of the stack and freed by unwinding it to an earlier point, all at once. musl does not provide
it. musl-obstack is a copy of the obstack functions and macros from GCC's `libiberty`, built as
`libobstack.so` with the header `obstack.h` and a `musl-obstack.pc` file for pkg-config.

In KDOS it is there for `elfutils`, whose `eu-ar`, `eu-ranlib` and `eu-nm` link
`libobstack.so.1`; `dwarves` names it too.

## Flags

- `./bootstrap.sh` — the source is GitHub's archive of a tag, which has no generated `configure`;
  upstream's script generates it with the autotools.

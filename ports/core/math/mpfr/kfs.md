## About

MPFR does floating-point arithmetic at any precision with correct rounding, on top of
[gmp](#port-gmp). [gcc](#port-gcc) needs it, both to build and to run: the compiler evaluates
floating-point constants and calls such as `sin(1.0)` at compile time through MPFR. [mpc](#port-mpc) is built on it, and
[gawk](#port-gawk) uses it for `gawk -M`. No phase list names it; `20_selfhost` installs it as a
dependency of gcc and gawk.

## Flags

- `--enable-thread-safe` — keep MPFR's global state (the default precision and rounding mode, the
  exponent range, the exception flags and its constant caches) in thread-local storage, so threads
  that use MPFR at once do not share it. configure detects this by default; given outright, it
  stops with `please configure with --disable-thread-safe` if the compiler's thread-local storage
  does not work, instead of quietly building a library that is not thread-safe.

## Notes

`cc1` and `cc1plus` link `libmpfr.so` dynamically, so the compiler does not start without this
package; see the note on [gmp](#port-gmp).

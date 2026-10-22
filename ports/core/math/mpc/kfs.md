## About

GNU MPC does arithmetic on complex numbers at any precision with correct rounding, built on
[mpfr](#port-mpfr) and [gmp](#port-gmp). Its one user in the base system is [gcc](#port-gcc), which
needs it to build and links it to fold complex-number constants and calls such as `cexp` at compile
time. The cross compilers of later phases depend on it for the same reason. No phase list names it;
`20_selfhost` installs it as gcc's dependency, after MPFR.

The build is upstream's with nothing changed: `--prefix=/usr` and the exported
[release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags).

## Notes

`cc1` and `cc1plus` link `libmpc.so` dynamically, so the compiler does not start without this
package; see the note on [gmp](#port-gmp).

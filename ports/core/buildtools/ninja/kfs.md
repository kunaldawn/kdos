## About

Ninja is the low-level build tool that runs the build files meson and CMake write. A large share
of the tree's ports is configured by one of those two and then compiled by ninja, so it is one of the build
tools `30_foundation` installs first, ahead of every shelf section of the phase list. Once
`/usr/bin/ninja` exists, `script/env/chroot.env` puts `script/bin` first on `PATH`, and the
`ninja` wrapper there adds `-j$KDOS_JOBS` to any call that names no job count; see
[The phase environment](../../../../docs/kdos/05-developer/build-system.md#the-phase-environment).

The port builds ninja from nothing: `configure.py --bootstrap` compiles a first ninja binary
directly with the C++ compiler, then has that binary build the final one. `configure.py` writes
no install rule, so `build.sh` copies the program and the bash and zsh completions into place
itself.

## Flags

- `--bootstrap` — Build ninja without an installed ninja. `configure.py` runs the compiler itself
  for a first binary, then rebuilds ninja with it. Python 3 is the only other tool it needs, which
  is why `python3` is the port's one dependency.
- `env -u CFLAGS` — `configure.py` appends both `CFLAGS` and `CXXFLAGS` to its compile and link
  lines, and ninja is written entirely in C++. Unsetting `CFLAGS` keeps the tree's C-only
  `-std=gnu11` off a C++ compile line and stops the release flags appearing twice; `CXXFLAGS`
  alone carries them.
- `-g0` — `configure.py` always compiles with `-g`. `-g0` comes after it on the line and cancels
  it, so the binary carries no debug information, like every other release build in the tree.

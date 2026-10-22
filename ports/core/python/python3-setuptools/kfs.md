## About

setuptools is a build backend for Python packages: the `setup.py` and
`setuptools.build_meta` machinery that turns a source tree into a wheel, and builds C extensions on
the way. It also provides `pkg_resources`, and a `distutils` module for programs that import
one, which Python's own standard library does not include. Many `python3-*` ports in the tree build with
it, and so does [meson](#port-meson), which `30_foundation` installs with pip once this port and
[python3-pip](#port-python3-pip) are in place.

It is built before pip exists, and it needs nothing but the interpreter to build itself: the
libraries it uses (packaging, wheel, platformdirs, tomli, jaraco's and the rest) are vendored inside
it under `setuptools/_vendor`. So `build.sh` does not use pip. It calls setuptools' own PEP 517
hook, `build_meta.build_wheel`, from the source directory, unpacks the resulting wheel into the
interpreter's `purelib` directory inside `$PKG` with the standard library's `zipfile` module, and
compiles the bytecode itself. [python3-pip](#port-python3-pip) depends on this port, and every later
Python port that installs with pip comes after both.

## Flags

- `-o 0` — with `-o 1` and `-o 2`, `compileall` writes bytecode for all three optimisation levels:
  the plain `.pyc` and the `.opt-1.pyc` and `.opt-2.pyc` that `python3 -O` and `-OO` load. A wheel
  carries no bytecode, and an installer such as pip would normally compile it; here nothing else
  does, and a package without the `.pyc` files would have Python try to write them into `/usr/lib`
  at run time, where an ordinary user cannot.
- `-d` — the directory recorded in each `.pyc` as its source's location: the installed path under
  `/usr`, not the `$PKG` staging path the files are compiled in. Tracebacks then name the real file.

## Notes

The source carries Windows launcher executables, `cli*.exe` and `gui*.exe`, which setuptools copies
next to console scripts it installs on Windows. `build.sh` deletes them from the package; nothing on
Linux runs them.

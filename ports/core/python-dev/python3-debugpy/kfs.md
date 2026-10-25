## About
[debugpy]{.idx} is Microsoft's implementation of the Debug Adapter Protocol for Python: a
debugger an editor drives over a socket. [spyder](#port-spyder) and
[python3-ipykernel](#port-python3-ipykernel) debug through it. It carries a copy of pydevd, the
debugger core, under `debugpy/_vendored/pydevd`, with C accelerators for its tracing written in
Cython.

## Flags
- `--force-cython` — To pydevd's `setup_pydevd_cython.py`: generate the C files again from the
  `.pyx` sources with the [python3-cython](#port-python3-cython) port, rather than compile the
  C files the source distribution ships, and build the extensions in place in the pydevd tree.
- `REQUIRE_CYTHON_BUILD=1` — `setup.py` runs the same compile again and, by default, carries on
  with only a warning when it fails, leaving debugpy with the slower pure-Python tracer. Set to
  `1`, a failure stops the build.
- `-nostartfiles` — In the hand-written link of `attach_linux_amd64.so`: link the library
  without the C runtime's start files. This is the command of upstream's
  `pydevd_attach_to_process/linux_and_mac/compile_linux.sh`, with the tree's `CXXFLAGS` and
  `LDFLAGS` in place of that script's own flags.

## Patches
- `cython-redeclared.patch` — In `_pydevd_sys_monitoring/_pydevd_sys_monitoring_cython.pyx`, drops the `: FuncCodeInfo` annotation from six assignments to `func_code_info`, a variable each of those functions has already declared with `cdef FuncCodeInfo func_code_info`. The Cython port rejects the second declaration of the same name, so without the patch the accelerator does not compile and, with `REQUIRE_CYTHON_BUILD=1`, the build stops.

## Notes
The attach library is what `debugpy --pid <pid>` injects into a running interpreter to attach to
it. The source distribution carries its source, `attach.cpp`, but no Linux build of it, so
`build.sh` compiles it before pip runs and `setup.py` packs it with the rest of the pydevd tree.

The three checks at the end stop the build if the package lacks the `debugpy` command, the
compiled `pydevd_cython` module or the attach library, any of which a quiet fallback would
otherwise leave out.

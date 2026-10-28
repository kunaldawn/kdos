## About
PyYAML reads and writes YAML from Python. Built against the C library libyaml, the
[yaml](#port-yaml) port, it adds the `yaml._yaml` extension, whose `CLoader` and `CDumper` are
much faster than the pure-Python parser and emitter.

Many build systems read YAML through it: [mesa](#port-mesa) and [libcamera](#port-libcamera)
depend on it, and [freecad](#port-freecad), [gnuradio](#port-gnuradio), [astropy](#port-astropy),
[picard](#port-picard), [qutebrowser](#port-qutebrowser) and [python3-sphinx](#port-python3-sphinx)
use it at run time.

## Flags
- `PYYAML_FORCE_LIBYAML=1` — Make the libyaml extension mandatory. PyYAML's `setup.py` builds it
  when it can and, when the compile or link fails, falls back to pure Python with only a warning;
  with the variable set, the failure stops the build, so a package without `CLoader` cannot be
  produced by accident. See [Python modules](../../../../docs/kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).
- `--find-links=vendor` — Look for distributions in the unpacked `vendor` directory, the only
  place pip looks with `--no-index`. The line has no `--no-build-isolation`, so pip builds
  PyYAML in a temporary environment and installs its build requirements, setuptools and Cython,
  there from the bundle.

## Notes
The source distribution ships the extension only as Cython source, `yaml/_yaml.pyx`, with no
generated `_yaml.c`, so the extension cannot be built without Cython. The tree's Cython port,
[python3-cython](#port-python3-cython), is built in `40_lang`, after this one, which is why
Cython comes from the vendor bundle rather than from `depends`.

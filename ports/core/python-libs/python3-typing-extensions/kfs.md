## About
[typing_extensions]{.idx} carries additions to Python's `typing` module ahead of the interpreter
that gains them, and their backports to older ones, so that a library can use new type-hint
features on any supported Python. It is a single pure-Python module built with flit_core, and a
runtime import of many of the tree's Python packages, [python3-pydantic-core](#port-python3-pydantic-core)
and [python3-referencing](#port-python3-referencing) among them. A module carried in two
packages' bundles would be a path two packages own, which kpkg refuses, so a module several
packages import is a port of its own in each one's `depends` (see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).

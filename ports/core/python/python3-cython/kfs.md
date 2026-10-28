## About
[Cython]{.idx} compiles Python with optional C type declarations into C extension modules. It
is a build-time dependency of many of the tree's Python packages with C code:
[numpy](#port-numpy), [pandas](#port-pandas), [scipy](#port-scipy),
[python3-lxml](#port-python3-lxml) and [python3-debugpy](#port-python3-debugpy) among them.

It is a port, not a member of each consumer's vendor bundle. Each consumer builds with
`--no-build-isolation` against the installed Cython; with isolation, pip would fetch Cython and
the rest of the backend chain from PyPI into a temporary environment for every build. See
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

## Notes
The Cython release here decides what the consumers' `.pyx` sources must accept.
[python3-debugpy](#port-python3-debugpy) carries a patch for it, and
[python3-meshtastic](#port-python3-meshtastic) skips dbus-fast's optional Cython build because
dbus-fast declares a Cython older than this one.

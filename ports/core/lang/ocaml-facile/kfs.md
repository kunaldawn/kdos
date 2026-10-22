## About
[FaCiLe]{.idx} is a constraint programming library for OCaml: finite-domain variables,
constraints over them and search goals. KDOS carries it for one consumer,
[kalzium](#port-kalzium), whose chemical equation balancer is written in OCaml against it;
kalzium's build requires it (`CMAKE_REQUIRE_FIND_PACKAGE_Libfacile=ON`).

## Patches
- `ocaml5-stdlib.patch` — Replaces every use of the `Pervasives` module in five of the library's sources (`fcl_cstr.ml`, `fcl_fdArray.ml`, `fcl_misc.ml`, `fcl_nonlinear.ml`, `fcl_sorting.ml`) with `Stdlib`, the same module under the name OCaml 5 keeps. OCaml 5 has no `Pervasives`, so the library does not compile against the [ocaml](#port-ocaml) port without it.

## Notes
The release describes its dune build in `jbuild` files, a format current dune does not read,
and the tree has no dune port. The library is built with the plain `Makefile` in `lib/`
instead, which compiles the same modules; `make -C lib facile.cma facile.cmxa` builds the
bytecode and native libraries and leaves out the `facile` toplevel that its default target also
makes.

The files go where kalzium's `FindLibfacile.cmake` looks: `facile.a` and `facile.cmi` under
`$(ocamlc -where)/facile`, that is `/usr/lib/ocaml/facile`. `build.sh` also writes a findlib
`META` file there, naming the bytecode and native archives.

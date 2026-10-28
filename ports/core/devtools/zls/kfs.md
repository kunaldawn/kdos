## About
[ZLS]{.idx} is the Zig language server. It parses Zig with the parser in Zig's standard library
(`std.zig.Ast`), and a tagged ZLS release names the Zig release of the same number as both its
minimum build and its minimum run-time version (`minimum_zig_version` in `build.zig.zon`,
`minimum_runtime_zig_version` in `build.zig`). It is built with the installed [zig](#port-zig),
so the recipe's version and the zig port's move together; the phase list's comment says
that a mismatch reports syntax errors in correct code.

## Flags
- `ZIG_GLOBAL_CACHE_DIR` — Zig's global cache, which holds compiled artifacts and, under `p/`,
  unpacked packages named by their hash. It is set to a directory under `$SRC_ROOT`, inside the
  build.
- `--system "$ZIG_GLOBAL_CACHE_DIR/p"` — Take every package from that directory and fetch none.
  `build.zig.zon` names four dependencies by URL and hash (known-folders, diffz, lsp-kit and
  tracy), and in system mode zig reports "remote package fetching disabled" instead of reaching
  for the network.
- `-Doptimize=ReleaseSafe`, `-Dcpu=baseline` — An optimised build with the safety checks kept,
  and code for the baseline x86-64 rather than the build machine's processor, which every zig
  port names; see
  [zig](../../../../docs/kdos/05-developer/writing-ports.md#zig).
- `-Dpie=true` — ZLS's own option for a position-independent build. Upstream applies it only to
  the modules' `pic` setting; with `pie.patch` it makes the executable a PIE as well, like
  every other program in the tree (see
  [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)).
- `--strip-debug` — On the installed `zls`: remove the DWARF zig writes into the executable and
  keep its symbol table.

## Patches
- `pie.patch` — Adds `exe.pie = pie;` to the `zls` executable in `build.zig`, so `-Dpie=true` produces a position-independent executable rather than only position-independent code. The built `zls` is a static PIE.

## Notes
The four dependencies are extra `source =` lines in the recipe, `zlsdep-<name>.tar.gz`, and
`unpack_zig_dep` unpacks each into `$ZIG_GLOBAL_CACHE_DIR/p/<hash>`. The hashes in `build.sh`
are copied from `build.zig.zon`; a version bump that changes a dependency changes both the
recipe's URL and its hash in `build.sh`, or zig does not find the package.

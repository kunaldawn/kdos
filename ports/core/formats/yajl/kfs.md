## About
YAJL (Yet Another JSON Library) is a small, event-driven JSON parser and generator in C: a program
gives it callbacks and it reports each value as it reads it, so a document need not fit in memory
at once. It also carries a tree API, and installs the `json_reformat` and `json_verify` tools. In
this tree [raptor2](#port-raptor2) uses it for its JSON parser; its JSON serializer does not need
it.

## Patches
- `cmake4-target-file.patch` — In `reformatter/CMakeLists.txt` and `verify/CMakeLists.txt`, replaces the read of each tool's `LOCATION` target property, which CMake 4 refuses, with the generator expression `$<TARGET_FILE:…>` in the post-build command that copies the tool into the build's output directory. Without it configuring stops.

## Notes
The release declares `CMAKE_MINIMUM_REQUIRED(VERSION 2.6)`, which the tree's CMake refuses unless
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` raises the floor.

The project always builds and installs both libraries, the shared `libyajl.so` and the static
`libyajl_s.a` the two tools link, and has no switch to leave the archive out. Its `yajl.pc` is
installed in `/usr/share/pkgconfig`, not `/usr/lib/pkgconfig`; pkg-config searches both.

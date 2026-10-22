## About
JSON for Modern C++, by Niels Lohmann, is a header-only C++ library that makes a JSON value behave
like an STL container: `json j = json::parse(text); j["key"] = 1;`. Among the tree's C++ programs
[fcitx5](#port-fcitx5), [imhex](#port-imhex),
[amiberry](#port-amiberry), [avogadrolibs](#port-avogadrolibs) and many others name it in
`depends`. The package is headers, a CMake package configuration (`nlohmann_json::nlohmann_json`)
and a pkg-config file; nothing is compiled.

## Flags
- `-D JSON_BuildTests=OFF` — Do not configure the unit tests. They default to on when the library
  is the top-level project, and the release tarball (`json.tar.xz`) carries no `tests/` directory,
  so with them on configure stops at the missing `add_subdirectory(tests)`.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes
There is no build step: `build.sh` configures and then runs `cmake --install` directly, which
copies the headers from `include/` (upstream's multiple-header layout, its default) and writes
the CMake and pkg-config files.

The release tarball's top directory is `json/`, not `nlohmann-json-<version>`; kpkg strips the one
wrapping directory whatever its name.

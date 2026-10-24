## About
Protocol Buffers is Google's language-neutral format for structured data: a message type is
declared once in a `.proto` file, and `protoc` generates the code that encodes and decodes it. This
port installs `protoc`, the C++ runtime (`libprotobuf`, `libprotobuf-lite`), the compiler library
`libprotoc` that code-generator plugins link, and the upb generators. [mosh](#port-mosh),
[mumble](#port-mumble), [kicad](#port-kicad), [android-tools](#port-android-tools) and
[protobuf-c](#port-protobuf-c) are among the ports that build against it.

## Flags
- `-Dprotobuf_BUILD_TESTS=OFF` — Build no test suite. Off is upstream's default; the tests would also
  need googletest.
- `-Dprotobuf_BUILD_SHARED_LIBS=ON` — Build shared libraries. Upstream follows `BUILD_SHARED_LIBS`,
  which is unset, so its default is static archives.
- `-Dprotobuf_WITH_ZLIB=ON` — Build the gzip stream classes against [zlib](#port-zlib). On is
  upstream's default, and a missing zlib turns the support off without an error; `depends` is what
  guarantees it.
- `-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON` — Never download a dependency. Without it, CMake fetches
  Abseil from GitHub with `FetchContent` whenever it cannot find an installed one, which fails in
  the offline build and on a networked machine would build a private copy. With it, the
  installed [abseil-cpp](#port-abseil-cpp) is used, and a missing one stops configure with "Cannot
  find abseil-cpp dependency".

## Notes
Every shared library carries the full release in its soname (`libprotobuf.so.<version>`), so each
release is a new ABI: a program built against one fails to load against the next, and every port
that links protobuf has to be rebuilt with a version bump. The public headers also include
Abseil's, so an [abseil-cpp](#port-abseil-cpp) bump calls for the same.

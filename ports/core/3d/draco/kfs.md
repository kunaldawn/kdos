## About
[Draco]{.idx} is Google's library for compressing 3D meshes and point clouds, with the
`draco_encoder` and `draco_decoder` commands. A glTF asset may carry its meshes compressed with it,
and in this tree [blender](#port-blender),
[opencascade](#port-opencascade), [pdal](#port-pdal) and [qgis](#port-qgis) link it.

## Flags
- `-D BUILD_SHARED_LIBS=ON` — add the shared `libdraco.so` to the build. On Linux draco always
  builds the static `libdraco.a`, and both are installed; with this switch the exported
  `draco::draco` target, which consumers link, is the shared library.
- `-D DRACO_TRANSCODER_SUPPORTED=OFF` — leave out the transcoder, which reads and writes glTF
  scenes. It needs the `tinygltf`, `eigen` and `filesystem` submodules under `third_party/`, and
  the release archive carries them as empty directories. Off is upstream's default.
- `-D DRACO_MESH_COMPRESSION=ON` — mesh compression, and `DRACO_POINT_CLOUD_COMPRESSION` point
  cloud compression, both upstream's defaults, named.
- `-D DRACO_JS_GLUE=OFF` — the JavaScript glue targets, which exist only in an Emscripten build.
  Upstream's default is on; on a native build it changes nothing.
- `-D DRACO_TESTS=OFF` — with `DRACO_WASM`, `DRACO_UNITY_PLUGIN`, `DRACO_MAYA_PLUGIN` and
  `DRACO_ANIMATION_ENCODING`, the parts of the tree this build leaves out: the test suite (whose
  `googletest` submodule is empty too), the WebAssembly decoder, the plugin libraries for Unity and
  Maya, and animation encoding. Each is off by upstream's default.

## Notes
The three patches are Debian's, and each carries its own description. Together they make the
installed CMake package usable: the headers go to their real paths under `/usr/include/draco`, the
package configuration goes to `/usr/lib/cmake/draco` rather than under `/usr/share`, and it sets
`DRACO_LIBRARIES` as well as the `draco::draco` target, so a consumer's `find_package(draco)`
resolves to the shared library whichever of the two it reads.

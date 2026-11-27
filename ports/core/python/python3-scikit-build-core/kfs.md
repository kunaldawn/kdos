## About
scikit-build-core is the build backend for Python packages whose compiled part is a CMake project:
it runs [CMake](#port-cmake) and [Ninja](#port-ninja) and packs what they install into a wheel.

## Notes
`runtime = python3-pathspec`: the backend selects the files of every build with pathspec, imported
from `scikit_build_core/build/_file_processor.py`, a submodule the [run-time needs](../../../../docs/kdos/03-architecture/packaging.md#run-time-needs) record
does not read. [python3-pathspec](#port-python3-pathspec) is a build-only port, so without the word
a prebuilt scikit-build-core, which a source build on an installed system is handed, would come
without it and stop the build that called it.

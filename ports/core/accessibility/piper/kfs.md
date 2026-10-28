## About

Piper is a neural text-to-speech engine that runs on the processor through
[onnxruntime](#port-onnxruntime), with the voice model as a file on the disk: the text it speaks
is sent nowhere. A voice is an `.onnx` model with an `.onnx.json` beside it, passed with `-m`.
Three parts of the program reach the network: the `piper.download_voices` module, which fetches
voices; the `piper.http_server` module's voice list and download routes; and a voice whose
phoneme type is pinyin (Mandarin), which fetches the g2pW model from Hugging Face into `g2pW`
under the current directory whenever `g2pW/g2pw.onnx` is not already there. The package installs
the `piper` Python package and a `piper` command, and with none of `-f`, `-d` and `--output-raw`
the `piper` command speaks through PipeWire.

## Flags

- `-DPIPER_ESPEAKNG_ARCHIVE="$d/$_espeak_tar"` — A cache variable that `espeak-ng-local-archive.patch`
  adds: the espeak-ng source archive CMake's ExternalProject builds, in place of the git clone
  upstream's `CMakeLists.txt` makes. `$d` is whichever of the port directory and kpkg's source
  directory holds the second source, and the build stops when neither does.

## Patches

- `espeak-ng-local-archive.patch` — Makes upstream's ExternalProject build espeak-ng from the archive named by `PIPER_ESPEAKNG_ARCHIVE` instead of cloning it from GitHub at a pinned commit, since the build has no network; the archive is the recipe's second source, at the commit `_espeak` names. It puts the tree's `$CFLAGS` and `$CXXFLAGS` in front of the flags upstream passes to that build, which otherwise replace them, so the static espeak-ng library is compiled with the tree's flags. It also passes placeholder `SONIC_LIB` and `SONIC_INC` values: espeak-ng's CMake clones the sonic library whenever it finds no installed copy, even with `USE_LIBSONIC` off, and nothing reads the two while that option is off.
- `pw-play-playback.patch` — Plays the synthesised audio through PipeWire's `pw-play`, as raw 16-bit mono at the voice's sample rate, instead of `ffplay`, and makes the check for a player look for `pw-play`. The [ffmpeg](#port-ffmpeg) port builds no `ffplay`, and without a player every plain `piper` run would write `output.wav` into the current directory instead of speaking.

## Notes

Piper phonemises through a function, `espeak_TextToPhonemesWithTerminator`, that no espeak-ng
release has, so upstream builds a static espeak-ng from a pinned commit and links it into its
Python extension. The recipe's `_espeak` helper names that commit; a bump of piper keeps it at or
past the commit the new release pins in its `CMakeLists.txt`. espeak-ng's compiled
`espeak-ng-data` is installed inside the package directory, where piper looks for it, and
`build.sh` stops if `phontab` or the `espeakbridge` extension is missing.

Upstream builds its Python package with scikit-build, the setuptools-based `skbuild`, which is
not a port (scikit-build-core, a different backend, is). `build.sh` does what that build does by
hand: it runs the project's CMake, which compiles the `espeakbridge` extension and espeak-ng's
data and copies the data into `src/piper`; copies `src/piper` into `site-packages` without the
training code and the extension's C source; installs the CMake output into it; byte-compiles it;
and writes the `piper` console script that the package's entry point would have made.

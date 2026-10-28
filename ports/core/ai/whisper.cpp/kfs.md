## About

whisper.cpp is a C/C++ implementation of OpenAI's Whisper speech recognition, running on the
processor or, through ggml's Vulkan backend, on a GPU, with the model as a file on the disk and
nothing sent anywhere. The package installs the library and the example programs, among them
`whisper-cli`, which transcribes a file, and `whisper-stream`, which transcribes the microphone
live. The English base model is the separate [whisper-model-base-en](#port-whisper-model-base-en)
port.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Build `libwhisper` as a shared library.
- `-DWHISPER_BUILD_TESTS=OFF` — Leave out the tests.
- `-DWHISPER_BUILD_EXAMPLES=ON` — Build the example programs, which are the command-line tools:
  `whisper-cli`, `whisper-server`, `whisper-bench` and the rest.
- `-DWHISPER_SDL2=ON` — Build the programs that read a microphone through SDL's audio device, the
  one this port wants being `whisper-stream`, which transcribes a rolling window of live audio.
  `SDL_Init(SDL_INIT_AUDIO)` opens no window, so it runs in a terminal with no compositor.
  [sdl2-compat](#port-sdl2-compat) provides SDL2.
- `-DWHISPER_COMMON_FFMPEG=ON` — Let the programs fall back to [ffmpeg](#port-ffmpeg) when the
  bundled miniaudio decoder, which reads WAV, MP3, FLAC and (through stb_vorbis) Ogg Vorbis, cannot
  open a file, so `whisper-cli` also reads Opus, AAC and the audio track of a video.
- `-DWHISPER_USE_SYSTEM_GGML=ON` — Link the [libggml](#port-libggml) port instead of building the
  copy of ggml in the tarball, so whisper.cpp and llama.cpp load one set of CPU, OpenBLAS and Vulkan
  backend modules from `/usr/lib/ggml`. `build.sh` fails if the install put a `libggml.so` or a
  `/usr/lib/ggml` of its own into the package, which would overwrite the port's files.

## Patches

- `sdl2-stream-only.patch` — `WHISPER_SDL2` builds four programs beside `whisper-stream`, and talk-llama's CMake fetches llama.cpp from GitHub at configure time, which ends an offline configure with `Could not resolve host`. No switch drops it, so the patch takes `command`, `talk-llama`, `lsp` and `wchess` out of the example subdirectories and the flag builds `whisper-stream` alone.

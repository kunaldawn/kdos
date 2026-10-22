## About

libime is the conversion library behind fcitx5's Chinese input: the pinyin and table engines, the
language model they rank candidates with, and the tools that compile the dictionaries.
[fcitx5-chinese-addons](#port-fcitx5-chinese-addons) is its one consumer in KDOS; see
[fcitx5](#port-fcitx5) for where the input method sits in the session.

Besides the library, the port compiles the data the engines need: the simplified-Chinese language
model, the pinyin dictionaries and the input tables (Wubi, Cangjie, Erbi, Ziranma and others).
Upstream publishes these as text archives on its download server, separate from the source
release, and the recipe declares each one as a source of its own.

## Flags

- `-D ENABLE_DATA=On` — Builds and installs the language model (`zh_CN.lm` and its prediction
  table), the pinyin dictionaries and the table dictionaries from the data archives.
- `-D ENABLE_TOOLS=On` — Builds and installs the `libime_*` command-line tools:
  `libime_slm_build_binary`, `libime_prediction`, `libime_pinyindict`, `libime_tabledict` and the
  rest. The data step above runs them on the text sources, so a build with data needs them, and
  [fcitx5-chinese-addons](#port-fcitx5-chinese-addons) runs `libime_pinyindict` from here for a
  dictionary of its own.

## Notes

Upstream's `data/CMakeLists.txt` fetches the language model, the dictionary and the table set
with `file(DOWNLOAD)` at build time, and the build runs with no network. The download step does
nothing when a file with the expected sha256 already sits where it would write it, resolved
against `data/`, so `build.sh` copies the archives there, still packed, before configuring;
the hash is taken of the archive, so it must not be unpacked first. The same applies to
[fcitx5](#port-fcitx5)'s English word list. As there, kpkg does not unpack a `.tar.zst`, so
`build.sh` unpacks the source archive itself.

The data archives' names and sha256 sums are written twice: in `kpkgbuild`, and in upstream's
`data/CMakeLists.txt`. A libime release that moves to newer data changes the CMake side, and the
recipe's names and `sha256` lines have to follow it. Otherwise the build is left without the
data: upstream's step deletes a copy whose hash does not match without fetching another, and tries
the network for a file name it does not find.

## About

The Chinese engines for [fcitx5](#port-fcitx5): pinyin, shuangpin and the table-based methods
(Wubi, Cangjie and the rest), on top of [libime](#port-libime), with the modules that go with
them: simplified-to-traditional conversion, the pinyin and stroke helpers that show how a
character is typed, and punctuation. It is the Chinese engine among the language engines KDOS
builds; the input method as a whole is described in
[Input methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods).

Two of upstream's features are left out: the Qt configuration tool, and cloud pinyin.

## Flags

- `-D ENABLE_GUI=Off` — Builds no configuration tool. Upstream's is Qt 6 Widgets and needs
  fcitx5-qt's widget addons, which are not a port; fcitx5's own configuration is text files.
- `-D ENABLE_BROWSER=Off` — Builds no built-in browser for the configuration tool, which would
  need Qt WebEngine. It is read only when `ENABLE_GUI` is on.
- `-D ENABLE_CLOUDPINYIN=Off` — Builds no cloud pinyin addon, which sends the pinyin being typed
  to an online service and offers what comes back as a candidate: off, what is typed never leaves
  the machine. With it on, configure also requires [curl](#port-curl)'s `libcurl`.
- `-D ENABLE_OPENCC=On` — Builds the `chttrans` module's conversion between simplified and
  traditional characters through [opencc](#port-opencc). Once on, OpenCC and
  [nlohmann-json](#port-nlohmann-json) are required at configure time.
- `-D ENABLE_DATA=On` — Compiles the pinyin engine's `chaizi.dict`, the dictionary of characters
  by their components, with libime's `libime_pinyindict`.
- `-D ENABLE_TOOLS=On` — Builds and installs `scel2org5`, which converts a `.scel` word list
  into a dictionary file libime reads.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Fcitx5ModuleLuaAddonLoader=ON` — Makes CMake skip the search for
  fcitx5's Lua addon loader. Upstream looks for it with a `find_package` that no option controls,
  and links the pinyin engine against it when it is found. There is no fcitx5-lua
  port, and the switch keeps the result from depending on what else is installed.

## Notes

The pinyin helper module fetches two data sets with `file(DOWNLOAD)` at build time, the pinyin
table and the stroke table, and the build runs with no network. As for
[libime](#port-libime), the download step does nothing when the archive with the expected sha256
is already where it would write it, so `build.sh` copies both archives, still packed, into
`modules/pinyinhelper/`. These two are prebuilt `.mb` tables that the module extracts and
installs as they are; they are among the data files
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)
exempts, as they have no other source form. kpkg does not unpack a `.tar.zst`, so `build.sh`
unpacks the source archive itself.

Each table's name and sha256 are written twice, in `kpkgbuild` and in
`modules/pinyinhelper/CMakeLists.txt`, and a release that moves to newer tables needs both
changed. Otherwise the build is left without the table: upstream's step deletes a copy whose hash
does not match without fetching another, and tries the network for a file name it does not find.

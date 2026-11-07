## About

KTextAddons is the set of text-editing add-ons that KDE PIM and other KDE applications share: a
translator with network engines (Google, Bing, DeepL, LibreTranslate, Lingva, Yandex), grammar
checking through LanguageTool or Grammalecte, autocorrection, an emoji and emoticon picker,
text-to-speech and speech-to-text (Google's service or a local Whisper model) for text fields, a custom
rich-text editor, and TextAutoGenerateText, a chat client for large language models. KMail, the
sieve editor and the rest of the KDE PIM stack link it, so the port is built before that stack.

## Flags

- `-D BUILD_KIO_SUPPORT=ON` — Link [kio](#port-kio) for the add-ons that open or save files
  through KIO. On is upstream's default; the recipe names it, so the KIO-backed parts cannot drop
  out of the package unnoticed.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D ENABLE_WARN_OUTDATED=OFF` — Compile out the banner that tells the user their version is
  more than six months old and asks them to upgrade. It measures age from the compile date, while
  KDOS updates the whole system as one release; the banner would appear on a current system half
  a year after its packages were built.
- `-D USE_FASTER_LINKER=OFF` — Link with the toolchain's own linker. Upstream's default looks for
  `wild` and then `mold` and switches to the first one installed, so the linker would depend on
  what happened to be present in the build root.
- `-D BERGAMOT_TRANSLATOR_PLUGIN=OFF` — No offline Bergamot translator. It needs `slimt`, which is
  not a port, and upstream builds it only from a source checkout downloaded separately. Off is
  upstream's default; the recipe names it.
- `-D SPEAK_TO_TEXT_VOSK_PLUGIN=OFF` — No Vosk speech recogniser. It needs the Vosk library, which is not a port; upstream points the builder at Vosk's
  prebuilt release archives. Off is upstream's default; the recipe names it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6TextToSpeech=ON` — Make Qt Text-to-Speech from
  [qt6-qtspeech](#port-qt6-qtspeech) a requirement. Upstream looks for it as optional and builds
  `KF6TextEditTextToSpeech`, the read-aloud add-on for text fields, only when it is found.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

TextAutoGenerateText has no switch in this release: the library and its back-end plugins (Ollama,
Ollama Cloud, LM Studio, llama.cpp and a generic network endpoint) are built every time. They
do nothing until an application exposes them, and the PIM consumers are built with that client
off (`OPTION_ADD_AUTOGENERATETEXT=OFF` in libksieve, KMail and the sieve editor).

The network translators, LanguageTool and Google's speech-to-text send text to a remote service
only when the user selects one and configures it. Qt Keychain from
[qtkeychain](#port-qtkeychain), which the configure requires, keeps DeepL's and the language-model
plugins' API keys in the secret store, and Qt Multimedia from
[qt6-qtmultimedia](#port-qt6-qtmultimedia), which the configure requires, records the audio for
speech-to-text.

Two engines are built every time and install their Python helpers in `/usr/bin`: Kokoro, a Qt
Text-to-Speech engine plugin (`texttospeech/KokoroTextToSpeechPlugin.so`) driven by
`kokoro_helper.py`, and Whisper speech-to-text, driven by `whisper_helper.py`. Neither has a
switch. Each runs its helper with the Python of a virtual environment in the user's home, which
the add-on offers to create and fill with `pip` from the network (PyTorch and `kokoro`, or
`faster-whisper`), and the models are downloaded from Hugging Face into `~/.cache/huggingface`
on first use. Until the user does that, the engine reports itself unavailable; the package
carries no Python dependency for them.

`KF6TextUtils` renders Markdown with a copy of cmark that the release carries in
`textutils/cmark-rc/` and installs as `libtextutils-cmark-rc-copy.so`; there is no switch to use
[cmark](#port-cmark) instead, so a security fix in cmark reaches this copy only through a
KTextAddons release.

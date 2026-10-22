## About

KTextWidgets supplies rich text editing widgets for KDE programs: `KTextEdit` and `KRichTextEdit`
with spell checking through [sonnet](#port-sonnet), the find and replace dialogs, and reading the
text aloud. Many KDE applications link it.

## Flags

- `-D WITH_TEXT_TO_SPEECH=ON` — Build the "Speak Text" action of `KTextEdit`, through Qt's
  TextToSpeech module. It is upstream's default; on, the configure requires
  [qt6-qtspeech](#port-qt6-qtspeech), which is why that port is in `depends`.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.

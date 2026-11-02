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
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

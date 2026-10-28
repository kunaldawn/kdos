## About

KWordQuiz is KDE's flashcard trainer: it drills a vocabulary deck as flashcards, multiple choice
or typed question-and-answer, and edits the decks. Decks are KVTML files, read and written through
[libkeduvocdocument](#port-libkeduvocdocument). The interface is Kirigami.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Fail configure if [kdoctools](#port-kdoctools)
  is missing. The handbook is optional upstream: when kdoctools is not found it is left out and
  the package is only listed as a missing optional one in configure's summary.

## Notes

The translation catalogues are not built (`KF_SKIP_PO_PROCESSING`), and the package prune removes
the translated handbooks; see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).

KVTML is not in the shared MIME database, so `build.sh` installs `kdos-kvtml.xml`, which defines
`application/x-kvtml` for `*.kvtml`; KWordQuiz is the one entry that claims it. The menu entry is
replaced: `KAboutData` makes the Wayland app_id `org.kde.kwordquiz`, which the entry gives as
`StartupWMClass`. Upstream's entry names the icon `org.kde.kwordquiz`, which exists only as a
scalable SVG the panel does not read; the hicolor PNGs upstream installs are named `kwordquiz`,
and the replacement entry names those.

## About

Arianna is KDE's EPUB reader: a library that finds the books on the machine, groups them by
author, series and publisher, and remembers how far each one has been read, and a reader with
search, a table of contents and its own colour themes. The pages are drawn by foliate-js, a
JavaScript renderer shipped in the source, inside a Qt WebEngine view; the application serves
the open book to that view from an HTTP server bound to `127.0.0.1`, guarded by a random token.
[calibre](#port-calibre)'s viewer is the e-book reader the desktop opens by default; Arianna is
installed beside it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineQuick=ON` — Make Qt WebEngine's QML module, from
  [qt6-qtwebengine](#port-qt6-qtwebengine), a requirement. Upstream looks for it without
  `REQUIRED` but links it unconditionally, so without it the configure succeeds and the link
  fails.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON` — The same for [baloo](#port-baloo), which upstream
  looks for as an optional component and links on every platform but Windows. Through it the
  library finds books that Baloo has indexed.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The renderer and its helpers are JavaScript compiled into the program's resources as upstream
ships them: foliate-js itself, and the built `pdf.js`, `zip.js`, `fflate.js` and `jszip.min.js`
bundles it loads. They are built JavaScript, the class of payload that
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)
lets ship as released.

The HTTP server listens on port 45961 of the loopback interface only, so nothing outside the
machine reaches it; while another program holds that port the reader cannot load a book. Qt
HttpServer comes from [qt6-qthttpserver](#port-qt6-qthttpserver), which the single
`find_package(Qt6 …)` call makes a requirement with every other Qt module the program uses.

[kitemmodels](#port-kitemmodels) is in `depends` for run time: the library view imports its
`org.kde.kitemmodels` QML module, which the configure does not look for.

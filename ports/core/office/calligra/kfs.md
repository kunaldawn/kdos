## About

Calligra is KDE's office suite: Words for text documents, Sheets for spreadsheets, Stage for
presentations and Karbon for vector drawing, all on OpenDocument, with a launcher window, the
plain `Calligra` menu entry, that starts any of the four. One source tree builds the four
applications, the shared office libraries under them, import and export filters for Microsoft
Office, WordPerfect, Works, Visio, Keynote, RTF, PDF and others, thumbnailers, and Okular
generators through which Okular shows OpenDocument and Office files.
[libreoffice](#port-libreoffice) is the office suite the desktop opens by default; Calligra is
installed beside it.

## Flags

- `-D RELEASE_BUILD=ON` — Build the release configuration. Unset, upstream builds as a development
  tree: the products it marks unmaintained are compiled, among them the Braindump note-mapping
  application, and the text shape gains a document inspector, a debugging aid. With it those are
  left out and the products marked staging, such as the 3D shape, stay.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6OpenGL=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Sql=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON`: make three of the four Qt modules
  upstream looks for quietly requirements. OpenGL draws Stage's slide transitions and the Qt Quick
  document view, Sql gives Sheets its database connection, and WebEngine is the web shape that
  embeds a page in a document; each is dropped without it. The fourth, Quick Widgets, serves only
  the Gemini shell, which is not built (see Notes).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Imath=ON` — Make [imath](#port-imath) a requirement, for the
  half-float colour channels of the pigment colour library. Every switch from here to
  `CMAKE_REQUIRE_FIND_PACKAGE_Iconv` turns one of upstream's optional searches into a requirement,
  each of which would otherwise drop its feature with a line in the summary.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_GSL=ON` — [gsl](#port-gsl), for the Sheets solver.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Phonon4Qt6=ON` — [phonon](#port-phonon), for Stage's sound event
  actions and the video shape; playback runs through
  [phonon-backend-vlc](#port-phonon-backend-vlc), in `depends` for run time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KChart6=ON` — KChart from [kdiagram](#port-kdiagram), for the
  chart shape.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON` — [eigen](#port-eigen). Without it the whole of
  Sheets is dropped.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt6=ON` — [qca](#port-qca), for opening encrypted
  OpenDocument files and encrypted Excel workbooks.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LCMS2=ON` — [lcms2](#port-lcms2), for colour management.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Fontconfig=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Freetype=ON`: [fontconfig](#port-fontconfig) and
  [freetype2](#port-freetype2), which together give exact font sizes in layout; both must be
  found or neither is used.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_SharedMimeInfo=ON` — [shared-mime-info](#port-shared-mime-info),
  through which the filters tell SVM and the OOXML formats apart.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Okular6=ON` — [okular](#port-okular), against which the Okular
  generators for ODT, ODP, DOC, DOCX, PPT, PPTX, RTF and WordPerfect files are built.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibRevenge=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LibRevengeStream=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_LibOdfGen=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LibWpd=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_LibWpg=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LibWps=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_LibVisio=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LibEtonyek=ON`: the Document Liberation libraries behind the
  legacy import filters, [librevenge](#port-librevenge), [libodfgen](#port-libodfgen),
  [libwpd](#port-libwpd), [libwpg](#port-libwpg), [libwps](#port-libwps),
  [libvisio](#port-libvisio) and [libetonyek](#port-libetonyek). Each filter is dropped when any
  library it names is missing: WordPerfect to ODT, Works to ODT, WPG to SVG and ODG, Visio to ODG
  and Keynote to ODP.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PopplerXPDFHeaders=ON`: [poppler](#port-poppler)'s Qt 6 interface
  and its XPDF headers, for Karbon's PDF import. The XPDF headers are Poppler's unstable internal
  API, which the poppler port installs with `ENABLE_UNSTABLE_API_ABI_HEADERS`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Iconv=ON` — iconv, for the Word `.doc` filter's text
  conversion. musl provides it in the C library, which CMake's `FindIconv` recognises.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Cauchy=ON` — Never look for Cauchy's M2MML, the Matlab and Octave
  to MathML compiler behind one formula tool. It is not a port, and refused, a stray copy in the
  build root cannot be picked up.
- `-D CMAKE_DISABLE_FIND_PACKAGE_OOoSDK=ON` — Never look for the OpenOffice SDK, which only the
  `slidecompare` developer tool uses. It is not a port either.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Building against Okular makes [okular](#port-okular) a build dependency of this port, so Okular is
listed in `43_toolkits` rather than `44_apps`, as any application another port builds against is.

Gemini, the touch shell over Words and Stage, is not built: upstream's top-level
`CMakeLists.txt` carries its `add_subdirectory(gemini)` commented out, and `RELEASE_BUILD` turns
its product off as well. Quick Widgets and libgit2, which only Gemini and its Git plugin use, are
therefore neither required nor in `depends`; whether the configure finds them changes nothing
that is built. [kirigami](#port-kirigami) and [kirigami-addons](#port-kirigami-addons) are in
`depends` for run time: the launcher, `calligralauncher`, imports their QML modules.

The EPS import filter is built and runs `pstoedit` at run time, which upstream only reports as a
`TYPE RUNTIME` package; pstoedit is not a port, so importing an EPS file fails with a missing
program. Vc stays off: upstream builds it only with `BUILD_VC`, which this recipe never sets,
and its SIMD kernels would be compiled per CPU level.

The PDF import filter follows Poppler's internal API, which changes without notice between
releases; a Poppler bump can break this port's build while every other Poppler consumer still
builds.

The tarball is the largest Gear source, and this is the longest Gear build in the tree.

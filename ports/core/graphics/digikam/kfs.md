## About

digiKam is KDE's photo library: albums, tags, ratings, face detection and recognition, camera and
scanner import, RAW development and a full image editor. The same build produces showFoto, the
editor on its own for single files. Both are built against Qt 6 and KDE Frameworks 6, and the
collection lives in SQLite.

## Flags

- `-D CMAKE_C_STANDARD=99` — KDE's compiler settings compile C as strict ISO C99 unless a standard
  is named. The panorama tool's flex and bison parser calls `fileno` and `strdup`, which strict ISO
  mode does not declare.
- `-D CMAKE_C_EXTENSIONS=ON` — Together with the line above, this makes the C standard GNU C99
  (`-std=gnu99`), which declares both functions.
- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KF6. Without it the project looks for Qt 5.
- `-D DIGIKAMSC_COMPILE_PO=OFF` — Build no translation catalogues; `KF_SKIP_PO_PROCESSING=ON`
  does the same for the parts that go through KDE's own catalogue rules. The program shows its
  English source strings.
- `-D ENABLE_MYSQLSUPPORT=OFF` — No MySQL database back end. It needs Qt's MySQL driver, which
  [qt6-qtbase](#port-qt6-qtbase) is built without (`-DFEATURE_sql_mysql=OFF`), so the collection
  is SQLite.
- `-D ENABLE_INTERNALMYSQL=OFF` — No bundled MySQL server: it needs a `mysqld`, and no port
  provides one.
- `-D ENABLE_KFILEMETADATASUPPORT=OFF` — No link to KFileMetaData for Plasma's file indexer. This
  is upstream's default.
- `-D ENABLE_AKONADICONTACTSUPPORT=OFF` — No address-book lookups through Akonadi, which is not a
  port. This is upstream's default.
- `-D ENABLE_GEOLOCATION=ON` — The map view and the geolocation editor, through the bundled map
  engine. Its world maps are in the source tarball and draw offline; OpenStreetMap tiles need the
  network.
- `-D ENABLE_MEDIAPLAYER=ON` — The video player in the preview, through Qt Multimedia over
  [ffmpeg](#port-ffmpeg).
- `-D ENABLE_SHOWFOTO=ON` — Build showFoto beside digiKam. This and the remaining `ENABLE_`
  switches (`ENABLE_DBUS`, `ENABLE_KIO` on; `ENABLE_APPSTYLES`, `ENABLE_DIGIKAM_MODELTEST`,
  `ENABLE_SANITIZERS` off) are upstream's defaults, written out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Jasper=ON` — Every library behind an optional feature is a quiet
  find in digiKam's CMake, and a missing one builds a digiKam without that tool. Each
  `CMAKE_REQUIRE_FIND_PACKAGE_<name>=ON` turns that find into an error instead, so the package
  always has the full feature set: Jasper for JPEG 2000, X11 for the monitor colour profile under Xwayland, KIO,
  IconThemes, Notifications, NotifyConfig and Sonnet for desktop integration and spell-checking,
  ThreadWeaver and FLEX/BISON for the panorama tool, CalendarCore for the calendar tool, Qt
  Multimedia for the video player, LensFun for lens correction, Gphoto2 for camera import,
  Eigen3 for the refocus tool, ImageMagick for extra codecs, GLIB2 for the content-aware resizer,
  Libheif and X265 for HEIF, Libjxl for JPEG XL, LibXml2 and LibXslt for the HTML gallery, and
  KSaneWidgets6 for scanner import. Each of those libraries is in `depends`.

## Patches

- `ai-tools-default-off.patch` — Make the AI auto-tools, aesthetic detection and auto-tagging default to off in `systemsettings.cpp`. Left on, every start offers to download their models from files.kde.org; they can still be switched on in the settings, which then asks for the models.
- `no-online-export.patch` — Drop the export plugins whose only function is uploading to an online service (Box, Dropbox, Flickr, Google, Imgur, ImageShack, iNaturalist, MediaWiki, OneDrive, Pinterest, Piwigo, SmugMug, Twitter) from `webservices/CMakeLists.txt`. No build option switches them off. Export to a local folder and through KIO stay.

## Notes

The face engine looks for its models under `digikam/facesengine` in the XDG data directories
before it offers to download them. The three later sources are those files, installed into
`/usr/share/digikam/facesengine`, so face detection and recognition work on first start with no
network. Their names and sizes must be the ones `core/data/dnnmodels/dnnmodels.conf` lists: a file
that differs counts as missing.

`build.sh` writes both desktop entries over upstream's, with `StartupWMClass` set to the Wayland
app ids KAboutData gives the programs, `org.kde.digikam` and `org.kde.showfoto`. showFoto's entry
carries no `MimeType`: the image types already have a handler, and a second claim would make the
default whichever entry sorts first.

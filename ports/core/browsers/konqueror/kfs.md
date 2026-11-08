## About

Konqueror is KDE's original browser and file manager in one: web pages render in its
WebEnginePart, a KPart over [qt6-qtwebengine](#port-qt6-qtwebengine), and folders, archives and
remote locations open through [kio](#port-kio) and any KPart viewer installed, with tabs, split
views, a sidebar of bookmarks, history and places, and plugins for web archives, user-agent
switching, translation and search. `kfmclient` opens a URL in a running Konqueror. Passwords that
pages ask it to keep go to [kwallet](#port-kwallet).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON` — Activity awareness: windows and tabs are
  linked to the Plasma activity they were opened in, through
  [plasma-activities](#port-plasma-activities). Optional upstream and dropped quietly without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Hunspell=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_WebEngineDictConverter=ON` and `-D USE_SYSTEM_DICTIONARIES=OFF`:
  spell checking in web forms. QtWebEngine reads only its own binary `.bdic` dictionaries, so the
  build asks the `hunspell` program from [hunspell](#port-hunspell) which dictionaries are
  installed, converts each with QtWebEngine's `qwebengine_convert_dict`, and installs the results
  under `/usr/share/konqueror/webengine_dictionaries`. Upstream finds both tools quietly and skips
  the conversion without them. The dictionaries converted are [hunspell-en](#port-hunspell-en)'s,
  which `depends` names so the set is fixed.
- `-D CMAKE_DISABLE_FIND_PACKAGE_QtTextToSpeech=ON` — No text-to-speech plugin. Upstream looks
  for a package named `QtTextToSpeech`, which Qt 6 does not install (its module is
  `Qt6TextToSpeech`), and links a target spelled `Qt6::::TextToSpeech`, so the plugin cannot be
  built either way; refused, the result does not depend on a stray package of that name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The window's app_id is `konqbrowser`, set by the program, and its menu entry is
`konqbrowser.desktop`; `org.kde.konqueror.desktop` and the four `kfmclient` entries are
`NoDisplay=true` handlers. `kfmclient_dir.desktop` claims `inode/directory` and
`kfmclient_html.desktop` the web types, beside Dolphin and Firefox; the desktop's `mimeapps.list`
decides which one opens a folder or a link. `konqy_preload.desktop` is an autostart entry that
preloads a Konqueror at a Plasma login only when its setting is turned on; the default is off.

The configure probes `mallinfo`, which musl does not have; the probe fails, and nothing in the
sources reads its result. libkonq, the library the plugins link, is installed with a CMake package
(`KF6Konq`) and uses [zlib](#port-zlib)'s `crc32` for the history, which upstream requires.

## About

KGuiAddons is the KDE Frameworks library of small helpers for Qt GUI programs: colour utilities,
font helpers, `KModifierKeyInfo` for the state of the modifier keys, `KSystemClipboard`, the key
sequence recorder that shortcut editors use, and the query for whether the desktop prefers a light
or dark colour scheme. [kcolorscheme](#port-kcolorscheme), [kxmlgui](#port-kxmlgui),
[kio](#port-kio) and many KDE applications in the tree link it.

## Flags

- `-D WITH_WAYLAND=ON` — Build the Wayland code of `KModifierKeyInfo`, `KSystemClipboard` and
  `KKeySequenceRecorder`, which inhibits the compositor's shortcuts while a shortcut is being
  recorded. It is upstream's default on Linux; on, the configure requires
  [qt6-qtwayland](#port-qt6-qtwayland), [wayland](#port-wayland),
  [wayland-protocols](#port-wayland-protocols) and
  [plasma-wayland-protocols](#port-plasma-wayland-protocols), so a missing one stops it.
- `-D WITH_X11=ON` — Build the X11 code of `KModifierKeyInfo`, which a KDE program uses when it
  runs under Xwayland. On, the configure requires [libX11](#port-libX11), [libxcb](#port-libxcb)
  and a [qt6-qtbase](#port-qt6-qtbase) built with its xcb feature. Every toolkit in the tree is
  built with both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D BUILD_GEO_SCHEME_HANDLER=OFF` — Build no `kde-geo-uri-handler`, the program that opens a
  `geo:` link in a web map, and none of the desktop entries that register it for OpenStreetMap,
  Google Maps, Wheelmap and Cartes. Each of those opens an online map site, which fails offline,
  and the shipped default for `geo:` is Organic Maps, which reads its maps from the disk.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.

## Patches

`release-pending-clipboard-locks.patch` fixes a deadlock in the Wayland `KSystemClipboard`.
`mimeData()` returns data its clipboard thread owns, so it takes the shared clipboard lock for the
caller and leaves the unlock to a zero-length timer. When the event loop quits before that timer
runs, the main thread still holds the lock as the `WaylandClipboard` destructor waits for the
clipboard thread, and the thread waits for the lock: the program never exits. A `plasmashell`
quit with a Dictionary widget on the desktop hangs this way about once in forty. The patch counts
the deferred locks and releases every one before either place that joins the thread, and a timer
that runs afterwards finds nothing to release. It is to be offered upstream as a merge request
against `frameworks/kguiaddons`.

`help-local-page.patch` gives a `help:` URL a local fallback. KGuiAddons registers the handler
that every Qt program linking it uses for `help:`, the address a Help button or a Help menu opens:
it hands the URL to KHelpCenter when `khelpcenter` is on the `PATH`, and otherwise turns it into a
docs.kde.org address only for a KDE program's own handbook or System Settings' `/kcontrol` pages,
and opens nothing for any other path. The live system has no KHelpCenter, so unpatched, the Help
button of a KDOS settings page, whose `X-DocPath` names a page of the KDOS book
(`help:/kdos/04-programs/kdos-settings.html#kdos-look`), opens nothing. With the patch, when
KHelpCenter is absent, a path that names an `.html` file under `doc/HTML/<language>/` in the
data directories, in the user's language or in English, is handed to `QDesktopServices::openUrl()`
as a `file:` URL with its fragment: the book is found through the link [kdos-doc](#port-kdos-doc)
installs at `/usr/share/doc/HTML/en/kdos`. A local file with a fragment reaches Qt's `openUrl()`
path, and [qt6-qtbase](#port-qt6-qtbase)'s `openurl-local-file.patch` gives it to `xdg-open`
rather than to the desktop portal's `OpenURI`, which refuses a `file:` URI; in the Plasma session
`xdg-open` runs `kde-open`, which opens the page in the default browser at the section. A path
with a `..` component is refused, and a path ending in `/` names that directory's `index.html`. A
KDE handbook is installed as DocBook, with no `.html` file, so it still goes to docs.kde.org. It
is to be offered upstream as a merge request against `frameworks/kguiaddons`.

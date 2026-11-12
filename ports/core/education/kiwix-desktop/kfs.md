## About

Kiwix Desktop reads ZIM archives, the offline copies of Wikipedia, Wiktionary, Stack Exchange and
other sites, with full-text search, tabs and a reading list. It is a Qt 6 program that renders the
pages through QtWebEngine and reads the archives through [libkiwix](#port-libkiwix) and
[libzim](#port-libzim).

## Notes

Downloads from the online catalogue go through [aria2](#port-aria2): at every start the download
manager has [libkiwix](#port-libkiwix) launch `aria2c` with its JSON-RPC interface enabled and
drives it over that. Without `aria2c` on `PATH`, Kiwix opens a "Cannot create downloader" error
at every start and offers no downloads, so `aria2` is in `depends`.

`app-id.patch` sets the desktop file name, which Qt gives every window as its Wayland app id,
first in `KiwixApp::init()`. Upstream sets it after constructing the content manager, which can
open an error dialog of its own, and that dialog then takes the executable's name,
`kiwix-desktop`, which no entry is named after. The patch is to be offered upstream.

`build.sh` writes the desktop entry, `org.kiwix.desktop.desktop`: KWin finds a Wayland window's
icon only in the entry whose file name is the window's app id, `org.kiwix.desktop`, which is also
the name the metainfo's launchable and `/usr/share/kdos/xdg/kde-mimeapps.list` use.

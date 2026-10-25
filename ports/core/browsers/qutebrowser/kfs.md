## About

qutebrowser is a keyboard-driven web browser with Vim-like key bindings, written in Python on
PyQt6 and its Qt WebEngine bindings ([python3-pyqt6-webengine](#port-python3-pyqt6-webengine)).
The page is drawn by Qt WebEngine; the browser's own interface is a minimal one of a status bar,
a tab bar and a command line.

## Patches

- `app-id.patch` — Edits `misc/org.qutebrowser.qutebrowser.desktop`. It changes `StartupWMClass=qutebrowser` to `StartupWMClass=org.qutebrowser.qutebrowser`, the entry's own desktop id, so the panel's lookup by file name and its lookup by window class find the one entry. It cuts the `MimeType=` line down to `x-scheme-handler/qute`, qutebrowser's own scheme: upstream also claims HTML, XHTML, XML and RDF, the GIF, WebP, JPEG and PNG image types, `http` and `https`. The web types are Firefox ESR's, their default in `/etc/xdg/kdos-mimeapps.list`, and an entry claims a type only where nothing else does (see [desktop entries](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).

## Notes

`pip install` installs only the Python package and its `qutebrowser` command. Everything else is
what upstream's `misc/Makefile` installs, and `build.sh` repeats those `install` lines rather
than running the Makefile, whose `install` target goes through `setup.py install` and depends on
a rule that builds the manual page with `a2x`. The page ships built in the tarball as
`doc/qutebrowser.1`. The lines install that page, the desktop entry, the AppStream file, the
hicolor icons at the Makefile's `ICONSIZES` and as SVG, every userscript from `misc/userscripts`,
and the helper scripts from `scripts/`, under `/usr/share/qutebrowser/`. The helpers are named one
by one: they are the Makefile's set less `mkvenv.py`, which creates a local virtualenv with its
own PyQt install. A release that adds or drops a helper script, or an icon size, has to be matched
in `build.sh`.

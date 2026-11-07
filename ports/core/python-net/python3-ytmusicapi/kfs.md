## About

ytmusicapi is an unofficial Python client for YouTube Music's web API: search, artist, album and
playlist pages, the user's library and uploads. [audiotube](#port-audiotube) embeds a Python
interpreter and calls it for everything it shows. It sends its requests through
[python3-requests](#port-python3-requests), which is in `depends`.

## Notes

It takes its version through [python3-setuptools-scm](#port-python3-setuptools-scm), which reads
the `PKG-INFO` in the release archive. The package installs the `ytmusicapi` command, which
writes the browser or OAuth credentials file a script then hands to the library.

YouTube Music changes its web API without notice, and ytmusicapi follows with frequent releases;
a version a few months old may fail on pages audiotube opens.

The package carries the translated result-parsing tables under `ytmusicapi/locales/` as `.mo`
files inside its Python package. They are data the parser matches YouTube Music's localised
responses against, not interface translations, and they are not under `/usr/share/locale`.

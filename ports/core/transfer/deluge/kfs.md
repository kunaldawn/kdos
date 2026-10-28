## About
[Deluge]{.idx} is a BitTorrent client split into a daemon, `deluged`, and the interfaces that
drive it: the GTK 3 window `deluge-gtk`, the web interface `deluge-web` and the console
`deluge-console`. It is written in Python on Twisted, and does its torrent work through the
Python binding of [libtorrent-rasterbar](#port-libtorrent-rasterbar). The port installs the daemon,
the three interfaces, the `deluge` command that starts whichever interface is set as the
default, and the menu entry for the GTK window.

## Patches
- `pyopenssl-26.patch` — Rewrites `generate_ssl_keys()` in `deluge/crypto_utils.py`, which makes the daemon's key and self-signed certificate on its first start, to use the `cryptography` package instead of `OpenSSL.crypto`. The bundled pyOpenSSL has no `X509Req`, which the unpatched code builds a certificate request with, so the first `deluged` start would stop with an `AttributeError`. The key is still 2048-bit RSA, the subject `CN=Deluge Daemon` and the validity three years; the serial number is random instead of 0. The patch's header says it is not yet submitted upstream.
- `release-check-off-by-default.patch` — Sets the core preference `new_release_check` to `False`, so the daemon does not ask deluge-torrent.org for a newer release at every start. It can be turned back on in the preferences.

## Notes
- **The vendor bundle.** `vendoring = python` with an explicit `pypackages` list: Twisted and the
  libraries it needs for TLS (pyOpenSSL, service-identity, pyasn1 and pyasn1-modules), Automat,
  constantly, hyperlink, incremental and zope.interface, and rencode and setproctitle. Everything
  else Deluge imports is a port in `depends`. How the three Python keys narrow a bundle is in
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- **Build backends stay out of the package.** `hatch-fancy-pypi-readme` and `incremental` are
  Twisted's build plugins, and `rjsmin` is the minifier `minify_web_js.py` uses to build the web
  interface's minified scripts when no Closure compiler is installed. The first `pip3 install`
  puts the three into `$SRC_ROOT/backends`, on `PYTHONPATH`, so they reach the build and never
  `$PKG`. `incremental` is named again in the second install
  because Twisted imports it at run time; `--ignore-installed` makes pip install it into `$PKG`
  even though the backends directory already has it.
- **Two optional imports are left out.** `setup.py` lists `chardet` and `ifaddr` as extras.
  `ifaddr` is already in [cura](#port-cura)'s bundle, and a module two packages carry is a path
  two packages own, which kpkg refuses (see
  [Who owns a file](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file)). Without
  `chardet`, `decode_bytes()` in `deluge/common.py` tries UTF-8 and then Latin-1, and the
  tracker-icon fetcher reads a tracker's HTML page as UTF-8.
- **The bundled plugins are unpacked.** Deluge's build makes each plugin a zipped `.egg` file. The
  loop after the install unpacks each into a directory of the same name: the patched plugin scan
  finds only `.egg` directories, and `resource_filename()` in `deluge/common.py` hands a plugin's
  interface files out of a zip as a temporary copy that is gone by the time the plugin opens it.
- **English only.** Every directory under `deluge/i18n` except the English ones is removed.
- **The menu entry** is written by `build.sh`. `gtkui.py` calls `set_prgname('deluge')`, so the
  window's app_id is `deluge`, which `StartupWMClass` names. It claims no MIME type or URL
  scheme, so it is not offered as the handler for `.torrent` files or magnet links.
- The ExtJS library inside the web interface ships as upstream built it; see
  [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

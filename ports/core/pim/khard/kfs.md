## About
khard is an address book for the terminal over a vdir of `.vcf` files, one contact to a file: it
lists, searches, adds and edits contacts, and answers address queries for mail clients.
[vdirsyncer](#port-vdirsyncer) keeps the vdir equal to a CardDAV address book.

## Flags
- `vobject` — The one PyPI package in khard's
  [Python vendor bundle](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys),
  pinned in `pypackages` and installed into the package beside khard; the rest of what khard
  imports is `python3-*` ports in `depends`.
- `-D` — Overrides `extensions` in khard's Sphinx `conf.py` for the manual page build, leaving
  out `autoapi.extension` (sphinx-autoapi), which has no port. `sphinxarg.ext` stays: the
  [python3-sphinx](#port-python3-sphinx) port bundles sphinx-argparse.

## Notes
The manual pages are built against the `site-packages` in `$PKG` (`PYTHONPATH=$_site`), where
khard and `vobject` are installed, because autodoc and sphinx-argparse import them. The build
writes `khard.1` and `khard.conf.5`, installed by glob into sections 1 and 5.

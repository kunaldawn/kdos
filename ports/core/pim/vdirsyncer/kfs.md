## About
vdirsyncer makes a remote CalDAV or CardDAV collection and a local vdir equal, in both directions,
the way `mbsync` does it for mail. It is how [khal](#port-khal)'s calendars and
[khard](#port-khard)'s address books reach a server.

## Flags
- `--target` — The first `pip3 install` puts `expandvars` and `hatch-fancy-pypi-readme`, two
  build backends the bundle's own packages need, into `$SRC_ROOT/backends` and onto
  `PYTHONPATH`, never into the build root, where their files would belong to no package. See
  [The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- `aiohappyeyeballs` — The second `pip3 install` names the bundle's runtime members before `.`:
  `aiohappyeyeballs`, `aiohttp`, `aiosignal`, `aiostream`, `frozenlist`, `multidict`,
  `propcache`, `tenacity` and `yarl`, vdirsyncer's asynchronous HTTP stack. Each is built from
  its source distribution, the C extensions of `aiohttp`, `frozenlist`, `multidict`, `propcache`
  and `yarl` included, and installed into this package. The modules vdirsyncer shares with other
  programs, such as `click-log` and `requests`, are `python3-*` ports in `depends`.

## Notes
Five of the bundled modules carry C, so the port compiles them; `frozenlist`, `propcache` and
`yarl` build through a PEP 517 backend shipped inside their own sdists, which needs `expandvars`.
Two members are pinned in `pypackages`, each below an upper bound another package declares:
`multidict` below the bound in `aiohttp`'s requirements, and `aiostream` below the bound in
vdirsyncer's own. A version bump that moves either bound must move its pin with it.

The manual page is built with `PYTHONPATH` pointing at the `site-packages` in `$PKG`, where
vdirsyncer and its bundle are installed.

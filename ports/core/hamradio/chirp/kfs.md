## About

CHIRP reads, edits and writes the memory channels of handheld and mobile amateur radios from
hundreds of models, over a programming cable. This port installs the wxPython program `chirp`
and the command-line `chirpc`, built from a dated snapshot of upstream's tree.

## Flags

- `--no-build-isolation` — Build each source distribution with the setuptools from the
  [python3-setuptools](#port-python3-setuptools) port, instead of letting pip create an isolated
  environment and download a build backend into it.

## Patches

- `no-reporting.patch` — Set `DISABLED = True` in `chirp/wxui/report.py`. That module posts every radio model CHIRP opens, with a per-install UUID, to chirpmyradio.com, and asks the same server for a newer release; disabled, it does neither. The repeater-directory queries stay, as menu actions the user starts.
- `version.patch` — Write the snapshot's date into `CHIRP_VERSION` in `chirp/__init__.py` (as `next-<date>`) and into `setup.py`, which otherwise say `py3dev` and `0`.

## Notes

The vendor bundle is the `pypackages` closure: the two distributions CHIRP needs that no port
provides, `yattag` and `suds-community`, as source distributions. `pypackages` is the closure
itself, so `ports/fetch` downloads exactly those two without resolving their dependencies. pyserial, requests, lark and wxPython come
from their `python3-*` ports in `depends`. The `pip3 install` line names both bundled
distributions and the program itself, installed `--no-deps` from `vendor/` with no index (see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).

`version.patch` carries the version, and the two `grep` lines after the patches check that it
names `$version`. A version bump must rewrite the patch, or the build stops there.

By default on Linux CHIRP sets `GDK_BACKEND=x11` before it imports wx, so it runs under
Xwayland, and GTK's X11 class is the program name capitalised, `Chirp`, which is the entry's
`StartupWMClass`. On its first run CHIRP offers to write `~/.local/share/applications/chirp.desktop`,
which would shadow the packaged entry with one that runs `~/.local/bin/chirp` or, where that
does not exist, `python3 -mchirp.wxui`; the entry's `--no-install-desktop-app` stops the offer.

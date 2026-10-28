## About
khal is a calendar for the terminal over a vdir: a directory of `.ics` files, one event to a file.
`khal` lists and adds events from the command line and `ikhal` is its interactive view; the
package adds a desktop entry, "Calendar", for `ikhal`. [vdirsyncer](#port-vdirsyncer) keeps the
vdir equal to a CalDAV calendar, and the panel's calendar, `kdos-cal`, asks `khal` for the
events it marks.

## Flags
- `icalendar` — The three PyPI packages named before `.` on the `pip3 install` line, `icalendar`,
  `tzdata` and `tzlocal`, are the recipe's `pypackages`: the runtime members of khal's
  [Python vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys),
  installed into the package beside khal itself. Every other module khal imports is a
  `python3-*` port in `depends`.
- `-D` — Overrides `extensions` in khal's Sphinx `conf.py` for the manual page build, to
  `sphinx.ext.autodoc` and `sphinx.ext.todo`. Upstream's list adds `sphinx.ext.intersphinx`,
  which fetches other projects' inventories over the network, and `sphinxfeed`, which has no port.

## Notes
`sphinx-build` runs with `PYTHONPATH` set to the `site-packages` inside `$PKG`, because autodoc
imports khal and its bundled modules, and those are installed only there.

A module in the bundle belongs to this package. A second program that needs `icalendar`,
`tzdata` or `tzlocal` cannot bundle its own copy, because two packages owning one path is
refused at install; such a module becomes a `python3-*` port of its own.

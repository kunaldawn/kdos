## About

KHelpCenter is the program behind every KDE application's Help menu: it opens `help:/` URLs, lists
the installed handbooks by category beside manual pages, info pages and the System Settings and
KIO worker documentation, and searches them. Pages render in a
[qt6-qtwebengine](#port-qt6-qtwebengine) view, which upstream requires. Full-text search runs
`khc_xapianindexer` to build a [xapian-core](#port-xapian-core) index of the handbooks, whose HTML
it reads with [libxml2](#port-libxml2), and `khc_xapiansearch` to query it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Handbooks are DocBook that [kdoctools](#port-kdoctools) renders when a `help:/` page is opened, so
what KHelpCenter lists is whatever each port built with its handbook on. `man:/` and `info:/` pages
are served by workers from [kio-extras](#port-kio-extras), which the build does not check and
`depends` names for run time.

The manual-page search is `khc_mansearch.py`, run with [python3](#port-python3), which calls
`whatis -w` and `apropos -L <lang> --and`. Those are man-db's options. This tree's `apropos` and
`whatis` are [mandoc](#port-mandoc)'s, whose `whatis -w` prints file names instead of matching
wildcards and whose `apropos` refuses `-L`, so the manual-page search returns nothing while
handbook search and browsing work. The entry is `OnlyShowIn=KDE`, so it is listed only in a
Plasma session.

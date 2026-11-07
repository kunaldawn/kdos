## About

Sweeper removes the traces a KDE session leaves: the recent documents and recent applications
lists, which it clears through the activity manager's usage statistics
([plasma-activities-stats](#port-plasma-activities-stats), which upstream requires), the Run
Command history, Klipper's saved clipboard, the thumbnail cache, and the web history, cache,
cookies, cookie policies, form completions and favicons that KDE's own browser components keep.
Each kind is a checkbox, and nothing is removed until it is ticked.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Sweeper clears only KDE's own stores; Firefox's and Thunderbird's histories and caches are
outside its reach. The usage statistics are held by the activity manager daemon of the Plasma
session, so the two recent lists are cleared only while it runs.

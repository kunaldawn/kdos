## About

KDominate is a tactical board game in the style of Ataxx: each move clones a piece to a
neighbouring square or jumps it two squares away, and every opposing piece next to where it lands
changes sides. It is played against a computer opponent, by two people at one screen, or by two
computer players while the person watches.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Upstream builds its unit tests only with `BUILD_TESTING`, which is off, so `Qt6Test` is not needed.
The tarball carries no translated handbook.

The window's Wayland app_id is `org.kde.kdominate`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.

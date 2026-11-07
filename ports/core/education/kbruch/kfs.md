## About

KBruch is KDE Education's fraction trainer. It sets exercises on adding, subtracting,
multiplying and dividing fractions, comparing them, converting between fractions and decimals,
mixed numbers, percentages and factorising numbers, checks each answer and keeps a score. It is a
plain Qt Widgets program.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream marks it optional and, without it, skips the
  `doc/` directory and the handbook with it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`KAboutData` gives the window the app_id `org.kde.kbruch`, the name of upstream's entry. The
entry's `Exec` passes `-qwindowtitle %c -qwindowicon kbruch`, Qt's own options, which set the
title and the icon the window starts with.

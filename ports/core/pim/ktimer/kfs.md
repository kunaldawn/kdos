## About

KTimer runs jobs after a delay: each job is a countdown and a command, run through
[kio](#port-kio) when the time is up, or a notification when no command is given. Jobs can loop
and be chained, and the window hides to the system tray.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

A job's command is whatever the person types, run as that person; KTimer adds nothing to it.

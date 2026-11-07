## About

Akonadi Console is the management and debugging tool for the Akonadi PIM server: it lists the
agents and resources and their state, browses collections and items down to their raw payload,
queries the search index, and shows the server's database, jobs, notifications and debug output
live. It is how a broken KMail or KOrganizer setup is diagnosed.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) is a framework here and installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

On Linux the console requires most of the PIM stack: [akonadi](#port-akonadi),
[akonadi-mime](#port-akonadi-mime), [akonadi-contacts](#port-akonadi-contacts),
[calendarsupport](#port-calendarsupport), [libkdepim](#port-libkdepim),
[messagelib](#port-messagelib)'s message viewer, and [akonadi-search](#port-akonadi-search) with
[xapian-core](#port-xapian-core) for the search pages, so it builds after the PIM libraries and
is listed with the PIM applications. `Qt6::Test` is required even with `BUILD_TESTING=OFF`; it is
part of qt6-qtbase.

The console talks to a running Akonadi server. While none runs, its agent and debug pages are
covered by Akonadi's error overlay, which offers to start the server; no Akonadi server starts by
itself in the default KDOS session.

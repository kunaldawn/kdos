## About

KDAV is the KDE Frameworks client for WebDAV collections and the two protocols built on them,
CalDAV for calendars and CardDAV for address books, as `KJob`s: discovering a server's
collections, listing and fetching items, and creating, changing and deleting them with the
conflict detection that ETags give. It also reads the WebDAV Push support a server advertises and
registers and removes a collection's push subscription. The DAV resource of
[kdepim-runtime](#port-kdepim-runtime), which keeps Akonadi in step with a CalDAV or CardDAV
server, requires it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.

## Notes

The jobs speak HTTP through Qt's own network module from [qt6-qtbase](#port-qt6-qtbase), not
through [kio](#port-kio), so the library links no KIO and credentials reach it from its caller, as
the user name and password of each collection's URL. A TLS certificate error fails the job unless
the program has installed a `KDAV::DavSslUiProxy` that asks the user and accepts it. The
configure asks for Qt's test module whatever `BUILD_TESTING` says; it is part of
[qt6-qtbase](#port-qt6-qtbase) and adds nothing to `depends`.

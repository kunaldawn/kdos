## About

kdepim-runtime holds the Akonadi resources and agents, the programs the Akonadi server from
[akonadi](#port-akonadi) starts to reach where mail, contacts and calendars actually are. The
resources are IMAP, POP3, Maildir and mixed Maildir, mbox, local vCard and iCalendar files and
folders, CalDAV and CardDAV through [kdav](#port-kdav), Google contacts and calendars through
[libkgapi](#port-libkgapi), Microsoft Exchange (EWS), Open-Xchange and a birthdays calendar built
from the address book. The agents send queued mail through [kmailtransport](#port-kmailtransport)
(the mail dispatcher), announce new mail (the new-mail notifier) and migrate old settings. The
port also installs the `akonadi:/` KIO worker, the LDAP servers settings module, and the
kmailtransport plugin that hands a message to a resource that sends it itself. Without this port
[kmail](#port-kmail), [korganizer](#port-korganizer), [kaddressbook](#port-kaddressbook),
[merkuro](#port-merkuro) and [zanshin](#port-zanshin) have no data source; each of them names it
in `depends`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6TextToSpeech=ON` — the new-mail notifier can speak a line
  naming a new message's sender and subject, through [qt6-qtspeech](#port-qt6-qtspeech), which
  upstream looks for without `REQUIRED`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON` — the EWS resource for Microsoft
  Exchange is built only when [qt6-qtwebengine](#port-qt6-qtwebengine) is found, which shows the
  Microsoft 365 OAuth2 sign-in page; without the requirement a missing Qt WebEngine drops the
  resource with no error.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonAkonadi=ON` — the IMAP resource links
  [pimcommon](#port-pimcommon)'s `PimCommonAkonadi` library for its folder access-control
  attributes. Upstream finds it without `REQUIRED` but links it unconditionally, so a missing one
  fails the generate step instead of the search; required, the configure names it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: the configuration dialogs of the IMAP,
  POP3, Maildir, mbox, vCard, iCalendar and contacts resources can tie the resource to Plasma
  activities. Upstream searches for pimcommon's `PimCommonActivities` quietly, and for
  [plasma-activities](#port-plasma-activities) only once that library is found; both are required,
  and the configure stops when pimcommon is built without activities.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Etebase=ON` — no EteSync resource. It needs libetebase, a Rust
  library that is not a port; refusing the search keeps the resource off whatever the build root
  holds, so a later libetebase port changes this build only through this recipe.

## Notes

Three packages need no flag, because upstream already stops the configure without them:
[qca](#port-qca) is found with `REQUIRED` and gives the EWS resource its certificate-based
(PKeyAuth) sign-in; [cyrus-sasl](#port-cyrus-sasl), for POP3 authentication, and
[qtkeychain](#port-qtkeychain), through which every resource stores its password with the Secret
Service, are marked `TYPE REQUIRED` for the feature summary. Qt's `NetworkAuth` module, from
[qt6-qtnetworkauth](#port-qt6-qtnetworkauth), is a `REQUIRED` component and carries the EWS
OAuth2 sign-in.

Every resource is a separate program that the Akonadi server starts when a resource of that type
is configured, so nothing in this package runs until the user adds an account. No systemd unit and
no autostart entry is installed; the server itself is started by the applications that need it.

The resources' desktop entries under `/usr/share/applications` are all `NoDisplay=true`, so no
resource is offered in a menu.

`kdepim-mime.xml` adds KDE PIM's own MIME types to `/usr/share/mime/packages`, and installing the
package regenerates the shared MIME database (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).

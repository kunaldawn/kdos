## About

Plasma Welcome Center is the tour a person sees at their first Plasma login: what the desktop is,
how to reach settings and software, and the distribution's own pages. A module of the KDE daemon,
`kded_plasma_welcome`, opens it when `LastSeenVersion` in `plasma-welcomerc` is empty, opens its
post-update page after a significant Plasma upgrade unless `ShowUpdatePage=false`, and opens it at
every login when `LiveEnvironment=true`, the live-medium form, whose `LiveInstaller` key names the
installer's desktop entry.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — No KUserFeedback. The Welcome Center
  looks for it as a run-time dependency of its "Share Anonymous Usage Information" page, which
  loads the `kcm_feedback` settings module and is shown only when that module exists. KDOS sends
  no telemetry, KUserFeedback is not a port, and refusing the search means a stray copy in the
  build root cannot be picked up.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Distribution pages are QML files in `/usr/share/plasma/plasma-welcome/extra-pages/`, shown in
file-name order, and the first page's extra text, icon, icon caption and icon link are the `Name`,
`Icon`, `Comment` and `URL` keys of `/usr/share/plasma/plasma-welcome/intro-customization.desktop`.
Both paths are compiled in from the `DISTRO_CUSTOM_PAGE_FOLDER` and `DISTRO_CUSTOM_INTRO_FILE`
cache variables, whose defaults are the paths KDOS's own pages are installed to, so the recipe
leaves them as they are.

`early-distro-pages.patch` shows the distribution pages named `00-` to `09-` straight after the
welcome page, before Plasma's own pages, and leaves every other distribution page where upstream
puts it, before the last page. KDOS's Accessibility page is `00-Accessibility.qml` (from
[kdos-welcome](#port-kdos-welcome)), so larger text, contrast, reduced motion and the screen reader
are offered before anything else is asked; upstream shows distribution pages only after its own
five. The patch documents the rule in the README's distribution-page section.

`shortcut-texts.patch` adds `Utils.globalShortcutTexts(component, action)`, which returns every
chord bound to a global action, where upstream's `globalShortcutText` returns only the first with a
modifier. kglobalaccel hands an action's chords back in no stable order, so "the first" names a
different chord from one session to the next; the KDOS Keys page reads them all and shows the one
it teaches while that one is bound. It is to be offered upstream.

The post-update page offers to download a preview of the Plasma release announcement from
kde.org. It does so only when the person asks, because `AutomaticUpdatePreview` defaults to
false; every other link on the pages opens in the web browser.

[kded](#port-kded) is in `depends` for run time: the module that decides when to open the Welcome
Center is a plugin that `kded6` loads at login. So are the ports whose QML modules the pages
import and the build does not check for: the cards and the mock desktop draw with
[qt6-qt5compat](#port-qt6-qt5compat)'s graphical effects, the mock clock reads the time through
[plasma5support](#port-plasma5support), the mock KRunner draws its results with
[milou](#port-milou), and the Network page lists networks through
[plasma-nm](#port-plasma-nm). Without plasma-nm the Network page shows a notice that Plasma's
networking support is not installed; without one of the others a page fails to load.

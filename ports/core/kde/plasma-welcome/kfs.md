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

`description-focus.patch` draws a frame in the theme's focus colour round a page's description
while a Tab or Backtab has put the focus there. Upstream makes the description focusable so a
screen reader reads it, and it is the first Tab stop of every page, Plasma's and the distribution's
alike, but it draws no focus: without the frame the first Tab seems to do nothing. A click,
which also focuses it, draws no frame: the label decides when it gains focus, from whether a press
of any button is held on it at that moment, and keeps no mark from a press that gave no focus, so
the next Tab is framed whichever button was pressed before it. Qt 6.12 focuses the label on a
right or middle press as well as a left one. It is to be offered upstream.

`kdos-powerful-cards.patch` changes two things on the "Powerful When Needed" page. Its Keyboard
Shortcuts card lists the Meta chords from `globalShortcutTexts` for each action it teaches (the
launcher, quick tiling, the task manager's entries, Spectacle's region recording, the lock screen,
the screen reader, panel focus, System Monitor, the emoji selector and Show Desktop), and leaves
out a row whose action has no chord; upstream's list is fixed text, and names `Meta+Shift+S` for
the screen reader, which on KDOS is a region screenshot, while the screen reader is `Meta+Alt+S`.
A row keeps only the chords that start with Meta, the key the card is about, so `Print` and
`Alt+F1` are left out while the launcher's row keeps the Meta key alone; the chords are sorted,
shortest first, because kglobalaccel returns them in no stable order and the row would otherwise
change between sessions. The task manager row is the run of entries 1 to 9 (`Meta+1 to Meta+9`):
entry 10 is `Meta+0`, which would end the run on a key that reads as lower than its start. The page also loses its Activities card, which teaches the activity switcher's
chord: KDOS ships no activities interface and leaves that action unbound, so the card would name
an unassigned shortcut. The chord-driven list is to be offered upstream; the card's removal is
KDOS's alone.

`release-notes-pages.patch` makes two changes for an entry that opens one page, as KDOS's Release
Notes runs `plasma-welcome --pages 90-WhatsNew.qml`. Welcome Center is a unique D-Bus application:
a second start hands its arguments to the running one and exits, and upstream's running window
only raises itself and drops them, so with the tour open the entry would bring the tour forward
and show no notes. With the patch the running window reads the second start's `--pages`, names
each page as the first start does, and shows them as layers over its own pages, so Back returns
to where it was; asking again replaces the layers, and a window that is itself a run of exactly
those pages is only raised. The post-update page's footer has only OK, which closes the window,
so the patch gives it a Back button while a layer is open; without one, notes opened over the
Plasma update page would leave no way back to it. And a run of one page with no layer open hides Skip and the page
indicator, so Finish is its one exit, where upstream offers Skip beside Finish and a single dot.
It is to be offered upstream.

`mock-wallpaper.patch` draws the mock desktops of Simple by Default, Powerful When Needed and the
post-update page on the wallpaper of the global theme in use, where upstream hard-codes Plasma's
`Next`, so the KDOS tour would show a desktop KDOS does not have. A new `App.defaultWallpaper`
reads `LookAndFeelPackage` from `kdeglobals`, that package's `defaults` (`[Wallpaper] Image`, which
each KDOS accent's global theme sets to its own `KDOS-<Accent>` package), and picks the package's
largest landscape image, from `images_dark` under a dark colour scheme as Plasma's image wallpaper
does; a theme that names no installed wallpaper keeps `Next`. It is to be offered upstream.

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

## About

libplasma is the library every piece of the Plasma shell is built on. It defines applets
(plasmoids), the containments that hold them (desktops and panels) and the corona that arranges
containments on the screens, and loads them as KPackage packages of QML. Its `PlasmaQuick` library
gives them windows: panels, popups and dialogs. Its QML modules, `org.kde.plasma.core`,
`org.kde.plasma.components`, `org.kde.plasma.extras` and `org.kde.plasma.plasmoid`, are what a
plasmoid is written in, and the Plasma style it draws with is a desktop theme of SVG pieces:
Breeze, installed under the id `default` and drawn in the colours of the colour scheme, is
installed here with Breeze Light and Breeze Dark, which reuse its SVGs with fixed colours;
[oxygen](#port-oxygen) adds Oxygen and Air.

[plasma-workspace](#port-plasma-workspace), [plasma-desktop](#port-plasma-desktop),
[kwin](#port-kwin) and [kdeplasma-addons](#port-kdeplasma-addons) link it, and so do the Gear
applications that ship a widget beside their window, such as kclock and kweather.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of its messages, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Upstream's defaults are the ones KDOS wants, so no other switch is passed. `ENABLE_ACTIVITIES`
is on by default and makes [plasma-activities](#port-plasma-activities) a requirement, so a
containment can belong to an activity. `GZIP_DESKTOPTHEME_SVG` is on by default: the theme's SVGs
are installed compressed as `.svgz`, made with `gzip -n` from [gzip](#port-gzip), which stores no
name and no time, so the package is the same on every build. `BUILD_EXAMPLES` is off by default.

The configure checks for four QML modules it needs at run time and stops when one is missing:
`Qt5Compat.GraphicalEffects` from [qt6-qt5compat](#port-qt6-qt5compat), `org.kde.config` from
[kconfig](#port-kconfig), `org.kde.kirigami` from [kirigami](#port-kirigami) and `org.kde.ksvg`
from [ksvg](#port-ksvg).

The library uses Qt's private GUI API, which carries no compatibility promise between Qt
releases, so it is rebuilt with every Qt bump; plasmoids are QML and load against
whichever release is installed. The plasmoid and wallpaper templates go to
`/usr/share/kdevappwizard/templates`, where KAppTemplate and KDevelop list them.

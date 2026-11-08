## About

Kolf is a miniature golf game: courses of holes seen from above, with slopes, sand, water, bridges
and windmills, for up to ten players taking turns, and an editor for making courses. It is a Qt 6
and KDE Frameworks application built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the theme's SVG to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

The ball physics is a copy of Box2D in `src/3rdparty`, compiled into the program: revision 141 of
Box2D's tree, with `qreal` in place of its single-precision floats. It has no switch for an outside
Box2D, and the current library's interface is not the one this copy has. Its zlib licence sits in
that directory, where kpkg's licence copy does not look, so `build.sh` installs it as
`/usr/share/licenses/kolf/Box2D-COPYING`. The rest of the program is GPL, the `tagaro` scene code
LGPL 2.0 only.

The desktop entry opens course and saved-game files, `application/x-kourse` and
`application/x-kolf`, two types the KDE MIME definitions of [kcoreaddons](#port-kcoreaddons)
declare.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.kolf.desktop`. `KAboutData` derives the window's Wayland
app id, `org.kde.kolf`, from the component name and the `apps.kde.org` home page, so it is the
entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

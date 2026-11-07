## About

KTuberling, "Potato Guy", is a picture-stamping game for young children: objects are dragged from
a tray onto a playground, a potato, a pizzeria, the Moon, a train valley or one of six more, and
each object says its name as it lands. A finished picture can be saved, reopened or printed. It is
a Qt 6 and KDE Frameworks application built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The spoken object names are recorded sound files, one directory per language under
`/usr/share/ktuberling/sounds`, and the Speech menu chooses among them. They are data the program
plays, not message catalogues, so the language rule keeps all of them, and they are most of the
package's size. They play through [qt6-qtmultimedia](#port-qt6-qtmultimedia).

The desktop entry opens saved pictures, `application/x-tuberling`, a type the KDE MIME definitions
of [kcoreaddons](#port-kcoreaddons) declare.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.ktuberling.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.ktuberling`, from the component name and its default `kde.org` home page,
so it is the entry's file name and Plasma pairs the window with the entry without a
`StartupWMClass`.

## About

FrameworkIntegration ties KDE Frameworks into the desktop a program runs on: `KStyle`, the base
of KDE widget styles such as [breeze](#port-breeze)'s, and the integration plugin through which
the Frameworks read desktop-wide settings (`KMessageBox`'s "do not ask again" answers, for one) and
report through [knotifications](#port-knotifications). It also carries KPackage's install handlers,
which KDOS does not build: the KDE Store handler would fetch a `kns://` link past the kiosk key that
keeps KNewStuff off.

## Flags

- `-D BUILD_KPACKAGE_INSTALL_HANDLERS=OFF` — Leave out `knshandler` and `appstreamhandler`, the
  KPackage install handlers. `knshandler` installs a KDE Store entry from a `kns://` link through
  KNewStuff's engine, which does not read `[KDE Action Restrictions] ghns`, so with it a store
  download would remain on a machine where `ghns=false` (KDOS's `/usr/share/kdos/xdg/kdeglobals`)
  hides every other one; `appstreamhandler` installs through PackageKit, which KDOS does not have.
  Off, the configure needs neither [knewstuff](#port-knewstuff) nor [kpackage](#port-kpackage) nor
  [ki18n](#port-ki18n), and looks for neither PackageKit-Qt nor AppStreamQt, so neither can be
  picked up from the build root.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips compiling translation catalogues. This source carries
  none, so the switch has nothing to skip here; it is passed as in every KDE recipe, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove a non-English catalogue anyway.

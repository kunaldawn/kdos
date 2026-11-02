## About

KNotifyConfig is the settings widget for a program's notifications: for each event in its
`.notifyrc` file, whether it shows a popup, plays a sound, marks the taskbar entry, logs to a file
or runs a command, with a button that previews the sound. The notification pages of System Settings
and the "Configure Notifications" dialogs of [konsole](#port-konsole),
[konversation](#port-konversation), [kdevelop](#port-kdevelop) and the PIM applications are built
on it.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_Canberra=ON` — Preview sounds through Qt Multimedia, from
  [qt6-qtmultimedia](#port-qt6-qtmultimedia), rather than libcanberra. Upstream uses libcanberra
  whenever it finds it, and it always would here, because [knotifications](#port-knotifications)
  depends on [libcanberra](#port-libcanberra); with it refused, the configure requires
  Qt6Multimedia instead. The switch fixes the preview backend in the recipe, so it does not follow
  whichever of the two libraries the build root holds. Only the preview button is affected: the
  notification itself is played by knotifications.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

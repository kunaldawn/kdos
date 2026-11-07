## About

KParts is KDE's component framework: a part is a plugin that views or edits one kind of document
inside another application's window, with its own actions merged into the host's menus through
[kxmlgui](#port-kxmlgui). Okular's viewer inside other programs and Konsole's terminal inside
Dolphin and Kate are parts. A part is found among the plugins in the `kf6/parts` namespace by the
MIME types its plugin metadata declares, which [kcoreaddons](#port-kcoreaddons) reads, and a
read-only part opens a remote URL by copying it to a local file through [kio](#port-kio).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

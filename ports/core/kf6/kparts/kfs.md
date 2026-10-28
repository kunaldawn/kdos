## About

KParts is KDE's component framework: a part is a plugin that views or edits one kind of document
inside another application's window, with its own actions merged into the host's menus through
[kxmlgui](#port-kxmlgui). Okular's viewer inside other programs and Konsole's terminal inside
Dolphin and Kate are parts. A part is found among the plugins in the `kf6/parts` namespace by the
MIME types its plugin metadata declares, which [kcoreaddons](#port-kcoreaddons) reads, and a
read-only part opens a remote URL by copying it to a local file through [kio](#port-kio).

## About

Qrca reads QR codes and barcodes through the camera and makes QR codes from text. A scanned code
is acted on by its content: a URL opens, a contact (vCard or MeCard) is imported through
[kcontacts](#port-kcontacts), and a Wi-Fi code joins the network it names. It is a Kirigami
application; the camera is read with [qt6-qtmultimedia](#port-qt6-qtmultimedia) and the codes
are decoded and drawn by [prison](#port-prison).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6NetworkManagerQt=ON` — Joining a Wi-Fi network from its code,
  through [networkmanager-qt](#port-networkmanager-qt). Upstream asks for it as an optional
  component, and without it leaves out the Wi-Fi action and the hidden `org.kde.qrca.wifi.desktop`
  entry that the network applet starts; required, a missing one stops the configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The scanner page imports `org.kde.prison.scanner`, which prison builds because it is built
against Qt Multimedia. A Wi-Fi code is joined through NetworkManager over D-Bus, so it works only
while NetworkManager runs and polkit lets the person add a connection.

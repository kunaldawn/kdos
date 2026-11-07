## About

Skanpage scans documents: it drives a scanner through SANE by way of
[ksanecore](#port-ksanecore), collects the pages, lets them be reordered, rotated and cropped with
[kquickimageeditor](#port-kquickimageeditor), recognises their text with
[tesseract](#port-tesseract) over [leptonica](#port-leptonica), and saves them as images or as a
multi-page PDF with a text layer, which can be shared through [purpose](#port-purpose). It is a
Kirigami application.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Qt PDF, which shows the saved PDF, is part of [qt6-qtwebengine](#port-qt6-qtwebengine) and
upstream requires it. The configure asks for KSaneCore at the version of its own release branch,
26.07 for a 26.08 release, so the two move together. Recognition runs in the languages whose
tessdata files are installed, which in this tree is English alone
([tessdata-eng](#port-tessdata-eng)).

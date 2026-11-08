## About

KHealthCertificate decodes and verifies digital health certificates: the EU Digital COVID
Certificate, SMART Health Cards, the Dutch CoronaCheck format, India's DIVOC and ICAO visible
digital seals. It reads the certificate out of its barcode payload, checks the issuer's signature
against the public keys it carries, and offers the result to C++ and, through the QML module
`org.kde.khealthcertificate`, to QML. KDE Itinerary keeps such certificates beside a trip's
tickets with it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Signatures are checked with [openssl](#port-openssl), and the EU certificate's payload is
inflated with [zlib](#port-zlib); the configure requires both. Verification uses only the issuer
keys built into the library, so it reaches no network.

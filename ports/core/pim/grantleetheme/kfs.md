## About

GrantleeTheme loads and renders the HTML themes the KDE PIM programs draw with: KMail's message
header styles, KAddressBook's contact view and Akregator's article view are
[ktexttemplate](#port-ktexttemplate) templates packaged as themes, and this library finds them,
fills them in and offers the theme menu. Its "Get New Themes" entry is a KNewStuff download.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[knewstuff](#port-knewstuff) is required by the configure for the theme download dialog. The
system-wide `ghns=false` restriction greys it out, so the themes a program offers are the ones
installed with it (see
[Offline by construction](../../../../docs/kdos/01-philosophy/principles.md#offline-by-construction)).

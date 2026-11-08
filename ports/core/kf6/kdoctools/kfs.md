## About

KDocTools turns the DocBook XML handbooks of KDE programs into HTML and manual pages. Its
`meinproc6` command transforms a handbook with the KDE stylesheets over
[libxslt](#port-libxslt), and `checkXML6` validates one against the DocBook DTD. It also carries the
CMake macros `kdoctools_create_handbook()` and `kdoctools_create_manpage()` that a KDE project
calls, so a KDE application that installs a handbook needs this port at build time. A recipe can make it a requirement with
`-DCMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON`, so that a missing one stops the configure.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch does not reach
  `kdoctools_install()`, which still builds the translated manual pages of `checkXML6`,
  `meinproc6`, `kf6options` and `qt6options`; the same rule removes them from the package.

## Notes

The configure looks up the DocBook 4.5 DTD of [docbook-xml](#port-docbook-xml) and the stylesheets
of [docbook-xsl](#port-docbook-xsl) in their installed directories and writes their paths into the
files it installs (the KDE DocBook DTD, `kdedbx45.dtd`, and the stylesheets that import DocBook
XSL's), so both have to be installed before this port builds. It also needs `xmllint`
from [libxml2](#port-libxml2) and stops without it. [perl](#port-perl) and
[perl-uri](#port-perl-uri) are in `depends` because the configure URI-encodes the DTD directory by
running `perl -MURI::Escape`.

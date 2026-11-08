## About

Syndication is the KDE Frameworks feed parser. It reads RSS 0.9 to 2.0, RDF (RSS 1.0) and Atom
documents into one abstract model of a feed and its items, enclosures, people and categories.
`Syndication::Loader` takes a feed's bytes from a `Syndication::DataRetriever` that the program
supplies, and parses them as whichever format they turn out to be. [akregator](#port-akregator),
[alligator](#port-alligator), [kasts](#port-kasts) and the RSS plugin of
[ktorrent](#port-ktorrent) build on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The tarball carries no `po/` directory and its CMake files do
  not read the switch, so the configure reports it as unused and it changes nothing in this build.
  Every KDE recipe in the tree passes it, so a later release that ships translations builds none
  of them without a recipe change; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The parser is built on Qt's XML module from [qt6-qtbase](#port-qt6-qtbase), and
[kcodecs](#port-kcodecs) resolves the HTML character entities feeds put in their text. The library
carries no network code: every fetch is the program's own retriever, so nothing here reaches the
network or polls a feed by itself.

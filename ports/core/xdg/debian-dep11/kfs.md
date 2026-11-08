## About
Debian trixie's AppStream catalogue, the DEP-11 `Components-amd64.yml.gz` of its `main`
archive, installed as `/usr/share/swcatalog/yaml/debian-trixie-main.yml.gz`. It describes every
Debian desktop application by name, summary, description, categories and package, and nothing
reads it but [kdos-discover](#port-kdos-discover): the catalogue's applications take their
descriptions from it, and Discover's Debian lane (origin `kdos-store`) searches it for the
applications no catalogue row names, which kdos-appbox builds into a box on request. Discover
reads it through libappstream's OS catalogue with no network.

## Notes
- **The source is a dated snapshot.** `deb.debian.org`'s file changes with every point release,
  so the URL names `snapshot.debian.org`'s copy of the archive as of the `version` date, which
  never changes and which the recipe's sha256 pins. A newer catalogue is a new `version` date and
  its sha256, then `ports/publish debian-dep11`.
- **Its icons and screenshots are not installed.** The catalogue names them under
  `MediaBaseUrl`, on `appstream.debian.org`; kdos-discover shows only a picture that is a file on
  the machine, so a Debian application shows its stock icon or a generic one, and no screenshot,
  rather than a download nobody asked for.

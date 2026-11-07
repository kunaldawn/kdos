## About

akonadi-search is full-text search over the data in [akonadi](#port-akonadi). The indexing agent,
`akonadi_indexing_agent`, writes mail, contacts and calendars into [xapian-core](#port-xapian-core)
databases as they change; the search plugin answers the Akonadi server's searches from them; the
libraries let KMail, KAddressBook and Merkuro query them directly. It also carries a KRunner plugin
that finds contacts from the search field, with its settings page, and the debug dialog
akonadiconsole opens.

## Flags

- `-D AKONADISEARCH_ENABLE_KRUNNER_RUNNER=ON` — Build the KRunner contacts plugin, which makes
  [krunner](#port-krunner) a requirement of the configure. It is upstream's default on Linux;
  passing it stops a change of default from dropping the plugin without notice.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Corrosion=ON` — Make [corrosion](#port-corrosion) a requirement.
  Upstream finds it quietly, and without it `akonadi_html_to_text`, which turns HTML mail into the
  text the indexer stores, falls back to `QTextDocument` instead of the `html2text` Rust crate
  upstream calls the safe parser.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `CARGO_HOME="$SRC_ROOT/.cargo"` — Keep cargo's own registry and cache in the work directory,
  so the build writes nothing to the builder's home and starts from the same empty state on every
  machine.
- `RUSTFLAGS="-C target-feature=-crt-static"` — Link the Rust code against musl dynamically. The
  musl target links statically unless told otherwise; see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).
- `CARGO_NET_OFFLINE=true` — Keep cargo off the network, so a crate missing from the bundle fails
  the build by name instead of being fetched.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The HTML parser is the `htmlparser` crate under `agent/rs/htmlparser`, a static library that
Corrosion builds with [rust](#port-rust) and links into `akonadi_html_to_text` through `cxx`. Its
64 crates come from the port's vendor bundle, `akonadi-search-vendor-<version>.tar.xz`, made by
`ports/fetch` from the crate's `Cargo.lock` with `vendordir = agent/rs/htmlparser`; `build.sh`
unpacks it into the same directory, where cargo finds the bundle's `.cargo/config.toml` (see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).
A version bump means a new bundle, its own `sha256 =` line and `ports/publish akonadi-search`.

The KRunner plugin is loaded by KRunner and by the Plasma search field in the session; nothing in
this port starts it. `KF6TextUtils` comes from [ktextaddons](#port-ktextaddons).

## About

kdev-php is KDevelop's PHP language plugin: a parser and semantic model (DUChain) of PHP, with
highlighting by meaning, code completion, navigation, refactoring, the PHP manual as
documentation, and a PHPUnit test provider. It also installs KDevelop project templates for PHP.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The plugin requires KDevPlatform of its own major and minor version, 6.0, from
[kdevelop](#port-kdevelop). The plugin interface is private to that series, so kdev-php and
kdevelop move to a new release together, and depending on it makes kdevelop a build dependency of
another port, which places kdevelop in `43_toolkits` rather than `44_apps`.

The parser is generated at build time from `php.g` by `kdev-pg-qt`, the parser generator from
[kdevelop-pg-qt](#port-kdevelop-pg-qt), found as `KDevelopPGQt` 2.4 or later.

The plugin installs headers and a `KDevPHP` CMake package, which only a KDevelop 5 build of
Umbrello's PHP importer would read.

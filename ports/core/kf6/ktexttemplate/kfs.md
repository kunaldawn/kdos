## About

KTextTemplate is a string template engine in the style of Django's: a template with variables,
filters, `{% if %}` and `{% for %}` tags and inheritance is rendered against a context of Qt
values. [kdevelop](#port-kdevelop) fills its file and project templates with it,
[rocs](#port-rocs) links it, and KDE PIM's message themes are rendered through it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Qml=ON` — Make Qt Qml, from
  [qt6-qtdeclarative](#port-qt6-qtdeclarative), a requirement. Upstream looks for it as optional
  and, when it finds it, builds the scriptable tags and filters, written in JavaScript; without it
  they are left out with only a line in the configure summary.

## Notes

The source carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.

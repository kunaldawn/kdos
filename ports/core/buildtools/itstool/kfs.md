## About

itstool extracts the translatable text of an XML document into a gettext `.po` template, guided by
W3C ITS rules, and writes translated copies of the document back from `.po` files. GNOME projects
run it at build time over their help pages and other XML data, so it is a build dependency of
ports such as `appstream`, `zenity`, `orca`, `rhythmbox` and the GNOME games, and of nothing on
the desktop itself. It is a Python script that uses the libxml2 Python bindings, which the
[libxml2](#port-libxml2) port builds.

## Patches

- `raw-strings.patch` — Turns the regular-expression literals in `itstool.in` (`'\s+'`, the translator-credits pattern and the locale pattern) into raw strings, `r'\s+'`. In an ordinary Python string `\s`, `\<` and `\.` are invalid escapes, and the tree's Python prints a `SyntaxWarning` for each when it compiles the script, which it does on every run; as raw strings they compile to the same patterns with no warning.

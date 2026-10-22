## About

intltool is a set of Perl scripts that extract translatable strings from desktop entries, XML,
Glade files and other non-C sources into a gettext `.pot` template, and merge the translations back
into the files a package installs. Its scripts are run at build time by the configure and make
rules of the GNOME and GTK programs whose build uses it, such as geany, gnumeric, brasero and
darktable, and by data packages such as `sound-theme-freedesktop`; it is a dependency of those
ports and of nothing on the desktop itself. It needs Perl's `XML::Parser`, the
[perl-xml-parser](#port-perl-xml-parser) port.

## Patches

- `regex-brace.patch` — Escapes the literal `{` and `}` in the regular expressions of `intltool-update.in` that match `${VAR}` and `$VAR` in `configure.ac` (`\${?` becomes `\$\{?`, and `}?` becomes `\}?`). `intltool-update` runs under `perl -w`, and the tree's Perl prints "Unescaped left brace in regex is passed through" for each of these patterns every time it runs; escaped, the patterns match the same text and print nothing.

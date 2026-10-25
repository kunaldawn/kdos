## About
less is the pager: it shows a file or a program's output a screen at a time, scrolls both ways,
and searches with regular expressions. On KDOS it is what `man` pages through
([mandoc](#port-mandoc) depends on it), and the login profile points it at an input filter from
[lesspipe](#port-lesspipe) with `LESSOPEN`, and sets `LESS=-R` so colour escapes are shown as
colour (see
[Shell environment](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/configuration.md#shell-environment)).
The port installs `less`, and its helpers `lessecho` and `less-osc8-open` in `/usr/libexec`.

## Flags
- `--with-regex=pcre2` — Use PCRE2 for `/` and `?` searches, so a search pattern has Perl syntax,
  through [pcre2](#port-pcre2).

## Notes
less's configure does not stop when the regular-expression library it was told to use is missing.
It prints "cannot find regular expression library" and builds a `less` with no regular expressions at
all, in which `/` matches only literal text. `build.sh` therefore checks the generated `defines.h`
for `HAVE_PCRE2` and stops the build when it is not there, so a less without PCRE2 never reaches a
package.

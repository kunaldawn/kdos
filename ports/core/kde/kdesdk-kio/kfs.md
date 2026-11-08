## About

kdesdk-kio is the `perldoc:/` KIO worker. Opening `perldoc:/perlfunc`, `perldoc:/functions/open`
or `perldoc:/Net::HTTP` in a KIO program shows that Perl documentation as HTML, rendered by a Perl
script the worker runs.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Perl=ON` — Make [perl](#port-perl) a requirement. Upstream finds
  it quietly and builds the worker only when it is found, so without perl in the build root the
  package would hold no worker at all.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The worker runs `/usr/share/kio_perldoc/pod2html.pl`, which reads each page through
`perldoc -u -T` and formats it with `Pod::Parser`. [perl](#port-perl) 5.44 does not ship
`Pod::Parser` and no port installs it, so the script stops at its `use base qw{ Pod::Parser }` line and every
`perldoc:/` page fails until a `perl-pod-parser` port exists and is added to `depends`. The build
does not check for the module.

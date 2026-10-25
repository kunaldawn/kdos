## About
GNU Autoconf turns a project's `configure.ac` into the portable `configure` shell script that
probes the system before a build. A release tarball usually ships its generated `configure`, and
building it needs nothing from this port; it matters to the recipes that regenerate theirs, because
the source is a tag archive with none (as [eudev](#port-eudev)'s and
[libcap-ng](#port-libcap-ng)'s are) or because a patch touches `configure.ac`. Those recipes run
`autoreconf`, which this port installs, and it is in the phase's opening run because a recipe
calls it without naming autoconf in `depends`.

autoconf is written in Perl and expands its macros with GNU m4, which is why the recipe depends on
[perl](#port-perl) and [m4](#port-m4).

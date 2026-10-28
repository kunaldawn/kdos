## About
Boost is the large collection of peer-reviewed C++ libraries that much C++ software is written
against. Most of it is headers only; the rest is compiled libraries, and this port builds the
compiled components that the catalogue's consumers require, as shared libraries, together with
all the headers and Boost's CMake package files. Its dependents run from KDE programs
([krita](#port-krita), [digikam](#port-digikam), [kdevelop](#port-kdevelop)) and office software (the document-import libraries
[librevenge](#port-librevenge) and [liborcus](#port-liborcus), [abiword](#port-abiword),
[gnucash](#port-gnucash)) through CAD and fabrication ([freecad](#port-freecad),
[kicad](#port-kicad), [openscad](#port-openscad), [prusaslicer](#port-prusaslicer)) to radio
([gnuradio](#port-gnuradio), [wsjtx](#port-wsjtx)) and games ([wesnoth](#port-wesnoth)).

## Flags
- `--with-libraries` (to `bootstrap.sh`) and the matching `--with-iostreams` … `--with-python`
  (to `b2`) — The compiled components to build: iostreams, system, filesystem, regex, date_time,
  test, program_options, thread, atomic, charconv, chrono, container, locale, random, coroutine,
  context, graph, process, log, nowide, serialization, stacktrace and python. They are the
  components the consumers ask for with `find_package(Boost … REQUIRED COMPONENTS …)`.
  `bootstrap.sh` writes its list into `project-config.jam` as the default set, and any `--with-*`
  on `b2`'s command line replaces that set whole, so the `b2` list is the one that decides what is
  built.
- `--with-icu=/usr` — Build Boost.Regex's and Boost.Locale's ICU support against
  [icu](#port-icu).
- `--with-python=/usr/bin/python3` — Build `boost_python` and `boost_numpy` for the system
  [python3](#port-python3); `boost_numpy` needs [numpy](#port-numpy).
- `--user-config=user-config.jam` — b2 reads no compiler flags from the environment. `build.sh`
  writes a `user-config.jam` whose `using gcc` line carries the tree's `$CXX`, `CFLAGS`,
  `CXXFLAGS` and `LDFLAGS`, and points b2 at it.
- `staging-prefix=$PKG/usr` — Install into the package while every path written into the
  installed files, Boost's CMake configuration among them, still names `--prefix=/usr`.
- `boost.stacktrace.backtrace=off` — Build no `boost_stacktrace_backtrace`, the stack trace
  backend over libbacktrace, which it links statically. b2 otherwise builds it whenever its probe
  links a libbacktrace; the tree carries none of its own, and the other backends
  (`boost_stacktrace_basic`, `_addr2line`, `_noop`) are built.
- `variant=release`, `link=shared`, `threading=multi` — Optimised, shared, thread-safe libraries
  only.

## Notes
`b2` probes at configure time and builds Boost.Regex's and Boost.Locale's ICU support only when
it can link ICU, and `boost_numpy` only when the configured Python imports numpy; a failed probe
skips the part without an error. Both are in `depends` for that reason: a build root that happened
to lack either would produce a different Boost without a word.

After the install `build.sh` checks that no file under `/usr/lib/cmake` names `/var/cache/kpkg`,
kpkg's work area, so a staging path written into Boost's CMake configuration stops the build
instead of reaching every consumer's `find_package(Boost)`. It also checks that
`libboost_regex.so` is in the package.

Removing a component from the list breaks every consumer that asks for it, because its
`find_package(Boost … REQUIRED COMPONENTS …)` then fails at configure time.

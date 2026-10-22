## About
POCO is a family of C++ class libraries for networked applications: `Foundation` (threads,
streams, logging, Unicode, regular expressions) and, on top of it, Encodings, XML, JSON, Util,
Net, Crypto, NetSSL, JWT and Zip. [mumble](#port-mumble) is the port that needs it, for `Poco::XML` and
`Poco::Zip`. This build installs the libraries listed above and no others.

## Flags
- `-DPOCO_UNBUNDLED=ON` — Link the system [zlib](#port-zlib), [pcre2](#port-pcre2),
  [utf8proc](#port-utf8proc) and [expat](#port-expat) instead of compiling the copies POCO keeps
  under `dependencies/`, and fail configure when one is missing. It also switches SQLite and
  libpng to the system copies, for the components this build leaves out. It is off by default.
- `-DENABLE_FASTLOGGER=OFF` — Build no `FastLogger`, which compiles in the bundled Quill logging
  library. It is on by default on Linux, and POCO has no switch to link a system Quill, so with it on
  `POCO_UNBUNDLED` would still compile POCO's copy.
- `-DENABLE_TRACE=OFF` — No stack traces in exceptions, which would compile in the bundled
  cpptrace. It is the default.
- `-DENABLE_ENCODINGS=ON` — The extra character encodings of `PocoEncodings`. This and
  `-DENABLE_XML=ON`, `-DENABLE_JSON=ON`, `-DENABLE_UTIL=ON`, `-DENABLE_NET=ON`, `-DENABLE_ZIP=ON`
  are on by default; `-DENABLE_CRYPTO=ON`, `-DENABLE_NETSSL=ON` and `-DENABLE_JWT=ON` default on
  only when OpenSSL is found, so naming them stops a missing OpenSSL from dropping them silently.
- `-DENABLE_DATA=OFF` — Build none of the database layer: with it go `-DENABLE_DATA_SQLITE=OFF`,
  `-DENABLE_DATA_MYSQL=OFF`,
  `-DENABLE_DATA_POSTGRESQL=OFF`, `-DENABLE_DATA_ODBC=OFF`, `-DENABLE_ACTIVERECORD=OFF` and
  `-DENABLE_ACTIVERECORD_COMPILER=OFF`.
- `-DENABLE_MONGODB=OFF` — No MongoDB, Redis (`-DENABLE_REDIS=OFF`) or Prometheus
  (`-DENABLE_PROMETHEUS=OFF`) clients, and no web server page compilers
  (`-DENABLE_PAGECOMPILER=OFF`, `-DENABLE_PAGECOMPILER_FILE2PAGE=OFF`) or Apache module
  (`-DENABLE_APACHECONNECTOR=OFF`). Nothing in the tree uses them.
- `-DENABLE_PDF=OFF` — No PDF library, which compiles the bundled libharu; no
  `SevenZip` (`-DENABLE_SEVENZIP=OFF`), which compiles the bundled 7-Zip code; no DNS-SD
  (`-DENABLE_DNSSD=OFF`); no C++ parser or documentation generator (`-DENABLE_CPPPARSER=OFF`,
  `-DENABLE_POCODOC=OFF`). All five are off by default.
- `-DENABLE_SAMPLES=OFF` — Build no samples, with `-DENABLE_TESTS=OFF` no tests and with
  `-DENABLE_BENCHMARK=OFF` no benchmarks.

## Notes
Even unbundled, `libPocoFoundation` compiles two files of POCO's own PCRE2 copy,
`pcre2_ucd.c` and `pcre2_tables.c`, whenever the system PCRE2 is a shared library: its
`Unicode.cpp` reads PCRE2's internal Unicode property tables, which `libpcre2-8.so` does not
export. Those tables are POCO's PCRE2 version, not the [pcre2](#port-pcre2) port's.

`POCO_UNBUNDLED` covers only the libraries that have a system build to find. The copies under
`dependencies/v8_double_conversion`, `dependencies/pdjson` and `dependencies/tessil` have no
unbundled path and are built into the libraries that use them in every build.

## About

Xerces-C++ is Apache's validating XML parser for C++, with DOM, SAX and SAX2 interfaces and
validation against DTDs and XML Schema. KDOS carries it for the three ports that build against it:
[gdal](#port-gdal), whose build names it with `-DGDAL_USE_XERCESC=ON`, [pdal](#port-pdal) and
[freecad](#port-freecad).

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Build `libxerces-c` as a shared library rather than a static
  archive.
- `-Dnetwork=OFF` — Build no network accessor. With network support on, a parser that resolves an
  external DTD or schema named by an `http://` URL fetches it at run time, through curl or a plain
  socket; on a machine with no network that fetch fails late, deep inside a parse.
- `-Dtranscoder=icu` — Convert between character encodings with [icu](#port-icu). ICU is upstream's
  first preference when it is found, and the build's other Linux choices are `gnuiconv` and
  `iconv`; named, a missing ICU stops the configuration with "icu transcoder unavailable" instead
  of falling back to one of them.
- `-Dmessage-loader=inmemory` — Compile the parser's error and warning texts into the library.
  The alternatives are `icu`, which loads them from an ICU resource bundle, and `iconv`, which
  reads a message catalogue through `catopen`. `inmemory` is also upstream's default.
- `-Dthreads=ON` — Build with thread support, so that the library's mutexes are real ones rather
  than the `nothreads` stubs. This is also upstream's default.

## Patches

- `cxx17.patch` — Raises `CMAKE_CXX_STANDARD` in the top `CMakeLists.txt` from 14 to 17. ICU's headers need C++17, and upstream sets the standard as a plain variable in the file, which a `-DCMAKE_CXX_STANDARD=17` on the command line cannot override, so the change has to be made in the file.

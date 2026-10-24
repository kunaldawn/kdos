## About
[lxml]{.idx} binds the C libraries [libxml2](#port-libxml2) and [libxslt](#port-libxslt) to
Python, with the ElementTree API, XPath, XSLT, XML Schema and an HTML parser. It is the XML parser
the tree's document tools build on, among them [calibre](#port-calibre),
[ocrmypdf](#port-ocrmypdf), [inkscape](#port-inkscape)'s extensions and
[fonttools](#port-fonttools).

## Notes
`lxml.etree` links the system `libxml2.so`, `libxslt.so` and `libexslt.so`: lxml's `setup.py`
builds against the installed libraries unless it is given its `--static-deps` option, which
downloads and compiles private copies, and the recipe passes none. The source distribution
ships the C files generated from lxml's Cython sources, and `setup.py` compiles those as they
are unless `--with-cython` is given or one of them is missing.

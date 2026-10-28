## About

openpyxl reads and writes Excel 2010 `.xlsx` and `.xlsm` workbooks from Python. In KDOS it is
there for [visidata](#port-visidata), which opens an `.xlsx` file only through openpyxl and
refuses the file when the module is missing; `sc-im` reads the format in C and does not use it.

## Notes

openpyxl's one dependency that is not a port, `et-xmlfile`, comes from the vendor bundle that
`ports/fetch` makes from `pypackages = et-xmlfile`: that one source distribution, downloaded
without its dependencies, plus the build backends `ports/fetch` collects for each source in the
bundle. It and openpyxl are installed with `--no-deps --no-index` from
`vendor/` alone, and `--no-build-isolation` builds them with the setuptools of the build root;
see [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

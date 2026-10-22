## About

[GDAL]{.idx} (the Geospatial Data Abstraction Library) reads and writes raster and vector
geospatial formats behind one API: GeoTIFF, shapefiles, GeoPackage, NetCDF, HDF5, PostGIS and
many more, with coordinate transformations through [proj](#port-proj) and geometry operations
through [geos](#port-geos). It installs the `gdal` command and the classic tools such as `gdalinfo`,
`gdal_translate` and `ogr2ogr`, and the `osgeo` Python module. [qgis](#port-qgis),
[qmapshack](#port-qmapshack) and [pdal](#port-pdal) are built on it.

## Flags

- `-DGDAL_USE_EXTERNAL_LIBS=OFF` — Turn every optional library off unless it is named with its own
  `GDAL_USE_<NAME>=ON`. A library GDAL finds but that is not named is reported as "disabled due to
  GDAL_USE_EXTERNAL_LIBS=OFF", and a library named `ON` that is missing stops the configure
  ("Configured to use ..., but not found"). The drivers built therefore follow `build.sh` and the
  `depends` line, never whatever else the build root holds. A library not named falls back to the
  copy GDAL carries where it carries one (libgeotiff, LERC and shapelib among them), and is
  otherwise absent.
- `-DGDAL_USE_CURL=OFF` — Build no HTTP client into the library. With curl, `/vsicurl/` and the
  network drivers turn a dataset path into a download. The phase list gives this the same reason
  as [proj](#port-proj)'s `ENABLE_CURL=OFF`.
- `-DGDAL_USE_POPPLER=ON` — The PDF driver's read side through [poppler](#port-poppler), for
  geospatial PDFs. It includes poppler's core headers (`PDFDoc.h`, `GlobalParams.h`), which the
  poppler port installs because it is built with `ENABLE_UNSTABLE_API_ABI_HEADERS=ON`.
- `-DGDAL_USE_PDFIUM=OFF` — Build the PDF driver without its PDFium backend, so poppler is the only
  one it reads with.
- `-DBUILD_PYTHON_BINDINGS=ON` — Build the `osgeo` Python module and the Python utilities that ship
  with it, such as `gdal_calc.py`, `gdal_merge.py` and `ogrmerge.py`. [swig](#port-swig) generates
  the wrappers and [python3-setuptools](#port-python3-setuptools) installs them.
- `-DBUILD_JAVA_BINDINGS=OFF`, `-DBUILD_CSHARP_BINDINGS=OFF` — No Java or C# bindings.
- `-DGDAL_USE_ZLIB=ON` through `-DGDAL_USE_BLOSC=ON` — Each library GDAL is to use, named, with
  its port in `depends`. Among them: [geos](#port-geos) for geometry operations,
  [sqlite](#port-sqlite) and [libspatialite](#port-libspatialite) for GeoPackage and SpatiaLite,
  [postgresql](#port-postgresql) for PostGIS, [netcdf-c](#port-netcdf-c), [hdf5](#port-hdf5) and
  [libaec](#port-libaec) for scientific data, [cfitsio](#port-cfitsio) for FITS, the image codecs
  (TIFF, JPEG, PNG, GIF, WebP, JPEG 2000, HEIF, AVIF, JPEG XL, OpenEXR), the compressors (zstd,
  libdeflate, LZ4, LZMA, Blosc), [libarchive](#port-libarchive) for `/vsi7z/` and `/vsirar/`,
  [freexl](#port-freexl) for the XLS driver, which reads an old Excel workbook's sheets as vector
  layers, [xerces-c](#port-xerces-c) for the GMLAS and ILI drivers, [qhull](#port-qhull), and
  OpenMP.

## Notes

The `osgeo.gdal_array` module, which `gdal_calc.py` and most raster scripting import, is built
only when CMake finds [numpy](#port-numpy), and GDAL leaves it out without an error when it does
not. `build.sh` therefore checks that `_gdal_array*.so` is in the package and fails the build when
it is not.

[libgeotiff](#port-libgeotiff) is a port, but `GDAL_USE_GEOTIFF` is not named, so under
`GDAL_USE_EXTERNAL_LIBS=OFF` GDAL builds its own copy of libgeotiff into the library.

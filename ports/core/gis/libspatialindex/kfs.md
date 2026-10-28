## About

libspatialindex is a C++ library of spatial indexes, the R-tree among them, with a C interface,
`libspatialindex_c`, beside the C++ one. [qgis](#port-qgis) indexes features with it for
selection, snapping and nearest-neighbour search, and [spyder](#port-spyder) reaches it through
the Python `rtree` module.

## Notes

`nearest-neighbor-data-distance.patch` is what makes QGIS correct against 2.1: QGIS refuses
libspatialindex 2.1 or later at configure time over
[libspatialindex issue 276](https://github.com/libspatialindex/libspatialindex/issues/276), and
its `10-libspatialindex_2_1.patch` removes that refusal only because this port carries the fix.
Without it, `nearestNeighborQuery` measures every leaf entry through the bounding-box form of the
comparator's `getMinimumDistance()`, so the data form QGIS overrides, which measures a feature's
stored geometry and applies the maximum distance, never runs.

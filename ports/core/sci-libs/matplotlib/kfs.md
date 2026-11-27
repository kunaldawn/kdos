## About
Matplotlib draws 2D plots from Python. This port builds the Agg raster backend and the PDF, PS and
SVG writers; the interactive GUI backends need toolkit bindings it does not depend on.

## Notes
`runtime = fonttools`: saving a figure as PDF or PostScript embeds subset fonts through
`matplotlib/backends/_backend_pdf_ps.py`, which imports `fontTools`. The import is in a submodule
the [run-time needs](../../../../docs/kdos/03-architecture/packaging.md#run-time-needs) record does not read, and [fonttools](#port-fonttools) is a build-only
port, so without the word a prebuilt install would leave it out and `savefig` to a `.pdf` or
`.ps` would fail while PNG works.

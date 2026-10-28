## About

OCRmyPDF adds an invisible, searchable text layer to a scanned PDF and leaves the scanned images
as they are. It is a Python program that drives other tools: [tesseract](#port-tesseract) reads
the page, [pypdfium2](#port-python3-pypdfium2) rasterises it by default, fpdf2 writes the text
layer, and [ghostscript](#port-ghostscript) produces PDF/A output.

## Notes

The text layer is set in Occulta, a glyphless font upstream ships prebuilt as
`src/ocrmypdf/data/Occulta.ttf`. `build.sh` deletes that file and regenerates it from upstream's
own `scripts/generate_glyphless_font.py` with [fonttools](#port-fonttools), so the installed font
is built from source like every other payload
([What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).
The script stamps the font's creation and modification dates with `time.time()`; the inline
Python replaces `time.time` with `SOURCE_DATE_EPOCH` before calling it, so two builds write the
same bytes ([Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages)).
`test -s` stops the build if the script wrote nothing.

The Python modules that only OCRmyPDF uses come from the vendor bundle,
`ocrmypdf-vendor-<version>.tar.xz`, which `ports/fetch` fills with the source distributions of
the recipe's `pypackages` list (`pydantic`, `annotated-types`, `typing-inspection`, `fpdf2`,
`img2pdf` and `pdfminer.six`) and the build backends pip needs to build them. Everything else it imports is a port in `depends`, and
`pip3 install --no-deps --no-index` installs exactly the six named distributions and OCRmyPDF
itself, resolving nothing: a distribution missing from the bundle fails the build, and a
dependency missing from `depends` is not noticed until the program imports it. See
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

`pydantic` is pinned in `pypackages` because its compiled half is the separate
[python3-pydantic-core](#port-python3-pydantic-core) port, held with `series` at the one version
that pydantic release requires. A bump of the pin here must move that port with it.

## About
[jsonschema]{.idx} validates JSON documents against JSON Schema, drafts 3, 4, 6 and 7, 2019-09 and
2020-12, and installs the `jsonschema` command. It resolves `$ref` through
[python3-referencing](#port-python3-referencing) and takes the published meta-schemas from
[python3-jsonschema-specifications](#port-python3-jsonschema-specifications).
[gnuradio](#port-gnuradio) and [python3-nbconvert](#port-python3-nbconvert) depend on it.

## Notes
jsonschema builds as [python3-attrs](#port-python3-attrs) does, with hatchling and hatch-vcs
from ports and the hatch-fancy-pypi-readme plugin from the bundle, installed into
`$SRC_ROOT/backends` and reached through `PYTHONPATH` for this build alone. The two recipes'
`pypackages` are the same, and so is the bundle: both `sha256 =` lines name one file's hash.

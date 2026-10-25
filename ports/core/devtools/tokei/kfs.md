## About
[tokei]{.idx} counts the lines of code, comments and blank lines in a source tree, per language.

## Flags
- `--features all` — Turn on the `cbor` and `yaml` output formats beside the default `cli`
  feature; JSON output needs no feature. Without it, `tokei -o yaml` and `-o cbor` are refused.
  Both are pure Rust crates from the vendor bundle.

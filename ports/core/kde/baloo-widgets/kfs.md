## About

baloo-widgets is the file metadata panel, the tag and rating editors and the file-information
dialog that [dolphin](#port-dolphin) shows, reading what [baloo](#port-baloo) has indexed and what
[kfilemetadata](#port-kfilemetadata) extracts. It also installs the `baloo_filemetadata_temp_extractor`
helper those widgets run for a file the index does not cover.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

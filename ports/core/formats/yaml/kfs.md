## About

LibYAML is a YAML parser and emitter in C. The port is named `yaml`; it installs the header
`yaml.h`, the library `libyaml-0.so`, and the pkg-config module `yaml-0.1`. In KDOS it is the fast
path under the YAML readers of two languages: [python3-yaml](#port-python3-yaml) is built with
`PYYAML_FORCE_LIBYAML=1`, so PyYAML's C loader and dumper are compiled against it, and Ruby's
`psych` extension links it. [dtc](#port-dtc) uses it to write device trees as YAML,
[universal-ctags](#port-universal-ctags) to index YAML files, and libcamera links it as well.
libblockdev and libfyaml also name it in `depends`.

The recipe is upstream's autotools build with no switches of its own.

## About
protobuf-c is Protocol Buffers for C: `libprotobuf-c`, a small C runtime that encodes and decodes
messages, and `protoc-gen-c`, a plugin for [protobuf](#port-protobuf)'s `protoc` that generates C
code from a `.proto` file (`protoc --c_out=`). In this tree [libomemo-c](#port-libomemo-c), the
OMEMO encryption library of XMPP clients, builds its message code with it.

## Flags
- `--enable-protoc` — Build `protoc-gen-c`, the code generator. On is upstream's default, and configure
  then stops unless protobuf's compiler headers and `protoc` are installed. The generator is the only part of the
  package that links protobuf's C++ libraries: the runtime is plain C with no dependencies.

## Notes
`protoc-gen-c` is compiled against `libprotoc`'s C++ API, which changes from one protobuf release
to the next. `protobuf-34-field-label.patch` replaces every call to `FieldDescriptor::label()`,
which the protobuf this tree builds does not provide, with a helper built on `is_repeated()` and
`is_required()`. Because the generator links protobuf's versioned libraries, a protobuf bump means
rebuilding this port.

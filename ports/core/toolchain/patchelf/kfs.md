## About

patchelf edits an ELF executable or shared library in place: it can change the dynamic loader a
program asks for (`--set-interpreter`), its run path (`--set-rpath`, `--remove-rpath`), its
`DT_NEEDED` list (`--add-needed`, `--replace-needed`) and a library's soname. With it, a binary
linked against another system's loader can be pointed at this one, `/lib/ld-musl-x86_64.so.1`,
and at libraries outside the default search path.

It is built in `30_foundation`. No recipe runs it and no port names
it in `depends`.

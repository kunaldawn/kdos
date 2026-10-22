## About
[typescript-go]{.idx} is the TypeScript compiler written in Go, the code base of TypeScript 7.
Its command, `tsgo`, type-checks and compiles TypeScript like the `tsc` of the npm package.
[chromium](#port-chromium) is the reason for the port: its build copies the binary into its own
`third_party/typescript` tree and runs it there.

The program is installed as `/usr/lib/typescript-go/tsc`, beside the `lib.*.d.ts` declaration
files it reads, and `/usr/bin/tsgo` is a symbolic link to it.

## Flags
- `-trimpath` — Leave the build directory's paths out of the binary; the exported `GOFLAGS`
  carries it already.
- `-tags=noembed` — Build without the declaration files compiled into the binary.
  `internal/bundled/noembed.go` then reads them from the directory of the executable's resolved
  path, and stops with "does not exist; this executable may be misplaced" when `lib.d.ts` is not
  there. A caller with its own edited set of declaration files can place a copy of the binary
  beside them, which is what Chromium's build does.
- `-X github.com/microsoft/typescript-go/internal/core.version=$version` — Set the version the
  compiler reports; `internal/core/version.go` declares it as a variable for this.

## Notes
The executable's path is resolved through symbolic links before the declaration files are looked
for, which is why the link in `/usr/bin` finds the files in `/usr/lib/typescript-go`. A copy of
the binary elsewhere, without the files beside it, stops with the message above.

`build.sh` unpacks the vendor bundle twice; the second extraction writes the same files again
and changes nothing.

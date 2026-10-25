## About
libsmi reads SNMP MIB modules written in SMIv1, SMIv2, SMIng and YANG, and gives programs a parsed
view of their objects and types. The package also installs command-line tools such as `smilint`,
`smidump` and `smiquery`, and MIB collections that include the IETF and IANA modules. [wireshark](#port-wireshark) links it to name OIDs
and decode values in SNMP traffic.

## Patches
- `gcc14-declarations.patch` — Add the declarations C99 and later require before a call: `lib/error.h` includes `smi-check.h`, `lib/parser-yang.y` and the shipped `lib/parser-yang.c` include `yang-check.h`, and `lib/yang-data.h` declares `createIdentifierRef()`. GCC 14 makes an implicit function declaration an error, so without them the library does not compile.

## Notes
The release tarball ships the bison parsers and flex scanners already generated. The patch
touches `parser-yang.y`, which makes it newer than `parser-yang.c`, and `make` would then try to
run bison; neither bison nor flex is in `depends`. So `build.sh` touches every generated parser,
header and scanner after patching, making them newer than their `.y` and `.l` sources, and the
shipped files, with the patch applied to `parser-yang.c` as well, are compiled as they are.

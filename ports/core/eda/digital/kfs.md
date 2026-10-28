## About
[Digital]{.idx} is a digital logic designer and circuit simulator made for teaching: gates,
flip-flops, memories and whole processors drawn as circuits and simulated, with truth tables,
state machines, and export to VHDL and Verilog. It is a Java Swing program; on KDOS it is an X11
client under Xwayland. The port compiles it, and the libraries it uses, from source with `javac`
alone, without upstream's Maven build.

## Flags
- `-sourcepath "$_dep:$SRC_ROOT/xmlpull-api-v1-$_xmlpull/src/java/api:src/main/java"`,
  `-implicit:class` — Let `javac` find every class it is not handed on the command line in the
  unpacked dependency sources (XStream, MXParser, the XML Pull API, org.json, SLF4J) and in
  Digital's own tree, and write a class file for each one it compiles that way. MXParser, the
  XML Pull API, org.json and SLF4J are not on the command line, so of those only the classes
  that Digital and XStream reach are compiled.
- `-encoding UTF-8` — Read the sources as UTF-8, whatever the build's locale.
- `-proc:none` — Run no annotation processors.
- `-nowarn` — Print no compiler warnings.

## Notes
- **Every dependency is a source jar.** The recipe takes each library Digital needs from Maven
  Central as its published `-sources.jar`, and the XML Pull API from its git archive, each with
  its own `sha256 =` line; `build.sh` unpacks them into `build/depsrc`. slf4j-api's sources
  carry placeholder binders, `org/slf4j/impl`, that its own jar leaves out, so `build.sh` removes
  them and unpacks slf4j-simple's binders in their place.
- **All of XStream is compiled, with exceptions.** XStream loads its mappers and converters by
  name at run time, so `-implicit:class` alone would miss them: `build.sh` hands `javac` every
  XStream source file except those that import a library this system does not carry (CGLIB,
  dom4j, JDOM, XOM, kXML2, XPP3, Jettison and the other `org.codehaus` packages, Joda, Woodstox,
  BEA's StAX, JavaBeans Activation and JAXB, the `_opt` pattern). XStream probes for those
  drivers and converters and skips the ones that are missing.
- **The jar is reproducible.** The class and resource files go into `Digital.jar` sorted by
  name, one quoted name per line because some resource names contain spaces, with every entry
  dated `SOURCE_DATE_EPOCH`. The manifest's `Build-Time` is the same date.
- **No update check.** The About box reads `Build-SCM-Revision` from the manifest, and Digital
  asks `api.github.com` for a newer release only when that revision is known and at most seven
  characters long. `build.sh` writes `v<version>-kdos`, which is longer, so it never asks.
- **The launcher and the entry.** `/usr/bin/digital` runs the jar with [openjdk](#port-openjdk)'s
  `java`. The menu entry's `StartupWMClass` is `de-neemann-digital-gui-Main`, the main class with
  its dots turned into dashes, which is the `WM_CLASS` Swing gives the window. The entry claims
  `text/x-digital`, the MIME type of Digital's own circuit files, whose definition the port
  installs from upstream's `digital-simulator.xml`.

## About

JOSM is the Java OpenStreetMap editor: it downloads map data from the OpenStreetMap API, edits it
over aerial imagery and GPS traces, validates the result and uploads it. It is a Swing program, run
on [openjdk](#port-openjdk).

Upstream builds JOSM with Apache Ant, which fetches every library through Ivy at build time. Ant
and Ivy are not ports and the build has no network, so `build.sh` does the same work with `javac`,
`java` and `jar` alone: it builds JavaCC from its source, generates JOSM's two grammars with it,
compiles JOSM together with the source of every library it uses, and packs one jar. Each library is
its own `source =` line, a `-sources.jar` from Maven Central (or the JOSM repository, for
JMapViewer) or a tag archive from GitHub, pinned by a helper variable in the recipe; no
upstream-compiled class reaches the package (see [What is built from
source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).
The tag2link index is the one exception, taken as data from its webjar (see [Data with no other
source
form](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/why-kdos.md#data-with-no-other-source-form)).

## Flags

- `-JDK_VERSION=1.11` — Tells JavaCC which Java level the generated MapCSS parser is written for,
  the value upstream's `build.xml` passes. `-DEBUG_PARSER=false`, `-DEBUG_TOKEN_MANAGER=false`,
  `-GRAMMAR_ENCODING=UTF-8` and `-UNICODE_INPUT=true` are the rest of upstream's JavaCC arguments
  for `MapCSSParser.jj`.
- `-proc:none` — Turns annotation processing off for the JavaCC and JOSM compiles, so javac looks
  for no processor on the class or source path.
- `-implicit:class` — With `-sourcepath "$_dep:src"`, javac compiles a library class only when
  something it is compiling refers to it, and writes that class file too. The dependency sources
  are unpacked whole into one tree, and only what JOSM uses of each library is built.
- `--release 21` — Compiles JOSM and its libraries against the Java 21 API and class-file level.
  Upstream's `build.xml` compiles at its own `java.lang.version`, a lower release.
- `-Djava.awt.headless=true` — Runs `BuildProjectionDefinitions`, upstream's generator for the
  `custom-epsg` projection list, with no display, as upstream's `epsg` target does.
- `--add-modules` — In the launcher: makes `java.scripting` and `java.sql` resolvable for a jar run
  from the class path, as upstream's own launcher does.
- `--add-exports` — In the launcher: opens the three JDK internals JOSM reaches into
  (`sun.security.action` and two `com.sun.imageio` packages), the same three upstream's launcher
  passes; the manifest's `Add-Exports` and `Add-Opens` lines carry the same list for a plain
  `java -jar`.
- `--offline` — In the launcher: starts the editor with `--offline=JOSM_WEBSITE`, which keeps JOSM
  from fetching its start page, version check, plugin list and help from josm.openstreetmap.de. The
  OpenStreetMap API, for downloading and uploading map data, stays available. The `render`,
  `validate` and `project` commands are passed through without it: each takes the first argument
  and accepts no editor option.

## Notes

The order inside `build.sh` matters. JavaCC must be compiled before either grammar is generated;
its source archive carries its own generated parsers, so it compiles with `javac` alone.
`MapCSSParser.jj` and OpeningHoursParser's `OpeningHoursParser.jj` must be turned into Java before
the main `javac`, which compiles the generated parsers with everything else. The empty
`resources/data/projection/custom-epsg` must exist before `BuildProjectionDefinitions` runs: the
generator loads the projection code from `build/classes`, and that code reads `custom-epsg` from
the class path. With no file there, the generator creates an empty one and stops, asking to be run
again, and the list is not written.

Two libraries are compiled explicitly rather than found through `-implicit:class`, because nothing
in JOSM's source names their classes: parsson, the `jakarta.json` implementation, which upstream's
`ivy.xml` lists as runtime-only and which is found through `ServiceLoader`, and commons-jcs3, which
builds its caches from class names given as strings in its configuration. Parts of commons-jcs3 are
deleted first: the JDBC disk cache (it needs commons-dbcp2), the HTTP remote cache and the servlet
utilities (the servlet API), and the Log4j 2 logger (Log4j). None of those libraries is a port, and
JOSM configures none of those parts. Every `module-info.java` is deleted: everything is compiled
into one unnamed module, and a module descriptor on the source path would turn the whole
compilation into that module. The Maven metadata under `META-INF` goes too.

jsvg and the Adobe XMP core come from GitHub tag archives rather than `-sources.jar` files; jsvg's
published sources lack its annotations module, which the release tree has.

Every translation except English is deleted from `resources/data` before the jar is packed. KDOS
ships English only (see [the size and language
policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
and the package's own pruning removes files in the package, not entries inside a jar.

The jar is reproducible: its file list is sorted, `resources/REVISION` and the manifest's
`Main-Date` come from `SOURCE_DATE_EPOCH`, and `jar --date` stamps every entry with the same time.
The list has one quoted name per line, because some resource names contain spaces.

Swing is an X11 client under Xwayland, and its `WM_CLASS` is the main class with its dots turned
into dashes, which is what the desktop entry's
`StartupWMClass=org-openstreetmap-josm-gui-MainApplication` matches.

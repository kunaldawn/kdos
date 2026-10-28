## About
[Logisim-evolution]{.idx} is a digital logic designer and simulator: circuits of gates,
flip-flops, memories and subcircuits, drawn and simulated, with chronograms, test vectors, and
VHDL and Verilog export for synthesis onto FPGA boards. It is a Java Swing program; on KDOS it is
an X11 client under Xwayland. Upstream builds with Gradle and bundles its dependencies' class
files; this port compiles Logisim and every library it uses from their published sources with
`javac`, and fetches nothing at build time.

## Flags
- `--release 21` — Compile against the Java 21 platform API, the source and target level
  upstream's Gradle build sets.
- `-processorpath build/proc`, `-processor org.kohsuke.metainf_services.AnnotationProcessorImpl`
  — Run the `metainf-services` annotation processor, compiled first into `build/proc`, during the
  main compile. It writes the `META-INF/services` files through which SwingX registers its
  look-and-feel add-ons.
- `-sourcepath "$_dep:src/main/java:build/gen"`, `-implicit:class` — Let `javac` find every
  class it is not handed on the command line in the unpacked dependency sources, in Logisim's own
  tree and in the generated `BuildInfo`, and write a class file for each one it compiles that
  way.
- `-encoding UTF-8`, `-nowarn` — Read the sources as UTF-8 and print no compiler warnings.
- `--enable-native-access=ALL-UNNAMED` — In the `logisim-evolution` launcher, and as
  `Enable-Native-Access` in the jar's manifest: allow code on the class path to use the JDK's
  restricted native-access methods without a warning, as upstream's own launchers do.

## Patches
- `colorpicker-no-lombok.patch` — The colour picker's sources use Lombok annotations (`@RequiredArgsConstructor`, `@Getter`, `@Slf4j`, `@NonNull`), which generate code at compile time and need Lombok's annotation processor. The patch removes them and writes out what they generate: the constructors, `ColorPickerMode.getMax()`, the SLF4J logger field, and `Objects.requireNonNull` checks where `@NonNull` guarded a field; the `@NonNull` on `ColorSliderUI`'s constructor parameters is dropped with no check in its place.
- `javahelp-no-awt-peer.patch` — Removes the unused `import java.awt.dnd.peer.*` from JavaHelp's `BasicFavoritesNavigatorUI`; the JDK does not export that package, so the import fails to compile.

## Notes
- **What is compiled, and how it is chosen.** Every library is a `-sources.jar` from Maven
  Central, or for JavaHelp its git archive, each with its own `sha256 =` line, unpacked into
  `build/depsrc`. Libraries that find their parts by name at run time are handed to `javac`
  whole: FlatLaf's UI delegates, RSyntaxTextArea's token makers, JavaHelp's views and search
  engine, SwingX's look-and-feel add-ons and slf4j-simple's provider. Everything else is compiled
  only as far as Logisim's own classes reach it.
- **One unnamed module.** `build.sh` deletes every `module-info.java` and the dependencies'
  `META-INF/versions` trees: a module descriptor on the source path would turn the whole
  compilation into that module. It also deletes JavaHelp's JSP tag library and servlet broker,
  which need a servlet container, and its native browser view, which needs JDIC; Logisim uses
  none of them.
- **BuildInfo is written by hand.** Gradle's `genBuildInfo` task writes
  `com.cburch.logisim.generated.BuildInfo` from the git checkout; `build.sh` writes the same class
  with the build ID `v<version>`, empty branch fields, and dates taken from `SOURCE_DATE_EPOCH`.
- **The help is generated and cut to English.** Logisim's `DocumentationGenerator` is compiled
  and run to write the JavaHelp descriptors from `src/main/doc/help-sets.xml`. `build.sh` then
  removes every language's manual and help set except English.
- **The jar is reproducible.** Class and resource files go into `logisim-evolution.jar` sorted by
  name, one quoted name per line because some resource names contain spaces, every entry dated
  `SOURCE_DATE_EPOCH`. Licence and notice files under the dependencies' `META-INF` are not copied
  into it.
- **The menu entry.** Its `StartupWMClass` is `com-cburch-logisim-Main`, the main class with its
  dots turned into dashes, which is the `WM_CLASS` Swing gives the window.

## About
`texlive-doc` is the documentation half of [texlive](#port-texlive): the English manuals and
examples of every TeX Live package that port ships, under `/usr/share/texmf-dist/doc`, where
`texdoc` looks for them. It compiles nothing. It is listed in `ports/unbuilt`, so no phase builds
it and the image does not carry it; `kpkg install texlive-doc` builds it on a machine. The
package-time prune removes every PDF and HTML file under that directory, so built as the tree
stands the package holds the examples and the plain-text and source files, and `texdoc` still has
no PDF to open; see
[`texdoc` finds no manuals](../../../../docs/kdos/06-reference/known-gaps.md#texdoc-finds-no-manuals-and-html-and-pdf-manuals-are-not-installed).

## Notes
- **The same cut as texlive.** It reads the same texmf tarball and `texlive.tlpdb` as the
  `texlive` port, and takes the same closure: the packages `TL_ROOTS` names and everything they
  depend on, following each package's `depends` in the tlpdb and skipping the per-platform
  `.ARCH` packages. `TL_ROOTS` must equal the texlive recipe's, or `texdoc` lists packages whose
  manuals are missing and this port ships manuals for packages that are not installed. The two
  share `group = texlive`, so they are bumped together.
- **What is kept.** For each selected package, the files its tlpdb record lists as documentation
  under `texmf-dist/doc/` are moved from the unpacked texmf tree into `$PKG/usr/share`, where the
  tlpdb marks no language or English. The manual pages and info files under `doc/man` and
  `doc/info` are skipped: `texlive` installs those into `/usr/share/man` and `/usr/share/info`.
  So is the Java bytecode (`.jar`, `.class`) a few packages' examples carry, which nothing here
  compiled. A file the tlpdb names that the tarball lacks stops the build.
- **Three sources, no compiler.** The texmf tarball is the first source, so it is unpacked into
  `$SRC`; the installer tarball supplies `tlpkg/TeXLive`, the Perl modules (`TeXLive::TLPDB`) the
  Perl block reads the database with; the tlpdb bundle, made by hand, is described in
  [A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
  The Perl block prints how many packages and files it moved.

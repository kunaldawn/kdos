## About

ShellCheck reads `sh` and `bash` scripts and reports what the shell would accept and do wrong: an
unquoted expansion that splits on spaces, a `cd` whose failure nothing checks, a variable set in a
subshell and lost, a construct a POSIX `sh` does not have. Every `build.sh` in this tree is a bash
script whose syntax `testing/preflight.sh` checks with `bash -n`; ShellCheck finds what parses and
still goes wrong. It is written in Haskell, and the port builds it from Hackage's `ShellCheck` source with [ghc](#port-ghc) and
[cabal-install](#port-cabal-install) against a vendored set of Hackage packages.

## Flags

- `--enable-split-sections` — cabal builds each library with GHC's `-split-sections`, so the linker
  can keep only the functions `shellcheck` uses from every Haskell library it links, which are
  linked statically into the one binary. It makes the program smaller and the build a little slower;
  cabal's default is off.
- `--enable-executable-stripping` — cabal strips the program as it copies it into `--installdir`.
  This is cabal's default as well; the recipe states it.
- `-Tman` — lowdown writes a man page instead of HTML, from upstream's `shellcheck.1.md`.
  Upstream's `manpage` script renders the page with pandoc; this port renders it with
  [lowdown](#port-lowdown).
- `--out-no-smarty` — Turns off lowdown's smart typography, which would turn the `--` in front of
  each long option into an en dash. Upstream's pandoc line turns the same typography off with
  `-f markdown-smart`.

## Patches

- `shellcheck-1-md-spaces.patch` — Replaces each backslash-escaped space (`\ `) in the option headings of `shellcheck.1.md` with a plain space. Pandoc reads `\ ` as a non-breaking space; lowdown reads it as a literal backslash before a space, so every heading of the man page would carry a stray `\`. The patch changes only the manual's source and is applied after the program is built.

## Notes

The vendor bundle comes from `cabal freeze`, run by `ports/fetch` in the fetch container with the
GHC and cabal versions this tree builds; the freeze rides in the bundle and `build.sh` puts it back
beside `ShellCheck.cabal` so the offline solve lands on exactly the packages that were downloaded.
How the bundle is made and checked, and why the repository is configured under Hackage's name with
no `--offline`, is in
[The Haskell bundle](../../../../docs/kdos/05-developer/writing-ports.md#the-haskell-bundle).

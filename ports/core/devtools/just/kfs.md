## About
[just]{.idx} is a command runner: a `justfile` in a project names recipes, short sequences of
shell commands with arguments, and `just <recipe>` runs one. It is a Rust program built from the
crates in its vendor bundle; see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust).

## Notes
The manual page and the bash, zsh and fish completions are not files in the source. `build.sh`
runs the binary it has just built, `just --man` and `just --completions <shell>`, and installs
what it prints, so they always describe the program in the package.

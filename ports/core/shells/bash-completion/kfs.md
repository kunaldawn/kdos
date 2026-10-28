## About

bash-completion is the programmable completion collection for bash: the `bash_completion` loader
and the completion scripts for common commands, kept under `/usr/share/bash-completion`. The
system `bash.bashrc` sources the loader in every interactive shell that is not in POSIX mode (see
[Shell environment](../../../../docs/kdos/06-reference/configuration.md#shell-environment)). The
loader reads a command's file the first time that command is completed, looking for
`<command>.bash` or `<command>` in `~/.local/share/bash-completion/completions` first, then in
`/usr/share/bash-completion/completions`, where other packages put theirs. Its own scripts live in
`completions-core` beside that directory and are read only when no package supplies one; those in
`completions-fallback`, for commands that usually ship their own, come last.

## Notes

A port can depend on it for its build rather than for run time. Build systems ask pkg-config for
`bash-completion` and read its `completionsdir` variable to decide where, and whether, to install
their own completion files; libnbd's configure, for one, installs none when the module is missing,
and fails when `--with-bash-completions` asks for them. A package that lacks its completion files,
with no error in the build log, may be missing this dependency.

A completion file belongs in `/usr/share/bash-completion/completions`, named after the command it
completes. The package lint refuses `/etc/bash_completion.d`, which upstream keeps only as a
compatibility directory; see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses).

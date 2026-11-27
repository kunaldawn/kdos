## About
toot is a Mastodon client for the terminal: a command line for posting, reading and following,
and `toot tui`, the full-screen timeline reader the desktop entry opens in a terminal.

## Notes
`runtime = python3-tomlkit`: toot reads its settings file through `toot/settings.py`, which imports
tomlkit on every start. The import is in a submodule the [run-time needs](../../../../docs/kdos/03-architecture/packaging.md#run-time-needs) record does not read,
and [python3-tomlkit](#port-python3-tomlkit) is a build-only port, so without the word a prebuilt
install would leave it out and every `toot` command would stop with `No module named 'tomlkit'`.

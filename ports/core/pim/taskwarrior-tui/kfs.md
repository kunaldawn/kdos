## About
taskwarrior-tui is a terminal interface for [Taskwarrior](#port-taskwarrior): a table of tasks
to filter, add to, edit and complete with single keys. It does not read Taskwarrior's database
itself, only watches the file's modification time to know when to refresh; every query and change
runs the `task` program, which is why `taskwarrior` is in
`depends` and why the two cannot disagree about a task. It is built from its
[Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
The package adds a desktop entry, "Tasks", which opens it in a terminal. It is `NoDisplay=true`:
until `task` has written a `~/.taskrc` taskwarrior-tui exits at once, so Start does not list it.

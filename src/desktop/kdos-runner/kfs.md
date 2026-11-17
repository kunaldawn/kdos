## About

`kdos-runner` puts KDOS into KRunner, the search that Alt+Space, Meta+Space and Kickoff's search
field open. It is KDOS's own C, linked to basu and SQLite and to nothing from Qt, and it speaks
KRunner's D-Bus runner interface, `org.kde.krunner1`, under the bus name `io.kdos.Runner` with two
objects. Two files under `/usr/share/krunner/dbusplugins/` declare them to KRunner as two runners,
and `io.kdos.Runner.service` lets the session bus start the program on the first query. It exits
after ten minutes without a call.

- `kdos.desktop` (runner id `kdos`, object `/runner`) is the command palette: the kdos verbs with
  their arguments filled in (`accent amber`, `accent next`, `phosphor on`, `tiling off`,
  `desk dock`, `night light`, `reduced motion`, `update check`, `doctor`, `box stop www`,
  `install gimp`, `why /etc/fstab`), two letters that open a favourite (`code=FF` on its line of
  `~/.config/kdos/favorites`), the routes of `/etc/kdos/menu.conf` by the words of their names or
  exactly as `route:<name>`, titled from the name (`setup.network` reads *Set up network*, over
  `route:setup.network`), the packs and `.ktar` sets on the boot medium after `install`
  (`install gimp` offers `app.gimp (on medium)`, opened in Discover with `--local-filename`, which
  lists it under the `medium` origin), `keys <words>`, the chords as KGlobalAccel holds them now,
  `lib <words>`, which opens KRunner again with the library alone, and `ask <question>`, answered
  by the language model llama-server serves on 127.0.0.1:8082, with an answer Enter copies.
- `kdos-library.desktop` (runner id `kdos-library`, object `/library`) is the offline library:
  headings of the KDOS and KFS books under `/usr/share/doc`, `kdos*` manual pages, entries of every
  Zeal docset, a typed coordinate as a `geo:` link, title suggestions from every ZIM file that
  kiwix-serve serves on 127.0.0.1:8080, and resources from Kolibri on 127.0.0.1:8081. Its rows
  rank below everything else.

One program serves both, so the library has no port of its own.

`/usr/share/kdos/xdg/krunnerrc` names `kdos` first among KRunner's favourite runners, whose
categories KRunner sorts above every other whatever a row's relevance; without it Applications and
System Settings lead and `night light` + Enter starts an application. Because the ranking is per
runner, a medium pack answers only after `install`, or `gimp` would rank the pack above the
installed GIMP. KDOS's System Settings pages are left to System Settings' own runner, which finds
them by the `X-KDE-Keywords` of their metadata; a palette row for each would be the same page
twice. `lib <words>` is a `querySingleRunner` call naming `kdos-library`, KRunner's own
single-runner mode: a `?` prefix cannot exclude other runners, and KRunner reads it as its help
trigger.

## Notes

Every row is an argument vector built word by word and executed without a shell. A box name, a
path after `why`, a search for Discover and the words of a route each reach the program as one
argument; the tests run routes whose values are `;`, `$()` and backquotes against stub programs and
check that each arrives as written. A verb that prints its answer (`update check`, `doctor`,
`why`) opens in Konsole with `--hold`; with two or more words after `-e` Konsole execs them as they
are. `box enter` runs `kdos-box enter <box> --window`: kdos-box opens its own Konsole window, in
the box's accent profile, by itself only from a terminal, and started from the runner its shell
would otherwise have none. A kdos verb run in the background is watched by a detached process, which posts the verb's
error output as a notification when it fails, so a refused `kdos theme` is never silent. The
programs a row may start are `kdos`, `kdos-box`, `kstart`, `systemsettings`, `plasma-discover`,
`konsole`, `man`, `zeal` and `xdg-open`; none is a dependency, and a missing one fails
only its row.

KRunner sends Match on every keystroke and the palette answers from memory: each file it reads is
parsed again only when its identity (device, inode, size, modification time) changes, and the
tests hold every palette match under 50 ms. The library's two servers are asked over loopback
without blocking, and that Match is answered when both have answered or 250 ms have passed,
whichever is first, so a slow server delays the library's rows and never the palette's. The book
index is built once per install of `kdos-doc` or `kdos-kfs`, which the identity of their entries
in the package database records, and is kept in `~/.cache/kdos/runner-books`. The runner builds it
in a child process, since the two books are thousands of pages and the palette answers from the
same loop; until the file is written the library answers without the books.

A match id stays good for ten seconds after KRunner's Teardown. On Wayland KRunner sends Run only
once the compositor has given it an activation token, and the Teardown it sends without waiting
can arrive first; a runner that forgot its rows at Teardown would drop the click.

kiwix-serve routes a book by its file name without `.zim`, with `_` for a space and `plus` for
`+`; the runner reads the books from `/var/lib/kiwix/library.xml`, the file the
[local servers](../../../docs/kdos/03-architecture/boot-and-init.md#the-local-servers) are started
with. A Zeal docset is searched by name prefix in its own `docSet.dsidx`, in either of the two
table layouts Zeal reads, and opened in Zeal as `<family>:<name>`. Place names are not searched:
neither Organic Maps nor Marble has an offline index another program can query.

`ask` reaches the model through the same loopback client as the library, in two exchanges:
`/v1/models` for the model to name, since llama-server runs as a router over every model under
`/usr/share/llama.cpp/models` and refuses a request that names none, then `/v1/chat/completions`
with the question, not streamed. It is sent 600 ms after the last keystroke
(`KDOS_RUNNER_ASK_QUIET_MS`), because KRunner sends Match on every keystroke and each prefix of a
question would otherwise be a request; a newer question closes the older one's socket. The
exchange has 30 seconds (`KDOS_RUNNER_ASK_TIMEOUT_MS`), but KRunner gives up on a Match reply
after its D-Bus default of 25, so the Match is answered by 20 (`KDOS_RUNNER_ASK_REPLY_MS`) with a
"still answering" row and the answer is kept ten minutes for the same question asked again. That
row's Run calls KRunner's `querySingleRunner` with the question, and the answer's Run calls
Klipper's `setClipboardContents`: two rows that run no program, each a D-Bus call from a detached
child, so the loop never waits on Plasma. `KDOS_RUNNER_LLAMA_PORT` changes the port for the tests,
whose `fakeserve.py` plays the router.

The runner ids `kdos` and `kdos-library` are an interface: `kdos menu`, the `lib` row and the
tests query a single runner by id, and krunnerrc ranks `kdos` by it. Changing either breaks every
caller that names it.

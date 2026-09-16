/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Starting a program — the one path every launch surface takes
 *
 * ONE PATH, BECAUSE A SECOND ONE IS A SURFACE THAT LAUNCHES DIFFERENTLY. Six
 * surfaces in this binary reach `sh_launch` — the Start menu, the palette, the
 * finder and the desktop's icons, the "Open with" chooser and the run box; the
 * first three arrive through `sh_apps_launch`, which records the use and hands
 * the index's entry over. A person who reaches an application by one of them
 * gets the same window the others give. A NEW SURFACE CALLS IT TOO; one that
 * splits the line for itself carries every fault below, and `panel.c`'s
 * quick-launch row, its taskbar chip and `kdos-menu` are the three that still
 * do.
 *
 * WHAT THE SHARED PATH IS FOR, in the order it matters:
 *
 *   - THE SESSION STARTS A GRAPHICAL PROGRAM, NOT THE SURFACE. On the console
 *     desktop `$KDOS_CON` names the session's surface socket, and `kcon_run`
 *     hands the vector there: the session decides between a cage-backed
 *     window and a terminal of its own, and it is the session that gives the
 *     guest a Wayland display. A vector forked here instead has no display and
 *     nothing holding it, so a boxed application draws nowhere, exits, and
 *     leaves nothing on the screen to say why.
 *   - AN `Exec=` LINE IS NOT A LIST OF WORDS. `kxdg_exec_split` reads its
 *     quoting and its field codes; a `strtok(" ")` execs a file whose name
 *     begins with a quote, and hands `%U` to a program as a document to open.
 *   - THE LINE ARRIVES AS THE ENTRY WROTE IT. A caller holding a copy keeps
 *     the field codes in it: this path spends them on the documents it is
 *     given, and reads them to decide that a line carrying none takes its
 *     documents appended instead. A pre-stripped line is indistinguishable
 *     from `Exec=xterm` here, so `--open=%f` runs as `--open=` with the path
 *     as a separate word. With no document to open the split drops every code
 *     and leaves no empty argument, so nothing needs deleting first.
 *   - NO SHELL AND NO `system()`. An Exec line comes from a file anything can
 *     write, so the words become an argument vector and never a command
 *     string.
 *
 * A HEADER OF ITS OWN, and not two lines in `shell.h`: a launch surface needs
 * this rule and nothing else about the panel, and the header that carries the
 * rule is the one a new surface is told to include.
 * ---------------------------------
 */

#ifndef SH_LAUNCH_H
#define SH_LAUNCH_H

/*
 * HOW MANY DOCUMENTS ONE LAUNCH CARRIES. The split writes the substituted
 * paths into a buffer sized for this many, and a buffer that will not hold
 * them yields NO arguments and starts nothing — so the count is a cap the
 * caller's files are clamped to rather than a promise.
 */
#define SH_LAUNCH_FILES 4

/*
 * WHAT A LAUNCH IS. Every field but `exec` may be zero, and zero everywhere
 * means "run this line the way the desktop runs anything".
 */
struct sh_launch {
	const char *exec;	/* the command, as the entry wrote it —
				   FIELD CODES INTACT, or the append
				   decision below is taken the wrong way   */
	const char *title;	/* what the session names the window; NULL
				   takes the program's own name             */
	const char *term;	/* X-KDOS-Term: the emulator the entry asked
				   for, or NULL for this session's          */
	const char *size;	/* X-KDOS-Size: COLSxROWS, or NULL          */
	int terminal;		/* Terminal=true — wrap it in an emulator   */
	int floating;		/* X-KDOS-Float: open unanchored            */
	/*
	 * A TYPED COMMAND LINE IS NOT A DESKTOP ENTRY. Its `%` is a character
	 * somebody typed and must reach the program, so no field code is
	 * expanded or dropped, and files are APPENDED rather than substituted.
	 * Quoting is still read: `run "my file"` is one argument on either
	 * setting.
	 */
	int verbatim;
};

/*
 * Start it. Returns 0 when something was started and -1 when nothing was —
 * an empty command, a line that does not fit, or a session with no terminal
 * left to give. A caller that closes itself on a launch must act on the -1,
 * or a surface disappears and no window replaces it.
 *
 * `files` are the documents to open with it: `%f`/`%u` take the first,
 * `%F`/`%U` take them all, wherever the code sits inside a word, and a line
 * with no field code at all gets them appended — which is the only way
 * `Exec=xterm` can be handed a file. THE CODES IN `exec` ARE WHAT DECIDES
 * BETWEEN THE TWO, so a caller that strips them before calling gets every
 * document appended and none substituted.
 */
int sh_launch(const struct sh_launch *l, const char *const *files, int nfiles);

#endif /* SH_LAUNCH_H */

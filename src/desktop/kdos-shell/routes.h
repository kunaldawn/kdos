/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The routes — /etc/kdos/menu.conf, see routes.c
 *
 * A ROUTE IS A NAME A SCRIPT CAN HOLD. A chord opens a surface and a person
 * clicks a row; neither is something a shell script, a documentation page or
 * another program can refer to. `kdos menu summon setup.network` is, and it
 * keeps resolving when the chord is rebound or the row moves.
 *
 * ONE READER FOR ONE FILE. Every surface that searches the system searches the
 * routes, and a second parse of `menu.conf` would be a second answer to what a
 * name resolves to — the answer differing only for the file a person edited.
 *
 * A HEADER OF ITS OWN, and not two lines in `shell.h`: that header pulls in
 * Wayland, the icon layer and the chrome band, none of which reading a
 * `name = argv` file has any use for.
 * ---------------------------------
 */

#ifndef SH_ROUTES_H
#define SH_ROUTES_H

/* Sixty-four routes, sixteen words, a name as long as a menu row's label and a
 * value long enough for a command with a `--page` on it. The whole table is
 * static: it outlives every row that points into it, and a route's argv is
 * handed to the spawner as it stands. */
#define SH_ROUTE_MAX  64
#define SH_ROUTE_ARGV 16
#define SH_ROUTE_NAME 64
#define SH_ROUTE_CMD  256

/*
 * Two copies of every value, and both are needed. `store` is split into words
 * IN PLACE, so its NULs stop a substring search at the first one; `cmd` is the
 * line as written, which is what makes the command a synonym for the route —
 * `dnd` finds `toggle.quiet`.
 */
struct sh_route {
	char name[SH_ROUTE_NAME];	  /* verb.noun                    */
	char cmd[SH_ROUTE_CMD];		  /* the value as written         */
	char store[SH_ROUTE_CMD];	  /* the split copy argv points into */
	const char *argv[SH_ROUTE_ARGV];  /* NULL-terminated              */
	int nargv;
};

/* Reads the system file and then the user's, and returns the count. Idempotent
 * — a second call re-reads nothing, so a surface that asks per keystroke costs
 * no file open. */
int sh_routes_load(void);

int sh_routes_count(void);
/* NULL outside the table. The pointer stays valid for the life of the program:
 * see SH_ROUTE_MAX. */
const struct sh_route *sh_route_at(int i);

/*
 * A `@name = value` line — a setting about the MENU rather than a route.
 * Returns the value as written, or NULL. `@` cannot begin a route name, which
 * is what keeps a setting from becoming a launchable row that runs its own
 * value as a program.
 */
const char *sh_route_setting(const char *name);

#endif /* SH_ROUTES_H */

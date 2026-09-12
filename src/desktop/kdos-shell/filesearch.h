/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Files by name or by contents, on a pipe — see filesearch.c
 *
 * A HEADER OF ITS OWN, and not two lines in `shell.h`: that header pulls in
 * Wayland, the icon layer and the chrome band, and a child on a pipe needs
 * none of them. Every surface that asks for a file asks through here, because
 * a second argument vector for `fd` would disagree with the first about hidden
 * files, ignore rules and symlinks, and the two surfaces would answer one
 * question two ways.
 * ---------------------------------
 */

#ifndef SH_FILESEARCH_H
#define SH_FILESEARCH_H

/* A line arrived from the child. tag is whatever the caller passed in,
 * so one consumer can tell names from contents. */
typedef void (*sh_fsearch_line)(const char *line, int tag, void *user);

/* Start fd over <dir> for <query>, or rga. Cancels any child already
 * running: two at once interleave their lines into one list. */
void sh_fsearch_names(const char *dir, const char *query, int tag,
                      sh_fsearch_line cb, void *user);
void sh_fsearch_contents(const char *dir, const char *query, int tag,
                         sh_fsearch_line cb, void *user);
/* The descriptor to poll, or -1 when nothing is running. */
int sh_fsearch_fd(void);
/* Read what is ready; calls the callback per whole line. Returns 1 while
 * the child is still running, 0 when it has finished and been reaped. */
int sh_fsearch_poll(void);
/* Kill and reap whatever is running. Safe when nothing is. */
void sh_fsearch_stop(void);

#endif /* SH_FILESEARCH_H */

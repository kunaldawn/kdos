/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The control centre's pages, for anything that wants to list them
 *
 * `kdos-settings` owns this list and always has: it is the program that has
 * the pages, and a second copy anywhere else is a page that exists and cannot
 * be reached, or a row that opens nothing. The palette searches the pages, so
 * it asks rather than repeating them — the same reason `kdos settings <page>`
 * passes its word through unchecked.
 *
 * A HEADER OF ITS OWN because `shell.h` drags in Wayland, and neither the list
 * nor anything that wants it needs a display.
 * ---------------------------------
 */

#ifndef SH_PAGES_H
#define SH_PAGES_H

/*
 * The pages, in the order the control centre shows them. `label` is what a
 * person reads and `name` is what `--page` takes; both are needed, because a
 * search has to match the word somebody types and then open the page it names.
 * Returns the count.
 */
int sh_settings_pages(const char *const **labels, const char *const **names);

#endif /* SH_PAGES_H */

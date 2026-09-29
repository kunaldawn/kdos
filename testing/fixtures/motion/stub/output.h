/* Stands in for kdos-comp's output.h: the fields and the two functions
 * kdos-motion.c and kdos-sched.c use. */
struct wlr_output;

struct output {
	struct wl_list link;
	struct wlr_output *wlr_output;
};
bool output_is_usable(struct output *output);
bool output_get_tearing_allowance(struct output *output);

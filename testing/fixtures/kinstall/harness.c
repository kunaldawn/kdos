/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall-core under test, with no disk and no root
 *
 *   harness ab <disk-GiB> <payload-KiB> <store-KiB> <swap-MiB> [lvm]
 *       the A/B decision for an erase plan on a disk of that size:
 *       `ab=<0|1> slot=<MiB> swap=<MiB> part_swap=<n> part_root=<n>`
 *   harness write <answers> <root>
 *       the login, look and keyboard writers into a fixture root
 *   harness xkb <share> <layout> [<variant>]
 *       the console keymap nearest a layout, out of <share>/keymaps
 *   harness check <answers>
 *       ki_answers_check's verdict: `ok`, or the refusal
 *
 * The writers are what an install runs against its target, so a fixture
 * directory with the image's files in it stands in for the target here, and
 * a wrong file is caught on the host rather than on a rig install.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kinstall-core.h"

static void say(const char *line)
{
	printf("%s\n", line);
}

int main(int argc, char **argv)
{
	char err[256];

	conf_defaults();
	if (argc >= 6 && !strcmp(argv[1], "ab")) {
		Disk *d = &ki_disk[0];

		memset(d, 0, sizeof(*d));
		kb_strlcpy(d->path, "/dev/fixture0", sizeof(d->path));
		kb_strlcpy(d->name, "fixture0", sizeof(d->name));
		d->sector_size = 512;
		d->sectors = strtoull(argv[2], NULL, 10) * 1024 * 1024 * 2;
		ki_ndisk = 1;
		kb_strlcpy(cfg.disk, d->path, sizeof(cfg.disk));
		ki_sys.payload_kb = strtoull(argv[3], NULL, 10);
		ki_sys.appbox_kb = strtoull(argv[4], NULL, 10);
		cfg.swap_mb = atol(argv[5]);
		cfg.swap = cfg.swap_mb ? SWAP_PART : SWAP_NONE;
		cfg.lvm = argc < 7 || atoi(argv[6]);
		printf("ab=%d slot=%llu swap=%llu part_swap=%d part_root=%d\n",
		       ki_ab(), ki_slot_mb(), ki_swap_lv_mb(),
		       ki_wipe_part_swap(), ki_wipe_part_root());
		return 0;
	}
	if (argc == 4 && !strcmp(argv[1], "write")) {
		if (conf_load(argv[2]) < 0) {
			fprintf(stderr, "harness: cannot read %s\n", argv[2]);
			return 2;
		}
		ki_log_hook = say;
		if (ki_write_keyboard(argv[3], err, sizeof(err)) ||
		    ki_write_login(argv[3], err, sizeof(err)) ||
		    ki_write_look(argv[3], err, sizeof(err))) {
			fprintf(stderr, "harness: %s\n", err);
			return 1;
		}
		return 0;
	}
	if ((argc == 4 || argc == 5) && !strcmp(argv[1], "xkb")) {
		char out[64];

		ki_xkb_to_vt(argv[2], argv[3], argc == 5 ? argv[4] : "", out,
			     sizeof(out));
		printf("%s\n", out);
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "check")) {
		if (conf_load(argv[2]) < 0)
			return 2;
		if (ki_answers_check(err, sizeof(err))) {
			printf("%s\n", err);
			return 1;
		}
		printf("ok\n");
		return 0;
	}
	fprintf(stderr, "usage: harness ab|write|xkb|check ...\n");
	return 2;
}

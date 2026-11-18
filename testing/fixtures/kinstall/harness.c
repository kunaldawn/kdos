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
 *   harness target <disk-MiB> <payload-KiB> <answers>
 *       ki_target_check's verdict for the answers on a disk of that size,
 *       which the answers' `disk =` is pointed at: `ok`, or the refusal
 *   harness swap <disk-MiB> <payload-KiB> <answers>
 *       ki_swap_check's verdict alone, as --dump plan's `swap_problem`
 *   harness copy <total-bytes> [<link-bytes>] < stream
 *       the Copy step's note for each `<seconds> <rsync progress2 line>`
 *       of the stream, one line each; <link-bytes> of rsync's total are
 *       hard-link names it never copies
 *   harness disksig <path>
 *       the whole-device signature of a disk image with no partitions:
 *       `fstype=<type> vg=<group>`
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

/* One probed disk of `sectors`, as the probe would leave it. */
static void fixture_disk(unsigned long long sectors)
{
	Disk *d = &ki_disk[0];

	memset(d, 0, sizeof(*d));
	kb_strlcpy(d->path, "/dev/fixture0", sizeof(d->path));
	kb_strlcpy(d->name, "fixture0", sizeof(d->name));
	d->sector_size = 512;
	d->sectors = sectors;
	ki_ndisk = 1;
	kb_strlcpy(cfg.disk, d->path, sizeof(cfg.disk));
}

int main(int argc, char **argv)
{
	char err[256];

	conf_defaults();
	if (argc == 5 && !strcmp(argv[1], "target")) {
		if (conf_load(argv[4]) < 0)
			return 2;
		fixture_disk(strtoull(argv[2], NULL, 10) * 2048);
		ki_sys.payload_kb = strtoull(argv[3], NULL, 10);
		if (ki_target_check(err, sizeof(err))) {
			printf("%s\n", err);
			return 1;
		}
		printf("ok\n");
		return 0;
	}
	if (argc == 5 && !strcmp(argv[1], "swap")) {
		if (conf_load(argv[4]) < 0)
			return 2;
		fixture_disk(strtoull(argv[2], NULL, 10) * 2048);
		ki_sys.payload_kb = strtoull(argv[3], NULL, 10);
		if (ki_swap_check(err, sizeof(err))) {
			printf("%s\n", err);
			return 1;
		}
		printf("ok\n");
		return 0;
	}
	if (argc >= 6 && !strcmp(argv[1], "ab")) {
		fixture_disk(strtoull(argv[2], NULL, 10) * 1024 * 1024 * 2);
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
	if ((argc == 3 || argc == 4) && !strcmp(argv[1], "copy")) {
		char line[512], note[128];
		unsigned long long total = strtoull(argv[2], NULL, 10);
		unsigned long long links =
			argc == 4 ? strtoull(argv[3], NULL, 10) : 0;
		KiCopyFiles files = { 0 };

		while (fgets(line, sizeof(line), stdin)) {
			char *rest;
			double el = strtod(line, &rest), frac;

			line[strcspn(line, "\n")] = 0;
			while (*rest == ' ')
				rest++;
			if (ki_copy_note(rest, el, total, links, &files, &frac,
					 note, sizeof(note)))
				printf("%s\n", note);
		}
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "disksig")) {
		Disk d;

		memset(&d, 0, sizeof(d));
		kb_strlcpy(d.path, argv[2], sizeof(d.path));
		kb_strlcpy(d.name, kb_basename(argv[2]), sizeof(d.name));
		ki_disk_signature(&d);
		printf("fstype=%s vg=%s\n", d.fstype, d.vg);
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
	fprintf(stderr, "usage: harness ab|write|xkb|check|target|swap|copy|disksig ...\n");
	return 2;
}

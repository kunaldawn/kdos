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
 *   harness seed <answers> <root> <home> <kdos>
 *       the Theme step's account run into <root><home>, with <kdos> as the
 *       program: its output, then `rc=<exit status>`
 *   harness xkb <share> <layout> [<variant>]
 *       the console keymap nearest a layout, out of <share>/keymaps
 *   harness keyname <name>
 *       exit 0 when <name> may go in EFI/kdos/keymap, 1 when it may not
 *   harness check <answers>
 *       ki_answers_check's verdict: `ok`, or the refusal
 *   harness rootcmd <answers> <cmd>
 *       how the Packs and Firmware warnings name <cmd> to the account
 *   harness target <disk-MiB> <payload-KiB> <answers>
 *       ki_target_check's verdict for the answers on a disk of that size,
 *       which the answers' `disk =` is pointed at: `ok`, or the refusal
 *   harness swap <disk-MiB> <payload-KiB> <answers>
 *       ki_swap_check's verdict alone, as --dump plan's `swap_problem`
 *   harness rootfree <disk-MiB> <payload-KiB> <answers>
 *       what the one root keeps free after the install, in MiB, and
 *       whether the Summary warns of it: `free=<MiB> low=<0|1>`, or
 *       `free=none` on the A/B layout
 *   harness copy <total-bytes> [<link-bytes> [<tree>]] < stream
 *       the Copy step's note for each `<seconds> <rsync progress2 line>`
 *       of the stream, one line each; <link-bytes> of rsync's total are
 *       hard-link names it never copies, and <tree> (live-tree.txt's
 *       format) says where in the list the cost lies; a `# prof` line
 *       in the stream, as KINSTALL_COPY_TRACE writes, says it instead,
 *       and a `# <copy> total=<bytes> links=<bytes>` line starts a new
 *       copy of those figures, printing `# <copy>` before its notes, so a
 *       trace of several copies replays each in turn
 *   harness disksig <path>
 *       the whole-device signature of a disk image with no partitions:
 *       `fstype=<type> vg=<group>`
 *   harness stick <image> [<fstype>]
 *       a written stick's image as the probe lists it, its partitions
 *       out of the image's MBR, the first holding <fstype> when named:
 *       `contents=<what the Summary names>`,
 *       then `--dump probe`'s text
 *   harness udevmodel <udev-data-file> <sysfs-model>
 *       the model the probe shows for a disk whose sysfs model is
 *       <sysfs-model> and whose udev record is the file
 *   harness reinstall
 *       a disk as a two-slot install leaves it (ESP, BIOS boot, an LVM
 *       physical volume of group kdos): its contents as the Summary names
 *       them, then the probe's JSON for it
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

/* Each run's HOME and output, so a case can see which homes the Theme step
 * wrote; its exit is the program's. */
static int seed_run(const KbArgv *a)
{
	KbBuf out = { 0 };
	int rc;

	printf("run %s\n", a->v[3]);
	rc = kb_run_capture_buf(a, &out);
	if (out.n)
		fwrite(out.p, 1, out.n, stdout);
	printf("\nrc=%d\n", rc);
	kb_buf_free(&out);
	return rc;
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
	if (argc == 5 && !strcmp(argv[1], "rootfree")) {
		long long f;

		if (conf_load(argv[4]) < 0)
			return 2;
		fixture_disk(strtoull(argv[2], NULL, 10) * 2048);
		ki_sys.payload_kb = strtoull(argv[3], NULL, 10);
		f = ki_root_free_bytes();
		if (f < 0)
			printf("free=none\n");
		else
			printf("free=%lld low=%d\n", f >> 20,
			       (unsigned long long)f < KI_ROOT_LOW_BYTES);
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
	if (argc == 5 && !strcmp(argv[1], "seed")) {
		int bad;

		if (conf_load(argv[2]) < 0) {
			fprintf(stderr, "harness: cannot read %s\n", argv[2]);
			return 2;
		}
		ki_log_hook = say;
		bad = ki_seed_accent(argv[3], argv[4], seed_run);
		printf("failed=%d\n", bad);
		return 0;
	}
	if ((argc == 4 || argc == 5) && !strcmp(argv[1], "xkb")) {
		char out[64];

		ki_xkb_to_vt(argv[2], argv[3], argc == 5 ? argv[4] : "", out,
			     sizeof(out));
		printf("%s\n", out);
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "keyname"))
		return ki_keymap_name_ok(argv[2]) ? 0 : 1;
	if (argc >= 3 && argc <= 5 && !strcmp(argv[1], "copy")) {
		/* Room for a `# prof` line's 130 figures. */
		char line[2048], note[128], what[64];
		int traced = 0;
		unsigned long long total = strtoull(argv[2], NULL, 10);
		unsigned long long links =
			argc >= 4 ? strtoull(argv[3], NULL, 10) : 0;
		KiCopyFiles files = { 0 };
		KiCopyProfile prof = { 0 };
		int ticks = getenv("KI_HARNESS_TICKS") != NULL;

		/* Each run's directories, then its files, then its later
		 * names and links, as the selftest's stream lays them out. */
		if (argc == 5) {
			FILE *f = fopen(argv[4], "r");
			unsigned char *kind = NULL;
			size_t nk = 0, cap = 0;
			unsigned long r[4];

			if (!f)
				return 2;
			while (fgets(line, sizeof(line), f)) {
				if (line[0] == '#' ||
				    sscanf(line, "%lu %lu %lu %lu", &r[0], &r[1],
					   &r[2], &r[3]) != 4)
					continue;
				if (nk + r[0] + r[1] + r[2] + r[3] > cap) {
					cap = 2 * cap + r[0] + r[1] + r[2] + r[3];
					kind = realloc(kind, cap);
					if (!kind)
						return 2;
				}
				memset(kind + nk, KI_ENT_OTHER, r[3]);
				nk += r[3];
				memset(kind + nk, KI_ENT_FILE, r[0]);
				nk += r[0];
				memset(kind + nk, KI_ENT_OTHER, r[1] + r[2]);
				nk += r[1] + r[2];
			}
			fclose(f);
			ki_copy_profile(&prof, kind, nk, 1);
			free(kind);
		}

		while (fgets(line, sizeof(line), stdin)) {
			char *rest;
			double el, frac;

			line[strcspn(line, "\n")] = 0;
			/* A KINSTALL_COPY_TRACE copy header: a new copy, of
			 * the figures it names, with nothing carried over
			 * from the one before; the marker it prints splits
			 * the notes by copy. */
			if (sscanf(line, "# %63s total=%llu links=%llu", what,
				   &total, &links) == 3) {
				memset(&files, 0, sizeof(files));
				if (traced) {
					memset(&prof, 0, sizeof(prof));
					argc = 4;
				}
				printf("# %s\n", what);
				continue;
			}
			/* A KINSTALL_COPY_TRACE profile line: the walk's own
			 * profile, which stands in for <tree>. */
			if (!strncmp(line, "# prof ", 7)) {
				char *p = line + 7, *e;

				memset(&prof, 0, sizeof(prof));
				prof.per_entry = strtod(p, &e);
				for (int i = 0; i <= KI_COPY_PROF && e != p; i++)
					prof.at[i] = strtof(p = e, &e);
				prof.ok = e != p;
				argc = 5;
				traced = 1;
				continue;
			}
			el = strtod(line, &rest);
			while (*rest == ' ')
				rest++;
			/* KI_HARNESS_TICKS set: the ticks the installer's
			 * reader gives each KI_COPY_TICK_S with no line, as
			 * `@<seconds> <note>`. */
			for (double tk = files.shown_el + KI_COPY_TICK_S;
			     ticks && files.shown_ok && tk < el;
			     tk += KI_COPY_TICK_S) {
				if (!ki_copy_tick(tk, total, links, &files, &frac,
						  note, sizeof(note)))
					break;
				printf("@%.3f %s\n", tk, note);
				tk = files.shown_el;
			}
			if (ki_copy_note(rest, el, total, links,
					 argc == 5 ? &prof : NULL, &files,
					 &frac, note, sizeof(note)))
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
	/* stick <image> [<fstype>]: the image as the probe lists a written
	 * stick, its partitions out of the MBR as sysfs would give them, the
	 * first holding <fstype> when one is named, then what the Summary
	 * names and the probe's text dump. */
	if ((argc == 3 || argc == 4) && !strcmp(argv[1], "stick")) {
		unsigned char mbr[512];
		char what[512];
		FILE *f = fopen(argv[2], "rb");
		Disk *d;

		if (!f || fread(mbr, 1, sizeof(mbr), f) != sizeof(mbr)) {
			fprintf(stderr, "harness: cannot read %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		fixture_disk((unsigned long long)ftell(f) / 512);
		fclose(f);
		d = &ki_disk[0];
		kb_strlcpy(d->path, argv[2], sizeof(d->path));
		for (int e = 0; e < 4; e++) {
			const unsigned char *m = mbr + 446 + 16 * e;
			Part *p = &d->part[d->nparts];

			if (!m[4])
				continue;
			memset(p, 0, sizeof(*p));
			snprintf(p->path, sizeof(p->path), "/dev/fixture0p%d", e + 1);
			p->start = (unsigned long long)m[8] | (unsigned long long)m[9] << 8 |
				   (unsigned long long)m[10] << 16 | (unsigned long long)m[11] << 24;
			p->sectors = (unsigned long long)m[12] | (unsigned long long)m[13] << 8 |
				     (unsigned long long)m[14] << 16 | (unsigned long long)m[15] << 24;
			if (argc == 4 && !d->nparts)
				kb_strlcpy(p->fstype, argv[3], sizeof(p->fstype));
			d->nparts++;
		}
		ki_disk_table(d);
		ki_disk_signature(d);
		kb_strlcpy(ki_sys.cpu, "Fixture CPU", sizeof(ki_sys.cpu));
		ki_sys.cores = 8;
		ki_disk_contents(d, 0, what, sizeof(what));
		printf("contents=%s\n", what);
		ki_dump_probe_text();
		return 0;
	}
	if (argc == 4 && !strcmp(argv[1], "udevmodel")) {
		char model[48];

		kb_strlcpy(model, argv[3], sizeof(model));
		ki_udev_model(argv[2], model, sizeof(model));
		printf("%s\n", model);
		return 0;
	}
	/* reinstall [<max> [<cap>]]: ki_disk_contents() naming at most
	 * <max> partitions into <cap> bytes, then the probe's dump. */
	if (argc >= 2 && argc <= 4 && !strcmp(argv[1], "reinstall")) {
		char what[512];
		int max = argc > 2 ? atoi(argv[2]) : 0;
		size_t cap = argc > 3 ? (size_t)atoi(argv[3]) : sizeof(what);
		Disk *d;
		Part *p;

		fixture_disk(30ULL << 21);
		d = &ki_disk[0];
		kb_strlcpy(d->model, "QEMU HARDDISK", sizeof(d->model));
		kb_strlcpy(d->table, "gpt", sizeof(d->table));
		p = &d->part[0];
		kb_strlcpy(p->path, "/dev/fixture0p1", sizeof(p->path));
		kb_strlcpy(p->fstype, "vfat", sizeof(p->fstype));
		kb_strlcpy(p->label, "KDOS_EFI", sizeof(p->label));
		p->start = 2048;
		p->sectors = 512ULL << 11;
		p->is_esp = 1;
		p = &d->part[1];
		kb_strlcpy(p->path, "/dev/fixture0p2", sizeof(p->path));
		p->start = 2048 + (512ULL << 11);
		p->sectors = 2048;
		p->is_bios_boot = 1;
		p = &d->part[2];
		kb_strlcpy(p->path, "/dev/fixture0p3", sizeof(p->path));
		kb_strlcpy(p->fstype, "LVM2_member", sizeof(p->fstype));
		kb_strlcpy(p->vg, "kdos", sizeof(p->vg));
		p->start = 4096 + (512ULL << 11);
		p->sectors = d->sectors - p->start - 34;
		d->nparts = 3;
		if (cap > sizeof(what))
			cap = sizeof(what);
		ki_disk_contents(d, max, what, cap);
		printf("contents=%s\n", what);
		ki_dump_probe_json();
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
	if (argc == 4 && !strcmp(argv[1], "rootcmd")) {
		if (conf_load(argv[2]) < 0)
			return 2;
		printf("%s\n", ki_root_cmd(argv[3]));
		return 0;
	}
	fprintf(stderr, "usage: harness ab|write|xkb|check|rootcmd|target|swap|rootfree|copy|disksig|udevmodel|reinstall ...\n");
	return 2;
}

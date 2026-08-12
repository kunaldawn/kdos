/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall --probe / --plan — what it sees, and what it would do
 *
 * The installer is the one program here that CANNOT be tested by running it:
 * a real run repartitions a disk. `--dry-run` exists for that and still needs
 * a terminal and a person to click through ten pages, which means the two
 * questions worth asking of an installer before trusting it —
 *
 *   what does it think this machine IS?
 *   what would it DO with these answers?
 *
 * — could only be answered by watching it. Both are answered here, headless,
 * before ktui_term_init() is ever called and before anything is written.
 *
 * ONE TRAVERSAL, TWO RENDERINGS. `--json` is a flag on these dumps, not a
 * second walk over the same data: the loops below branch on it per field. A
 * separate JSON writer would be a second description of the machine, and the
 * defect it would hide — a disk the text form lists and the JSON form does not
 * — is exactly the kind of disagreement that makes a dump untrustworthy.
 *
 * `--probe` is also the right thing to paste into a bug report, which is why
 * the text form exists at all rather than JSON alone.
 */

#include <stdio.h>
#include <string.h>

#include "kinstall.h"

static const char *state_name(int st)
{
	switch (st) {
	case ST_PENDING: return "pending";
	case ST_RUNNING: return "running";
	case ST_DONE:    return "done";
	case ST_FAIL:    return "failed";
	case ST_SKIP:    return "skipped";
	}
	return "?";
}

/* Sectors are what /sys reports and 512 is what it reports them IN, whatever
 * the drive's physical sector size is — the same conversion the pages use. */
static unsigned long long disk_mb(const Disk *d)
{
	return d->sectors / 2048;
}

/* ── probe ─────────────────────────────────────────────────────────────── */

static void probe_text(void)
{
	printf("firmware      %s%s\n", ki_sys.uefi ? "UEFI" : "BIOS",
	       ki_sys.secure_boot ? " (secure boot on)" : "");
	printf("cpu           %s (%d cores)\n", ki_sys.cpu, ki_sys.cores);
	printf("memory        %llu MB\n", ki_sys.mem_kb / 1024);
	printf("session       %s\n", ki_sys.live ? "live" : "installed");
	printf("payload       %llu MB (appbox %llu MB)\n",
	       ki_sys.payload_kb / 1024, ki_sys.appbox_kb / 1024);
	printf("disks         %d\n", ki_ndisk);

	for (int i = 0; i < ki_ndisk; i++) {
		const Disk *d = &ki_disk[i];
		printf("\n  %-10s %8llu MB  %-6s %-4s %s%s%s%s\n", d->path,
		       disk_mb(d), d->tran, d->table,
		       d->model[0] ? d->model : "-",
		       d->removable ? "  removable" : "",
		       d->readonly ? "  read-only" : "",
		       d->is_boot_media ? "  BOOT MEDIA" : "");
		for (int k = 0; k < d->nparts; k++) {
			const Part *p = &d->part[k];
			printf("    %-12s %8llu MB  %-8s %-16s%s%s\n", p->path,
			       p->sectors / 2048,
			       p->fstype[0] ? p->fstype : "-",
			       p->label[0] ? p->label : "",
			       p->is_esp ? "  ESP" : "",
			       p->mounted ? "  mounted" : "");
		}
	}
}

static void probe_json(void)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "{\n  \"firmware\": \"%s\", \"secure_boot\": %s,\n",
		      ki_sys.uefi ? "uefi" : "bios",
		      ki_sys.secure_boot ? "true" : "false");
	kb_buf_str(&b, "  \"cpu\": ");
	kb_json_str(&b, ki_sys.cpu);
	kb_buf_printf(&b, ", \"cores\": %d, \"memory_kb\": %llu,\n",
		      ki_sys.cores, ki_sys.mem_kb);
	kb_buf_printf(&b, "  \"live\": %s, \"payload_kb\": %llu, "
		      "\"appbox_kb\": %llu,\n",
		      ki_sys.live ? "true" : "false", ki_sys.payload_kb,
		      ki_sys.appbox_kb);
	kb_buf_str(&b, "  \"disks\": [");

	for (int i = 0; i < ki_ndisk; i++) {
		const Disk *d = &ki_disk[i];
		kb_buf_printf(&b, "%s\n    {\"path\": ", i ? "," : "");
		kb_json_str(&b, d->path);
		kb_buf_str(&b, ", \"model\": ");
		kb_json_str(&b, d->model);
		kb_buf_str(&b, ", \"transport\": ");
		kb_json_str(&b, d->tran);
		kb_buf_str(&b, ", \"table\": ");
		kb_json_str(&b, d->table);
		kb_buf_printf(&b, ",\n     \"sectors\": %llu, "
			      "\"sector_size\": %d, \"rotational\": %s, "
			      "\"removable\": %s, \"readonly\": %s, "
			      "\"boot_media\": %s,\n",
			      d->sectors, d->sector_size,
			      d->rotational ? "true" : "false",
			      d->removable ? "true" : "false",
			      d->readonly ? "true" : "false",
			      d->is_boot_media ? "true" : "false");
		kb_buf_str(&b, "     \"partitions\": [");
		for (int k = 0; k < d->nparts; k++) {
			const Part *p = &d->part[k];
			kb_buf_printf(&b, "%s\n       {\"path\": ",
				      k ? "," : "");
			kb_json_str(&b, p->path);
			kb_buf_printf(&b, ", \"start\": %llu, \"sectors\": %llu",
				      p->start, p->sectors);
			kb_buf_str(&b, ", \"fstype\": ");
			kb_json_str(&b, p->fstype);
			kb_buf_str(&b, ", \"label\": ");
			kb_json_str(&b, p->label);
			kb_buf_str(&b, ", \"uuid\": ");
			kb_json_str(&b, p->uuid);
			kb_buf_printf(&b, ", \"esp\": %s, \"mounted\": %s",
				      p->is_esp ? "true" : "false",
				      p->mounted ? "true" : "false");
			kb_buf_str(&b, ", \"mountpoint\": ");
			kb_json_str(&b, p->mountpoint);
			kb_buf_str(&b, "}");
		}
		kb_buf_printf(&b, "%s]}", d->nparts ? "\n     " : "");
	}
	kb_buf_printf(&b, "%s]\n}\n", ki_ndisk ? "\n  " : "");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
}

/* ── plan ──────────────────────────────────────────────────────────────── */

static const char *plan_name(void)
{
	switch (cfg.plan) {
	case PLAN_WIPE:  return "wipe";
	case PLAN_REUSE: return "reuse";
	case PLAN_MANUAL: return "manual";
	}
	return "?";
}

static const char *swap_name(void)
{
	switch (cfg.swap) {
	case SWAP_NONE: return "none";
	case SWAP_FILE: return "file";
	case SWAP_PART: return "partition";
	}
	return "?";
}

/*
 * The services a plan turns OFF, by name. `svc_off` is a bitmask over
 * ki_services and a consumer cannot resolve it without that table, so the bit
 * numbers stay out of the dump entirely.
 */
static void plan_services(KbBuf *b, int json)
{
	int first = 1;
	for (int i = 0; i < ki_nservices; i++) {
		if (!(cfg.svc_off & (1u << i)))
			continue;
		if (json) {
			kb_buf_str(b, first ? "" : ", ");
			kb_json_str(b, ki_services[i].name);
		} else {
			kb_buf_printf(b, "%s%s", first ? "" : " ",
				      ki_services[i].name);
		}
		first = 0;
	}
}

static void plan_dump(int json)
{
	/* The same call the wizard makes on its way to the install page, so
	 * the steps listed here are the steps that would run — including which
	 * ones these answers skip. */
	install_plan();

	KbBuf svc = {0};
	plan_services(&svc, json);

	if (!json) {
		printf("keymap        %s\n", cfg.keymap);
		printf("timezone      %s (%s)\n", cfg.tz_label, cfg.tz);
		printf("disk          %s   plan %s   fs %s\n", cfg.disk,
		       plan_name(), cfg.fstype);
		printf("esp           %s%s\n", cfg.part_esp,
		       cfg.format_esp ? " (format)" : "");
		printf("root          %s\n", cfg.part_root);
		printf("swap          %s", swap_name());
		if (cfg.swap != SWAP_NONE)
			printf(" %ld MB", cfg.swap_mb);
		printf("\n");
		printf("hostname      %s\n", cfg.hostname);
		printf("user          %s%s\n", cfg.username,
		       cfg.user_wheel ? " (wheel)" : "");
		printf("root account  %s\n", cfg.root_locked ? "locked"
							     : "password set");
		printf("theme         %s   appbox %s\n", cfg.theme,
		       cfg.with_appbox ? "yes" : "no");
		printf("services off  %s\n", svc.n ? svc.p : "-");
		printf("dry run       %s\n", cfg.dry_run ? "yes" : "no");
		printf("\nsteps\n");
		for (int i = 0; i < inst.nsteps; i++)
			printf("  %-2d %-24s %-8s %s\n", i + 1,
			       inst.step[i].title, state_name(inst.step[i].state),
			       inst.step[i].detail);
		kb_buf_free(&svc);
		return;
	}

	KbBuf b = {0};
	kb_buf_str(&b, "{\n  \"keymap\": ");
	kb_json_str(&b, cfg.keymap);
	kb_buf_str(&b, ", \"timezone\": ");
	kb_json_str(&b, cfg.tz);
	kb_buf_str(&b, ", \"timezone_label\": ");
	kb_json_str(&b, cfg.tz_label);
	kb_buf_str(&b, ",\n  \"disk\": ");
	kb_json_str(&b, cfg.disk);
	kb_buf_printf(&b, ", \"plan\": \"%s\"", plan_name());
	kb_buf_str(&b, ", \"fstype\": ");
	kb_json_str(&b, cfg.fstype);
	kb_buf_str(&b, ",\n  \"esp\": ");
	kb_json_str(&b, cfg.part_esp);
	kb_buf_printf(&b, ", \"format_esp\": %s",
		      cfg.format_esp ? "true" : "false");
	kb_buf_str(&b, ", \"root\": ");
	kb_json_str(&b, cfg.part_root);
	kb_buf_printf(&b, ",\n  \"swap\": \"%s\", \"swap_mb\": %ld",
		      swap_name(), cfg.swap_mb);
	kb_buf_str(&b, ",\n  \"hostname\": ");
	kb_json_str(&b, cfg.hostname);
	kb_buf_str(&b, ", \"username\": ");
	kb_json_str(&b, cfg.username);
	kb_buf_printf(&b, ", \"wheel\": %s, \"root_locked\": %s",
		      cfg.user_wheel ? "true" : "false",
		      cfg.root_locked ? "true" : "false");
	kb_buf_str(&b, ",\n  \"theme\": ");
	kb_json_str(&b, cfg.theme);
	kb_buf_printf(&b, ", \"appbox\": %s, \"dry_run\": %s",
		      cfg.with_appbox ? "true" : "false",
		      cfg.dry_run ? "true" : "false");
	kb_buf_str(&b, ",\n  \"services_off\": [");
	if (svc.n)
		kb_buf_str(&b, svc.p);
	kb_buf_str(&b, "],\n  \"steps\": [");
	for (int i = 0; i < inst.nsteps; i++) {
		kb_buf_printf(&b, "%s\n    {\"index\": %d, \"title\": ",
			      i ? "," : "", i + 1);
		kb_json_str(&b, inst.step[i].title);
		kb_buf_str(&b, ", \"detail\": ");
		kb_json_str(&b, inst.step[i].detail);
		kb_buf_printf(&b, ", \"state\": \"%s\"}",
			      state_name(inst.step[i].state));
	}
	kb_buf_printf(&b, "%s]\n}\n", inst.nsteps ? "\n  " : "");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	kb_buf_free(&svc);
}

/*
 * NO PASSWORD REACHES EITHER FORM. cfg carries `userpass` and `rootpass` in
 * the clear — it has to, it is about to call crypt() — and a dump is the kind
 * of output that ends up pasted into a bug report or a CI log. Whether the
 * account HAS a password is the answer to every question the dump exists for.
 */
int ki_dump(const char *what, int json)
{
	if (!strcmp(what, "probe")) {
		probe_system();
		probe_disks();
		if (json)
			probe_json();
		else
			probe_text();
		return 0;
	}
	if (!strcmp(what, "plan")) {
		plan_dump(json);
		return 0;
	}
	fprintf(stderr, "kinstall: --dump takes 'probe' or 'plan'\n");
	return 2;
}

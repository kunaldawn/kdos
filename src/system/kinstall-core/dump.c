/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall --dump — what it sees, what it would do, and what it offers
 *
 * The installer is the one program here that CANNOT be tested by running it:
 * a real run repartitions a disk. `--dry-run` exists for that and still needs
 * a front end and a person to click through its pages, which means the two
 * questions worth asking of an installer before trusting it —
 *
 *   what does it think this machine IS?
 *   what would it DO with these answers?
 *
 * — could only be answered by watching it. Both are answered here, headless,
 * before any front end takes the screen and before anything is written. The
 * graphical installer builds its pages from the same dumps: `probe` for the
 * disks, `apps` for the groups, `look` for the accents and desks, and `plan`
 * for its summary, so it offers exactly what this backend accepts.
 *
 * ONE TRAVERSAL, TWO RENDERINGS. `--json` is a flag on these dumps, not a
 * second walk over the same data: the loops below branch on it per field. A
 * separate JSON writer would be a second description of the machine, and the
 * defect it would hide — a disk the text form lists and the JSON form does not
 * — is exactly the kind of disagreement that makes a dump untrustworthy.
 *
 * `--dump probe` is also the right thing to paste into a bug report, which is why
 * the text form exists at all rather than JSON alone.
 */

#include <stdio.h>
#include <string.h>

#include "kinstall-core.h"

/* Sectors are what /sys reports and 512 is what it reports them IN, whatever
 * the drive's physical sector size is — the same conversion the pages use. */
static unsigned long long disk_mb(const Disk *d)
{
	return d->sectors / 2048;
}

/* ── probe ─────────────────────────────────────────────────────────────── */

static void probe_text(void)
{
	printf("firmware      %s%s%s\n", ki_sys.uefi ? "UEFI" : "BIOS",
	       ki_sys.fw_bits == 32 ? " (32-bit)" : "",
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
			printf("    %-12s %8llu MB  %-8s %-16s%s%s%s\n", p->path,
			       p->sectors / 2048,
			       p->fstype[0] ? p->fstype : "-",
			       p->label[0] ? p->label : "",
			       p->is_esp ? "  ESP" : "",
			       p->is_bios_boot ? "  BIOS boot" : "",
			       p->mounted ? "  mounted" : "");
		}
	}

	printf("\nlogical vols  %d\n", ki_nlv);
	for (int i = 0; i < ki_nlv; i++) {
		const Lv *l = &ki_lv[i];
		printf("  %-24s %8llu MB  %-8s %-16s%s%s\n", l->path,
		       l->sectors / 2048, l->fstype[0] ? l->fstype : "-",
		       l->label[0] ? l->label : "", l->held ? "  held" : "",
		       l->mounted ? "  mounted" : "");
	}
}

static void probe_json(void)
{
	KbBuf b = {0};

	kb_buf_printf(&b, "{\n  \"firmware\": \"%s\", \"firmware_bits\": %d, "
		      "\"secure_boot\": %s,\n",
		      ki_sys.uefi ? "uefi" : "bios", ki_sys.fw_bits,
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
			kb_buf_printf(&b, ", \"esp\": %s, \"bios_boot\": %s, "
				      "\"mounted\": %s",
				      p->is_esp ? "true" : "false",
				      p->is_bios_boot ? "true" : "false",
				      p->mounted ? "true" : "false");
			kb_buf_str(&b, ", \"mountpoint\": ");
			kb_json_str(&b, p->mountpoint);
			kb_buf_str(&b, "}");
		}
		kb_buf_printf(&b, "%s]}", d->nparts ? "\n     " : "");
	}
	kb_buf_printf(&b, "%s],\n  \"logical_volumes\": [",
		      ki_ndisk ? "\n  " : "");
	for (int i = 0; i < ki_nlv; i++) {
		const Lv *l = &ki_lv[i];
		kb_buf_printf(&b, "%s\n    {\"path\": ", i ? "," : "");
		kb_json_str(&b, l->path);
		kb_buf_str(&b, ", \"vg\": ");
		kb_json_str(&b, l->vg);
		kb_buf_str(&b, ", \"lv\": ");
		kb_json_str(&b, l->lv);
		kb_buf_printf(&b, ", \"sectors\": %llu", l->sectors);
		kb_buf_str(&b, ", \"fstype\": ");
		kb_json_str(&b, l->fstype);
		kb_buf_str(&b, ", \"label\": ");
		kb_json_str(&b, l->label);
		kb_buf_str(&b, ", \"uuid\": ");
		kb_json_str(&b, l->uuid);
		kb_buf_printf(&b, ", \"held\": %s, \"mounted\": %s",
			      l->held ? "true" : "false",
			      l->mounted ? "true" : "false");
		kb_buf_str(&b, ", \"mountpoint\": ");
		kb_json_str(&b, l->mountpoint);
		kb_buf_str(&b, "}");
	}
	kb_buf_printf(&b, "%s]\n}\n", ki_nlv ? "\n  " : "");
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
	/*
	 * THE MACHINE IS PROBED AS THE INSTALL PROBES IT, or the plan answers
	 * about a disk it never looked at: the firmware decides the BIOS
	 * boot code, the disk's size and the payload decide how much of the
	 * volume group slot A takes, and a reused disk's partitions decide
	 * whether it can start on legacy BIOS. The payload is the slow probe —
	 * a walk of the whole root filesystem — and it is asked for only by
	 * the plan whose answer depends on it.
	 */
	probe_system();
	probe_disks();
	if (cfg.plan == PLAN_WIPE && cfg.lvm && disk_by_path(cfg.disk))
		probe_payload();
	char problem[256] = "";
	int bad = ki_answers_check(problem, sizeof(problem));

	/* The scan FIRST: whether the Packs step runs at all is a question
	 * about the medium, and install_plan() answers it. Planning before
	 * scanning reports the step skipped on a machine that would carry
	 * three gigabytes of applications. */
	ki_apps_enter();
	/* The same call the wizard makes on its way to the install page, so
	 * the steps listed here are the steps that would run — including which
	 * ones these answers skip. */
	install_plan();

	KbBuf svc = {0};
	plan_services(&svc, json);

	/* The partitions by the names the install will use: the erase plan's
	 * come from its layout, a reuse plan's from the answers. */
	char esp[96], root[192], bbn[96] = "";
	int bb = ki_bios_boot_part();

	if (cfg.plan == PLAN_WIPE) {
		ki_part_name(cfg.disk, KI_PART_ESP, esp, sizeof(esp));
		ki_part_name(cfg.disk, ki_wipe_part_root(), root, sizeof(root));
	} else {
		kb_strlcpy(esp, cfg.part_esp, sizeof(esp));
		kb_strlcpy(root, cfg.part_root, sizeof(root));
	}
	if (bb > 0)
		ki_part_name(cfg.disk, bb, bbn, sizeof(bbn));

	if (!json) {
		printf("keymap        %s\n", cfg.keymap);
		printf("timezone      %s (%s)\n", cfg.tz_label, cfg.tz);
		printf("disk          %s   plan %s   fs %s\n", cfg.disk,
		       plan_name(), cfg.fstype);
		/* What the filesystem choice actually turns into. A dump is
		 * what ends up in a bug report, and "fs btrfs" alone does not
		 * say which mkfs will run or what lands in fstab. */
		printf("mkfs          %s %s -L KDOS\n", ki_fs(cfg.fstype)->mkfs,
		       ki_fs(cfg.fstype)->force);
		printf("fstab root    %s %s 0 %d\n", cfg.fstype,
		       ki_fs(cfg.fstype)->opts, ki_fs(cfg.fstype)->passno);
		printf("esp           %s%s\n", esp,
		       cfg.format_esp ? " (format)" : "");
		if (bb > 0)
			printf("bios boot     %s%s\n", bbn,
			       cfg.plan == PLAN_WIPE ? ", 1 MiB, made by the "
						       "layout"
						     : "");
		else if (bb == 0)
			printf("bios boot     the gap after the MBR\n");
		else
			printf("bios boot     none: the disk starts on UEFI "
			       "only\n");
		if (cfg.plan == PLAN_WIPE && cfg.lvm && ki_ab()) {
			printf("root          lvm %s/%s on %s, %llu MiB%s\n",
			       KI_VG, KI_LV_A, root, ki_slot_mb(),
			       cfg.luks ? ", inside LUKS2" : "");
			printf("slot b        lvm %s/%s, %llu MiB, a copy of "
			       "slot a\n", KI_VG, KI_LV_B, ki_slot_mb());
			if (ki_swap_lv_mb())
				printf("swap volume   lvm %s/%s, %llu MiB\n",
				       KI_VG, KI_LV_SWAP, ki_swap_lv_mb());
			printf("home          lvm %s/%s, the rest, shared by "
			       "both slots\n", KI_VG, KI_LV_HOME);
			printf("pack store    %s on the home volume, bound at "
			       "/var/lib/kdos/packs\n", KI_SHARED_PACKS);
		} else if (cfg.plan == PLAN_WIPE && cfg.lvm)
			printf("root          lvm %s/%s on %s, all of the group "
			       "(-l 100%%FREE)%s; no room for a second slot\n",
			       KI_VG, KI_LV_A, root,
			       cfg.luks ? ", inside LUKS2" : "");
		else
			printf("root          %s%s\n", root,
			       cfg.luks ? ", inside LUKS2" : "");
		printf("swap          %s", swap_name());
		if (cfg.swap != SWAP_NONE)
			printf(" %ld MB", cfg.swap_mb);
		printf("\n");
		printf("hostname      %s\n", cfg.hostname);
		if (cfg.oem)
			printf("user          none: the first boot sets one up "
			       "(OEM)\n");
		else
			printf("user          %s%s\n", cfg.username,
			       cfg.user_wheel ? " (wheel)" : "");
		printf("root account  %s\n", cfg.root_locked ? "locked"
							     : "password set");
		printf("login         %s\n",
		       cfg.oem ? "plasma-setup on the first boot"
		       : !cfg.autologin ? "the login screen"
		       : cfg.autologin_lock ? "automatic, into a locked session"
					    : "automatic");
		printf("keyboard      %s%s%s%s%s\n", cfg.keymap,
		       cfg.xkb_layout[0] ? ", session " : "", cfg.xkb_layout,
		       cfg.xkb_variant[0] ? " " : "", cfg.xkb_variant);
		printf("theme         %s   desk %s   appbox %s\n", cfg.theme,
		       cfg.desk, cfg.with_appbox ? "yes" : "no");
		printf("services off  %s\n", svc.n ? svc.p : "-");
		/* THE APPLICATIONS, WHAT THEY COST AND HOW THEY WILL ARRIVE.
		 * `plan` is what somebody pastes into a bug report, and the
		 * ROUTE is the part that decides whether anything is installed
		 * at all — a reader who saw only the selection could not tell
		 * an install that built them from one that recorded them. */
		if (ki_apps_present) {
			static const char *const ROUTE[] = {
				"nothing chosen",
				"import, from the archive below",
				"built during the install, over the network",
				"recorded; the first session offers them",
			};
			int r = ki_apps_route();

			printf("apps          %s\n",
			       cfg.apps[0] ? cfg.apps : "(none chosen)");
			printf("apps route    %s\n", ROUTE[r]);
			if (r == APPS_IMPORT)
				printf("apps archive  %s\n", ki_apps_archive);
			/* AN ESTIMATE, and it says so: what apt resolves on the
			 * day depends on the snapshot, and shared runtime
			 * layers are stored once however many applications
			 * name them. */
			printf("apps cost     %s (an estimate)\n",
			       kb_human_size(ki_apps_bytes()));
		}
		printf("dry run       %s\n", cfg.dry_run ? "yes" : "no");
		if (bad)
			printf("refused       %s\n", problem);
		printf("\nsteps\n");
		for (int i = 0; i < inst.nsteps; i++)
			printf("  %-2d %-24s %-8s %s\n", i + 1,
			       inst.step[i].title, ki_state_name(inst.step[i].state),
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
	kb_buf_str(&b, ", \"mkfs\": ");
	kb_json_str(&b, ki_fs(cfg.fstype)->mkfs);
	kb_buf_str(&b, ", \"fstab_options\": ");
	kb_json_str(&b, ki_fs(cfg.fstype)->opts);
	kb_buf_printf(&b, ", \"fstab_passno\": %d", ki_fs(cfg.fstype)->passno);
	kb_buf_str(&b, ",\n  \"esp\": ");
	kb_json_str(&b, esp);
	kb_buf_printf(&b, ", \"format_esp\": %s",
		      cfg.format_esp ? "true" : "false");
	/* The partition's node, "mbr" for the gap after a dos table's boot
	 * sector, or null: no BIOS boot code, a disk that starts on UEFI
	 * only. */
	kb_buf_str(&b, ", \"bios_boot\": ");
	if (bb > 0)
		kb_json_str(&b, bbn);
	else
		kb_buf_str(&b, bb == 0 ? "\"mbr\"" : "null");
	kb_buf_str(&b, ", \"root\": ");
	kb_json_str(&b, root);
	kb_buf_printf(&b, ", \"luks\": %s", cfg.luks ? "true" : "false");
	/* The A/B layout's volumes in MiB, or the one root's when the disk
	 * holds only that; null off LVM. `ab` is the answer to "will updates
	 * have a second slot", which a front end must say before the install. */
	if (cfg.plan == PLAN_WIPE && cfg.lvm)
		kb_buf_printf(&b, ", \"lvm\": {\"vg\": \"%s\", \"lv\": \"%s\", "
			      "\"ab\": %s, \"slot_mb\": %llu, \"swap_mb\": %llu, "
			      "\"home\": %s}", KI_VG, KI_LV_A,
			      ki_ab() ? "true" : "false",
			      ki_ab() ? ki_slot_mb() : 0,
			      ki_ab() ? ki_swap_lv_mb() : 0,
			      ki_ab() ? "\"" KI_VG "/" KI_LV_HOME "\"" : "null");
	else
		kb_buf_str(&b, ", \"lvm\": null");
	kb_buf_printf(&b, ", \"ab\": %s", ki_ab() ? "true" : "false");
	kb_buf_printf(&b, ",\n  \"swap\": \"%s\", \"swap_mb\": %ld",
		      swap_name(), cfg.swap_mb);
	kb_buf_str(&b, ",\n  \"hostname\": ");
	kb_json_str(&b, cfg.hostname);
	kb_buf_printf(&b, ", \"oem\": %s", cfg.oem ? "true" : "false");
	kb_buf_str(&b, ", \"username\": ");
	if (cfg.oem)
		kb_buf_str(&b, "null");
	else
		kb_json_str(&b, cfg.username);
	kb_buf_str(&b, ", \"fullname\": ");
	kb_json_str(&b, cfg.oem ? "" : cfg.fullname);
	kb_buf_printf(&b, ", \"wheel\": %s, \"root_locked\": %s",
		      cfg.user_wheel ? "true" : "false",
		      cfg.root_locked ? "true" : "false");
	kb_buf_printf(&b, ", \"autologin\": %s, \"autologin_lock\": %s",
		      cfg.autologin && !cfg.oem ? "true" : "false",
		      cfg.autologin_lock ? "true" : "false");
	kb_buf_str(&b, ",\n  \"xkb_layout\": ");
	kb_json_str(&b, cfg.xkb_layout);
	kb_buf_str(&b, ", \"xkb_variant\": ");
	kb_json_str(&b, cfg.xkb_variant);
	kb_buf_str(&b, ",\n  \"theme\": ");
	kb_json_str(&b, cfg.theme);
	kb_buf_str(&b, ", \"desk\": ");
	kb_json_str(&b, cfg.desk);
	kb_buf_printf(&b, ", \"appbox\": %s, \"dry_run\": %s",
		      cfg.with_appbox ? "true" : "false",
		      cfg.dry_run ? "true" : "false");
	kb_buf_str(&b, ",\n  \"apps\": [");
	{
		static const char *const ROUTE[] = { "none", "import",
						     "network", "pending" };
		int first = 1;

		for (int i = 0; i < ki_ngroup; i++) {
			if (!ki_group[i].chosen)
				continue;
			kb_buf_printf(&b, "%s\n    {\"id\": ", first ? "" : ",");
			first = 0;
			kb_json_str(&b, ki_group[i].id);
			kb_buf_printf(&b, ", \"apps\": %d", ki_group[i].napp);
			kb_buf_printf(&b, ", \"bytes\": %llu}",
				      ki_group[i].bytes);
		}
		/* ONE TRAVERSAL, TWO RENDERINGS: the same groups the text form
		 * lists, so a disagreement between the two cannot happen. */
		kb_buf_printf(&b, "%s], \"apps_bytes\": %llu",
			      first ? "" : "\n  ", ki_apps_bytes());
		kb_buf_str(&b, ", \"apps_route\": ");
		kb_json_str(&b, ROUTE[ki_apps_route()]);
	}
	kb_buf_str(&b, ",\n  \"services_off\": [");
	if (svc.n)
		kb_buf_str(&b, svc.p);
	/* The first problem that would refuse these answers, or null: the
	 * same check the install makes before it writes anything. */
	kb_buf_str(&b, "],\n  \"refused\": ");
	if (bad)
		kb_json_str(&b, problem);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ",\n  \"steps\": [");
	for (int i = 0; i < inst.nsteps; i++) {
		kb_buf_printf(&b, "%s\n    {\"index\": %d, \"title\": ",
			      i ? "," : "", i + 1);
		kb_json_str(&b, inst.step[i].title);
		kb_buf_str(&b, ", \"detail\": ");
		kb_json_str(&b, inst.step[i].detail);
		kb_buf_printf(&b, ", \"state\": \"%s\"}",
			      ki_state_name(inst.step[i].state));
	}
	kb_buf_printf(&b, "%s]\n}\n", inst.nsteps ? "\n  " : "");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	kb_buf_free(&svc);
}

/* ── apps ──────────────────────────────────────────────────────────────── */

/* Every group the medium's catalogue offers, ticked as these answers tick
 * them, and the route the ticked ones would take. */
static void apps_dump(int json)
{
	static const char *const ROUTE[] = { "none", "import", "network",
					     "pending" };
	KbBuf b = {0};

	ki_apps_enter();
	if (!json) {
		printf("catalogue     %s\n", ki_apps_present ? "present" : "none");
		for (int i = 0; i < ki_ngroup; i++)
			printf("  [%c] %-16s %3d apps %9s  %s\n",
			       ki_group[i].chosen ? 'x' : ' ', ki_group[i].id,
			       ki_group[i].napp,
			       kb_human_size(ki_group[i].bytes),
			       ki_group[i].desc);
		printf("route         %s\n", ROUTE[ki_apps_route()]);
		if (ki_apps_archive[0])
			printf("archive       %s\n", ki_apps_archive);
		return;
	}
	kb_buf_printf(&b, "{\n  \"present\": %s, \"route\": \"%s\", "
		      "\"archive\": ",
		      ki_apps_present ? "true" : "false", ROUTE[ki_apps_route()]);
	kb_json_str(&b, ki_apps_archive);
	kb_buf_str(&b, ",\n  \"groups\": [");
	for (int i = 0; i < ki_ngroup; i++) {
		kb_buf_printf(&b, "%s\n    {\"id\": ", i ? "," : "");
		kb_json_str(&b, ki_group[i].id);
		kb_buf_str(&b, ", \"desc\": ");
		kb_json_str(&b, ki_group[i].desc);
		kb_buf_printf(&b, ", \"apps\": %d, \"bytes\": %llu, "
			      "\"chosen\": %s}",
			      ki_group[i].napp, ki_group[i].bytes,
			      ki_group[i].chosen ? "true" : "false");
	}
	kb_buf_printf(&b, "%s]\n}\n", ki_ngroup ? "\n  " : "");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
}

/* ── look ──────────────────────────────────────────────────────────────── */

/* The accents libkcolor knows, which are the names `theme =` accepts and the
 * global themes org.kdos.<name>.desktop that kdos-plasma-look installs, and
 * the desks `desk =` accepts. */
static void look_dump(int json)
{
	KbBuf b = {0};

	if (!json) {
		for (int i = 0; i < kcol_nscheme; i++)
			printf("accent        %-10s %s%s\n", kcol_schemes[i].name,
			       kcol_schemes[i].label,
			       !strcmp(kcol_schemes[i].name, KCOL_DEFAULT_NAME)
				       ? "  (default)"
				       : "");
		for (int i = 0; i < ki_ndesks; i++)
			printf("desk          %s%s\n", ki_desks[i],
			       i ? "" : "  (default)");
		return;
	}
	kb_buf_str(&b, "{\n  \"accents\": [");
	for (int i = 0; i < kcol_nscheme; i++) {
		kb_buf_printf(&b, "%s\n    {\"name\": ", i ? "," : "");
		kb_json_str(&b, kcol_schemes[i].name);
		kb_buf_str(&b, ", \"label\": ");
		kb_json_str(&b, kcol_schemes[i].label);
		kb_buf_str(&b, ", \"lookandfeel\": ");
		{
			char lnf[96];

			snprintf(lnf, sizeof(lnf), "org.kdos.%s.desktop",
				 kcol_schemes[i].name);
			kb_json_str(&b, lnf);
		}
		kb_buf_printf(&b, ", \"default\": %s}",
			      !strcmp(kcol_schemes[i].name, KCOL_DEFAULT_NAME)
				      ? "true"
				      : "false");
	}
	kb_buf_str(&b, "\n  ],\n  \"desks\": [");
	for (int i = 0; i < ki_ndesks; i++) {
		kb_buf_str(&b, i ? ", " : "");
		kb_json_str(&b, ki_desks[i]);
	}
	kb_buf_str(&b, "]\n}\n");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
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
		probe_payload();
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
	if (!strcmp(what, "apps")) {
		apps_dump(json);
		return 0;
	}
	if (!strcmp(what, "look")) {
		look_dump(json);
		return 0;
	}
	fprintf(stderr, "kinstall: --dump takes 'probe', 'plan', 'apps' or "
			"'look'\n");
	return 2;
}

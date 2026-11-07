/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   decidetest — kdos-notice's facts and decisions with no bus
 *
 *   decidetest restarts FILE      `kdos restarts --json` as FILE says
 *   decidetest cve FILE           `kdos cve --json`
 *   decidetest boot FILE          `kdos-bootctl status --json`, both the
 *                                 rollback and the trial
 *   decidetest oom LINE PREV      a kdos-oomd status line, PREV kills seen
 *   decidetest pending FILE       the installer's apps-pending file
 *   decidetest action KIND ACTION the vector an action runs
 *
 * Each prints what kdos-notice would do — `post KIND`, `close KIND` or
 * `none KIND` — and records it in $XDG_STATE_HOME/kdos/notice as main.c
 * does after a server accepted it, ids counting up, so a run of calls is a
 * session seeing its facts change. $KDOS_NOTICE_BOOT_ID is the boot.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "notice.h"

static KnState st;

static void apply(int got, int kind, const KnPost *p)
{
	if (!got) {
		printf("none %s\n", kn_kind_name(kind));
		return;
	}
	if (p->close) {
		printf("close %s %u\n", kn_kind_name(p->kind), st.id[p->kind]);
		st.id[p->kind] = 0;
		st.key[p->kind][0] = '\0';
	} else {
		unsigned id = 1;

		for (int k = 0; k < KN_COUNT; k++)
			if (st.id[k] >= id)
				id = st.id[k] + 1;
		printf("post %s replaces=%u persistent=%d urgency=%d\n",
		       kn_kind_name(p->kind), st.id[p->kind], p->persistent,
		       p->urgency);
		printf("  summary %s\n  body %s\n  icon %s\n", p->summary,
		       p->body, p->icon ? p->icon : "");
		for (int i = 0; p->actions[i]; i += 2)
			printf("  action %s %s\n", p->actions[i], p->actions[i + 1]);
		st.id[p->kind] = id;
		kb_strlcpy(st.key[p->kind], p->key, sizeof(st.key[0]));
	}
	if (kn_state_save(&st) != 0) {
		fprintf(stderr, "decidetest: the state was not saved\n");
		exit(1);
	}
}

static char *slurp(const char *path)
{
	char *t = kb_read_whole(path, NULL);

	/* An unreadable file is a command that printed nothing. */
	return t ? t : kb_strdup("");
}

int main(int argc, char **argv)
{
	char boot[64];
	KnPost p;

	kb_set_progname("decidetest");
	if (argc < 3) {
		fprintf(stderr, "usage: decidetest restarts|cve|boot|oom|pending|action ...\n");
		return 2;
	}
	kn_state_load(&st);
	kn_boot_id(boot, sizeof(boot));
	if (!strcmp(argv[1], "restarts")) {
		KnRestarts *r = kb_calloc(1, sizeof(*r));
		char *j = slurp(argv[2]);

		if (kn_parse_restarts(j, r))
			kn_name_procs(r);
		for (int i = 0; i < r->n; i++)
			printf("proc %d %s name=%s desktop=%d\n", r->p[i].pid,
			       r->p[i].comm, r->p[i].name, r->p[i].desktop);
		apply(kn_decide_restarts(r, boot, &st, &p), KN_RESTARTS, &p);
		free(j);
		free(r);
	} else if (!strcmp(argv[1], "cve")) {
		KnCve *c = kb_calloc(1, sizeof(*c));
		char *j = slurp(argv[2]);

		kn_parse_cve(j, c);
		printf("cves %d in %d\n", c->ncve, c->n);
		apply(kn_decide_cve(c, &st, &p), KN_CVE, &p);
		free(j);
		free(c);
	} else if (!strcmp(argv[1], "boot")) {
		KnBoot b;
		char *j = slurp(argv[2]);

		kn_parse_boot(j, &b);
		printf("boot ok=%d running=%s active=%s trying=%s failed=%s "
		       "a=%s b=%s\n", b.ok, b.running, b.active, b.trying,
		       b.failed, b.ver[0], b.ver[1]);
		apply(kn_decide_rollback(&b, &st, &p), KN_ROLLBACK, &p);
		apply(kn_decide_trial(&b, boot, &st, &p), KN_TRIAL, &p);
		free(j);
	} else if (!strcmp(argv[1], "oom") && argc > 3) {
		KnOom o;

		kn_parse_oom(argv[2], &o);
		printf("oom ok=%d kills=%d last=%s\n", o.ok, o.kills, o.last);
		apply(kn_decide_oom(&o, atoi(argv[3]), &p), KN_OOM, &p);
	} else if (!strcmp(argv[1], "pending")) {
		KnPending pd;
		char *t = slurp(argv[2]);

		kn_parse_pending(t, &pd);
		printf("pending %d\n", pd.n);
		apply(kn_decide_pending(&pd, &st, &p), KN_PENDING, &p);
		free(t);
	} else if (!strcmp(argv[1], "action") && argc > 3) {
		KbArgv a;
		int k = kn_kind_of(argv[2]);

		if (k < 0 || !kn_action_argv(k, argv[3], &a)) {
			printf("no action\n");
			return 0;
		}
		for (int i = 0; i < a.n && a.v[i]; i++)
			printf("[%s]\n", a.v[i]);
	} else {
		fprintf(stderr, "decidetest: unknown step %s\n", argv[1]);
		return 2;
	}
	return 0;
}

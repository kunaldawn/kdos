/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * /proc/meminfo and /proc/pressure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

/* meminfo values are kB; everything above this layer is bytes. */
static int field(const char *data, const char *key, unsigned long long *out)
{
	size_t klen = strlen(key);
	for (const char *line = data; line && *line; ) {
		const char *nl = strchr(line, '\n');
		if (!strncmp(line, key, klen) && line[klen] == ':') {
			unsigned long long kb = 0;
			if (sscanf(line + klen + 1, "%llu", &kb) == 1) {
				*out = kb * 1024ULL;
				return 0;
			}
		}
		if (!nl)
			break;
		line = nl + 1;
	}
	return -1;
}

int kpr_mem_read(KprMem *m)
{
	memset(m, 0, sizeof(*m));
	char *d = kpr_slurp_proc("meminfo");
	if (!d)
		return -1;

	field(d, "MemTotal", &m->total);
	field(d, "MemFree", &m->free);
	field(d, "Cached", &m->cached);
	field(d, "Buffers", &m->buffers);
	field(d, "Shmem", &m->shmem);
	field(d, "SwapTotal", &m->swap_total);
	field(d, "SwapFree", &m->swap_free);
	field(d, "Dirty", &m->dirty);
	field(d, "SReclaimable", &m->reclaimable);

	/*
	 * MemAvailable is the kernel's own estimate of what a new allocation
	 * can have. MemTotal - MemFree is not the used figure: Linux spends
	 * every spare page on page cache, so that arithmetic reports a healthy
	 * machine at 95% and this page becomes the one nobody believes.
	 *
	 * Kernels before 3.14 have no MemAvailable. The fallback is the
	 * closest honest approximation rather than the wrong subtraction.
	 */
	if (field(d, "MemAvailable", &m->available) != 0)
		m->available = m->free + m->cached + m->reclaimable;

	free(d);
	return 0;
}

/*
 * A PSI line is `some avg10=0.00 avg60=0.00 avg300=0.00 total=0`.
 *
 * A kernel with CONFIG_PSI off has no file at all, and that is reported as
 * present = 0 rather than as a stall of zero. The two look identical in a
 * number and mean opposite things: one is "this machine is not starved", the
 * other is "this machine cannot tell you".
 */
static void psi_line(const char *line, double *a10, double *a60, double *a300)
{
	const char *p;
	if ((p = strstr(line, "avg10=")))
		*a10 = strtod(p + 6, NULL);
	if ((p = strstr(line, "avg60=")))
		*a60 = strtod(p + 6, NULL);
	if ((p = strstr(line, "avg300=")))
		*a300 = strtod(p + 7, NULL);
}

int kpr_psi_read(const char *what, KprPsi *out)
{
	memset(out, 0, sizeof(*out));
	char *d = kpr_slurp_proc("pressure/%s", what);
	if (!d)
		return -1;

	for (char *line = d, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "some", 4))
			psi_line(line, &out->some10, &out->some60, &out->some300);
		else if (!strncmp(line, "full", 4))
			psi_line(line, &out->full10, &out->full60, &out->full300);
		if (nl)
			*nl = '\n';
	}
	free(d);
	out->present = 1;
	return 0;
}

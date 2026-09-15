/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Every sensor the kernel publishes, hwmon and thermal
 *
 * NO libsensors. `hwmon` is four files per reading and `thermal_zone` is two;
 * the library exists to parse a configuration file that renames and scales
 * them for a human, and this desktop shows the kernel's own names because a
 * name the kernel did not choose is one nothing else on the machine agrees
 * with.
 *
 * A CHIP'S `name` PLUS A CHANNEL'S `label` IS THE IDENTITY, and where a
 * channel has no label the channel's own number is used. Two chips can both
 * call a channel `temp1`, so the chip name is not decoration.
 *
 * THE UNITS ARE THE KERNEL'S AND THE SCALING IS PER KIND. hwmon publishes
 * millidegrees, millivolts and microwatts; a fan is plain RPM and is NOT
 * scaled, which is the one exception and the reason `kind` exists rather than
 * a single divisor.
 *
 * A READING THAT IS ABSENT IS SKIPPED, NEVER ZERO. A missing `temp3_input` is
 * a channel this chip does not have; drawing it as 0 °C would put a frozen
 * sensor on the screen.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kproc.h"

/* Trailing newline off, and a NULL in is a NULL out so the callers can chain. */
static char *trim(char *s)
{
	if (!s)
		return NULL;
	s[strcspn(s, "\r\n")] = '\0';
	return s;
}

static int cmp_int(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return x < y ? -1 : x > y;
}

/*
 * The chip indices that exist under a sysfs class, in NUMERIC order.
 *
 * There is no bound worth probing up to. The hwmon core allocates the index
 * from one system-wide ida, so a chip's number says nothing about how many
 * chips there are: a laptop with acpitz, coretemp, a wireless card, two power
 * supplies and a discrete GPU has the drive's chip at hwmon7 before anything
 * unusual has happened, and an index loop is both a wasted open per absent
 * number and a chip that cannot be seen at all above the bound. It must also
 * be ONE answer — two readers with two different bounds are two surfaces
 * reporting different sensors for the same die.
 *
 * Numeric order, not the readdir sort: strcmp puts hwmon10 before hwmon2, and
 * the order decides both the row order of a sensor list and which chip a
 * preference walk meets first.
 *
 * Never cached. A hwmon chip appears when its module loads — drivetemp, an
 * NVMe hotplug — and the readdir is microseconds.
 */
int kpr_sysfs_indices(const char *cls, const char *prefix, int *out, int cap)
{
	char dir[768];
	size_t plen = strlen(prefix);
	int n = 0, got = 0;
	char **v;

	snprintf(dir, sizeof(dir), "%s/%s", kpr_sys(), cls);
	v = kb_listdir(dir, &got);
	if (!v)
		return 0;
	for (int i = 0; i < got && n < cap; i++) {
		const char *d;
		char *end = NULL;
		long idx;

		if (strncmp(v[i], prefix, plen) || strlen(v[i]) <= plen)
			continue;
		d = v[i] + plen;
		idx = strtol(d, &end, 10);
		if (!end || *end || idx < 0 || idx > 1000000)
			continue;
		out[n++] = (int)idx;
	}
	kb_strv_free(v);
	qsort(out, (size_t)n, sizeof(*out), cmp_int);
	return n;
}

/* `kind` is an enumeration, so a walk selecting kinds needs a bit per value
 * rather than the values themselves. */
#define SEN_BIT(k) (1u << (k))

static void push(KprSensor **v, int *n, int *cap, const KprSensor *s)
{
	if (*n == *cap) {
		int want = *cap ? *cap * 2 : 32;
		KprSensor *bigger = realloc(*v, (size_t)want * sizeof(**v));

		if (!bigger)
			return;
		*v = bigger;
		*cap = want;
	}
	(*v)[(*n)++] = *s;
}

/*
 * One hwmon channel. `pre` is `temp`, `fan`, `in` or `power`; the file layout
 * is identical across all four and only the unit differs, which is what makes
 * one function right rather than four.
 *
 * `named` off drops the label and the critical threshold, which are two more
 * opens per channel that no caller computing a maximum can use: a channel's
 * name cannot change which reading is the largest.
 */
static void hwmon_channel(KprSensor **v, int *n, int *cap, int chip,
			  const char *chipname, const char *pre, int idx,
			  int kind, int named)
{
	long long raw = kpr_num_sys(-1, "class/hwmon/hwmon%d/%s%d_input", chip,
				    pre, idx);
	KprSensor s;
	char *label;

	if (raw < 0)
		return;

	memset(&s, 0, sizeof(s));
	s.kind = kind;
	snprintf(s.chip, sizeof(s.chip), "%s", chipname);

	label = named ? trim(kpr_slurp_sys("class/hwmon/hwmon%d/%s%d_label",
					   chip, pre, idx))
		      : NULL;
	if (label && *label)
		snprintf(s.label, sizeof(s.label), "%s", label);
	else
		snprintf(s.label, sizeof(s.label), "%s%d", pre, idx);
	free(label);

	switch (kind) {
	case KPR_SENSOR_TEMP:
		s.value = (double)raw / 1000.0;		/* millidegrees */
		break;
	case KPR_SENSOR_VOLT:
		s.value = (double)raw / 1000.0;		/* millivolts   */
		break;
	case KPR_SENSOR_POWER:
		s.value = (double)raw / 1000000.0;	/* microwatts   */
		break;
	default:
		s.value = (double)raw;			/* RPM, as-is   */
		break;
	}

	/* The thresholds the chip itself publishes, where it does. `crit` is
	 * the one worth drawing: `max` is often a design figure a busy machine
	 * sits above all day, and colouring that red teaches people to ignore
	 * the colour. */
	if (kind == KPR_SENSOR_TEMP && named) {
		long long c = kpr_num_sys(-1,
					  "class/hwmon/hwmon%d/temp%d_crit",
					  chip, idx);

		s.crit = c > 0 ? (double)c / 1000.0 : -1.0;
	} else {
		s.crit = -1.0;
	}
	push(v, n, cap, &s);
}

/*
 * The one walk, with the channel kinds the caller can actually use.
 *
 * A caller wanting the hottest temperature and a caller drawing the Sensors
 * page must meet the same chips in the same order and dedup the same way, or
 * the panel meter and the page report two numbers for one die. So there is
 * one enumeration and a mask over it, never a second private scan.
 */
static int sensors_walk(KprSensor **out, unsigned kinds, int named)
{
	KprSensor *v = NULL;
	int n = 0, cap = 0;
	int idx[256];
	int nchip = kpr_sysfs_indices("class/hwmon", "hwmon", idx,
				      (int)(sizeof(idx) / sizeof(*idx)));

	*out = NULL;

	for (int c = 0; c < nchip; c++) {
		int i = idx[c];
		char *name = trim(kpr_slurp_sys("class/hwmon/hwmon%d/name", i));

		if (!name)
			continue;
		/* Eight of each: more channels than any consumer chip has, and
		 * a bound rather than a scan because a missing file is the
		 * normal case here and stopping at the first gap would drop
		 * `temp3` on a chip with no `temp2`. */
		if (kinds & SEN_BIT(KPR_SENSOR_TEMP))
			for (int k = 1; k <= 8; k++)
				hwmon_channel(&v, &n, &cap, i, name, "temp", k,
					      KPR_SENSOR_TEMP, named);
		if (kinds & SEN_BIT(KPR_SENSOR_FAN))
			for (int k = 1; k <= 8; k++)
				hwmon_channel(&v, &n, &cap, i, name, "fan", k,
					      KPR_SENSOR_FAN, named);
		if (kinds & SEN_BIT(KPR_SENSOR_POWER))
			for (int k = 1; k <= 8; k++)
				hwmon_channel(&v, &n, &cap, i, name, "power", k,
					      KPR_SENSOR_POWER, named);
		free(name);
	}

	/*
	 * `thermal_zone` LAST AND ONLY WHERE hwmon SAID NOTHING for that
	 * name. The two trees overlap on most boards — the same die reported
	 * twice under different names is a Sensors page that looks like it has
	 * found twice as many sensors as the machine has. The dedup is not
	 * cosmetic even for a maximum: a zone and a chip of the same name can
	 * read differently, so dropping it here and keeping it there is two
	 * answers for one die.
	 */
	if (!(kinds & SEN_BIT(KPR_SENSOR_TEMP)))
		goto done;
	nchip = kpr_sysfs_indices("class/thermal", "thermal_zone", idx,
				  (int)(sizeof(idx) / sizeof(*idx)));
	for (int c = 0; c < nchip; c++) {
		int i = idx[c];
		char *type = trim(kpr_slurp_sys(
			"class/thermal/thermal_zone%d/type", i));
		long long mc;
		KprSensor s;
		int dup = 0;

		if (!type)
			continue;
		mc = kpr_num_sys(-1, "class/thermal/thermal_zone%d/temp", i);
		if (mc < 0) {
			free(type);
			continue;
		}
		for (int k = 0; k < n; k++)
			if (v[k].kind == KPR_SENSOR_TEMP &&
			    !strcmp(v[k].chip, type))
				dup = 1;
		if (dup) {
			free(type);
			continue;
		}
		memset(&s, 0, sizeof(s));
		s.kind = KPR_SENSOR_TEMP;
		s.value = (double)mc / 1000.0;
		s.crit = -1.0;
		snprintf(s.chip, sizeof(s.chip), "%s", type);
		snprintf(s.label, sizeof(s.label), "%s", "zone");
		free(type);
		push(&v, &n, &cap, &s);
	}

done:
	*out = v;
	return n;
}

int kpr_sensors_list(KprSensor **out)
{
	return sensors_walk(out, SEN_BIT(KPR_SENSOR_TEMP) |
				 SEN_BIT(KPR_SENSOR_FAN) |
				 SEN_BIT(KPR_SENSOR_VOLT) |
				 SEN_BIT(KPR_SENSOR_POWER), 1);
}

void kpr_sensors_free(KprSensor *v)
{
	free(v);
}

/*
 * THE HOTTEST TEMPERATURE, for a caller with one cell to draw it in.
 *
 * -1 when nothing answered, which a renderer must draw as an em dash: a
 * machine with no sensor is not a machine running at 0 °C.
 */
double kpr_sensors_hottest(void)
{
	KprSensor *v = NULL;
	/* Temperatures only, and unlabelled: a fan's RPM, a rail's voltage and
	 * every channel name are files opened to be thrown away by a caller
	 * with one cell to draw a maximum in. */
	int n = sensors_walk(&v, SEN_BIT(KPR_SENSOR_TEMP), 0);
	double hot = -1.0;

	for (int i = 0; i < n; i++)
		if (v[i].kind == KPR_SENSOR_TEMP && v[i].value > hot)
			hot = v[i].value;
	kpr_sensors_free(v);
	return hot;
}

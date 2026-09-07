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
 */
static void hwmon_channel(KprSensor **v, int *n, int *cap, int chip,
			  const char *chipname, const char *pre, int idx,
			  int kind)
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

	label = trim(kpr_slurp_sys("class/hwmon/hwmon%d/%s%d_label", chip, pre,
				   idx));
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
	if (kind == KPR_SENSOR_TEMP) {
		long long c = kpr_num_sys(-1,
					  "class/hwmon/hwmon%d/temp%d_crit",
					  chip, idx);

		s.crit = c > 0 ? (double)c / 1000.0 : -1.0;
	} else {
		s.crit = -1.0;
	}
	push(v, n, cap, &s);
}

int kpr_sensors_list(KprSensor **out)
{
	KprSensor *v = NULL;
	int n = 0, cap = 0;

	*out = NULL;

	for (int i = 0; i < 64; i++) {
		char *name = trim(kpr_slurp_sys("class/hwmon/hwmon%d/name", i));

		if (!name)
			continue;
		/* Eight of each: more channels than any consumer chip has, and
		 * a bound rather than a scan because a missing file is the
		 * normal case here and stopping at the first gap would drop
		 * `temp3` on a chip with no `temp2`. */
		for (int k = 1; k <= 8; k++)
			hwmon_channel(&v, &n, &cap, i, name, "temp", k,
				      KPR_SENSOR_TEMP);
		for (int k = 1; k <= 8; k++)
			hwmon_channel(&v, &n, &cap, i, name, "fan", k,
				      KPR_SENSOR_FAN);
		for (int k = 1; k <= 8; k++)
			hwmon_channel(&v, &n, &cap, i, name, "power", k,
				      KPR_SENSOR_POWER);
		free(name);
	}

	/*
	 * `thermal_zone` LAST AND ONLY WHERE hwmon SAID NOTHING for that
	 * name. The two trees overlap on most boards — the same die reported
	 * twice under different names is a Sensors page that looks like it has
	 * found twice as many sensors as the machine has.
	 */
	for (int i = 0; i < 32; i++) {
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

	*out = v;
	return n;
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
	int n = kpr_sensors_list(&v);
	double hot = -1.0;

	for (int i = 0; i < n; i++)
		if (v[i].kind == KPR_SENSOR_TEMP && v[i].value > hot)
			hot = v[i].value;
	kpr_sensors_free(v);
	return hot;
}

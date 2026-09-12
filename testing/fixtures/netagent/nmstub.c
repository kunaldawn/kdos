/*
 * The half of NetworkManager an agent can see: a bus name, an AgentManager
 * that accepts a Register, and one GetSecrets asked back down the connection
 * the registration arrived on.
 *
 * It exists because the contract being tested is NetworkManager's, not ours —
 * the argument order, the hint array, the flag bits and the shape of the reply
 * are all read off libnm, and a test that called the agent with a signature of
 * its own invention would agree with the agent and with nothing else.
 *
 *   nmstub FLAGS SETTING HINT ID [CANCEL_MS]
 *
 * Prints one line per event: REGISTER, SECRET, ERROR, or TIMEOUT.
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#define AM_PATH		"/org/freedesktop/NetworkManager/AgentManager"
#define AM_IFACE	"org.freedesktop.NetworkManager.AgentManager"
#define SA_PATH		"/org/freedesktop/NetworkManager/SecretAgent"
#define SA_IFACE	"org.freedesktop.NetworkManager.SecretAgent"
#define CONN_PATH	"/org/freedesktop/NetworkManager/Settings/1"

static sd_bus *bus;
static char agent[256];
static int done;

static uint32_t flags_arg;
static const char *setting_arg, *hint_arg, *id_arg;

static int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int m_register(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const char *id = NULL;

	(void)u;
	(void)e;
	if (sd_bus_message_read(m, "s", &id) < 0)
		return -EINVAL;
	snprintf(agent, sizeof(agent), "%s", sd_bus_message_get_sender(m));
	printf("REGISTER %s\n", id);
	fflush(stdout);
	return sd_bus_reply_method_return(m, "");
}

static int m_register_caps(sd_bus_message *m, void *u, sd_bus_error *e)
{
	return m_register(m, u, e);
}

static int m_unregister(sd_bus_message *m, void *u, sd_bus_error *e)
{
	(void)u;
	(void)e;
	agent[0] = '\0';
	printf("UNREGISTER\n");
	fflush(stdout);
	return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable am_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("Register", "s", "", m_register,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RegisterWithCapabilities", "su", "", m_register_caps,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Unregister", "", "", m_unregister,
		      SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

/* ── the reply ─────────────────────────────────────────────────────────── */

static void dump_secrets(sd_bus_message *m)
{
	if (sd_bus_message_enter_container(m, 'a', "{sa{sv}}") <= 0)
		return;
	while (sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0) {
		const char *setting = NULL;

		if (sd_bus_message_read(m, "s", &setting) < 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		if (sd_bus_message_enter_container(m, 'a', "{sv}") <= 0) {
			sd_bus_message_exit_container(m);
			break;
		}
		while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
			const char *key = NULL, *contents = NULL;
			char type = 0;

			if (sd_bus_message_read(m, "s", &key) < 0) {
				sd_bus_message_exit_container(m);
				break;
			}
			sd_bus_message_peek_type(m, &type, &contents);
			if (contents && !strcmp(contents, "s") &&
			    sd_bus_message_enter_container(m, 'v', "s") > 0) {
				const char *v = NULL;

				sd_bus_message_read(m, "s", &v);
				printf("SECRET %s.%s=%s\n", setting, key,
				       v ? v : "");
				sd_bus_message_exit_container(m);
			} else if (contents && !strcmp(contents, "a{ss}") &&
				   sd_bus_message_enter_container(m, 'v',
								  "a{ss}") > 0) {
				sd_bus_message_enter_container(m, 'a', "{ss}");
				for (;;) {
					const char *a = NULL, *b = NULL;

					if (sd_bus_message_read(m, "{ss}", &a,
								&b) <= 0)
						break;
					printf("SECRET %s.%s.%s=%s\n", setting,
					       key, a ? a : "", b ? b : "");
				}
				sd_bus_message_exit_container(m);
				sd_bus_message_exit_container(m);
			} else {
				sd_bus_message_skip(m, "v");
			}
			sd_bus_message_exit_container(m);
		}
		sd_bus_message_exit_container(m);
		sd_bus_message_exit_container(m);
	}
	sd_bus_message_exit_container(m);
	fflush(stdout);
}

static int on_secrets(sd_bus_message *m, void *u, sd_bus_error *e)
{
	const sd_bus_error *err = sd_bus_message_get_error(m);

	(void)u;
	(void)e;
	if (err)
		printf("ERROR %s %s\n", err->name ? err->name : "",
		       err->message ? err->message : "");
	else
		dump_secrets(m);
	fflush(stdout);
	done = 1;
	return 0;
}

/* The connection, as NetworkManager serialises one: every setting it has,
 * secrets stripped. Two properties is enough for an agent that is only allowed
 * to read the profile's NAME out of it. */
static int append_conn(sd_bus_message *m)
{
	int r;

	if ((r = sd_bus_message_open_container(m, 'a', "{sa{sv}}")) < 0)
		return r;
	if ((r = sd_bus_message_open_container(m, 'e', "sa{sv}")) < 0)
		return r;
	sd_bus_message_append_basic(m, 's', "connection");
	sd_bus_message_open_container(m, 'a', "{sv}");
	sd_bus_message_append(m, "{sv}", "id", "s", id_arg);
	sd_bus_message_append(m, "{sv}", "type", "s", "802-11-wireless");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
	if ((r = sd_bus_message_open_container(m, 'e', "sa{sv}")) < 0)
		return r;
	sd_bus_message_append_basic(m, 's', setting_arg);
	sd_bus_message_open_container(m, 'a', "{sv}");
	sd_bus_message_append(m, "{sv}", "key-mgmt", "s", "wpa-psk");
	sd_bus_message_close_container(m);
	sd_bus_message_close_container(m);
	return sd_bus_message_close_container(m);
}

static int ask(void)
{
	sd_bus_message *m = NULL;
	int r = sd_bus_message_new_method_call(bus, &m, agent, SA_PATH, SA_IFACE,
					       "GetSecrets");

	if (r < 0)
		return r;
	if ((r = append_conn(m)) < 0)
		goto out;
	if ((r = sd_bus_message_append(m, "os", CONN_PATH, setting_arg)) < 0)
		goto out;
	if ((r = sd_bus_message_open_container(m, 'a', "s")) < 0)
		goto out;
	if (hint_arg && *hint_arg)
		sd_bus_message_append_basic(m, 's', hint_arg);
	if ((r = sd_bus_message_close_container(m)) < 0)
		goto out;
	if ((r = sd_bus_message_append(m, "u", flags_arg)) < 0)
		goto out;
	r = sd_bus_call_async(bus, NULL, m, on_secrets, NULL, 130000000);
out:
	sd_bus_message_unref(m);
	return r;
}

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stderr,
			"usage: nmstub FLAGS SETTING HINT ID [CANCEL_MS]\n");
		return 2;
	}
	flags_arg = (uint32_t)strtoul(argv[1], NULL, 0);
	setting_arg = argv[2];
	hint_arg = argv[3];
	id_arg = argv[4];

	int64_t cancel_ms = argc > 5 ? strtoll(argv[5], NULL, 10) : 0;
	int r = sd_bus_open_system(&bus);

	if (r < 0) {
		fprintf(stderr, "nmstub: no bus: %s\n", strerror(-r));
		return 1;
	}
	sd_bus_add_object_vtable(bus, NULL, AM_PATH, AM_IFACE, am_vtable, NULL);
	r = sd_bus_request_name(bus, "org.freedesktop.NetworkManager", 0);
	if (r < 0) {
		fprintf(stderr, "nmstub: cannot take the name: %s\n",
			strerror(-r));
		return 1;
	}

	int64_t start = now_ms(), asked_at = 0;
	int asked = 0, cancelled = 0;

	while (!done && now_ms() - start < 30000) {
		while (sd_bus_process(bus, NULL) > 0)
			;
		if (agent[0] && !asked) {
			if (ask() < 0) {
				printf("ERROR call-failed\n");
				break;
			}
			asked = 1;
			asked_at = now_ms();
		}
		if (asked && cancel_ms && !cancelled &&
		    now_ms() - asked_at >= cancel_ms) {
			sd_bus_call_method_async(bus, NULL, agent, SA_PATH,
						 SA_IFACE, "CancelGetSecrets",
						 NULL, NULL, "os", CONN_PATH,
						 setting_arg);
			cancelled = 1;
			printf("CANCEL sent\n");
			fflush(stdout);
		}
		sd_bus_wait(bus, 100000);
	}
	if (!done) {
		printf("TIMEOUT\n");
		fflush(stdout);
	}
	sd_bus_unref(bus);
	return done ? 0 : 1;
}

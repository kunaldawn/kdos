/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   polkit.c — may this caller do that, asked without blocking the bus
 *
 * EVERY GUARDED METHOD ASKS POLKIT, AND ASKS IT ASYNCHRONOUSLY. An interactive
 * check waits for a person to type a password into the KDE agent, which can
 * take a minute; a daemon that waited for the answer in its method handler
 * would answer nothing else for that minute, and Discover pings it on every
 * start. So the call is sent, the method's message is kept, and the method
 * runs when the answer arrives.
 *
 * THE SUBJECT IS THE CALLER'S BUS NAME, never a PID: polkit resolves a
 * system-bus-name to its process and session itself, which is not open to
 * the PID-reuse race a PID subject is. Interaction is allowed only when the
 * caller's message allows it (Discover sets the flag on every proxy) and the
 * method asks for it; Bless never does, because a session must not be able
 * to talk its way into confirming a boot.
 *
 * ROOT IS ALLOWED WITHOUT ASKING on the system bus, as polkit itself would
 * answer. On a session bus — a test — with nothing owning
 * org.freedesktop.PolicyKit1, every caller is allowed; that is the only place
 * no answer means yes.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "sysupdated.h"

#define PK_BUS   "org.freedesktop.PolicyKit1"
#define PK_PATH  "/org/freedesktop/PolicyKit1/Authority"
#define PK_IFACE "org.freedesktop.PolicyKit1.Authority"

/* An interactive check waits for a person; a non-interactive one for
 * polkitd alone. */
#define PK_TIMEOUT_INTERACTIVE (300ULL * 1000000ULL)
#define PK_TIMEOUT             (25ULL * 1000000ULL)

typedef struct {
	sd_bus_message *m;
	AuthDone done;
	void *ud;
} AuthReq;

static int pk_reply(sd_bus_message *reply, void *userdata,
		    sd_bus_error *ret_error)
{
	AuthReq *r = userdata;
	int allowed = 0, authorized = 0, challenge = 0;
	const char *en = SD_BUS_ERROR_ACCESS_DENIED;
	const char *em = "Access denied by the system policy";

	(void)ret_error;
	if (sd_bus_message_is_method_error(reply, NULL)) {
		const sd_bus_error *e = sd_bus_message_get_error(reply);
		su_log("polkit did not answer: %s", e && e->message ? e->message
								    : "?");
		em = "The system policy could not be asked";
	} else if (sd_bus_message_enter_container(reply, 'r', "bba{ss}") >= 0 &&
		   sd_bus_message_read(reply, "bb", &authorized, &challenge) >= 0) {
		if (authorized) {
			allowed = 1;
		} else if (challenge) {
			en = SD_BUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED;
			em = "Interactive authentication required";
		}
	}
	r->done(r->m, r->ud, allowed, allowed ? NULL : en,
		allowed ? NULL : em);
	sd_bus_message_unref(r->m);
	free(r);
	return 0;
}

/* Is anybody answering polkit on this bus? Asked of the bus daemon on every
 * check in session mode, so a test may start its stub before or after. */
static int polkit_present(void)
{
	sd_bus_message *reply = NULL;
	int has = 0;

	if (sd_bus_call_method(su.bus, "org.freedesktop.DBus",
			       "/org/freedesktop/DBus", "org.freedesktop.DBus",
			       "NameHasOwner", NULL, &reply, "s", PK_BUS) >= 0) {
		sd_bus_message_read(reply, "b", &has);
		sd_bus_message_unref(reply);
	}
	return has;
}

/*
 * Ask whether the sender of `m` may perform `action`, then call `done` with
 * the answer — at once, when it is known without polkit, else when polkit
 * replies. `m` stays referenced until then. Returns 1: the method's reply is
 * the continuation's to send.
 */
int auth_check(sd_bus_message *m, const char *action, int interactive,
	       AuthDone done, void *ud)
{
	const char *sender = sd_bus_message_get_sender(m);
	sd_bus_message *call = NULL;
	int r;

	if (!su.session) {
		sd_bus_creds *creds = NULL;
		uid_t euid = (uid_t)-1;
		if (sd_bus_query_sender_creds(m, SD_BUS_CREDS_EUID, &creds) >= 0) {
			sd_bus_creds_get_euid(creds, &euid);
			sd_bus_creds_unref(creds);
		}
		if (euid == 0) {
			done(m, ud, 1, NULL, NULL);
			return 1;
		}
	} else if (!polkit_present()) {
		done(m, ud, 1, NULL, NULL);
		return 1;
	}
	if (!sender) {
		done(m, ud, 0, SD_BUS_ERROR_ACCESS_DENIED, "No sender");
		return 1;
	}

	int allow = interactive &&
		    sd_bus_message_get_allow_interactive_authorization(m) > 0;
	r = sd_bus_message_new_method_call(su.bus, &call, PK_BUS, PK_PATH,
					   PK_IFACE, "CheckAuthorization");
	if (r >= 0)
		r = sd_bus_message_append(call, "(sa{sv})s", "system-bus-name",
					  1, "name", "s", sender, action);
	if (r >= 0)
		r = sd_bus_message_append(call, "a{ss}us", 0,
					  allow ? 1u : 0u, "");
	if (r < 0) {
		sd_bus_message_unref(call);
		done(m, ud, 0, SD_BUS_ERROR_FAILED,
		     "The system policy could not be asked");
		return 1;
	}

	AuthReq *req = kb_calloc(1, sizeof(*req));
	req->m = sd_bus_message_ref(m);
	req->done = done;
	req->ud = ud;
	r = sd_bus_call_async(su.bus, NULL, call, pk_reply, req,
			      allow ? PK_TIMEOUT_INTERACTIVE : PK_TIMEOUT);
	sd_bus_message_unref(call);
	if (r < 0) {
		sd_bus_message_unref(req->m);
		free(req);
		done(m, ud, 0, SD_BUS_ERROR_FAILED,
		     "The system policy could not be asked");
	}
	return 1;
}

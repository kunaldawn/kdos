/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   appstream.c — the AppStream file GetAppStream names
 *
 * DISCOVER DROPS A TARGET WHOSE APPSTREAM IS EMPTY, and says so in a message:
 * it fetches every URL GetAppStream returns, parses the first component, and
 * only then asks CheckNew. So the file always exists, always holds one
 * `operating-system` component, and is regenerated from the update document
 * every time that changes — the name, the summary and the description of the
 * update Discover shows, and one <release> whose version is the offer, which
 * is where Discover's changelog comes from.
 *
 * THE SIGNATURE IS STATED IN IT. "Signed by <key> (ed25519 ab12…cd34),
 * verified on this machine" is the first paragraph of the release text: the
 * slot image manifest or binhost index `kdos update check` read was verified
 * against /etc/kdos/keys before the offer was made, each fetched through the
 * release index that was verified the same way, and an offer is never made
 * from one that was not. The second paragraph says which of the two the
 * update is (the document's `lane`) and how much it downloads.
 *
 * It lives on /run, root-owned and world-readable: Discover runs as the user
 * and fetches it with QNetworkAccessManager through a file:// URL, and a file
 * regenerated on every change is state, not part of a package. A copy that
 * could not be written falls back to the packaged one in /usr/share, which
 * names the system and offers nothing.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sysupdated.h"

#define AS_RUN   "/run/kdos/sysupdate/io.kdos.system.metainfo.xml"
#define AS_SHARE "/usr/share/kdos/update/io.kdos.system.metainfo.xml"

static int as_written;

static void xml_text(KbBuf *b, const char *s)
{
	for (; s && *s; s++) {
		switch (*s) {
		case '&': kb_buf_str(b, "&amp;"); break;
		case '<': kb_buf_str(b, "&lt;"); break;
		case '>': kb_buf_str(b, "&gt;"); break;
		case '"': kb_buf_str(b, "&quot;"); break;
		case '\'': kb_buf_str(b, "&apos;"); break;
		default:
			/* XML 1.0 has no place for the other control
			 * characters; a tab or newline reads as a space. */
			if ((unsigned char)*s < 0x20)
				kb_buf_str(b, " ");
			else
				kb_buf_add(b, s, 1);
			break;
		}
	}
}

/* "ab12…cd34": the first and last four of the key id, the way a person
 * compares two of them. */
static void key_short(const char *id, char *out, size_t cap)
{
	size_t n = strlen(id);
	if (n > 8)
		snprintf(out, cap, "%.4s…%s", id, id + n - 4);
	else
		snprintf(out, cap, "%s", id);
}

/* The indentation of a <p>: under the component's description, or one
 * level deeper under its release's. */
static const char *ind = "    ";

static void para_open(KbBuf *b)
{
	kb_buf_str(b, ind);
	kb_buf_str(b, "<p>");
}

/* The release notes the new tree carries, paragraph by paragraph: blank
 * lines separate them, and every other line break is a space. */
static void notes_paragraphs(KbBuf *b, const char *notes)
{
	const char *p = notes;
	while (p && *p) {
		while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		const char *end = strstr(p, "\n\n");
		size_t n = end ? (size_t)(end - p) : strlen(p);
		char *para = kb_calloc(n + 1, 1);
		memcpy(para, p, n);
		para_open(b);
		xml_text(b, para);
		kb_buf_str(b, "</p>\n");
		free(para);
		p += n;
	}
}

static void package_sentence(KbBuf *b, const char *slot_letter,
			     const char *running)
{
	char line[256];
	int image = !strcmp(su.lane, "image");
	int shown = image ? 0 : su.npkgs < 6 ? su.npkgs : 5;

	if (image)
		snprintf(line, sizeof(line), "The signed system image of %s, "
			 "written whole", su.offer);
	else
		snprintf(line, sizeof(line), "%d package%s from the signed "
			 "binhost", su.behind, su.behind == 1 ? "" : "s");
	para_open(b);
	xml_text(b, line);
	if (shown) {
		kb_buf_str(b, ": ");
		for (int i = 0; i < shown; i++) {
			if (i)
				kb_buf_str(b, i == shown - 1 && su.npkgs == shown
						      ? " and " : ", ");
			xml_text(b, su.pkgs[i]);
		}
		if (su.npkgs > shown) {
			snprintf(line, sizeof(line), " and %d more",
				 su.npkgs - shown);
			xml_text(b, line);
		}
	}
	/* The size Discover would ask localhost:3129 for, which nothing
	 * serves: what is still to download, from the update document. */
	if (su.size > 0) {
		snprintf(line, sizeof(line), "; %s to download",
			 kb_human_size((unsigned long long)su.size));
		xml_text(b, line);
	}
	snprintf(line, sizeof(line), ". %s into slot %s, the root this system "
		 "is not running, and take%s effect at the next restart; a "
		 "restart that does not reach a working session goes back to "
		 "%s by itself.", image ? "It is written" : "They are installed",
		 slot_letter, image ? "s" : "",
		 running[0] ? running : "this version");
	xml_text(b, line);
	kb_buf_str(b, "</p>\n");
}

/* The update text, once in the component (Discover's description of the
 * update) and once in the release (its changelog). The slot it names is the
 * one this boot is not running, which is where `kdos update apply` puts it. */
static void update_text(KbBuf *b)
{
	char kshort[64], line[256], running[64];
	char cur = su.running[0] ? su.running[0] : su.active[0];
	char other[2] = { cur == 'b' ? 'A' : 'B', 0 };

	state_version(running, sizeof(running));
	key_short(su.signer_key, kshort, sizeof(kshort));
	snprintf(line, sizeof(line), "Signed by %s (ed25519 %s), verified on "
		 "this machine.", su.signer_name[0] ? su.signer_name
						    : "a trusted key",
		 kshort);
	para_open(b);
	xml_text(b, line);
	kb_buf_str(b, "</p>\n");
	package_sentence(b, other, running);
	if (su.notes)
		notes_paragraphs(b, su.notes);
}

int appstream_write(void)
{
	char path[1024], dir[1024];
	/* A document another state of this root wrote offers nothing here,
	 * and its release text is not this system's to show (state.c). */
	const char *offer = state_doc_current() ? su.offer : "";
	KbBuf b = {0};

	kb_buf_str(&b,
		   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		   "<!-- Generated by kdos-sysupdated from "
		   "/var/lib/kdos/update.json, and rewritten\n"
		   "     whenever that file changes. -->\n"
		   "<component type=\"operating-system\">\n"
		   "  <id>io.kdos.system</id>\n"
		   "  <metadata_license>CC0-1.0</metadata_license>\n"
		   "  <name>KDOS</name>\n"
		   "  <summary>The KDOS system, updated as a whole into the "
		   "other root slot</summary>\n"
		   "  <developer id=\"io.kdos\">\n"
		   "    <name>KDOS</name>\n"
		   "  </developer>\n"
		   "  <description>\n");
	ind = "    ";
	if (offer[0]) {
		update_text(&b);
	} else {
		kb_buf_str(&b, "    <p>KDOS installs a system update into the "
			   "root slot it is not running and starts it at the "
			   "next restart, keeping the running version to go "
			   "back to.</p>\n");
	}
	kb_buf_str(&b, "  </description>\n"
		   "  <url type=\"homepage\">https://github.com/kunaldawn/kdos"
		   "</url>\n");
	if (offer[0]) {
		kb_buf_str(&b, "  <releases>\n    <release version=\"");
		xml_text(&b, offer);
		kb_buf_str(&b, "\" type=\"stable\"");
		if (strlen(su.date) == 10) {
			kb_buf_str(&b, " date=\"");
			xml_text(&b, su.date);
			kb_buf_str(&b, "\"");
		}
		kb_buf_str(&b, ">\n      <description>\n");
		ind = "        ";
		update_text(&b);
		kb_buf_str(&b, "      </description>\n    </release>\n"
			   "  </releases>\n");
	}
	kb_buf_str(&b, "</component>\n");

	su_path("/run/kdos/sysupdate", dir, sizeof(dir));
	kb_mkdir_p(dir);
	su_path(AS_RUN, path, sizeof(path));
	int rc = kb_write_file_atomic(path, b.p);
	kb_buf_free(&b);
	as_written = rc == 0;
	if (rc != 0)
		su_log("cannot write %s; GetAppStream names %s", path, AS_SHARE);
	return rc;
}

/* file:// with the absolute path, which is the form QUrl reads as a local
 * file, every byte outside the unreserved set percent-encoded: a fixture's
 * --root may hold a space, and an unescaped one ends the URL there. */
void appstream_url(char *out, size_t cap)
{
	static const char hex[] = "0123456789ABCDEF";
	char p[1024];
	size_t o;

	su_path(as_written ? AS_RUN : AS_SHARE, p, sizeof(p));
	o = (size_t)snprintf(out, cap, "file://");
	for (const char *s = p; *s && o + 4 < cap; s++) {
		unsigned char c = (unsigned char)*s;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '/' || c == '.' ||
		    c == '-' || c == '_' || c == '~') {
			out[o++] = (char)c;
		} else {
			out[o++] = '%';
			out[o++] = hex[c >> 4];
			out[o++] = hex[c & 15];
		}
	}
	out[o] = 0;
}

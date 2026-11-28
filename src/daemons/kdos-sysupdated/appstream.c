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
 * it fetches every URL GetAppStream returns and parses the first component.
 * KDOS's Discover asks GetVersion and CheckNew first and fetches the file
 * after them (ports/core/kde/discover, sysupdate-appstream-after-check.patch),
 * so the file it reads is the one the check that made the offer wrote; read
 * before CheckNew, it would describe the previous check, and the first
 * session to see a new release would show its size as Unknown. So the file
 * always exists, always holds one `operating-system` component, and is
 * regenerated from the update document every time that changes — the name,
 * the summary and the description of the update Discover shows, the
 * project's licence for its Licenses field, and one <release> whose version
 * is the offer, which is where Discover's changelog comes from, with an
 * artifact giving its download size when the update document has one.
 *
 * THE SIGNATURE IS STATED IN IT. "Signed by <key> (ed25519 ab12…cd34),
 * verified on this machine" is the first paragraph of the release text: the
 * slot image manifest or binhost index `kdos update check` read was verified
 * against /etc/kdos/keys before the offer was made, each fetched through the
 * release index that was verified the same way, and an offer is never made
 * from one that was not. The second paragraph says which of the two the
 * update is (the document's `lane`) and how much of it is still to fetch:
 * "to copy" when that lane's origin (the document's `image` for the image,
 * `binhost` for packages) is a directory or a file:// URL, such as a stick,
 * and "to download" otherwise. The tree's `channel` does not decide it: a
 * tree read from a stick can still name packages fetched over https.
 *
 * THE LIVE MEDIUM IS NAMED AS SUCH. It has no slots, so CheckNew offers it
 * nothing and Discover shows the system as up to date; the summary and the
 * description are the one place Discover says why, and they say that the
 * live session is not updated and that installing KDOS gives one that is.
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

/* The release notes the new tree carries, from the Markdown subset
 * ports/RELEASE is written in to AppStream's description markup, which has
 * paragraphs and lists and nothing else: Discover shows the text of a <p>
 * as it is, so a `#` or a backtick left in it reaches the page verbatim.
 * Blank lines separate blocks. A `#` or `##` heading is a <p> of its text
 * alone. A run of `- ` items is one <ul>, each item an <li>, and a line
 * under an item that starts no new one continues it. Any other line break
 * is a space, and backticks are dropped. */
enum { NB_NONE, NB_PARA, NB_ITEM };

static void notes_flush(KbBuf *b, KbBuf *text, int *kind)
{
	if (*kind != NB_NONE && text->p && text->p[0]) {
		if (*kind == NB_ITEM) {
			kb_buf_str(b, ind);
			kb_buf_str(b, "  <li>");
		} else {
			para_open(b);
		}
		xml_text(b, text->p);
		kb_buf_str(b, *kind == NB_ITEM ? "</li>\n" : "</p>\n");
	}
	kb_buf_free(text);
	*kind = NB_NONE;
}

static void notes_list_end(KbBuf *b, int *in_list)
{
	if (*in_list) {
		kb_buf_str(b, ind);
		kb_buf_str(b, "</ul>\n");
	}
	*in_list = 0;
}

/* One line's words onto the block's text, a space between lines and no
 * backtick. */
static void notes_words(KbBuf *text, const char *s, size_t n)
{
	while (n && (*s == ' ' || *s == '\t')) {
		s++;
		n--;
	}
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
		n--;
	if (!n)
		return;
	if (text->p && text->p[0])
		kb_buf_str(text, " ");
	for (size_t i = 0; i < n; i++)
		if (s[i] != '`')
			kb_buf_add(text, s + i, 1);
}

static void notes_paragraphs(KbBuf *b, const char *notes)
{
	KbBuf text = {0};
	int kind = NB_NONE, in_list = 0;
	const char *p = notes;

	while (p && *p) {
		const char *nl = strchr(p, '\n');
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		const char *s = p;
		size_t m = n;

		p += n + (nl ? 1 : 0);
		while (m && (*s == ' ' || *s == '\t')) {
			s++;
			m--;
		}
		if (!m || (m == 1 && *s == '\r')) {
			notes_flush(b, &text, &kind);
			notes_list_end(b, &in_list);
			continue;
		}
		if (*s == '#') {
			size_t h = 0;
			while (h < m && s[h] == '#')
				h++;
			if (h < m && (s[h] == ' ' || s[h] == '\t')) {
				notes_flush(b, &text, &kind);
				notes_list_end(b, &in_list);
				kind = NB_PARA;
				notes_words(&text, s + h, m - h);
				notes_flush(b, &text, &kind);
				continue;
			}
		}
		if (m >= 2 && (*s == '-' || *s == '*') && s[1] == ' ') {
			notes_flush(b, &text, &kind);
			if (!in_list) {
				kb_buf_str(b, ind);
				kb_buf_str(b, "<ul>\n");
				in_list = 1;
			}
			kind = NB_ITEM;
			notes_words(&text, s + 2, m - 2);
			continue;
		}
		if (kind == NB_NONE) {
			notes_list_end(b, &in_list);
			kind = NB_PARA;
		}
		notes_words(&text, s, m);
	}
	notes_flush(b, &text, &kind);
	notes_list_end(b, &in_list);
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
	/* What is still to fetch, from the update document; the release's
	 * artifact carries the same number for Discover's list. */
	if (su.size > 0) {
		int local = su.origin[0] == '/' ||
			    !strncmp(su.origin, "file://", 7);
		snprintf(line, sizeof(line), "; %s to %s",
			 kb_human_size((unsigned long long)su.size),
			 local ? "copy" : "download");
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
	const char *offer = state_doc_current() && !state_live() ? su.offer
								  : "";
	KbBuf b = {0};

	kb_buf_str(&b,
		   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		   "<!-- Generated by kdos-sysupdated from "
		   "/var/lib/kdos/update.json, and rewritten\n"
		   "     whenever that file changes. -->\n"
		   "<component type=\"operating-system\">\n"
		   "  <id>io.kdos.system</id>\n"
		   "  <metadata_license>CC0-1.0</metadata_license>\n"
		   "  <project_license>MIT</project_license>\n"
		   "  <name>KDOS</name>\n"
		   "  <summary>");
	kb_buf_str(&b, state_live()
			       ? "The KDOS live session, which system updates do "
				 "not reach"
			       : "The KDOS system, updated as a whole into the "
				 "other root slot");
	kb_buf_str(&b, "</summary>\n"
		   "  <developer id=\"io.kdos\">\n"
		   "    <name>KDOS</name>\n"
		   "  </developer>\n"
		   "  <description>\n");
	ind = "    ";
	if (state_live()) {
		kb_buf_str(&b, "    <p>This session runs from the installation "
			   "medium and keeps nothing across a restart, so no "
			   "system update is installed into it. Install KDOS to "
			   "have a system that is updated into the root slot it "
			   "is not running and keeps the previous version to go "
			   "back to.</p>\n");
	} else if (offer[0]) {
		update_text(&b);
	} else {
		kb_buf_str(&b, "    <p>KDOS installs a system update into the "
			   "root slot it is not running and starts it at the "
			   "next restart, keeping the running version to go "
			   "back to.</p>\n");
	}
	kb_buf_str(&b, "  </description>\n"
		   "  <url type=\"homepage\">https://kdos.dev/"
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
		kb_buf_str(&b, "      </description>\n");
		/* Discover's update list reads the size from here, the download
		 * size of an artifact of the offered release; with none it
		 * asks localhost:3129, which nothing serves, and shows the
		 * size as Unknown. */
		if (su.size > 0) {
			char n[32];

			snprintf(n, sizeof(n), "%llu",
				 (unsigned long long)su.size);
			kb_buf_str(&b, "      <artifacts>\n"
				   "        <artifact type=\"binary\">\n"
				   "          <size type=\"download\">");
			kb_buf_str(&b, n);
			kb_buf_str(&b, "</size>\n        </artifact>\n"
				   "      </artifacts>\n");
		}
		kb_buf_str(&b, "    </release>\n  </releases>\n");
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

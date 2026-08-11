#include <ctype.h>
#include <string.h>

#include "portup.h"

/* A run may contain letters (1.4rc5, r62) but must contain a digit, or every
 * word in a filename would come back as a candidate. */
static int run_has_digit(const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (isdigit((unsigned char)s[i]))
			return 1;
	return 0;
}

static int is_run_char(char c)
{
	return isalnum((unsigned char)c) || c == '.' || c == '-';
}

/* Check if a candidate is already in the output. Used for final deduplication. */
static int already_in_output(char out[][PU_MAX_VER], int n, const char *cand, size_t len)
{
	for (int i = 0; i < n; i++) {
		if (strlen(out[i]) == len && !strncmp(out[i], cand, len))
			return 1;
	}
	return 0;
}

/* Add a candidate to output if it's valid and not already present. */
static int add_candidate(char out[][PU_MAX_VER], int n, int max,
			 const char *start, size_t len)
{
	if (n >= max || !len || len >= PU_MAX_VER)
		return n;
	if (!run_has_digit(start, len))
		return n;
	if (!isdigit((unsigned char)start[0]) && !isalpha((unsigned char)start[0]))
		return n;
	if (already_in_output(out, n, start, len))
		return n;

	memcpy(out[n], start, len);
	out[n][len] = 0;
	return n + 1;
}

/* Apply transformations iteratively (closure) until no new candidates are
 * generated. This ensures rules compose — e.g., node-v22.14.0 yields 22.14.0
 * by first stripping "node-" to get "v22.14.0", then stripping "v". */
static void apply_transformations(const char *cand, size_t cand_len,
				   char out[][PU_MAX_VER], int *n_ptr, int max)
{
	/* Transformation 1: Strip leading <letters>- group. */
	size_t skip = 0;
	while (skip < cand_len && isalpha((unsigned char)cand[skip]))
		skip++;
	if (skip > 0 && skip < cand_len && cand[skip] == '-') {
		*n_ptr = add_candidate(out, *n_ptr, max, cand + skip + 1,
		                        cand_len - skip - 1);
	}

	/* Transformation 2: Strip leading letter immediately followed by digit. */
	if (cand_len > 1 && isalpha((unsigned char)cand[0]) &&
	    isdigit((unsigned char)cand[1])) {
		*n_ptr = add_candidate(out, *n_ptr, max, cand + 1, cand_len - 1);
	}

	/* Transformation 3: Emit hyphen-separated segments that contain a digit. */
	for (size_t i = 0; i < cand_len && *n_ptr < max; i++) {
		if (cand[i] == '-' && i + 1 < cand_len) {
			size_t seg_start = i + 1;
			size_t seg_end = seg_start;
			while (seg_end < cand_len && cand[seg_end] != '-')
				seg_end++;
			size_t seg_len = seg_end - seg_start;
			if (seg_len > 0) {
				*n_ptr = add_candidate(out, *n_ptr, max,
				                       cand + seg_start, seg_len);
			}
		}
	}
}

int pu_extract(const char *raw, char out[][PU_MAX_VER], int max)
{
	int n = 0;
	const char *p = raw;

	while (*p && n < max) {
		if (!is_run_char(*p)) {
			p++;
			continue;
		}
		const char *start = p;
		while (*p && is_run_char(*p))
			p++;
		size_t len = (size_t)(p - start);

		/* Trailing separators belong to the surrounding text, not to
		 * the version: "3.19.0." from "3.19.0.tar" is not a version. */
		while (len && (start[len - 1] == '.' || start[len - 1] == '-'))
			len--;

		/* Trim known archive and compression suffixes iteratively. Recognise:
		 * tar, gz, xz, bz2, lz, lzma, zst, tgz, txz, tbz2, zip, pem. Loop to
		 * handle compound suffixes like .tar.xz (trim .xz, then .tar). Cost a
		 * debug cycle: 129 of 390 ports use .tar.xz; without this, they report
		 * unknown forever. */
		int trimmed;
		do {
			trimmed = 0;
			static const char *suf[] = { "tar", "gz", "xz", "bz2", "lz", "lzma",
						     "zst", "tgz", "txz", "tbz2", "zip", "pem",
						     "src", "orig", NULL };
			for (int i = 0; suf[i]; i++) {
				size_t sl = strlen(suf[i]);
				/* Check if run ends with ".<suffix>". */
				if (len > sl + 1 && start[len - sl - 1] == '.' &&
				    !strncmp(start + len - sl, suf[i], sl)) {
					len -= (sl + 1);  /* Remove ".<suffix>" */
					trimmed = 1;
					break;
				}
			}
		} while (trimmed && len > 0);

		if (!len)
			continue;

		/* Use a closure/fixpoint approach: apply transformations iteratively
		 * until no new candidates are generated. Cost a debug cycle: node-v22.14.0
		 * needs both "node-" stripping AND "v" stripping to yield "22.14.0". */
		char temp_cands[PU_MAX_CAND][PU_MAX_VER];
		int temp_n = 0;
		char queue[PU_MAX_CAND][PU_MAX_VER];
		int queue_head = 0, queue_tail = 0;

		/* Start with the run (after suffix trimming). The seed needs the same
		 * bound add_candidate enforces. A run longer than PU_MAX_VER is not a
		 * version anyone ships; truncating it would invent one, so it is dropped.
		 * Reached from forge tag feeds, where nothing constrains the input — a
		 * 20 KB run aborted the process. */
		if (len < PU_MAX_VER && run_has_digit(start, len) &&
		    (isdigit((unsigned char)start[0]) ||
		     isalpha((unsigned char)start[0]))) {
			memcpy(queue[queue_tail], start, len);
			queue[queue_tail][len] = 0;
			queue_tail = (queue_tail + 1) % PU_MAX_CAND;
		}

		/* Process queue with a max iteration limit to prevent infinite loops. */
		int max_passes = 10;
		while (queue_head != queue_tail && max_passes > 0) {
			max_passes--;
			int new_cands_before = temp_n;

			/* Process all candidates currently in queue. */
			int items_in_queue = (queue_tail - queue_head + PU_MAX_CAND) % PU_MAX_CAND;
			for (int i = 0; i < items_in_queue && temp_n < max; i++) {
				const char *cand = queue[queue_head];
				size_t cand_len = strlen(cand);

				/* Add the candidate itself to output. */
				temp_n = add_candidate(temp_cands, temp_n, max, cand, cand_len);

				/* Apply transformations and add new candidates to queue. */
				apply_transformations(cand, cand_len, temp_cands, &temp_n, max);

				queue_head = (queue_head + 1) % PU_MAX_CAND;
			}

			/* If we generated new candidates, add them to the queue for next pass. */
			if (temp_n > new_cands_before) {
				for (int i = new_cands_before; i < temp_n && queue_tail != ((queue_head - 1 + PU_MAX_CAND) % PU_MAX_CAND); i++) {
					memcpy(queue[queue_tail], temp_cands[i], strlen(temp_cands[i]) + 1);
					queue_tail = (queue_tail + 1) % PU_MAX_CAND;
				}
			}
		}

		/* Copy deduplicated results to output. */
		for (int i = 0; i < temp_n && n < max; i++) {
			n = add_candidate(out, n, max, temp_cands[i], strlen(temp_cands[i]));
		}
	}

	return n;
}

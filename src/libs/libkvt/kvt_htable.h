/*
 * libkvt — a small chained hash table.
 *
 * WRITTEN FOR THIS TREE, and that is the point. Upstream libtsm keeps its
 * symbol table in an LGPL hash table borrowed from CCAN; KDOS is MIT and every
 * fork it takes is MIT, so a library that would put relinking obligations on
 * every binary linking libkvt is not one this tree can carry. Five operations
 * are used and they are these five.
 *
 * The stored object IS the key: the caller hashes and compares it, because only
 * the caller knows what a key looks like.
 */

#ifndef KVT_HTABLE_H
#define KVT_HTABLE_H

#include <stdbool.h>
#include <stddef.h>

struct kvt_ht_entry;

struct kvt_shl_htable {
	bool (*compare)(const void *a, const void *b);
	size_t (*rehash)(const void *obj, void *priv);
	void *priv;
	struct kvt_ht_entry **buckets;
	size_t nbuckets;		/* always a power of two, or 0 */
	size_t nelems;
};

void kvt_shl_htable_init(struct kvt_shl_htable *t,
			 bool (*compare)(const void *a, const void *b),
			 size_t (*rehash)(const void *obj, void *priv),
			 void *priv);

/* `free_cb` is called for every stored object; the table is left empty. */
void kvt_shl_htable_clear(struct kvt_shl_htable *t,
			  void (*free_cb)(void *obj, void *ctx), void *ctx);

/* True when found, and `*out` is the STORED object, not the key. */
bool kvt_shl_htable_lookup(struct kvt_shl_htable *t, const void *obj,
			   size_t hash, void **out);

/* 0, or a negative errno. A duplicate is inserted, not merged: the caller
 * looks up first, and upstream relies on that. */
int kvt_shl_htable_insert(struct kvt_shl_htable *t, const void *obj,
			  size_t hash);

bool kvt_shl_htable_remove(struct kvt_shl_htable *t, const void *obj,
			   size_t hash, void **out);

#endif /* KVT_HTABLE_H */

/* libkvt — a small chained hash table. See kvt_htable.h. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "kvt_htable.h"

struct kvt_ht_entry {
	struct kvt_ht_entry *next;
	size_t hash;
	void *obj;
};

#define KVT_HT_MIN 16

void
kvt_shl_htable_init(struct kvt_shl_htable *t,
		    bool (*compare)(const void *a, const void *b),
		    size_t (*rehash)(const void *obj, void *priv), void *priv)
{
	memset(t, 0, sizeof(*t));
	t->compare = compare;
	t->rehash = rehash;
	t->priv = priv;
}

void
kvt_shl_htable_clear(struct kvt_shl_htable *t,
		     void (*free_cb)(void *obj, void *ctx), void *ctx)
{
	for (size_t i = 0; i < t->nbuckets; i++) {
		struct kvt_ht_entry *e = t->buckets[i];

		while (e) {
			struct kvt_ht_entry *next = e->next;

			if (free_cb)
				free_cb(e->obj, ctx);
			free(e);
			e = next;
		}
	}

	free(t->buckets);
	t->buckets = NULL;
	t->nbuckets = 0;
	t->nelems = 0;
}

/* Power-of-two buckets, so the index is a mask rather than a division. */
static int grow(struct kvt_shl_htable *t)
{
	size_t n = t->nbuckets ? t->nbuckets * 2 : KVT_HT_MIN;
	struct kvt_ht_entry **b = calloc(n, sizeof(*b));

	if (!b)
		return -ENOMEM;

	for (size_t i = 0; i < t->nbuckets; i++) {
		struct kvt_ht_entry *e = t->buckets[i];

		while (e) {
			struct kvt_ht_entry *next = e->next;
			size_t k = e->hash & (n - 1);

			e->next = b[k];
			b[k] = e;
			e = next;
		}
	}

	free(t->buckets);
	t->buckets = b;
	t->nbuckets = n;
	return 0;
}

bool
kvt_shl_htable_lookup(struct kvt_shl_htable *t, const void *obj, size_t hash,
		      void **out)
{
	if (!t->nbuckets)
		return false;

	for (struct kvt_ht_entry *e = t->buckets[hash & (t->nbuckets - 1)];
	     e; e = e->next) {
		if (e->hash != hash)
			continue;
		if (!t->compare(e->obj, obj))
			continue;
		if (out)
			*out = e->obj;
		return true;
	}

	return false;
}

int
kvt_shl_htable_insert(struct kvt_shl_htable *t, const void *obj, size_t hash)
{
	/* Grow at three quarters full: past that the chains get long enough
	 * that the table stops being one. */
	if (!t->nbuckets || t->nelems * 4 >= t->nbuckets * 3) {
		int r = grow(t);

		if (r)
			return r;
	}

	struct kvt_ht_entry *e = malloc(sizeof(*e));

	if (!e)
		return -ENOMEM;

	size_t k = hash & (t->nbuckets - 1);

	e->hash = hash;
	/* The caller owns the object and outlives the entry; the table stores
	 * the pointer and never the bytes. */
	e->obj = (void *)obj;
	e->next = t->buckets[k];
	t->buckets[k] = e;
	t->nelems++;
	return 0;
}

bool
kvt_shl_htable_remove(struct kvt_shl_htable *t, const void *obj, size_t hash,
		      void **out)
{
	if (!t->nbuckets)
		return false;

	struct kvt_ht_entry **pp = &t->buckets[hash & (t->nbuckets - 1)];

	while (*pp) {
		struct kvt_ht_entry *e = *pp;

		if (e->hash == hash && t->compare(e->obj, obj)) {
			if (out)
				*out = e->obj;
			*pp = e->next;
			free(e);
			t->nelems--;
			return true;
		}
		pp = &e->next;
	}

	return false;
}

#ifndef GET_CACHE_LINE_SIZE_H_INCLUDED
#define GET_CACHE_LINE_SIZE_H_INCLUDED



#include <climits>

#include <stddef.h>
size_t cache_line_size();

#if defined(__APPLE__)

#include <sys/sysctl.h>
size_t cache_line_size() {
	size_t line_size = 0;
	size_t sizeof_line_size = sizeof(line_size);
	sysctlbyname("hw.cachelinesize", &line_size, &sizeof_line_size, 0, 0);
	return line_size;
}

#elif defined(_WIN32)

#include <stdlib.h>
#include <windows.h>
size_t cache_line_size() {
	size_t line_size = 0;
	DWORD buffer_size = 0;
	DWORD i = 0;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION * buffer = 0;

	GetLogicalProcessorInformation(0, &buffer_size);
	buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(buffer_size);
	GetLogicalProcessorInformation(&buffer[0], &buffer_size);

	for (i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
		if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
			line_size = buffer[i].Cache.LineSize;
			break;
		}
	}

	free(buffer);
	return line_size;
}

#elif defined(__linux__)

#include <stdio.h>
size_t cache_line_size() {
	FILE * p = 0;
	p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
	unsigned int i = 0;
	if (p) {
		fscanf(p, "%u", &i);
		fclose(p);
	}
	return i;
}

#else
#error Unrecognized platform
#endif

#endif

#include "cmap.h"
#include "hash.h"
#include <iostream>
#include "random.h"



#define CACHE_LINE_SIZE 64

#define CMAP_ENTRY_SIZE (4 + (UINTPTR_MAX == UINT32_MAX ? 4 : 8))


#define CMAP_K ((CACHE_LINE_SIZE - 4) / CMAP_ENTRY_SIZE)

#define CMAP_PADDING ((CACHE_LINE_SIZE - 4) - (CMAP_K * CMAP_ENTRY_SIZE))

#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

static inline int
raw_ctz(uint64_t n)
{
#ifdef _WIN64
	unsigned long r = 0;
	_BitScanForward64(&r, n);
	return r;
#elif defined(__linux__)
	return __builtin_ctzll(n);
#else
	unsigned long low = n, high, r = 0;
	if (_BitScanForward(&r, low)) {
		return r;
	}
	high = n >> 32;
	_BitScanForward(&r, high);
	return r + 32;
#endif
}

#define MEM_ALIGN 8
#define DIV_ROUND_UP(X, Y) (((X) + ((Y) - 1)) / (Y))
#define ROUND_UP(X, Y) (DIV_ROUND_UP(X, Y) * (Y))
static inline uintmax_t
	zero_rightmost_1bit(uintmax_t x)
	{
		    return x & (x - 1);
		}

void *
xmalloc(size_t size)
{
	void *p = malloc(size ? size : 1);
	if (p == NULL) {
		printf("cannot allocate xmalloc");
	}
	return p;
}

void *
xmalloc_cacheline(size_t size)
{
#ifdef HAVE_POSIX_MEMALIGN
	void *p;
	int error;

	COVERAGE_INC(util_xalloc);
	error = posix_memalign(&p, CACHE_LINE_SIZE, size ? size : 1);
	if (error != 0) {
		out_of_memory();
	}
	return p;
#else
	void **payload;
	void *base;

	
	base = xmalloc((CACHE_LINE_SIZE - 1) + ROUND_UP(MEM_ALIGN + size, CACHE_LINE_SIZE));

	payload = (void **)ROUND_UP((uintptr_t)base, CACHE_LINE_SIZE);
	*payload = base;

	return (char *)payload + MEM_ALIGN;
#endif
}
#define ULLONG_FOR_EACH_1(IDX, MAP)                 \
    for (uint64_t map__ = (MAP);                    \
         map__ && (((IDX) = raw_ctz(map__)), true); \
         map__ = zero_rightmost_1bit(map__))

#define ULLONG_SET0(MAP, OFFSET) ((MAP) &= ~(1ULL << (OFFSET)))
#define ULLONG_SET1(MAP, OFFSET) ((MAP) |= 1ULL << (OFFSET))


void *
xzalloc_cacheline(size_t size)
{
	void *p = xmalloc_cacheline(size);
	memset(p, 0, size);
	return p;
}


struct cmap_bucket {

	uint32_t counter;

	
	uint32_t hashes[CMAP_K];
	struct cmap_node nodes[CMAP_K];

	
#if CMAP_PADDING > 0
	uint8_t pad[CMAP_PADDING];
#endif
};



#define CMAP_MAX_LOAD ((uint32_t) (UINT32_MAX * .85))


#define CMAP_MIN_LOAD ((uint32_t) (UINT32_MAX * .20))

void
free_cacheline(void *p)
{
#ifdef HAVE_POSIX_MEMALIGN
	free(p);
#else
	if (p) {
		free(*(void **)((uintptr_t)p - MEM_ALIGN));
	}
#endif
}



struct cmap_impl {
	unsigned int n;             
	unsigned int max_n;         
	unsigned int min_n;         
	uint32_t mask;              
	uint32_t basis;             

	uint8_t pad[CACHE_LINE_SIZE - sizeof(unsigned int) * 5];

	struct cmap_bucket  buckets[];
};


static struct cmap_impl *cmap_rehash(struct cmap *, uint32_t mask);

static inline uint32_t
other_hash(uint32_t hash)
{
	return (hash << 16) | (hash >> 16);
}

static inline uint32_t
rehash(const struct cmap_impl *impl, uint32_t hash)
{
	return hash_finish(impl->basis, hash);
}

static inline struct cmap_impl *
cmap_get_impl(const struct cmap *cmap)
{
	return cmap->impl;
}

static uint32_t
calc_max_n(uint32_t mask)
{
	return ((uint64_t)(mask + 1) * CMAP_K * CMAP_MAX_LOAD) >> 32;
}

static uint32_t
calc_min_n(uint32_t mask)
{
	return ((uint64_t)(mask + 1) * CMAP_K * CMAP_MIN_LOAD) >> 32;
}

static struct cmap_impl *
cmap_impl_create(uint32_t mask)
{
	struct cmap_impl *impl;


	impl = (struct cmap_impl *)xzalloc_cacheline(sizeof *impl
							 + (mask + 1) * sizeof *impl->buckets);
	
	impl->n = 0;
	impl->max_n = calc_max_n(mask);
	impl->min_n = calc_min_n(mask);
	impl->mask = mask;
	impl->basis = random_uint32();

	return impl;
}


void
cmap_init(struct cmap *cmap)
{
	cmap->impl = cmap_impl_create(0);
}

void
cmap_destroy(struct cmap *cmap)
{
	if (cmap) {
		free_cacheline(cmap_get_impl(cmap));
	}
}


size_t
cmap_count(const struct cmap *cmap)
{
	return cmap_get_impl(cmap)->n;
}


bool
cmap_is_empty(const struct cmap *cmap)
{
	return cmap_count(cmap) == 0;
}

static inline uint32_t
read_counter(const struct cmap_bucket *bucket_)
{
	return bucket_->counter;
}

static inline uint32_t
read_even_counter(const struct cmap_bucket *bucket)
{
	uint32_t counter;

	do {
		counter = read_counter(bucket);
	} while (counter & 1);

	return counter;
}

static inline bool
counter_changed(const struct cmap_bucket *b_, uint32_t c)
{
	struct cmap_bucket *b = (struct cmap_bucket *) b_;
	uint32_t counter = b->counter;

	return counter != c;
}

static inline  struct cmap_node *
cmap_find_in_bucket(const struct cmap_bucket *bucket, uint32_t hash)
{
	for (int i = 0; i < CMAP_K; i++) {
		if (bucket->hashes[i] == hash) {
			return bucket->nodes[i].next;
		}
	}
	return NULL;
}

static inline  struct cmap_node *
cmap_find__(const struct cmap_bucket *b1, const struct cmap_bucket *b2,
uint32_t hash)
{
	uint32_t c1, c2;
	 struct cmap_node *node;

	do {
		do {
			c1 = read_even_counter(b1);
			node = cmap_find_in_bucket(b1, hash);
		} while (counter_changed(b1, c1));
		if (node) {
			break;
		}
		do {
			c2 = read_even_counter(b2);
			node = cmap_find_in_bucket(b2, hash);
		} while (counter_changed(b2, c2));
		if (node) {
			break;
		}
	} while (counter_changed(b1, c1));

	return node;
}

struct cmap_node *
cmap_find(const struct cmap *cmap, uint32_t hash)
{

	
	const struct cmap_impl *impl = cmap_get_impl(cmap);
	uint32_t h1 = rehash(impl, hash);
	uint32_t h2 = other_hash(h1);

	return cmap_find__(&impl->buckets[h1 & impl->mask],
					   &impl->buckets[h2 & impl->mask],
					   hash);
}




unsigned long
cmap_find_batch(const struct cmap *cmap, unsigned long map,
uint32_t hashes[], const struct cmap_node *nodes[])
{
	const struct cmap_impl *impl = cmap_get_impl(cmap);
	unsigned long result = map;
	int i;
	uint32_t h1s[sizeof map * CHAR_BIT];
	const struct cmap_bucket *b1s[sizeof map * CHAR_BIT];
	const struct cmap_bucket *b2s[sizeof map * CHAR_BIT];
	uint32_t c1s[sizeof map * CHAR_BIT];

	
	ULLONG_FOR_EACH_1(i, map) {
		h1s[i] = rehash(impl, hashes[i]);
		b1s[i] = &impl->buckets[h1s[i] & impl->mask];
	}
	
	ULLONG_FOR_EACH_1(i, map) {
		uint32_t c1;
		const struct cmap_bucket *b1 = b1s[i];
		const struct cmap_node *node;

		do {
			c1 = read_even_counter(b1);
			node = cmap_find_in_bucket(b1, hashes[i]);
		} while (counter_changed(b1, c1));

		if (!node) {
			
			b2s[i] = &impl->buckets[other_hash(h1s[i]) & impl->mask];
	
			c1s[i] = c1; /* We may need to check this after Round 2. */
			continue;
		}
		
		ULLONG_SET0(map, i); /* Ignore this on round 2. */

		nodes[i] = node;
	}
	
	ULLONG_FOR_EACH_1(i, map) {
		uint32_t c2;
		const struct cmap_bucket *b2 = b2s[i];
		const struct cmap_node *node;

		do {
			c2 = read_even_counter(b2);
			node = cmap_find_in_bucket(b2, hashes[i]);
		} while (counter_changed(b2, c2));

		if (!node) {
			
			if (counter_changed(b1s[i], c1s[i])) {
				node = cmap_find__(b1s[i], b2s[i], hashes[i]);
				if (node) {
					goto found;
				}
			}
			/* Not found. */
			ULLONG_SET0(result, i); /* Fix the result. */
			continue;
		}
	found:
		nodes[i] = node;
	}
	return result;
}

static int
cmap_find_slot_protected(struct cmap_bucket *b, uint32_t hash)
{
	int i;

	for (i = 0; i < CMAP_K; i++) {
		if (b->hashes[i] == hash && b->nodes[i].next) {
			return i;
		}
	}
	return -1;
}

static struct cmap_node *
cmap_find_bucket_protected(struct cmap_impl *impl, uint32_t hash, uint32_t h)
{
	struct cmap_bucket *b = &impl->buckets[h & impl->mask];
	int i;

	for (i = 0; i < CMAP_K; i++) {
		if (b->hashes[i] == hash) {
			return b->nodes[i].next;
		}
	}
	return NULL;
}

struct cmap_node *
	cmap_find_protected(const struct cmap *cmap, uint32_t hash)
{
	struct cmap_impl *impl = cmap_get_impl(cmap);
	uint32_t h1 = rehash(impl, hash);
	uint32_t h2 = other_hash(hash);
	struct cmap_node *node;

	node = cmap_find_bucket_protected(impl, hash, h1);
	if (node) {
		return node;
	}
	return cmap_find_bucket_protected(impl, hash, h2);
}

static int
cmap_find_empty_slot_protected(const struct cmap_bucket *b)
{
	int i;

	for (i = 0; i < CMAP_K; i++) {
		if (!b->nodes[i].next) {
			return i;
		}
	}
	return -1;
}

static void
cmap_set_bucket(struct cmap_bucket *b, int i,
struct cmap_node *node, uint32_t hash)
{
	
	uint32_t c = b->counter;

	b->counter= c + 1;

	b->nodes[i].next= node; /* Also atomic. */
	b->hashes[i] = hash;
	b->counter = c + 2;

}


static bool
cmap_insert_dup(struct cmap_node *new_node, uint32_t hash,
struct cmap_bucket *b)
{
	int i;

	for (i = 0; i < CMAP_K; i++) {
		if (b->hashes[i] == hash) {
			struct cmap_node *node = b->nodes[i].next;

			if (node) {
				struct cmap_node *p;

			
				p = new_node;
				for (;;) {
					struct cmap_node *next = p->next;

					if (!next) {
						break;
					}
					p = next;
				}
				p->next = node;
			} else {
			}

			b->nodes[i].next = new_node;

			return true;
		}
	}
	
	return false;
}

static bool
cmap_insert_bucket(struct cmap_node *node, uint32_t hash,
struct cmap_bucket *b)
{

	int i;

	for (i = 0; i < CMAP_K; i++) {
		if (!b->nodes[i].next) {
			
			cmap_set_bucket(b, i, node, hash);
			return true;
		}
	}
	
	return false;
}

static struct cmap_bucket *
other_bucket_protected(struct cmap_impl *impl, struct cmap_bucket *b, int slot)
{
	uint32_t h1 = rehash(impl, b->hashes[slot]);
	uint32_t h2 = other_hash(h1);
	uint32_t b_idx = b - impl->buckets;
	uint32_t other_h = (h1 & impl->mask) == b_idx ? h2 : h1;

	return &impl->buckets[other_h & impl->mask];
}

static bool
cmap_insert_bfs(struct cmap_impl *impl, struct cmap_node *new_node,
uint32_t hash, struct cmap_bucket *b1, struct cmap_bucket *b2)
{
	enum { MAX_DEPTH = 4 };

	struct cmap_path {
		struct cmap_bucket *start; 
		struct cmap_bucket *end;   
		uint8_t slots[MAX_DEPTH]; 
		int n;                    
	};

	enum { MAX_QUEUE = 500 };
	struct cmap_path queue[MAX_QUEUE];
	int head = 0;
	int tail = 0;

	
	queue[head].start = b1;
	queue[head].end = b1;
	queue[head].n = 0;
	head++;
	if (b1 != b2) {
		queue[head].start = b2;
		queue[head].end = b2;
		queue[head].n = 0;
		head++;
	}

	while (tail < head) {
		
		const struct cmap_path *path = &queue[tail++];
		struct cmap_bucket *dat = path->end;
		int i;

		for (i = 0; i < CMAP_K; i++) {
			struct cmap_bucket *next = other_bucket_protected(impl, dat, i);
			int j;

			if (dat == next) {
				continue;
			}

			j = cmap_find_empty_slot_protected(next);
			if (j >= 0) {
				
				struct cmap_bucket *buckets[MAX_DEPTH + 2];
				int slots[MAX_DEPTH + 2];
				int k;

				
				for (k = 0; k < path->n; k++) {
					slots[k] = path->slots[k];
				}
				slots[path->n] = i;
				slots[path->n + 1] = j;

				
				buckets[0] = path->start;
				for (k = 0; k <= path->n; k++) {
					buckets[k + 1] = other_bucket_protected(impl, buckets[k], slots[k]);
				}

				for (k = path->n + 1; k > 0; k--) {
					int slot = slots[k - 1];

					cmap_set_bucket(
						buckets[k], slots[k],
						buckets[k - 1]->nodes[slot].next,
						buckets[k - 1]->hashes[slot]);
				}

				cmap_set_bucket(buckets[0], slots[0], new_node, hash);

				return true;
			}

			if (path->n < MAX_DEPTH && head < MAX_QUEUE) {
				struct cmap_path *new_path = &queue[head++];

				*new_path = *path;
				new_path->end = next;
				new_path->slots[new_path->n++] = i;
			}
		}
	}

	return false;
}

static bool
cmap_try_insert(struct cmap_impl *impl, struct cmap_node *node, uint32_t hash)
{
	uint32_t h1 = rehash(impl, hash);
	uint32_t h2 = other_hash(h1);
	struct cmap_bucket *b1 = &impl->buckets[h1 & impl->mask];
	struct cmap_bucket *b2 = &impl->buckets[h2 & impl->mask];

	return (cmap_insert_dup(node, hash, b1) ||
		cmap_insert_dup(node, hash, b2) ||
		(cmap_insert_bucket(node, hash, b1) ||
		cmap_insert_bucket(node, hash, b2)) ||
		cmap_insert_bfs(impl, node, hash, b1, b2));
}

size_t
cmap_insert(struct cmap *cmap, struct cmap_node *node, uint32_t hash)
{
	struct cmap_impl *impl = cmap_get_impl(cmap);

	node->next = nullptr;

	if (impl->n >= impl->max_n) {
		impl = cmap_rehash(cmap, (impl->mask << 1) | 1);
	}
	
	while (!cmap_try_insert(impl, node, hash)) {
	
		impl = cmap_rehash(cmap, impl->mask);
	}
	return ++impl->n;
}

static bool
cmap_replace__(struct cmap_impl *impl, struct cmap_node *node,
struct cmap_node *replacement, uint32_t hash, uint32_t h)
{
	struct cmap_bucket *b = &impl->buckets[h & impl->mask];
	int slot;

	slot = cmap_find_slot_protected(b, hash);
	if (slot < 0) {
		return false;
	}

	
	if (!replacement) {
		replacement = node->next;
	} else {
		replacement->next = node->next;
	}

	struct cmap_node *iter = &b->nodes[slot];
	for (;;) {
		struct cmap_node *next = iter->next;

		if (next == node) {
			iter->next = replacement;
			return true;
		}
		iter = next;
	}
}


size_t
cmap_replace(struct cmap *cmap, struct cmap_node *old_node,
struct cmap_node *new_node, uint32_t hash)
{
	struct cmap_impl *impl = cmap_get_impl(cmap);
	uint32_t h1 = rehash(impl, hash);
	uint32_t h2 = other_hash(h1);
	bool ok;

	ok = cmap_replace__(impl, old_node, new_node, hash, h1)
		|| cmap_replace__(impl, old_node, new_node, hash, h2);
	
	if (!new_node) {
		impl->n--;
		if (impl->n < impl->min_n) {
			impl = cmap_rehash(cmap, impl->mask >> 1);
		}
	}
	return impl->n;
}

static bool
cmap_try_rehash(const struct cmap_impl *old, struct cmap_impl *neww)
{
	const struct cmap_bucket *b;

	for (b = old->buckets; b <= &old->buckets[old->mask]; b++) {
		int i;

		for (i = 0; i < CMAP_K; i++) {
			
			struct cmap_node *node = b->nodes[i].next;

			if (node && !cmap_try_insert(neww, node, b->hashes[i])) {
				return false;
			}
		}
	}
	return true;
}

static struct cmap_impl *
cmap_rehash(struct cmap *cmap, uint32_t mask)
{
	struct cmap_impl *old = cmap_get_impl(cmap);
	struct cmap_impl *neww;

	neww = cmap_impl_create(mask);
	

	while (!cmap_try_rehash(old, neww)) {
		memset(neww->buckets, 0, (mask + 1) * sizeof *neww->buckets);
		neww->basis = random_uint32();
	}

	neww->n = old->n;
	cmap->impl =neww;
	free_cacheline(old);

	return neww;
}

struct cmap_cursor
	cmap_cursor_start(const struct cmap *cmap)
{
	struct cmap_cursor cursor;

	cursor.impl = cmap_get_impl(cmap);
	cursor.bucket_idx = 0;
	cursor.entry_idx = 0;
	cursor.node = nullptr;
	cmap_cursor_advance(&cursor);

	return cursor;
}

void
cmap_cursor_advance(struct cmap_cursor *cursor)
{
	const struct cmap_impl *impl = cursor->impl;

	if (cursor->node) {
		cursor->node = cursor->node->next;
		if (cursor->node) {
			return;
		}
	}

	while (cursor->bucket_idx <= impl->mask) {
		const struct cmap_bucket *b = &impl->buckets[cursor->bucket_idx];

		while (cursor->entry_idx < CMAP_K) {
			cursor->node = b->nodes[cursor->entry_idx++].next;
			if (cursor->node) {
				return;
			}
		}

		cursor->bucket_idx++;
		cursor->entry_idx = 0;
	}
}


struct cmap_node *
	cmap_next_position(const struct cmap *cmap,
struct cmap_position *pos)
{
	struct cmap_impl *impl = cmap_get_impl(cmap);
	unsigned int bucket = pos->bucket;
	unsigned int entry = pos->entry;
	unsigned int offset = pos->offset;

	while (bucket <= impl->mask) {
		const struct cmap_bucket *b = &impl->buckets[bucket];

		while (entry < CMAP_K) {
			const struct cmap_node *node = b->nodes[entry].next;
			unsigned int i;

			for (i = 0; node; i++, node = node->next) {
				if (i == offset) {
					if (node->next) {
						offset++;
					} else {
						entry++;
						offset = 0;
					}
					pos->bucket = bucket;
					pos->entry = entry;
					pos->offset = offset;
					return  (struct cmap_node *)node;
				}
			}

			entry++;
			offset = 0;
		}

		bucket++;
		entry = offset = 0;
	}

	pos->bucket = pos->entry = pos->offset = 0;
	return nullptr;
}

int cmap_largest_chain(const struct cmap* cmap) 
{
	int longest = 0;
	struct cmap_impl *impl = cmap_get_impl(cmap);

	for (uint32_t i = 0; i <= impl->mask; i++) {
		for (int j = 0; j < CMAP_K; j++) {
			int chain = 0;
			cmap_node* n = &impl->buckets[i].nodes[j];
			unsigned int key = 0;
			while (n) {
				chain++;
				
				n = n->next;
			}
			if (chain > longest) longest = chain;
		}
	}
	return longest;
}

int cmap_array_size(const struct cmap* cmap)
{
	struct cmap_impl *impl = cmap_get_impl(cmap);
	return impl->mask + 1;
}

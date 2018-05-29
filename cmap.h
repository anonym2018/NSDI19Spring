#ifndef CMAP_H
#define CMAP_H 1

#include <stdbool.h>
#include <stdint.h>
#include "hash.h"
#include "ElementaryClasses.h"
#include <memory>


#ifdef __GNUC__
#define OVS_TYPEOF(OBJECT) typeof(OBJECT)
#else
#define OVS_TYPEOF(OBJECT) void *
#endif


#ifdef __GNUC__
#define OBJECT_OFFSETOF(OBJECT, MEMBER) offsetof(typeof(*(OBJECT)), MEMBER)
#else
#define OBJECT_OFFSETOF(OBJECT, MEMBER) \
    ((char *) &(OBJECT)->MEMBER - (char *) (OBJECT))
#endif


#define MEMBER_SIZEOF(STRUCT, MEMBER) (sizeof(((STRUCT *) NULL)->MEMBER))



#define CONTAINER_OF(POINTER, STRUCT, MEMBER)                           \
        ((STRUCT *) (void *) ((char *) (POINTER) - offsetof (STRUCT, MEMBER)))

#define OBJECT_CONTAINING(POINTER, OBJECT, MEMBER)                      \
    ((OVS_TYPEOF(OBJECT)) (void *)                                      \
     ((char *) (POINTER) - OBJECT_OFFSETOF(OBJECT, MEMBER)))


#define ASSIGN_CONTAINER(OBJECT, POINTER, MEMBER) \
    ((OBJECT) = OBJECT_CONTAINING(POINTER, OBJECT, MEMBER), (void) 0)


#define INIT_CONTAINER(OBJECT, POINTER, MEMBER) \
    ((OBJECT) = NULL, ASSIGN_CONTAINER(OBJECT, POINTER, MEMBER))


struct cmap_node {

	cmap_node(unsigned int key) : key(key), next(nullptr) {  }
	cmap_node(const Rule& r) : priority(r.priority), rule_ptr(std::make_shared<Rule>(r)), next(nullptr){ }
	unsigned int key;
	int priority;
	std::shared_ptr<Rule> rule_ptr;
	struct cmap_node * next; /* Next node with same hash. */
};


static inline struct cmap_node *
cmap_node_next(const struct cmap_node *node)
{
	return node->next;
}

static inline struct cmap_node *
cmap_node_next_protected(const struct cmap_node *node)
{
	return node->next;
}

/* Concurrent hash map. */

struct cmap {
	struct cmap_impl * impl;
};

/* Initialization. */
void cmap_init(struct cmap *);
void cmap_destroy(struct cmap *);

/* Count. */
size_t cmap_count(const struct cmap *);
bool cmap_is_empty(const struct cmap *);

/* Insertion and deletion.  Return the current count after the operation. */
size_t cmap_insert(struct cmap *, struct cmap_node *, uint32_t hash);
static inline size_t cmap_remove(struct cmap *, struct cmap_node *,
								 uint32_t hash);
size_t cmap_replace(struct cmap *, struct cmap_node *old_node,
struct cmap_node *new_node, uint32_t hash);

#define CMAP_NODE_FOR_EACH(NODE, MEMBER, CMAP_NODE)                     \
    for (INIT_CONTAINER(NODE, CMAP_NODE, MEMBER);                       \
         (NODE) != OBJECT_CONTAINING(NULL, NODE, MEMBER);               \
         ASSIGN_CONTAINER(NODE, cmap_node_next(&(NODE)->MEMBER), MEMBER))
#define CMAP_NODE_FOR_EACH_PROTECTED(NODE, MEMBER, CMAP_NODE)           \
    for (INIT_CONTAINER(NODE, CMAP_NODE, MEMBER);                       \
         (NODE) != OBJECT_CONTAINING(NULL, NODE, MEMBER);               \
         ASSIGN_CONTAINER(NODE, cmap_node_next_protected(&(NODE)->MEMBER), \
                          MEMBER))
#define CMAP_FOR_EACH_WITH_HASH(NODE, MEMBER, HASH, CMAP)   \
    CMAP_NODE_FOR_EACH(NODE, MEMBER, cmap_find(CMAP, HASH))
#define CMAP_FOR_EACH_WITH_HASH_PROTECTED(NODE, MEMBER, HASH, CMAP)     \
    CMAP_NODE_FOR_EACH_PROTECTED(NODE, MEMBER, cmap_find_locked(CMAP, HASH))

struct cmap_node *cmap_find(const struct cmap *, uint32_t hash);
struct cmap_node *cmap_find_protected(const struct cmap *, uint32_t hash);


unsigned long cmap_find_batch(const struct cmap *cmap, unsigned long map,
							  uint32_t hashes[],
							  const struct cmap_node *nodes[]);


struct cmap_cursor {
	const struct cmap_impl *impl;
	uint32_t bucket_idx;
	int entry_idx;
	struct cmap_node *node;
};

struct cmap_cursor cmap_cursor_start(const struct cmap *);
void cmap_cursor_advance(struct cmap_cursor *);




#define CMAP_CURSOR_FOR_EACH__(NODE, CURSOR, MEMBER)    \
    ((CURSOR)->node                                     \
     ? (INIT_CONTAINER(NODE, (CURSOR)->node, MEMBER),   \
        cmap_cursor_advance(CURSOR),                    \
        true)                                           \
     : false)

#define CMAP_CURSOR_FOR_EACH(NODE, MEMBER, CURSOR, CMAP)    \
    for (*(CURSOR) = cmap_cursor_start(CMAP);               \
         CMAP_CURSOR_FOR_EACH__(NODE, CURSOR, MEMBER);      \
        )

#define CMAP_CURSOR_FOR_EACH_CONTINUE(NODE, MEMBER, CURSOR)   \
	    while (CMAP_CURSOR_FOR_EACH__(NODE, CURSOR, MEMBER))

#define CMAP_FOR_EACH(NODE, MEMBER, CMAP)                       \
    for (struct cmap_cursor cursor__ = cmap_cursor_start(CMAP); \
         CMAP_CURSOR_FOR_EACH__(NODE, &cursor__, MEMBER);       \
        )

static inline struct cmap_node *cmap_first(const struct cmap *);

struct cmap_position {
	unsigned int bucket;
	unsigned int entry;
	unsigned int offset;
};

struct cmap_node *cmap_next_position(const struct cmap *,
struct cmap_position *);


static inline struct cmap_node *
cmap_first(const struct cmap *cmap)
{
	struct cmap_position pos = { 0, 0, 0 };

	return cmap_next_position(cmap, &pos);
}


static inline size_t
cmap_remove(struct cmap *cmap, struct cmap_node *node, uint32_t hash)
{
	return cmap_replace(cmap, node, nullptr, hash);
}


int cmap_largest_chain(const struct cmap* cmap);


int cmap_array_size(const struct cmap* cmap);
#endif /* cmap.h */

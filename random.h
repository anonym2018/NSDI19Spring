#ifndef RANDOM_H
#define RANDOM_H 1

#include <stddef.h>
#include <stdint.h>
#include "ElementaryClasses.h"

void random_init(void);
void random_set_seed(uint32_t);

void random_bytes(void *, size_t);
uint32_t random_uint32(void) {
	return Random::random_unsigned_int();
}
uint64_t random_uint64(void) {
	printf("warning random_uint64 unimplemented\n");
	return Random::random_unsigned_int();
}

static inline int
random_range(int max)
{
	return Random::random_unsigned_int() % max;
}

static inline uint8_t
random_uint8(void)
{
	return Random::random_unsigned_int();
}

static inline uint16_t
random_uint16(void)
{
	return Random::random_unsigned_int();
}

#endif /* random.h */

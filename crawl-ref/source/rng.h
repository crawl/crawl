#ifndef RNG_H
#define RNG_H

uint32_t hash32(const void *data, int len) PURE;

unsigned int hash_rand(int x, uint32_t seed, uint32_t id = 0);

#endif

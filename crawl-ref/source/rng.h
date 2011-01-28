#ifndef RNG_H
#define RNG_H

void seed_rng();
void seed_rng(long seed);
void push_rng_state();
void pop_rng_state();

void cf_setseed();

uint32_t random_int();

uint32_t hash(const void *data, int len);

#endif

#ifndef RNG_H
#define RNG_H

void seed_rng();
void seed_rng(long seed);
void push_rng_state();
void pop_rng_state();

void cf_setseed();

unsigned long random_int();

#endif

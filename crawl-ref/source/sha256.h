#ifndef sha256_h
#define sha256_h

unsigned long sha256_genrand();
void push_sha256_state();
void pop_sha256_state();

void reset_sha256_state();

#endif


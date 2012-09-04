#ifndef PERLIN_H
#define PERLIN_H

uint64_t hash3(int x, int y, int z);
double perlin(double x, double y, double z);
double fBM(double x, double y, double z, int octaves);

#endif /* PERLIN_H */

#ifndef PERLIN_H
#define PERLIN_H

namespace perlin
{
    double noise(double xin, double yin);
    double noise(double xin, double yin, double zin); // Praise be to Zin!
    double noise(double xin, double yin, double zin, double win);
    double fBM(double xin, double yin, double zin, uint32_t octaves);
}
#endif /* PERLIN_H */

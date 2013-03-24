#ifndef PERLIN_H
#define PERLIN_H

namespace perlin
{
    double noise(double xin, double yin) REALLYPURE;
    double noise(double xin, double yin, double zin) REALLYPURE; // Praise Zin!
    double noise(double xin, double yin, double zin, double win) REALLYPURE;
    double fBM(double xin, double yin, double zin, uint32_t octaves) REALLYPURE;
}
#endif /* PERLIN_H */

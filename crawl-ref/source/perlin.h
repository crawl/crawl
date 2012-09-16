#ifndef PERLIN_H
#define PERLIN_H

namespace perlin
{
    double noise(double xin, double yin);
    double noise(double xin, double yin, double zin); // Praise be to Zin!
    double noise(double xin, double yin, double zin, double win);
}
#endif /* PERLIN_H */

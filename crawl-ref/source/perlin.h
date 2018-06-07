#pragma once

namespace perlin
{
    double noise(double xin, double yin) IMMUTABLE;
    double noise(double xin, double yin, double zin) IMMUTABLE; // Praise Zin!
    double noise(double xin, double yin, double zin, double win) IMMUTABLE;
    double fBM(double xin, double yin, double zin, uint32_t octaves) IMMUTABLE;
}

#include "AppHdr.h"
#include <math.h>
#include "perlin.h"

double primes[] = {
   0.2,  0.3,  0.5,  0.7,  1.1,  1.3,  1.7,  1.9,  2.3,  2.9,
   3.1,  3.7,  4.1,  4.3,  4.7,  5.3,  5.9,  6.1,  6.7,  7.1,
   7.3,  7.9,  8.3,  8.9,  9.7, 1.01, 1.03, 1.07, 1.09, 1.13,
  1.27, 1.31, 1.37, 1.39, 1.49, 1.51, 1.57, 1.63, 1.67, 1.73,
  1.79, 1.81, 1.91, 1.93, 1.97, 1.99, 2.11, 2.23, 2.27, 2.29,
  2.33, 2.39, 2.41, 2.51, 2.57, 2.63, 2.69, 2.71, 2.77, 2.81,
  2.83, 2.93, 3.07, 3.11, 3.13, 3.17, 3.31, 3.37, 3.47, 3.49,
  3.53, 3.59, 3.67, 3.73, 3.79, 3.83, 3.89, 3.97, 4.01, 4.09,
  4.19, 4.21, 4.31, 4.33, 4.39, 4.43, 4.49, 4.57, 4.61, 4.63,
  4.67, 4.79, 4.87, 4.91, 4.99, 5.03, 5.09, 5.21, 5.23, 5.41
};

double theta = 0.81649658092;

static double fhash3(int x, int y, int z)
{
    // Some compilers choke on big unsigneds, need to give them in hex.
    uint64_t hash=0xcbf29ce484222325ULL; // 14695981039346656037
#define FNV64 1099511628211ULL
    hash^=x;
    hash*=FNV64;
    hash^=y;
    hash*=FNV64;
    hash^=z;
    hash*=FNV64;

    x=hash ^ (hash >> 27) ^ (hash << 24) ^ (hash >> 48);
    return x / 2147483648.0;
}

static double interpolate(double x, double y, double d)
{
    double s = (1 - cos(d * M_PI))*0.5;
    return x * (1 - s) + y * s;
}

double perlin(double x, double y, double z)
{
    int x0 = x, y0 = y, z0 = z;

    return interpolate(
               interpolate(
                   interpolate(fhash3(x0  , y0  , z0), fhash3(x0  , y0  , z0+1), z-z0),
                   interpolate(fhash3(x0  , y0+1, z0), fhash3(x0  , y0+1, z0+1), z-z0),
                   y-y0),
               interpolate(
                   interpolate(fhash3(x0+1, y0  , z0), fhash3(x0+1, y0  , z0+1), z-z0),
                   interpolate(fhash3(x0+1, y0+1, z0), fhash3(x0+1, y0+1, z0+1), z-z0),
                   y-y0),
               x-x0);
}

double fBM(double x, double y, double z, int octaves)
{
    double lacunarity = 2.0;
    double persistence = 2.0;
    if (octaves == 0)
       return 0;

    double xi = x, yi = y, zi = z;
    double value = 0;
    double maxValue = 0;
    for (int octave = 0; octave < octaves; ++octave)
    {
        double scale = pow(lacunarity, octave);
        double divisor = pow(persistence, octave);
        maxValue += 1.0 / divisor;
        value += perlin(xi / scale , yi / scale, zi / scale ) / divisor;
        // Perform a rotation
        xi = x * cos(theta * octave) - y * sin(theta * octave);
        yi = y * cos(theta * octave) + x * sin(theta * octave);
        // And an offset
        xi += primes[(octave * 3) % 100];
        yi += primes[(octave * 3 + 1) % 100];
        zi += primes[(octave * 3 + 2) % 100];
    }

    return value / maxValue;
}

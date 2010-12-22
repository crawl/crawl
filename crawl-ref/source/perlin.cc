#include "AppHdr.h"
#include <math.h>
#include "perlin.h"

static double fhash3(int x, int y, int z)
{
    // Some compilers choke on big unsigneds, need to give them in hex.
    uint64_t hash=0xcbf29ce484222325; // 14695981039346656037
#define FNV64 1099511628211
    hash^=x;
    hash*=FNV64;
    hash^=y;
    hash*=FNV64;
    hash^=z;
    hash*=FNV64;
    
    x=hash ^ (hash >> 27) ^ (hash << 24) ^ (hash >> 48);
    return x / 2147483648.0;
}

/*
static double fhash3(int x, int y, int z)
{
#define r1 8329
#define r2 259751
#define r3 1954532719
    int n = x + y*57 + z*761;
    n = (n<<13) ^ n;

    return (1.0 - ((n * (n * n * r1 + r2) + r3) & 0x7fffffff) / 1073741824.0);
}
*/

static double interpolate(double x, double y, double d)
{
    double s = (1 - cos(d * M_PI))*0.5;
    return x * (1 - s) + y * s;
}

static double smooth(double x, double y, double z)
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


double perlin(int x, int y, int z)
{
    double total = 0.0;
    int i;

    double frequency = .015;
    double persistence = .65;
    int octaves = 8;
    double amplitude = 1;

    for(i=0; i < octaves; i++)
    {
        total += smooth(x * frequency, y * frequency, z * frequency) * amplitude;
        frequency = frequency * 2;
        amplitude = amplitude * persistence;
    }
    
    return total / 2.7661014820; // p^0+p^1+p^2+...+p^(octaves-1)
}

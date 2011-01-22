#ifndef ORB_H
#define ORB_H

bool orb_noise (const coord_def& where, int loudness);

void orb_pickup_noise (const coord_def& where, int loudness = 30,
                       const char* msg = NULL, const char* msg2 = NULL);

#endif

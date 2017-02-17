#ifndef ORB_H
#define ORB_H

#include "game-chapter.h"

void orb_pickup_noise(const coord_def& where, int loudness = 30,
                      const char* msg = nullptr, const char* msg2 = nullptr);
bool orb_limits_translocation();
void start_orb_run(game_chapter new_chapter, const char* message);

#endif

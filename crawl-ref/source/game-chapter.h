#pragma once

enum game_chapter
{
#if TAG_MAJOR_VERSION == 34
    CHAPTER_POCKET_ABYSS = 0, // an AK who hasn't yet entered the dungeon
    CHAPTER_ORB_HUNTING, // entered the dungeon but not found the orb yet
#else
    CHAPTER_ORB_HUNTING = 0, // entered the dungeon but not found the orb yet
#endif
    CHAPTER_ESCAPING, // ascending with the orb
    CHAPTER_ANGERED_PANDEMONIUM, // moved the orb without picking it up
    NUM_CHAPTERS,
};

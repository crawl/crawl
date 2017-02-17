#pragma once

enum game_chapter
{
    CHAPTER_POCKET_ABYSS = 0, // an AK who hasn't yet entered the dungeon
    CHAPTER_ORB_HUNTING, // entered the dungeon but not found the orb yet
    CHAPTER_ESCAPING, // ascending with the orb
    CHAPTER_ANGERED_PANDEMONIUM, // moved the orb without picking it up
    NUM_CHAPTERS,
};

/**
 * @file
 * @brief Contains version information
**/

#pragma once

// change these defs for forks (please)
// (there are various places in the build system that will also need manual
// attention, as well as the main menu, etc)
#define CRAWL "Dungeon Crawl Stone Soup"
#define CRAWL_SHORT "Crawl"
// shown in crashes:
#define CRAWL_BUG_REPORT "https://github.com/crawl/crawl/issues"
// reminder, modifications of the following should obey the licensing, you
// can't just replace this with your name. Also, this should fit (maybe with
// multiple lines) in 80 chars.
#define CRAWL_COPYRIGHT "(c) Copyright 1997-2002 Linley Henzell, 2002-2025 Crawl DevTeam"

#ifdef USE_TILE_WEB
#  define CRAWL_BUILD_NAME "Console/Webtiles"
#elif defined(USE_TILE_LOCAL)
#  define CRAWL_BUILD_NAME "SDL Tiles"
#else
#  define CRAWL_BUILD_NAME "Console"
#endif


enum rel_type
{
    VER_ALPHA,
    VER_BETA,
    VER_FINAL,
};

namespace Version
{
    /// The major version string.
    /**
     * This version is just the major release number, e.g. '0.10' for
     * all of 0.10-a0, 0.10, and 0.10.1 (assuming this last is even
     * released).
     */
    extern const char* Major;

    /// The short version string.
    /**
     * This version will generally match the last version tag. For instance,
     * if the last tag of Crawl before this build was '0.1.2', you'd see
     * '0.1.2'. This version number does not include some rather important
     * extra information useful for getting the exact revision (the Git commit
     * hash and the number of revisions since the tag). For that extra information,
     * use Version::Long instead.
     */
    extern const char* Short;

    /// The long version string.
    /**
     * This string contains detailed version information about the CrissCross
     * build in use. The string will always start with the Git tag that this
     * build descended from. If this build is not an exact match for a given
     * tag, this string will also include the number of commits since the tag
     * and the Git commit id (the SHA-1 hash).
     */
    extern const char* Long;

    /// The release type.
    /**
     * Indicates whether it's a devel or a stable version.
     */
    extern const rel_type ReleaseType;

    void record(string prev);
    string history();
    size_t history_size();
}

extern const char* compilation_info;

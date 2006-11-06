/*
 * mapdef.h:
 * Header for map structures used by the level compiler.
 *
 * NOTE: When we refer to map, this could be a full map, filling an entire
 * level or a minivault that occupies just a portion of the level.
 */

#ifndef __MAPDEF_H__
#define __MAPDEF_H__

#include <string>
#include <vector>

#include "enum.h"

enum map_flags {
    MAPF_PANDEMONIUM_VAULT = 0x01,     // A pandemonium minivault.

    MAPF_MIRROR_VERTICAL   = 0x10,     // The map may be mirrored vertically
    MAPF_MIRROR_HORIZONTAL = 0x20      // may be mirrored horizontally.
};

class level_range {
public:
    int shallowest, deepest;

public:
    level_range(int s = -1, int d = -1);

    void set(int s, int d = -1);
    void reset();
    bool contains(int depth) const;
};

class map_lines {
public:
    map_lines();

    // NULL-terminated list of map lines.
    map_lines(int nlines, ...);

    void add_line(const std::string &s);
    void set_orientation(const std::string &s);

    int width() const;
    int height() const;

    void resolve(const std::string &fillins);

    // Make all lines the same length.
    void normalise();

    // Rotate 90 degrees either clockwise or anticlockwise
    void rotate(bool clockwise);
    void hmirror();
    void vmirror();

    void clear();

    std::string get_initialiser() const;
    const std::vector<std::string> &get_lines() const;

private:
    void resolve(std::string &s, const std::string &fill);

private:
    std::vector<std::string> lines;
    int map_width;
};

class mons_list {
public:
    mons_list(int nids, ...);
    mons_list();

    void clear();
    const std::vector<int> &get_ids() const;

    void add_mons(const std::string &s);

    std::string get_initialiser() const;

private:
    std::vector<std::string> mons_names;
    std::vector<int> mons_ids;

    std::string canonical(std::string in) const;
};

// Not providing a constructor to make life easy for C-style initialisation.
class map_def {
public:
    std::string     name;
    std::string     tags;
    std::string     place;
    level_range     depth;
    map_section_type orient;
    int             chance;
    long            flags;

    map_lines       map;
    mons_list       mons;

    std::string     random_symbols;

public:
    void init();
    std::string get_initialiser() const;

    void hmirror();
    void vmirror();
    void rotate(bool clockwise);
};

std::string escape_string(std::string in, const std::string &toesc,
                          const std::string &escapewith);

#endif

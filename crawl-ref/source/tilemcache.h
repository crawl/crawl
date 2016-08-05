/**
 * @file
 * @brief Monster cache support
**/

#ifdef USE_TILE
#ifndef TILEMCACHE_H
#define TILEMCACHE_H

#include <vector>

struct dolls_data;

// The monster cache is designed to hold extra information about monsters that
// can't be contained in a single tile. This is usually for equipment,
// doll parts, or demon parts.
//
// Monster cache entries for monsters that are out of sight are ref-counted
// that they can be drawn even if that monster no longer exists. When no
// out-of-sight tiles refer to them, they can be deleted.

class tile_draw_info
{
public:
    tile_draw_info() : idx(~0), ofs_x(0), ofs_y(0) {}

    void set(tileidx_t _idx, int _ofs_x = 0, int _ofs_y = 0)
        { idx = _idx; ofs_x = _ofs_x; ofs_y = _ofs_y; }

    tileidx_t idx;
    int ofs_x;
    int ofs_y;
};

class mcache_entry
{
public:
    mcache_entry() : m_ref_count(0) {}
    virtual ~mcache_entry() {}

    void inc_ref() { m_ref_count++; }
    void dec_ref() { m_ref_count--; if (m_ref_count < 0) m_ref_count = 0; }
    int ref_count() { return m_ref_count; }

    enum
    {
        // The maximum number of values written in the info function.
        // XXX: just use a vector?
        MAX_INFO_COUNT = 9
    };

    virtual int info(tile_draw_info *dinfo) const { return 0; }
    virtual const dolls_data *doll() const { return nullptr; }

    virtual bool transparent() const { return false; }

protected:

    // ref count in backstore
    int m_ref_count;
};

class mcache_manager
{
public:
    ~mcache_manager();

    unsigned int register_monster(const monster_info& mon);
    mcache_entry *get(tileidx_t idx);

    void clear_nonref();
    void clear_all();

    bool empty() { return m_entries.empty(); }

protected:
    vector<mcache_entry*> m_entries;
};

// The global monster cache.
extern mcache_manager mcache;

#endif
#endif

#pragma once

#include "los-def.h"

void set_auto_exclude(const monster* mon);
void remove_auto_exclude(const monster* mon, bool sleepy = false);

void init_exclusion_los();
void update_exclusion_los(vector<coord_def> changed);
void deferred_exclude_update();

bool is_exclude_root(const coord_def &p);
int get_exclusion_radius(const coord_def &p);
string get_exclusion_desc(const coord_def &p);
void cycle_exclude_radius(const coord_def &p);
void del_exclude(const coord_def &p);
void set_exclude(const coord_def &p, int radius = LOS_RADIUS,
                 bool autoexcl = false, bool vaultexcl = false,
                 bool defer_updates = false);
void maybe_remove_autoexclusion(const coord_def &p);
void clear_excludes();

class travel_exclude
{
public:
    coord_def     pos;          // exclusion centre
    int           radius;       // exclusion radius
    los_def       los;          // los from exclusion centre
    bool          uptodate;     // Is los up to date?
    bool          autoex;       // Was set automatically.
    string        desc;         // Exclusion description.
    bool          vault;        // Is this exclusion set by a vault?

    travel_exclude(const coord_def &p, int r = LOS_RADIUS,
                   bool autoex = false, string desc = "",
                   bool vault = false);
    // For exclude_map[p] = foo;
    travel_exclude();

    int radius_sq() const;
    bool in_bounds(const coord_def& p) const;
    bool affects(const coord_def& p) const;

private:
    void set_los();

    friend class exclude_set;
};

class exclude_set
{
public:
    exclude_set();

    typedef map<coord_def, travel_exclude> exclmap;
    typedef exclmap::iterator       iterator;
    typedef exclmap::const_iterator const_iterator;

    void clear();

    void erase(const coord_def &p);
    void add_exclude(travel_exclude &ex);
    void add_exclude(const coord_def &p, int radius = LOS_RADIUS,
                     bool autoexcl = false,
                     string desc = "",
                     bool vaultexcl = false);

    void update_excluded_points(bool recompute_los = false);
    void recompute_excluded_points(bool recompute_los = false);

    travel_exclude* get_exclude_root(const coord_def &p);

    bool is_excluded(const coord_def &p) const;
    bool is_exclude_root(const coord_def &p) const;
    string get_exclusion_desc();

    size_t size()  const;
    bool   empty() const;

    const_iterator begin() const;
    const_iterator end() const;

    iterator  begin();
    iterator  end();

private:
    typedef set<coord_def> exclset;

    exclmap exclude_roots;
    exclset exclude_points;

private:
    void add_exclude_points(travel_exclude& ex);
};

extern exclude_set curr_excludes; // in travel.cc

bool is_excluded(const coord_def &p, const exclude_set &exc = curr_excludes);

class writer;
class reader;
void marshallExcludes(writer& outf, const exclude_set& excludes);
void unmarshallExcludes(reader& inf, int minorVersion, exclude_set& excludes);


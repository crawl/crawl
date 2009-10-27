#ifndef EXCLUDE_H
#define EXCLUDE_H

#include "los.h"

bool need_auto_exclude(const monsters *mon, bool sleepy = false);
void set_auto_exclude(const monsters *mon);
void remove_auto_exclude(const monsters *mon, bool sleepy = false);

void init_exclusion_los();
void update_exclusion_los(std::vector<coord_def> changed);

bool is_exclude_root(const coord_def &p);
void cycle_exclude_radius(const coord_def &p);
void del_exclude(const coord_def &p);
void set_exclude(const coord_def &p, int radius = LOS_RADIUS,
                 bool autoexcl = false, bool vaultexcl = false);
void maybe_remove_autoexclusion(const coord_def &p);
std::string get_exclusion_desc();
void clear_excludes();

struct travel_exclude
{
    coord_def     pos;          // exclusion centre
    int           radius;       // exclusion radius
    los_def       los;          // los from exclusion centre
    bool          uptodate;     // Is los up to date?
    bool          autoex;       // Was set automatically.
    monster_type  mon;          // Monster around which exclusion is centered.
    bool          vault;        // Is this exclusion set by a vault?

    travel_exclude(const coord_def &p, int r = LOS_RADIUS,
                   bool autoex = false, monster_type mons = MONS_NO_MONSTER,
                   bool vault = false);

    int radius_sq() const;
    void set_los();
    bool in_bounds(const coord_def& p) const;
    bool affects(const coord_def& p) const;
};

typedef std::vector<travel_exclude> exclvec;
extern exclvec curr_excludes; // in travel.cc

bool is_excluded(const coord_def &p, const exclvec &exc = curr_excludes);

class writer;
class reader;
void marshallExcludes(writer& outf, const exclvec& excludes);
void unmarshallExcludes(reader& inf, char minorVersion, exclvec& excludes);

#endif


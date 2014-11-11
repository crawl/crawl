/**
 * @file
 * @brief Tracks monsters the player has killed.
**/

#ifndef KILLS_H
#define KILLS_H

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "enum.h"

class monster;
struct monster_info;
class reader;
class writer;

// Not intended for external use!
struct kill_monster_desc
{
    kill_monster_desc(const monster*);
    kill_monster_desc(const monster_info&);
    kill_monster_desc() { }

    void save(writer&) const;
    void load(reader&);

    enum name_modifier
    {
        M_NORMAL, M_ZOMBIE, M_SKELETON, M_SIMULACRUM, M_SPECTRE,
        M_SHAPESHIFTER, // A shapeshifter pretending to be 'monnum'
    };

    monster_type monnum;        // Number of the beast
    name_modifier modifier;     // Nature of the beast

    struct less_than
    {
        bool operator () (const kill_monster_desc &m1,
                          const kill_monster_desc &m2) const
        {
            return m1.monnum < m2.monnum
                   || (m1.monnum == m2.monnum && m1.modifier < m2.modifier);
        }
    };
};

#define PLACE_LIMIT 5   // How many unique kill places we're prepared to track
class kill_def
{
public:
    kill_def(const monster* mon);
    kill_def() : kills(0), exp(0)
    {
        // This object just says to the world that it's uninitialised
    }

    void save(writer&) const;
    void load(reader&);

    void add_kill(const monster* mon, level_id place);
    void add_place(level_id place, bool force = false);

    void merge(const kill_def &k, bool unique_monster);

    string info(const kill_monster_desc &md) const;
    string base_name(const kill_monster_desc &md) const;

    unsigned short kills;    // How many kills does the player have?
    int            exp;      // Experience gained for slaying the beast.
                             // Only set *once*, even for shapeshifters.

    vector<level_id> places; // Places where we've killed the beast.
private:
    string append_places(const kill_monster_desc &md, const string &name) const;
};

// Ghosts and random Pandemonium demons.
class kill_ghost
{
public:
    kill_ghost(const monster* mon);
    kill_ghost() { }

    void save(writer&) const;
    void load(reader&);

    string info() const;

    string ghost_name;
    int exp;
    level_id place;
};

// This is the structure that Lua sees.
struct kill_exp
{
    int nkills;
    int exp;
    string base_name;
    string desc;

    monster_type monnum; // Number of the beast
    int modifier;        // Nature of the beast

    vector<level_id> places;

    kill_exp(const kill_def &k, const kill_monster_desc &md)
        : nkills(k.kills), exp(k.exp), base_name(k.base_name(md)),
          desc(k.info(md)),
          monnum(md.monnum), modifier(md.modifier)
    {
        places = k.places;
    }

    kill_exp(const kill_ghost &kg)
        : nkills(1), exp(kg.exp), base_name(), desc(kg.info()),
          monnum(MONS_PROGRAM_BUG), modifier(0)
    {
        places.push_back(kg.place);
    }

    // operator< is implemented for a descending sort.
    bool operator < (const kill_exp &b) const
    {
        return exp == b.exp? (base_name < b.base_name) : (exp > b.exp);
    }
};

class Kills
{
public:
    void record_kill(const monster* mon);
    void merge(const Kills &k);

    bool empty() const;
    void save(writer&) const;
    void load(reader&);

    int get_kills(vector<kill_exp> &v) const;
    int num_kills(const monster* mon) const;
    int num_kills(const monster_info& mon) const;
private:
    int num_kills(kill_monster_desc desc) const;

    typedef map<kill_monster_desc, kill_def,
                kill_monster_desc::less_than> kill_map;
    typedef vector<kill_ghost> ghost_vec;

    kill_map    kills;
    ghost_vec   ghosts;

    void record_ghost_kill(const monster* mon);
};

class KillMaster
{
public:
    KillMaster();
    KillMaster(const KillMaster& other);
    ~KillMaster();

    void record_kill(const monster* mon, int killer, bool ispet);

    bool empty() const;
    void save(writer&) const;
    void load(reader&);

    // Number of kills, by category.
    int num_kills(const monster* mon, kill_category cat) const;
    int num_kills(const monster_info& mon, kill_category cat) const;
    // Number of kills, any category.
    int num_kills(const monster* mon) const;
    int num_kills(const monster_info& mon) const;

    int total_kills() const;

    string kill_info() const;
private:
    const char *category_name(kill_category kc) const;

    Kills categorized_kills[KC_NCATEGORIES];
private:
    void add_kill_info(string &, vector<kill_exp> &,
                       int count, const char *c, bool separator) const;
};

enum KILL_DUMP_OPTIONS
{
    KDO_NO_PLACES,          // Don't dump places at all
    KDO_ONE_PLACE,          // Show places only for single kills and uniques.
    KDO_ALL_PLACES,         // Show all available place information
};

#endif

#ifndef MON_INFO_H
#define MON_INFO_H

#include "monstuff.h"

// Monster info used by the pane; precomputes some data
// to help with sorting and rendering.
class monster_info
{
 public:
    static bool less_than(const monster_info& m1,
                          const monster_info& m2, bool zombified = true);

    static bool less_than_wrapper(const monster_info& m1,
                                  const monster_info& m2);

    monster_info(const monsters* m);

    void to_string(int count, std::string& desc, int& desc_color) const;

    const monsters* m_mon;
    mon_attitude_type m_attitude;
    int m_difficulty;
    int m_brands;
    bool m_fullname;
    unsigned int m_glyph;
    unsigned short m_glyph_colour;
    std::string m_damage_desc;
    mon_dam_level_type m_damage_level;
};

void get_monster_info(std::vector<monster_info>& mons);

#endif

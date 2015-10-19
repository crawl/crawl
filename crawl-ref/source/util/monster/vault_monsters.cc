/**
 * @file vault_monsters.cc
 * @author Jude Brown <bookofjude@users.sourceforge.net>
 * @version 1
 *
 * @section DESCRIPTION
 *
 * Parse the data created by parse_des.py and stored in vault_monster_data.cc,
 * and possibly return a monster spec if the provided name is actually the name
 * of a vault-defined monster.
 *
**/

#include "AppHdr.h"

#include "dungeon.h"
#include "env.h"
#include "externs.h"
#include "mapdef.h"
#include "message.h"
#include "monster-main.h"
#include "stringutil.h"
#include "vault_monster_data.h"

/**
 * Return a vault-defined monster spec.
 *
 * This function parses the contents of (the generated) vault_monster_data.cc
 * and attempts to return a specification. If there is an invalid specification,
 * no error will be recorded.
 *
 * @param monster_name Monster being searched for.
 * @return A mons_spec instance that either contains the relevant data, or
 *         nothing if not found.
 *
**/
mons_spec get_vault_monster(std::string monster_name, std::string* vault_spec)
{
    lowercase(monster_name);
    trim_string(monster_name);

    monster_name = replace_all_of(monster_name, "'", "");

    std::vector<std::string> monsters = get_vault_monsters();

    mons_list mons;
    mons_spec no_monster;

    std::vector<std::string>::iterator it;

    for (it = monsters.begin(); it != monsters.end(); ++it)
    {
        mons.clear();

        const std::string err = mons.add_mons(*it, false);
        if (err.empty())
        {
            mons_spec this_mons = mons.get_monster(0);
            int index = mi_create_monster(this_mons);

            if (index < 0 || index >= MAX_MONSTERS)
                continue;

            bool this_spec = false;

            monster* mp = &menv[index];

            if (mp)
            {
                std::string name =
                    replace_all_of(mp->name(DESC_PLAIN, true), "'", "");
                lowercase(name);
                if (name == monster_name)
                    this_spec = true;
            }

            mons_remove_from_grid(mp);

            if (this_spec)
            {
                if (vault_spec)
                    *vault_spec = *it;
                return this_mons;
            }
        }
    }

    if (vault_spec)
        *vault_spec = "";

    return no_monster;
}

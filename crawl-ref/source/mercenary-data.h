/**
 * @file
 * @brief Tracking permaallies granted by Yred and Beogh.
**/

#pragma once

#include "AppHdr.h"

#include <list>
#include <map>
#include <vector>


#include "art-enum.h"
#include "monster.h"
#include "monster-type.h"
#include "mon-book.h"
#include "mon-transit.h"
#include "externs.h"

struct witchcraft
{
    monster_type montype;
    vector<mon_spell_slot> spells;
};

static const map<unrand_type, vector<witchcraft>> urand_staff_to_spell= 
{
    
    { UNRAND_MAJIN, {
                        {
                            MONS_MERC_WITCH,
                            {
                                {SPELL_CALL_IMP, 9, MON_SPELL_WIZARD},
                                {SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD},
                            }
                        },
                        { 
                            MONS_MERC_SORCERESS,
                            {
                                {SPELL_SUMMON_MINOR_DEMON, 9, MON_SPELL_WIZARD},
                                {SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD},
                            }
                        },
                        { MONS_MERC_ELEMENTALIST, 
                            {
                                {SPELL_SUMMON_DEMON, 9, MON_SPELL_WIZARD},
                                {SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD},
                            }
                        }
                    }
    },
    { UNRAND_ELEMENTAL_STAFF,
                {
                    { MONS_MERC_WITCH,{
                        {SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD},
                        {SPELL_THROW_FROST, 66, MON_SPELL_WIZARD},
                        {SPELL_SANDBLAST, 66, MON_SPELL_WIZARD},
                        {SPELL_SHOCK, 66, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_SORCERESS,{
                        {SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD},
                        {SPELL_BOLT_OF_COLD, 66, MON_SPELL_WIZARD},
                        {SPELL_STONE_ARROW, 66, MON_SPELL_WIZARD | MON_SPELL_SHORT_RANGE},
                        {SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                        }
                    },
                    { MONS_MERC_ELEMENTALIST, {
                        {SPELL_FIREBALL, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                        {SPELL_FREEZING_CLOUD, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                        {SPELL_IRON_SHOT, 66, MON_SPELL_WIZARD},
                        {SPELL_CHAIN_LIGHTNING, 66, MON_SPELL_WIZARD},
                        }
                    },
                }
    },
    { UNRAND_WUCAD_MU,    
                {
                    { MONS_MERC_WITCH,{
                        {SPELL_MAGIC_DART, 33, MON_SPELL_WIZARD},
                        {SPELL_CORONA, 11, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_SORCERESS,{
                        {SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD},
                        {SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD},
                        {SPELL_CONFUSE, 11, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_ELEMENTALIST, {
                        {SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD},
                        {SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD},
                        {SPELL_CONFUSE, 11, MON_SPELL_WIZARD},
                        {SPELL_BLINK_RANGE, 33, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY},
                        }
                    }
                }
    },
    { UNRAND_BATTLE,
                {
                    { MONS_MERC_WITCH,{
                        {SPELL_MAGIC_DART, 66, MON_SPELL_WIZARD},
                        {SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_SORCERESS,{
                        {SPELL_FORCE_LANCE, 66, MON_SPELL_WIZARD},
                        {SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_ELEMENTALIST, {
                        {SPELL_ISKENDERUNS_MYSTIC_BLAST, 66, MON_SPELL_WIZARD},
                        {SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD},
                        {SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD},
                        }
                    },
                }
        
    },
    { UNRAND_OLGREB,
                {
                    { MONS_MERC_WITCH,{
                        {SPELL_STING, 33, MON_SPELL_WIZARD},
                        {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                        {SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_SORCERESS,{
                        {SPELL_VENOM_BOLT, 33, MON_SPELL_WIZARD},
                        {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                        {SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_ELEMENTALIST, {
                        {SPELL_POISON_ARROW, 33, MON_SPELL_WIZARD},
                        {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                        {SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD},
                        }
                    },
                }
    }
};
    
static const map<stave_type, vector<witchcraft>> staff_to_spell= 
{
    { STAFF_FIRE, {
                        { MONS_MERC_WITCH,{
                            {SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD},
                            }
                        },
                        { MONS_MERC_SORCERESS,{
                            {SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                            {SPELL_BOLT_OF_FIRE, 80, MON_SPELL_WIZARD},
                            }
                        },
                        { MONS_MERC_ELEMENTALIST, {
                            {SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD},
                            {SPELL_FIREBALL, 80, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                            }
                        },
                    }
    },
    { STAFF_COLD,
                {
                    { MONS_MERC_WITCH,{
                        {SPELL_THROW_FROST, 66, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_SORCERESS,{
                        {SPELL_THROW_FROST, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                        {SPELL_BOLT_OF_COLD, 80, MON_SPELL_WIZARD},
                        }
                    },
                    { MONS_MERC_ELEMENTALIST, {
                        {SPELL_BOLT_OF_COLD, 66, MON_SPELL_WIZARD},
                        {SPELL_FREEZING_CLOUD, 80, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                        }
                    },
                }
    },
    { STAFF_AIR,
            {
                { MONS_MERC_WITCH,{
                    {SPELL_SHOCK, 66, MON_SPELL_WIZARD},
                    }
                },
                { MONS_MERC_SORCERESS,{
                    {SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD},
                    }
                },
                { MONS_MERC_ELEMENTALIST, {
                    {SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE},
                    {SPELL_CHAIN_LIGHTNING, 80, MON_SPELL_WIZARD},
                    }
                },
            }
        },
    { STAFF_EARTH,
        {
            { MONS_MERC_WITCH,{
                {SPELL_STONESKIN, 11, MON_SPELL_WIZARD},
                {SPELL_SANDBLAST, 80, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_SORCERESS,{
                {SPELL_STONESKIN, 11, MON_SPELL_WIZARD},
                {SPELL_STONE_ARROW, 80, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_ELEMENTALIST, {
                {SPELL_STONESKIN, 11, MON_SPELL_WIZARD},
                {SPELL_IRON_SHOT, 80, MON_SPELL_WIZARD},
                }
            },
        }
    },
    { STAFF_POISON,
        {
            { MONS_MERC_WITCH,{
                {SPELL_STING, 33, MON_SPELL_WIZARD},
                {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_SORCERESS,{
                {SPELL_VENOM_BOLT, 33, MON_SPELL_WIZARD},
                {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_ELEMENTALIST, {
                {SPELL_POISON_ARROW, 33, MON_SPELL_WIZARD},
                {SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD},
                }
            },
        }
    },
    { STAFF_DEATH,
        {
            { MONS_MERC_WITCH,{
                {SPELL_PAIN, 66, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_SORCERESS,{
                {SPELL_AGONY, 66, MON_SPELL_WIZARD},
                {SPELL_DISPEL_UNDEAD, 66, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_ELEMENTALIST, {
                {SPELL_AGONY, 66, MON_SPELL_WIZARD},
                {SPELL_DISPEL_UNDEAD, 66, MON_SPELL_WIZARD},
                {SPELL_DEATHS_DOOR, 80, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY},
                }
            },
        }
    },
    { STAFF_CONJURATION,
        {
            { MONS_MERC_WITCH,{
                {SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_SORCERESS,{
                {SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD},
                {SPELL_FORCE_LANCE, 80, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_ELEMENTALIST, {
                {SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD},
                {SPELL_FORCE_LANCE, 80, MON_SPELL_WIZARD},
                {SPELL_BATTLESPHERE, 11, MON_SPELL_WIZARD},
                }
            },
        }
    },
    { STAFF_SUMMONING,
        {
            { MONS_MERC_WITCH,{
                {SPELL_CALL_CANINE_FAMILIAR, 50, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_SORCERESS,{
                {SPELL_SUMMON_ICE_BEAST, 50, MON_SPELL_WIZARD},
                }
            },
            { MONS_MERC_ELEMENTALIST, {
                {SPELL_MONSTROUS_MENAGERIE, 50, MON_SPELL_WIZARD},
                }
            },
        }
    }
};
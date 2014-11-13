/**
 * @file
 * @brief Functions related to special abilities.
**/

#include "AppHdr.h"

#include "ability.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "abyss.h"
#include "acquire.h"
#include "areas.h"
#include "branch.h"
#include "butcher.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "evoke.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "godprayer.h"
#include "hints.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "maps.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"
#include "uncancel.h"
#include "unicode.h"
#include "zotdef.h"

enum ability_flag_type
{
    ABFLAG_NONE           = 0x00000000,
    ABFLAG_BREATH         = 0x00000001, // ability uses DUR_BREATH_WEAPON
    ABFLAG_DELAY          = 0x00000002, // ability has its own delay
    ABFLAG_PAIN           = 0x00000004, // ability must hurt player (ie torment)
    ABFLAG_PIETY          = 0x00000008, // ability has its own piety cost
    ABFLAG_EXHAUSTION     = 0x00000010, // fails if you.exhausted
    ABFLAG_INSTANT        = 0x00000020, // doesn't take time to use
    ABFLAG_PERMANENT_HP   = 0x00000040, // costs permanent HPs
    ABFLAG_PERMANENT_MP   = 0x00000080, // costs permanent MPs
    ABFLAG_CONF_OK        = 0x00000100, // can use even if confused
    ABFLAG_FRUIT          = 0x00000200, // ability requires fruit
    ABFLAG_VARIABLE_FRUIT = 0x00000400, // ability requires fruit or piety
    ABFLAG_HEX_MISCAST    = 0x00000800, // severity 3 enchantment miscast
    ABFLAG_TLOC_MISCAST   = 0x00001000, // severity 3 translocation miscast
    ABFLAG_NECRO_MISCAST_MINOR = 0x00002000, // severity 2 necro miscast
    ABFLAG_NECRO_MISCAST  = 0x00004000, // severity 3 necro miscast
    ABFLAG_TRMT_MISCAST   = 0x00008000, // severity 3 transmutation miscast
    ABFLAG_LEVEL_DRAIN    = 0x00010000, // drains 2 levels
    ABFLAG_STAT_DRAIN     = 0x00020000, // stat drain
    ABFLAG_ZOTDEF         = 0x00040000, // ZotDef ability, w/ appropriate hotkey
    ABFLAG_SKILL_DRAIN    = 0x00080000, // drains skill levels
    ABFLAG_GOLD           = 0x00100000, // costs gold
};

static int  _find_ability_slot(const ability_def& abil);
static spret_type _do_ability(const ability_def& abil, bool fail);
static void _pay_ability_costs(const ability_def& abil, int zpcost);
static int _scale_piety_cost(ability_type abil, int original_cost);
static string _zd_mons_description_for_ability(const ability_def &abil);
static monster_type _monster_for_ability(const ability_def& abil);

/**
 * This all needs to be split into data/util/show files
 * and the struct mechanism here needs to be rewritten (again)
 * along with the display routine to piece the strings
 * together dynamically ... I'm getting to it now {dlb}
 *
 * This array corresponds with ::god_gain_power_messages and
 * ::god_lose_power_messages, which have the same shape.
 *
 * It makes more sense to think of them as an array
 * of structs than two arrays that share common index
 * values -- well, doesn't it? {dlb}
 *
 * @note Declaring this const messes up externs later, so don't do it!
 */
ability_type god_abilities[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY },
    // Zin
    { ABIL_ZIN_RECITE, ABIL_ZIN_VITALISATION, ABIL_ZIN_IMPRISON,
      ABIL_NON_ABILITY, ABIL_ZIN_SANCTUARY },
    // TSO
    { ABIL_NON_ABILITY, ABIL_TSO_DIVINE_SHIELD, ABIL_NON_ABILITY,
      ABIL_TSO_CLEANSING_FLAME, ABIL_TSO_SUMMON_DIVINE_WARRIOR },
    // Kikubaaqudgha
    { ABIL_KIKU_RECEIVE_CORPSES, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_KIKU_TORMENT },
    // Yredelemnul
    { ABIL_YRED_ANIMATE_REMAINS_OR_DEAD, ABIL_YRED_RECALL_UNDEAD_SLAVES,
      ABIL_NON_ABILITY, ABIL_YRED_DRAIN_LIFE, ABIL_YRED_ENSLAVE_SOUL },
    // Xom
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY },
    // Vehumet
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY },
    // Okawaru
    { ABIL_OKAWARU_HEROISM, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_OKAWARU_FINESSE },
    // Makhleb
    { ABIL_NON_ABILITY, ABIL_MAKHLEB_MINOR_DESTRUCTION,
      ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB, ABIL_MAKHLEB_MAJOR_DESTRUCTION,
      ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB },
    // Sif Muna
    { ABIL_SIF_MUNA_CHANNEL_ENERGY, ABIL_SIF_MUNA_FORGET_SPELL,
      ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Trog
    { ABIL_TROG_BERSERK, ABIL_TROG_REGEN_MR, ABIL_NON_ABILITY,
      ABIL_TROG_BROTHERS_IN_ARMS, ABIL_NON_ABILITY },
    // Nemelex
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NEMELEX_TRIPLE_DRAW,
      ABIL_NEMELEX_DEAL_FOUR, ABIL_NEMELEX_STACK_FIVE },
    // Elyvilon
    { ABIL_ELYVILON_LESSER_HEALING_SELF, ABIL_ELYVILON_PURIFICATION,
      ABIL_ELYVILON_GREATER_HEALING_OTHERS, ABIL_NON_ABILITY,
      ABIL_ELYVILON_DIVINE_VIGOUR },
    // Lugonu
    { ABIL_LUGONU_ABYSS_EXIT, ABIL_LUGONU_BEND_SPACE, ABIL_LUGONU_BANISH,
      ABIL_LUGONU_CORRUPT, ABIL_LUGONU_ABYSS_ENTER },
    // Beogh
    { ABIL_NON_ABILITY, ABIL_BEOGH_SMITING, ABIL_NON_ABILITY,
      ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS, ABIL_BEOGH_GIFT_ITEM },
    // Jiyva
    { ABIL_JIYVA_CALL_JELLY, ABIL_JIYVA_JELLY_PARALYSE, ABIL_NON_ABILITY,
      ABIL_JIYVA_SLIMIFY, ABIL_JIYVA_CURE_BAD_MUTATION },
    // Fedhas
    { ABIL_FEDHAS_EVOLUTION, ABIL_FEDHAS_SUNLIGHT, ABIL_FEDHAS_PLANT_RING,
      ABIL_FEDHAS_SPAWN_SPORES, ABIL_FEDHAS_RAIN},
    // Cheibriados
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_CHEIBRIADOS_DISTORTION,
      ABIL_CHEIBRIADOS_SLOUCH, ABIL_CHEIBRIADOS_TIME_STEP },
    // Ashenzari
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_ASHENZARI_SCRYING, ABIL_ASHENZARI_TRANSFER_KNOWLEDGE },
    // Dithmenos
    { ABIL_NON_ABILITY, ABIL_DITHMENOS_SHADOW_STEP, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_DITHMENOS_SHADOW_FORM },
    // Gozag
    { ABIL_GOZAG_POTION_PETITION, ABIL_GOZAG_CALL_MERCHANT,
      ABIL_GOZAG_BRIBE_BRANCH, ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Qazlal
    { ABIL_NON_ABILITY, ABIL_QAZLAL_UPHEAVAL, ABIL_QAZLAL_ELEMENTAL_FORCE,
      ABIL_NON_ABILITY, ABIL_QAZLAL_DISASTER_AREA },
    // Ru
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_RU_DRAW_OUT_POWER,
      ABIL_RU_POWER_LEAP, ABIL_RU_APOCALYPSE }
};

// The description screen was way out of date with the actual costs.
// This table puts all the information in one place... -- bwr
//
// The five numerical fields are: MP, HP, food, piety and ZP.
// Note:  food_cost  = val + random2avg(val, 2)
//        piety_cost = val + random2((val + 1) / 2 + 1);
//        hp cost is in per-mil of maxhp (i.e. 20 = 2% of hp, rounded up)
static const ability_def Ability_List[] =
{
    // NON_ABILITY should always come first
    { ABIL_NON_ABILITY, "No ability", 0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_SPIT_POISON, "Spit Poison", 0, 0, 40, 0, 0, ABFLAG_BREATH},

    { ABIL_BLINK, "Blink", 0, 50, 50, 0, 0, ABFLAG_NONE},

    { ABIL_BREATHE_FIRE, "Breathe Fire", 0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_FROST, "Breathe Frost", 0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_POISON, "Breathe Poison Gas",
      0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_MEPHITIC, "Breathe Noxious Fumes",
      0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_LIGHTNING, "Breathe Lightning",
      0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_POWER, "Breathe Dispelling Energy", 0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_STICKY_FLAME, "Breathe Sticky Flame",
      0, 0, 125, 0, 0, ABFLAG_BREATH},
    { ABIL_BREATHE_STEAM, "Breathe Steam", 0, 0, 75, 0, 0, ABFLAG_BREATH},
    { ABIL_TRAN_BAT, "Bat Form", 2, 0, 0, 0, 0, ABFLAG_NONE},

    { ABIL_SPIT_ACID, "Spit Acid", 0, 0, 125, 0, 0, ABFLAG_BREATH},

    { ABIL_FLY, "Fly", 3, 0, 100, 0, 0, ABFLAG_NONE},
    { ABIL_STOP_FLYING, "Stop Flying", 0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_HELLFIRE, "Hellfire", 0, 150, 200, 0, 0, ABFLAG_NONE},

    { ABIL_DELAYED_FIREBALL, "Release Delayed Fireball",
      0, 0, 0, 0, 0, ABFLAG_INSTANT},
    { ABIL_STOP_SINGING, "Stop Singing",
      0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_MUMMY_RESTORATION, "Self-Restoration",
      1, 0, 0, 0, 0, ABFLAG_PERMANENT_MP},

    { ABIL_DIG, "Dig", 0, 0, 0, 0, 0, ABFLAG_INSTANT},
    { ABIL_SHAFT_SELF, "Shaft Self", 0, 0, 250, 0, 0, ABFLAG_DELAY},

    // EVOKE abilities use Evocations and come from items.
    // Teleportation and Blink can also come from mutations
    // so we have to distinguish them (see above).  The off items
    // below are labeled EVOKE because they only work now if the
    // player has an item with the evocable power (not just because
    // you used a wand, potion, or miscast effect).  I didn't see
    // any reason to label them as "Evoke" in the text, they don't
    // use or train Evocations (the others do).  -- bwr
    { ABIL_EVOKE_TELEPORTATION, "Evoke Teleportation",
      3, 0, 200, 0, 0, ABFLAG_NONE},
    { ABIL_EVOKE_BLINK, "Evoke Blink", 1, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_RECHARGING, "Device Recharging", 1, 0, 0, 0, 0, ABFLAG_PERMANENT_MP},

    { ABIL_EVOKE_BERSERK, "Evoke Berserk Rage", 0, 0, 0, 0, 0, ABFLAG_NONE},

    { ABIL_EVOKE_TURN_INVISIBLE, "Evoke Invisibility",
      2, 0, 250, 0, 0, ABFLAG_NONE},
    { ABIL_EVOKE_TURN_VISIBLE, "Turn Visible", 0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_EVOKE_FLIGHT, "Evoke Flight", 1, 0, 100, 0, 0, ABFLAG_NONE},
    { ABIL_EVOKE_FOG, "Evoke Fog", 2, 0, 250, 0, 0, ABFLAG_NONE},
    { ABIL_EVOKE_TELEPORT_CONTROL, "Evoke Teleport Control", 4, 0, 200, 0, 0, ABFLAG_NONE},

    { ABIL_END_TRANSFORMATION, "End Transformation", 0, 0, 0, 0, 0, ABFLAG_NONE},

    // INVOCATIONS:
    // Zin
    { ABIL_ZIN_RECITE, "Recite", 0, 0, 0, 0, 0, ABFLAG_BREATH},
    { ABIL_ZIN_VITALISATION, "Vitalisation", 0, 0, 0, 1, 0, ABFLAG_NONE},
    { ABIL_ZIN_IMPRISON, "Imprison", 5, 0, 125, 4, 0, ABFLAG_NONE},
    { ABIL_ZIN_SANCTUARY, "Sanctuary", 7, 0, 150, 15, 0, ABFLAG_NONE},
    { ABIL_ZIN_CURE_ALL_MUTATIONS, "Cure All Mutations",
      0, 0, 0, 0, 0, ABFLAG_NONE},

    // The Shining One
    { ABIL_TSO_DIVINE_SHIELD, "Divine Shield", 3, 0, 50, 2, 0, ABFLAG_NONE},
    { ABIL_TSO_CLEANSING_FLAME, "Cleansing Flame",
      5, 0, 100, 2, 0, ABFLAG_NONE},
    { ABIL_TSO_SUMMON_DIVINE_WARRIOR, "Summon Divine Warrior",
      8, 0, 150, 6, 0, ABFLAG_NONE},

    // Kikubaaqudgha
    { ABIL_KIKU_RECEIVE_CORPSES, "Receive Corpses",
      3, 0, 50, 2, 0, ABFLAG_NONE},
    { ABIL_KIKU_TORMENT, "Torment", 4, 0, 0, 8, 0, ABFLAG_NONE},

    // Yredelemnul
    { ABIL_YRED_INJURY_MIRROR, "Injury Mirror", 0, 0, 0, 0, 0, ABFLAG_PIETY},
    { ABIL_YRED_ANIMATE_REMAINS, "Animate Remains",
      2, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_YRED_RECALL_UNDEAD_SLAVES, "Recall Undead Slaves",
      2, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_YRED_ANIMATE_DEAD, "Animate Dead", 2, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_YRED_DRAIN_LIFE, "Drain Life", 6, 0, 200, 2, 0, ABFLAG_NONE},
    { ABIL_YRED_ENSLAVE_SOUL, "Enslave Soul", 8, 0, 150, 4, 0, ABFLAG_NONE},
    // Placeholder for Animate Remains or Animate Dead.
    { ABIL_YRED_ANIMATE_REMAINS_OR_DEAD, "Animate Remains or Dead",
      2, 0, 100, 0, 0, ABFLAG_NONE},

    // Okawaru
    { ABIL_OKAWARU_HEROISM, "Heroism", 2, 0, 50, 2, 0, ABFLAG_NONE},
    { ABIL_OKAWARU_FINESSE, "Finesse", 5, 0, 100, 4, 0, ABFLAG_NONE},

    // Makhleb
    { ABIL_MAKHLEB_MINOR_DESTRUCTION, "Minor Destruction",
      0, scaling_cost::fixed(1), 20, 0, 0, ABFLAG_NONE},
    { ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB, "Lesser Servant of Makhleb",
      0, scaling_cost::fixed(4), 50, 2, 0, ABFLAG_NONE},
    { ABIL_MAKHLEB_MAJOR_DESTRUCTION, "Major Destruction",
      0, scaling_cost::fixed(6), 100, generic_cost::range(0, 1), 0, ABFLAG_NONE},
    { ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB, "Greater Servant of Makhleb",
      0, scaling_cost::fixed(10), 100, 5, 0, ABFLAG_NONE},

    // Sif Muna
    { ABIL_SIF_MUNA_CHANNEL_ENERGY, "Channel Energy",
      0, 0, 100, 0, 0, ABFLAG_NONE},
    { ABIL_SIF_MUNA_FORGET_SPELL, "Forget Spell", 5, 0, 0, 8, 0, ABFLAG_NONE},

    // Trog
    { ABIL_TROG_BURN_SPELLBOOKS, "Burn Spellbooks",
      0, 0, 10, 0, 0, ABFLAG_NONE},
    { ABIL_TROG_BERSERK, "Berserk", 0, 0, 200, 0, 0, ABFLAG_NONE},
    { ABIL_TROG_REGEN_MR, "Trog's Hand",
      0, 0, 50, generic_cost::range(2, 3), 0, ABFLAG_NONE},
    { ABIL_TROG_BROTHERS_IN_ARMS, "Brothers in Arms",
      0, 0, 100, generic_cost::range(5, 6), 0, ABFLAG_NONE},

    // Elyvilon
    { ABIL_ELYVILON_LIFESAVING, "Divine Protection",
      0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_ELYVILON_LESSER_HEALING_SELF, "Lesser Self-Healing",
      1, 0, 100, generic_cost::range(0, 1), 0, ABFLAG_CONF_OK},
    { ABIL_ELYVILON_LESSER_HEALING_OTHERS, "Lesser Healing",
      1, 0, 100, 0, 0, ABFLAG_NONE},
    { ABIL_ELYVILON_PURIFICATION, "Purification", 3, 0, 300, 3, 0,
      ABFLAG_CONF_OK},
    { ABIL_ELYVILON_GREATER_HEALING_SELF, "Greater Self-Healing",
      2, 0, 250, 3, 0, ABFLAG_CONF_OK},
    { ABIL_ELYVILON_GREATER_HEALING_OTHERS, "Greater Healing",
      2, 0, 250, 2, 0, ABFLAG_NONE},
    { ABIL_ELYVILON_DIVINE_VIGOUR, "Divine Vigour", 0, 0, 600, 6, 0,
      ABFLAG_CONF_OK},

    // Lugonu
    { ABIL_LUGONU_ABYSS_EXIT, "Depart the Abyss",
      1, 0, 150, 10, 0, ABFLAG_NONE},
    { ABIL_LUGONU_BEND_SPACE, "Bend Space", 1, 0, 50, 0, 0, ABFLAG_PAIN},
    { ABIL_LUGONU_BANISH, "Banish",
      4, 0, 200, generic_cost::range(3, 4), 0, ABFLAG_NONE},
    { ABIL_LUGONU_CORRUPT, "Corrupt",
      7, scaling_cost::fixed(5), 500, generic_cost::range(10, 14), 0, ABFLAG_NONE},
    { ABIL_LUGONU_ABYSS_ENTER, "Enter the Abyss",
      9, 0, 500, generic_cost::fixed(35), 0, ABFLAG_PAIN},

    // Nemelex
    { ABIL_NEMELEX_TRIPLE_DRAW, "Triple Draw", 2, 0, 100, 2, 0, ABFLAG_NONE},
    { ABIL_NEMELEX_DEAL_FOUR, "Deal Four", 8, 0, 200, 8, 0, ABFLAG_NONE},
    { ABIL_NEMELEX_STACK_FIVE, "Stack Five", 5, 0, 250, 10, 0, ABFLAG_NONE},

    // Beogh
    { ABIL_BEOGH_SMITING, "Smiting",
      3, 0, 80, generic_cost::fixed(3), 0, ABFLAG_NONE},
    { ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS, "Recall Orcish Followers",
      2, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_BEOGH_GIFT_ITEM, "Give Item to Named Follower",
      0, 0, 0, 0, 0, ABFLAG_NONE},

    // Jiyva
    { ABIL_JIYVA_CALL_JELLY, "Request Jelly", 2, 0, 20, 1, 0, ABFLAG_NONE},
    { ABIL_JIYVA_JELLY_PARALYSE, "Jelly Paralyse", 0, 0, 0, 0, 0, ABFLAG_PIETY},
    { ABIL_JIYVA_SLIMIFY, "Slimify", 4, 0, 100, 8, 0, ABFLAG_NONE},
    { ABIL_JIYVA_CURE_BAD_MUTATION, "Cure Bad Mutation",
      8, 0, 200, 15, 0, ABFLAG_NONE},

    // Fedhas
    { ABIL_FEDHAS_EVOLUTION, "Evolution", 2, 0, 0, 0, 0, ABFLAG_VARIABLE_FRUIT},
    { ABIL_FEDHAS_SUNLIGHT, "Sunlight", 2, 0, 50, 0, 0, ABFLAG_NONE},
    { ABIL_FEDHAS_PLANT_RING, "Growth", 2, 0, 0, 0, 0, ABFLAG_FRUIT},
    { ABIL_FEDHAS_SPAWN_SPORES, "Reproduction", 4, 0, 100, 1, 0, ABFLAG_NONE},
    { ABIL_FEDHAS_RAIN, "Rain", 4, 0, 150, 4, 0, ABFLAG_NONE},

    // Cheibriados
    { ABIL_CHEIBRIADOS_TIME_BEND, "Bend Time", 3, 0, 50, 1, 0, ABFLAG_NONE},
    { ABIL_CHEIBRIADOS_DISTORTION, "Temporal Distortion",
      4, 0, 200, 3, 0, ABFLAG_INSTANT},
    { ABIL_CHEIBRIADOS_SLOUCH, "Slouch", 5, 0, 100, 8, 0, ABFLAG_NONE},
    { ABIL_CHEIBRIADOS_TIME_STEP, "Step From Time",
      10, 0, 200, 10, 0, ABFLAG_NONE},

    // Ashenzari
    { ABIL_ASHENZARI_SCRYING, "Scrying",
      4, 0, 50, generic_cost::range(2, 3), 0, ABFLAG_INSTANT},
    { ABIL_ASHENZARI_TRANSFER_KNOWLEDGE, "Transfer Knowledge",
      0, 0, 0, 20, 0, ABFLAG_NONE},
    { ABIL_ASHENZARI_END_TRANSFER, "End Transfer Knowledge",
      0, 0, 0, 0, 0, ABFLAG_NONE},

    // Dithmenos
    { ABIL_DITHMENOS_SHADOW_STEP, "Shadow Step",
      4, 0, 0, 4, 0, ABFLAG_NONE },
    { ABIL_DITHMENOS_SHADOW_FORM, "Shadow Form",
      9, 0, 0, 10, 0, ABFLAG_SKILL_DRAIN },

    // Ru
    { ABIL_RU_DRAW_OUT_POWER, "Draw Out Power",
      0, 0, 0, 0, 0, ABFLAG_EXHAUSTION|ABFLAG_SKILL_DRAIN|ABFLAG_CONF_OK },
    { ABIL_RU_POWER_LEAP, "Power Leap",
      5, 0, 0, 0, 0, ABFLAG_EXHAUSTION },
    { ABIL_RU_APOCALYPSE, "Apocalypse",
      8, 0, 0, 0, 0, ABFLAG_EXHAUSTION|ABFLAG_SKILL_DRAIN },

    { ABIL_RU_SACRIFICE_PURITY, "Sacrifice Purity",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_WORDS, "Sacrifice Words",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_DRINK, "Sacrifice Drink",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_ESSENCE, "Sacrifice Essence",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_HEALTH, "Sacrifice Health",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_STEALTH, "Sacrifice Stealth",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_ARTIFICE, "Sacrifice Artifice",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_LOVE, "Sacrifice Love",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_COURAGE, "Sacrifice Courage",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_ARCANA, "Sacrifice Arcana",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_NIMBLENESS, "Sacrifice Nimbleness",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_DURABILITY, "Sacrifice Durability",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_SACRIFICE_HAND, "Sacrifice a Hand",
      0, 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_RU_REJECT_SACRIFICES, "Reject Sacrifices",
      0, 0, 0, 0, 0, ABFLAG_NONE },

    // Gozag
    { ABIL_GOZAG_POTION_PETITION, "Potion Petition",
      0, 0, 0, 0, 0, ABFLAG_GOLD },
    { ABIL_GOZAG_CALL_MERCHANT, "Call Merchant",
      0, 0, 0, 0, 0, ABFLAG_GOLD },
    { ABIL_GOZAG_BRIBE_BRANCH, "Bribe Branch",
      0, 0, 0, 0, 0, ABFLAG_GOLD },

    // Qazlal
    { ABIL_QAZLAL_UPHEAVAL, "Upheaval", 4, 0, 0, 3, 0, ABFLAG_NONE },
    { ABIL_QAZLAL_ELEMENTAL_FORCE, "Elemental Force",
      6, 0, 0, 6, 0, ABFLAG_NONE },
    { ABIL_QAZLAL_DISASTER_AREA, "Disaster Area", 7, 0, 0,
      generic_cost::range(10, 14), 0, ABFLAG_NONE },

    { ABIL_STOP_RECALL, "Stop Recall", 0, 0, 0, 0, 0, ABFLAG_NONE},

    // zot defence abilities
    { ABIL_MAKE_FUNGUS, "Make mushroom circle", 0, 0, 0, 0, 10, ABFLAG_ZOTDEF},
    { ABIL_MAKE_PLANT, "Make plant", 0, 0, 0, 0, 2, ABFLAG_ZOTDEF},
    { ABIL_MAKE_OKLOB_SAPLING, "Make oklob sapling", 0, 0, 0, 0, 60, ABFLAG_ZOTDEF},
    { ABIL_MAKE_BURNING_BUSH, "Make burning bush", 0, 0, 0, 0, 200, ABFLAG_ZOTDEF},
    { ABIL_MAKE_OKLOB_PLANT, "Make oklob plant", 0, 0, 0, 0, 250, ABFLAG_ZOTDEF},
    { ABIL_MAKE_ICE_STATUE, "Make ice statue", 0, 0, 0, 0, 2000, ABFLAG_ZOTDEF},
    { ABIL_MAKE_OCS, "Make crystal statue", 0, 0, 0, 0, 2000, ABFLAG_ZOTDEF},
    { ABIL_MAKE_OBSIDIAN_STATUE, "Make obsidian statue", 0, 0, 0, 0, 3000, ABFLAG_ZOTDEF},
    { ABIL_MAKE_CURSE_SKULL, "Make curse skull",
      0, 0, 600, 0, 10000, ABFLAG_ZOTDEF|ABFLAG_NECRO_MISCAST_MINOR},
    { ABIL_MAKE_TELEPORT, "Zot-teleport", 0, 0, 0, 0, 2, ABFLAG_ZOTDEF},
    { ABIL_MAKE_ARROW_TRAP, "Make arrow trap", 0, 0, 0, 0, 30, ABFLAG_ZOTDEF},
    { ABIL_MAKE_BOLT_TRAP, "Make bolt trap", 0, 0, 0, 0, 300, ABFLAG_ZOTDEF},
    { ABIL_MAKE_SPEAR_TRAP, "Make spear trap", 0, 0, 0, 0, 50, ABFLAG_ZOTDEF},
    { ABIL_MAKE_NEEDLE_TRAP, "Make needle trap", 0, 0, 0, 0, 30, ABFLAG_ZOTDEF},
    { ABIL_MAKE_NET_TRAP, "Make net trap", 0, 0, 0, 0, 2, ABFLAG_ZOTDEF},
    { ABIL_MAKE_ALARM_TRAP, "Make alarm trap", 0, 0, 0, 0, 2, ABFLAG_ZOTDEF},
    { ABIL_MAKE_BLADE_TRAP, "Make blade trap", 0, 0, 0, 0, 3000, ABFLAG_ZOTDEF},
    { ABIL_MAKE_OKLOB_CIRCLE, "Make oklob circle", 0, 0, 0, 0, 1000, ABFLAG_ZOTDEF},
    { ABIL_MAKE_ACQUIRE_GOLD, "Acquire gold",
      0, 0, 0, 0, 0, ABFLAG_ZOTDEF|ABFLAG_LEVEL_DRAIN},
    { ABIL_MAKE_ACQUIREMENT, "Acquirement",
      0, 0, 0, 0, 0, ABFLAG_ZOTDEF|ABFLAG_LEVEL_DRAIN},
    { ABIL_MAKE_WATER, "Make water", 0, 0, 0, 0, 10, ABFLAG_ZOTDEF},
    { ABIL_MAKE_LIGHTNING_SPIRE, "Make lightning spire", 0, 0, 0, 0, 100, ABFLAG_ZOTDEF},
    { ABIL_MAKE_BAZAAR, "Make bazaar",
      0, 30, 0, 0, 100, ABFLAG_ZOTDEF|ABFLAG_PERMANENT_HP},
    { ABIL_MAKE_ALTAR, "Make altar", 0, 0, 0, 0, 50, ABFLAG_ZOTDEF},
    { ABIL_MAKE_GRENADES, "Make grenades", 0, 0, 0, 0, 2, ABFLAG_ZOTDEF},
    { ABIL_REMOVE_CURSE, "Remove Curse",
      0, 0, 0, 0, 0, ABFLAG_ZOTDEF|ABFLAG_STAT_DRAIN},

    { ABIL_RENOUNCE_RELIGION, "Renounce Religion", 0, 0, 0, 0, 0, ABFLAG_NONE},
    { ABIL_CONVERT_TO_BEOGH, "Convert to Beogh", 0, 0, 0, 0, 0, ABFLAG_NONE},
};

const ability_def& get_ability_def(ability_type abil)
{
    for (unsigned int i = 0;
         i < sizeof(Ability_List) / sizeof(Ability_List[0]); i++)
    {
        if (Ability_List[i].ability == abil)
            return Ability_List[i];
    }

    return Ability_List[0];
}

bool string_matches_ability_name(const string& key)
{
    for (int i = ABIL_SPIT_POISON; i <= ABIL_RENOUNCE_RELIGION; ++i)
    {
        const ability_def abil = get_ability_def(static_cast<ability_type>(i));
        if (abil.ability == ABIL_NON_ABILITY)
            continue;

        const string name = lowercase_string(ability_name(abil.ability));
        if (name.find(key) != string::npos)
            return true;
    }
    return false;
}

string print_abilities()
{
    string text = "\n<w>a:</w> ";

    const vector<talent> talents = your_talents(false);

    if (talents.empty())
        text += "no special abilities";
    else
    {
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (i)
                text += ", ";
            text += ability_name(talents[i].which);
        }
    }

    return text;
}

static monster_type _monster_for_ability(const ability_def& abil)
{
    monster_type mtyp = MONS_PROGRAM_BUG;
    switch (abil.ability)
    {
        case ABIL_MAKE_PLANT:         mtyp = MONS_PLANT;         break;
        case ABIL_MAKE_FUNGUS:        mtyp = MONS_FUNGUS;        break;
        case ABIL_MAKE_OKLOB_SAPLING: mtyp = MONS_OKLOB_SAPLING; break;
        case ABIL_MAKE_OKLOB_CIRCLE:
        case ABIL_MAKE_OKLOB_PLANT:   mtyp = MONS_OKLOB_PLANT;   break;
        case ABIL_MAKE_BURNING_BUSH:  mtyp = MONS_BURNING_BUSH;  break;
        case ABIL_MAKE_LIGHTNING_SPIRE:  mtyp = MONS_LIGHTNING_SPIRE;  break;
        case ABIL_MAKE_ICE_STATUE:    mtyp = MONS_ICE_STATUE;    break;
        case ABIL_MAKE_OCS:           mtyp = MONS_ORANGE_STATUE; break;
        case ABIL_MAKE_OBSIDIAN_STATUE: mtyp = MONS_OBSIDIAN_STATUE; break;
        case ABIL_MAKE_CURSE_SKULL:   mtyp = MONS_CURSE_SKULL;   break;
        default:
            mprf("DEBUG: NO RELEVANT MONSTER FOR %d", abil.ability);
            break;
    }
    return mtyp;
}

static string _zd_mons_description_for_ability(const ability_def &abil)
{
    switch (abil.ability)
    {
    case ABIL_MAKE_PLANT:
        return "Tendrils and shoots erupt from the earth and gnarl into the form of a plant.";
    case ABIL_MAKE_OKLOB_SAPLING:
        return "A rhizome shoots up through the ground and merges with vitriolic spirits in the atmosphere.";
    case ABIL_MAKE_OKLOB_PLANT:
        return "A rhizome shoots up through the ground and merges with vitriolic spirits in the atmosphere.";
    case ABIL_MAKE_BURNING_BUSH:
        return "Blackened shoots writhe from the ground and burst into flame!";
    case ABIL_MAKE_ICE_STATUE:
        return "Water vapor collects and crystallises into an icy humanoid shape.";
    case ABIL_MAKE_OCS:
        return "Quartz juts from the ground and forms a humanoid shape. You smell citrus.";
    case ABIL_MAKE_OBSIDIAN_STATUE:
        return "Molten obsidian falls from the ceiling and solidifies into a vaguely humanoid shape.";
    case ABIL_MAKE_CURSE_SKULL:
        return "You sculpt a terrible being from the primitive principle of evil.";
    case ABIL_MAKE_LIGHTNING_SPIRE:
        return "You mount a charged rod inside a coil.";
    default:
        return "";
    }
}

static int _count_relevant_monsters(const ability_def& abil)
{
    monster_type mtyp = _monster_for_ability(abil);
    if (mtyp == MONS_PROGRAM_BUG)
        return 0;
    return count_monsters(mtyp, true);        // Friendly ones only
}

static trap_type _trap_for_ability(const ability_def& abil)
{
    switch (abil.ability)
    {
        case ABIL_MAKE_ARROW_TRAP: return TRAP_ARROW;
        case ABIL_MAKE_BOLT_TRAP: return TRAP_BOLT;
        case ABIL_MAKE_SPEAR_TRAP: return TRAP_SPEAR;
        case ABIL_MAKE_NEEDLE_TRAP: return TRAP_NEEDLE;
        case ABIL_MAKE_NET_TRAP: return TRAP_NET;
        case ABIL_MAKE_ALARM_TRAP: return TRAP_ALARM;
        case ABIL_MAKE_BLADE_TRAP: return TRAP_BLADE;
        default: return TRAP_UNASSIGNED;
    }
}

// Scale the zp cost by the number of friendly monsters
// of that type. Each successive critter costs 20% more
// than the last one, after the first two.
static int _zp_cost(const ability_def& abil)
{
    int cost = abil.zp_cost;
    int scale10 = 0;        // number of times to scale up by 10%
    int scale20 = 0;        // number of times to scale up by 20%
    int num;
    switch (abil.ability)
    {
        default:
            return abil.zp_cost;

        // Monster type 1: reasonably generous
        case ABIL_MAKE_PLANT:
        case ABIL_MAKE_FUNGUS:
        case ABIL_MAKE_OKLOB_SAPLING:
        case ABIL_MAKE_OKLOB_PLANT:
        case ABIL_MAKE_OKLOB_CIRCLE:
        case ABIL_MAKE_BURNING_BUSH:
        case ABIL_MAKE_LIGHTNING_SPIRE:
            num = _count_relevant_monsters(abil);
            // special case for oklob circles
            if (abil.ability == ABIL_MAKE_OKLOB_CIRCLE)
                num /= 3;
            // ... and for harmless stuff
            else if (abil.ability == ABIL_MAKE_PLANT
                     || abil.ability == ABIL_MAKE_FUNGUS)
            {
                num /= 5;
            }
            num -= 2;        // first two are base cost
            num = max(num, 0);
            scale10 = min(num, 10);       // next 10 at 10% increment
            scale20 = num - scale10;      // after that at 20% increment
            break;

        // Monster type 2: less generous
        case ABIL_MAKE_ICE_STATUE:
        case ABIL_MAKE_OCS:
            num = _count_relevant_monsters(abil);
            num -= 2; // first two are base cost
            scale20 = max(num, 0);        // after first two, 20% increment
            break;

        // Monster type 3: least generous
        case ABIL_MAKE_OBSIDIAN_STATUE:
        case ABIL_MAKE_CURSE_SKULL:
            scale20 = _count_relevant_monsters(abil); // scale immediately
            break;

        // Simple Traps
        case ABIL_MAKE_ARROW_TRAP:
        case ABIL_MAKE_BOLT_TRAP:
        case ABIL_MAKE_SPEAR_TRAP:
        case ABIL_MAKE_NEEDLE_TRAP:
        case ABIL_MAKE_NET_TRAP:
        case ABIL_MAKE_ALARM_TRAP:
            num = count_traps(_trap_for_ability(abil));
            scale10 = max(num-5, 0);   // First 5 at base cost
            break;

        case ABIL_MAKE_BLADE_TRAP:
            scale10 = count_traps(TRAP_BLADE); // Max of 18-ish at base cost 3000
            break;
    }

    float c = cost; // stave off round-off errors
    for (; scale10 > 0; scale10--)
        c = c * 1.1;        // +10%
    for (; scale20 > 0; scale20--)
        c = c * 1.2;        // +20%

    return c;
}

int get_gold_cost(ability_type ability)
{
    switch (ability)
    {
    case ABIL_GOZAG_CALL_MERCHANT:
        return gozag_price_for_shop(true);
    case ABIL_GOZAG_POTION_PETITION:
        return gozag_potion_price();
    case ABIL_GOZAG_BRIBE_BRANCH:
        return GOZAG_BRIBE_AMOUNT;
    default:
        return 0;
    }
}

const string make_cost_description(ability_type ability)
{
    const ability_def& abil = get_ability_def(ability);
    string ret;
    if (abil.mp_cost)
    {
        ret += make_stringf(", %d %sMP", abil.mp_cost,
            abil.flags & ABFLAG_PERMANENT_MP ? "Permanent " : "");
    }

    if (abil.hp_cost)
    {
        ret += make_stringf(", %d %sHP", abil.hp_cost.cost(you.hp_max),
            abil.flags & ABFLAG_PERMANENT_HP ? "Permanent " : "");
    }

    if (abil.zp_cost)
        ret += make_stringf(", %d ZP", (int)_zp_cost(abil));

    if (abil.food_cost && !you_foodless(true)
        && (you.undead_state() != US_SEMI_UNDEAD
            || you.hunger_state > HS_STARVING))
    {
        ret += ", Hunger"; // randomised and exact amount hidden from player
    }

    if (abil.piety_cost || abil.flags & ABFLAG_PIETY)
        ret += ", Piety"; // randomised and exact amount hidden from player

    if (abil.flags & ABFLAG_BREATH)
        ret += ", Breath";

    if (abil.flags & ABFLAG_DELAY)
        ret += ", Delay";

    if (abil.flags & ABFLAG_PAIN)
        ret += ", Pain";

    if (abil.flags & ABFLAG_EXHAUSTION)
        ret += ", Exhaustion";

    if (abil.flags & ABFLAG_INSTANT)
        ret += ", Instant"; // not really a cost, more of a bonus - bwr

    if (abil.flags & ABFLAG_FRUIT)
        ret += ", Fruit";

    if (abil.flags & ABFLAG_VARIABLE_FRUIT)
        ret += ", Fruit or Piety";

    if (abil.flags & ABFLAG_LEVEL_DRAIN)
        ret += ", Level drain";

    if (abil.flags & ABFLAG_STAT_DRAIN)
        ret += ", Stat drain";

    if (abil.flags & ABFLAG_SKILL_DRAIN)
        ret += ", Skill drain";

    if (abil.flags & ABFLAG_GOLD)
    {
        const int amount = get_gold_cost(ability);
        if (amount)
            ret += make_stringf(", %d Gold", amount);
        else
            ret += ", Gold";
    }

    // If we haven't output anything so far, then the effect has no cost
    if (ret.empty())
        return "None";

    ret.erase(0, 2);
    return ret;
}

static string _get_piety_amount_str(int value)
{
    return value > 15 ? "extremely large" :
           value > 10 ? "large" :
           value > 5  ? "moderate" :
                        "small";
}

static const string _detailed_cost_description(ability_type ability)
{
    const ability_def& abil = get_ability_def(ability);
    ostringstream ret;
    vector<string> values;
    string str;

    bool have_cost = false;
    ret << "This ability costs: ";

    if (abil.mp_cost > 0)
    {
        have_cost = true;
        if (abil.flags & ABFLAG_PERMANENT_MP)
            ret << "\nMax MP : ";
        else
            ret << "\nMP     : ";
        ret << abil.mp_cost;
    }
    if (abil.hp_cost)
    {
        have_cost = true;
        if (abil.flags & ABFLAG_PERMANENT_HP)
            ret << "\nMax HP : ";
        else
            ret << "\nHP     : ";
        ret << abil.hp_cost.cost(you.hp_max);
    }
    if (abil.zp_cost)
    {
        have_cost = true;
        ret << "\nZP     : ";
        ret << abil.zp_cost;
    }

    if (abil.food_cost && !you_foodless(true)
        && (you.undead_state() != US_SEMI_UNDEAD
            || you.hunger_state > HS_STARVING))
    {
        have_cost = true;
        ret << "\nHunger : ";
        ret << hunger_cost_string(abil.food_cost + abil.food_cost / 2);
    }

    if (abil.piety_cost || abil.flags & ABFLAG_PIETY)
    {
        have_cost = true;
        ret << "\nPiety  : ";
        if (abil.flags & ABFLAG_PIETY)
            ret << "variable";
        else
        {
            int avgcost = abil.piety_cost.base + abil.piety_cost.add / 2;
            ret << _get_piety_amount_str(avgcost);
        }
    }

    if (abil.flags & ABFLAG_GOLD)
    {
        have_cost = true;
        ret << "\nGold   : ";
        int gold_amount = get_gold_cost(ability);
        if (gold_amount)
            ret << gold_amount;
        else
            ret << "variable";
    }

    if (!have_cost)
        ret << "nothing.";

    if (abil.flags & ABFLAG_BREATH)
        ret << "\nYou must catch your breath between uses of this ability.";

    if (abil.flags & ABFLAG_DELAY)
        ret << "\nIt takes some time before being effective.";

    if (abil.flags & ABFLAG_PAIN)
        ret << "\nUsing this ability will hurt you.";

    if (abil.flags & ABFLAG_EXHAUSTION)
        ret << "\nIt cannot be used when exhausted.";

    if (abil.flags & ABFLAG_INSTANT)
        ret << "\nIt is instantaneous.";

    if (abil.flags & ABFLAG_CONF_OK)
        ret << "\nYou can use it even if confused.";

    if (abil.flags & ABFLAG_LEVEL_DRAIN)
        ret << "\nIt will lower your experience level by one when used.";

    if (abil.flags & ABFLAG_STAT_DRAIN)
        ret << "\nIt will temporarily drain your strength, intelligence or dexterity when used.";

    if (abil.flags & ABFLAG_SKILL_DRAIN)
        ret << "\nIt will temporarily drain your skills when used.";

    return ret.str();
}

static ability_type _fixup_ability(ability_type ability)
{
    switch (ability)
    {
    case ABIL_YRED_ANIMATE_REMAINS_OR_DEAD:
        // Placeholder for Animate Remains or Animate Dead.
        if (yred_can_animate_dead())
            return ABIL_YRED_ANIMATE_DEAD;
        else
            return ABIL_YRED_ANIMATE_REMAINS;

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        if (!you.recall_list.empty())
            return ABIL_STOP_RECALL;
        return ability;

    case ABIL_EVOKE_BERSERK:
    case ABIL_TROG_BERSERK:
        switch (you.species)
        {
#if TAG_MAJOR_VERSION == 34
        case SP_DJINNI:
#endif
        case SP_GHOUL:
        case SP_MUMMY:
        case SP_FORMICID:
            return ABIL_NON_ABILITY;
        default:
            return ability;
        }

    case ABIL_OKAWARU_FINESSE:
    case ABIL_BLINK:
    case ABIL_EVOKE_BLINK:
        if (you.species == SP_FORMICID)
            return ABIL_NON_ABILITY;
        else
            return ability;

    default:
        return ability;
    }
}

talent get_talent(ability_type ability, bool check_confused)
{
    ASSERT(ability != ABIL_NON_ABILITY);

    talent result;
    // Placeholder handling, part 1: The ability we have might be a
    // placeholder, so convert it into its corresponding ability before
    // doing anything else, so that we'll handle its flags properly.
    result.which = _fixup_ability(ability);

    const ability_def &abil = get_ability_def(result.which);

    int failure = 0;
    bool invoc = false;

    if (check_confused)
    {
        if (you.confused() && !testbits(abil.flags, ABFLAG_CONF_OK))
        {
            // Initialize these so compilers don't complain.
            result.is_invocation = 0;
            result.is_zotdef = 0;
            result.hotkey = 0;
            result.fail = 0;

            result.which = ABIL_NON_ABILITY;
            return result;
        }
    }

    // Look through the table to see if there's a preference, else find
    // a new empty slot for this ability. - bwr
    const int index = _find_ability_slot(abil);
    if (index != -1)
        result.hotkey = index_to_letter(index);
    else
        result.hotkey = 0;      // means 'find later on'

    switch (ability)
    {
    // begin spell abilities
    case ABIL_DELAYED_FIREBALL:
    case ABIL_MUMMY_RESTORATION:
    case ABIL_STOP_SINGING:
        failure = 0;
        break;

    // begin zot defence abilities
    case ABIL_MAKE_FUNGUS:
    case ABIL_MAKE_PLANT:
    case ABIL_MAKE_OKLOB_PLANT:
    case ABIL_MAKE_OKLOB_SAPLING:
    case ABIL_MAKE_BURNING_BUSH:
    case ABIL_MAKE_ICE_STATUE:
    case ABIL_MAKE_OCS:
    case ABIL_MAKE_OBSIDIAN_STATUE:
    case ABIL_MAKE_CURSE_SKULL:
    case ABIL_MAKE_TELEPORT:
    case ABIL_MAKE_ARROW_TRAP:
    case ABIL_MAKE_BOLT_TRAP:
    case ABIL_MAKE_SPEAR_TRAP:
    case ABIL_MAKE_NEEDLE_TRAP:
    case ABIL_MAKE_NET_TRAP:
    case ABIL_MAKE_ALARM_TRAP:
    case ABIL_MAKE_BLADE_TRAP:
    case ABIL_MAKE_OKLOB_CIRCLE:
    case ABIL_MAKE_ACQUIRE_GOLD:
    case ABIL_MAKE_ACQUIREMENT:
    case ABIL_MAKE_WATER:
    case ABIL_MAKE_LIGHTNING_SPIRE:
    case ABIL_MAKE_BAZAAR:
    case ABIL_MAKE_ALTAR:
    case ABIL_MAKE_GRENADES:
    case ABIL_REMOVE_CURSE:
        failure = 0;
        break;

    // begin species abilities - some are mutagenic, too {dlb}
    case ABIL_SPIT_POISON:
        failure = ((you.species == SP_NAGA) ? 20 : 40)
                        - 10 * player_mutation_level(MUT_SPIT_POISON)
                        - you.experience_level;
        break;

    case ABIL_BREATHE_FIRE:
        failure = ((you.species == SP_RED_DRACONIAN) ? 30 : 50)
                        - 10 * player_mutation_level(MUT_BREATHE_FLAMES)
                        - you.experience_level;

        if (you.form == TRAN_DRAGON)
            failure -= 20;
        break;
    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
    case ABIL_BREATHE_MEPHITIC:
        failure = 30 - you.experience_level;

        if (you.form == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_BREATHE_STEAM:
        failure = 20 - you.experience_level;

        if (you.form == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_FLY:
        failure = 42 - (3 * you.experience_level);
        break;

    case ABIL_TRAN_BAT:
        failure = 45 - (2 * you.experience_level);
        break;

    case ABIL_RECHARGING:       // this is for deep dwarves {1KB}
        failure = 45 - (2 * you.experience_level);
        break;

    case ABIL_DIG:
    case ABIL_SHAFT_SELF:
        failure = 0;
        break;
        // end species abilities (some mutagenic)

        // begin demonic powers {dlb}
    case ABIL_HELLFIRE:
        failure = 50 - you.experience_level;
        break;
        // end demonic powers {dlb}

    case ABIL_BLINK:
        failure = 48 - (12 * player_mutation_level(MUT_BLINK))
                  - you.experience_level / 2;
        break;

        // begin transformation abilities {dlb}
    case ABIL_END_TRANSFORMATION:
        failure = 0;
        break;
        // end transformation abilities {dlb}

        // begin item abilities - some possibly mutagenic {dlb}
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_TELEPORTATION:
        failure = 60 - you.skill(SK_EVOCATIONS, 2);
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
    case ABIL_STOP_FLYING:
        failure = 0;
        break;

    case ABIL_EVOKE_FLIGHT:
    case ABIL_EVOKE_BLINK:
        failure = 40 - you.skill(SK_EVOCATIONS, 2);
        break;
    case ABIL_EVOKE_BERSERK:
    case ABIL_EVOKE_FOG:
    case ABIL_EVOKE_TELEPORT_CONTROL:
        failure = 50 - you.skill(SK_EVOCATIONS, 2);
        break;
        // end item abilities - some possibly mutagenic {dlb}

        // begin invocations {dlb}
    // Abilities with no fail rate.
    case ABIL_ZIN_CURE_ALL_MUTATIONS:
    case ABIL_ELYVILON_LIFESAVING:
    case ABIL_TROG_BURN_SPELLBOOKS:
    case ABIL_ASHENZARI_TRANSFER_KNOWLEDGE:
    case ABIL_ASHENZARI_END_TRANSFER:
    case ABIL_ASHENZARI_SCRYING:
    case ABIL_BEOGH_GIFT_ITEM:
    case ABIL_JIYVA_CALL_JELLY:
    case ABIL_JIYVA_CURE_BAD_MUTATION:
    case ABIL_JIYVA_JELLY_PARALYSE:
    case ABIL_GOZAG_POTION_PETITION:
    case ABIL_GOZAG_CALL_MERCHANT:
    case ABIL_GOZAG_BRIBE_BRANCH:
    case ABIL_RU_DRAW_OUT_POWER:
    case ABIL_RU_POWER_LEAP:
    case ABIL_RU_APOCALYPSE:
    case ABIL_RU_SACRIFICE_PURITY:
    case ABIL_RU_SACRIFICE_WORDS:
    case ABIL_RU_SACRIFICE_DRINK:
    case ABIL_RU_SACRIFICE_ESSENCE:
    case ABIL_RU_SACRIFICE_HEALTH:
    case ABIL_RU_SACRIFICE_STEALTH:
    case ABIL_RU_SACRIFICE_ARTIFICE:
    case ABIL_RU_SACRIFICE_LOVE:
    case ABIL_RU_SACRIFICE_COURAGE:
    case ABIL_RU_SACRIFICE_ARCANA:
    case ABIL_RU_SACRIFICE_NIMBLENESS:
    case ABIL_RU_SACRIFICE_DURABILITY:
    case ABIL_RU_SACRIFICE_HAND:
    case ABIL_RU_REJECT_SACRIFICES:
    case ABIL_STOP_RECALL:
        invoc = true;
        failure = 0;
        break;

    case ABIL_YRED_ANIMATE_REMAINS_OR_DEAD: // Placeholder.
        invoc = true;
        break;

    // Trog and Jiyva abilities, only based on piety.
    case ABIL_TROG_BERSERK:    // piety >= 30
        invoc = true;
        failure = 0;
        break;

    case ABIL_TROG_REGEN_MR:            // piety >= 50
        invoc = true;
        failure = piety_breakpoint(2) - you.piety; // starts at 25%
        break;

    case ABIL_TROG_BROTHERS_IN_ARMS:    // piety >= 100
        invoc = true;
        failure = piety_breakpoint(5) - you.piety; // starts at 60%
        break;

    case ABIL_JIYVA_SLIMIFY:
        invoc = true;
        failure = 90 - you.piety / 2;
        break;

    // Other invocations, based on piety and Invocations skill.
    case ABIL_ELYVILON_PURIFICATION:
        invoc = true;
        failure = 20 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 5);
        break;

    case ABIL_ZIN_RECITE:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
    case ABIL_OKAWARU_HEROISM:
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    case ABIL_LUGONU_ABYSS_EXIT:
    case ABIL_FEDHAS_SUNLIGHT:
    case ABIL_FEDHAS_EVOLUTION:
    case ABIL_DITHMENOS_SHADOW_STEP:
        invoc = true;
        failure = 30 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 6);
        break;

    case ABIL_YRED_ANIMATE_REMAINS:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_YRED_INJURY_MIRROR:
    case ABIL_CHEIBRIADOS_TIME_BEND:
        invoc = true;
        failure = 40 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 4);
        break;

    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_BEOGH_SMITING:
    case ABIL_SIF_MUNA_FORGET_SPELL:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_GREATER_HEALING_SELF:
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
    case ABIL_LUGONU_BEND_SPACE:
    case ABIL_FEDHAS_PLANT_RING:
    case ABIL_QAZLAL_UPHEAVAL:
        invoc = true;
        failure = 40 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 5);
        break;

    case ABIL_KIKU_RECEIVE_CORPSES:
        invoc = true;
        failure = 40 - (you.piety / 20) - you.skill(SK_NECROMANCY, 5);
        break;

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        invoc = true;
        failure = 40 - you.intel() - you.skill(SK_INVOCATIONS, 1);
        break;

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
        invoc = true;
        failure = 50 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 4);
        break;

    case ABIL_ZIN_IMPRISON:
    case ABIL_LUGONU_BANISH:
    case ABIL_CHEIBRIADOS_DISTORTION:
    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        invoc = true;
        failure = 60 - (you.piety / 20) - you.skill(SK_INVOCATIONS, 5);
        break;

    case ABIL_KIKU_TORMENT:
        invoc = true;
        failure = 60 - (you.piety / 20) - you.skill(SK_NECROMANCY, 5);
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_FEDHAS_SPAWN_SPORES:
    case ABIL_YRED_DRAIN_LIFE:
    case ABIL_CHEIBRIADOS_SLOUCH:
    case ABIL_OKAWARU_FINESSE:
        invoc = true;
        failure = 60 - (you.piety / 25) - you.skill(SK_INVOCATIONS, 4);
        break;

    case ABIL_TSO_CLEANSING_FLAME:
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
    case ABIL_LUGONU_CORRUPT:
    case ABIL_FEDHAS_RAIN:
    case ABIL_QAZLAL_DISASTER_AREA:
        invoc = true;
        failure = 70 - (you.piety / 25) - you.skill(SK_INVOCATIONS, 4);
        break;

    case ABIL_ZIN_SANCTUARY:
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_YRED_ENSLAVE_SOUL:
    case ABIL_ELYVILON_DIVINE_VIGOUR:
    case ABIL_LUGONU_ABYSS_ENTER:
    case ABIL_CHEIBRIADOS_TIME_STEP:
    case ABIL_DITHMENOS_SHADOW_FORM:
        invoc = true;
        failure = 80 - (you.piety / 25) - you.skill(SK_INVOCATIONS, 4);
        break;

    case ABIL_NEMELEX_STACK_FIVE:
        invoc = true;
        failure = 80 - (you.piety / 25) - you.skill(SK_EVOCATIONS, 4);
        break;

    case ABIL_NEMELEX_DEAL_FOUR:
        invoc = true;
        failure = 70 - (you.piety * 2 / 45) - you.skill(SK_EVOCATIONS, 9) / 2;
        break;

    case ABIL_NEMELEX_TRIPLE_DRAW:
        invoc = true;
        failure = 60 - (you.piety / 20) - you.skill(SK_EVOCATIONS, 5);
        break;

    case ABIL_RENOUNCE_RELIGION:
    case ABIL_CONVERT_TO_BEOGH:
        invoc = true;
        failure = 0;
        break;

        // end invocations {dlb}
    default:
        failure = -1;
        break;
    }

    if (failure < 0)
        failure = 0;

    if (failure > 100)
        failure = 100;

    result.fail = failure;
    result.is_invocation = invoc;
    result.is_zotdef = abil.flags & ABFLAG_ZOTDEF;

    return result;
}

const char* ability_name(ability_type ability)
{
    return get_ability_def(ability).name;
}

vector<const char*> get_ability_names()
{
    vector<talent> talents = your_talents(false);
    vector<const char*> result;
    for (unsigned int i = 0; i < talents.size(); ++i)
        result.push_back(ability_name(talents[i].which));
    return result;
}

// XXX: should this be in describe.cc?
string get_ability_desc(const ability_type ability)
{
    const string& name = ability_name(ability);

    string lookup = getLongDescription(name + " ability");

    if (lookup.empty()) // Nothing found?
        lookup = "No description found.\n";

    if (god_hates_ability(ability, you.religion))
    {
        lookup += uppercase_first(god_name(you.religion))
                  + " frowns upon the use of this ability.\n";
    }

    ostringstream res;
    res << name << "\n\n" << lookup << "\n"
        << _detailed_cost_description(ability);

    const string quote = getQuoteString(name + " ability");
    if (!quote.empty())
        res << "\n\n" << quote;

    return res.str();
}

static void _print_talent_description(const talent& tal)
{
    clrscr();

    print_description(get_ability_desc(tal.which));

    getchm();
    clrscr();
}

void no_ability_msg()
{
    // Give messages if the character cannot use innate talents right now.
    // * Vampires can't turn into bats when full of blood.
    // * Tengu can't start to fly if already flying.
    if (you.species == SP_VAMPIRE && you.experience_level >= 3)
        mpr("Sorry, you're too full to transform right now.");
    else if (you.species == SP_TENGU && you.experience_level >= 5
             || player_mutation_level(MUT_BIG_WINGS))
    {
        if (you.flight_mode())
            mpr("You're already flying!");
    }
    else
        mpr("Sorry, you're not good enough to have a special ability.");
}

bool activate_ability()
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        crawl_state.zero_turns_taken();
        return false;
    }

    vector<talent> talents = your_talents(false);
    if (talents.empty())
    {
        no_ability_msg();
        crawl_state.zero_turns_taken();
        return false;
    }

    if (you.confused())
    {
        talents = your_talents(true);
        if (talents.empty())
        {
            canned_msg(MSG_TOO_CONFUSED);
            crawl_state.zero_turns_taken();
            return false;
        }
    }
#ifdef TOUCH_UI
    int selected = choose_ability_menu(talents);
    if (selected == -1)
    {
        canned_msg(MSG_OK);
        crawl_state.zero_turns_taken();
        return false;
    }
#else
    int selected = -1;
    while (selected < 0)
    {
        msg::streams(MSGCH_PROMPT) << "Use which ability? (? or * to list) "
                                   << endl;

        const int keyin = get_ch();

        if (keyin == '?' || keyin == '*')
        {
            selected = choose_ability_menu(talents);
            if (selected == -1)
            {
                canned_msg(MSG_OK);
                crawl_state.zero_turns_taken();
                return false;
            }
        }
        else if (key_is_escape(keyin) || keyin == ' ' || keyin == '\r'
                 || keyin == '\n')
        {
            canned_msg(MSG_OK);
            crawl_state.zero_turns_taken();
            return false;
        }
        else if (isaalpha(keyin))
        {
            // Try to find the hotkey.
            for (unsigned int i = 0; i < talents.size(); ++i)
            {
                if (talents[i].hotkey == keyin)
                {
                    selected = static_cast<int>(i);
                    break;
                }
            }

            // If we can't, cancel out.
            if (selected < 0)
            {
                mpr("You can't do that.");
                crawl_state.zero_turns_taken();
                return false;
            }
        }
    }
#endif
    return activate_talent(talents[selected]);
}

// Check prerequisites for a number of abilities.
// Abort any attempt if these cannot be met, without losing the turn.
// TODO: Many more cases need to be added!
static bool _check_ability_possible(const ability_def& abil,
                                    bool hungerCheck = true,
                                    bool quiet = false)
{
    if (you.berserk())
    {
        if (!quiet)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (you.confused() && !testbits(abil.flags, ABFLAG_CONF_OK))
    {
        if (!quiet)
            canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (silenced(you.pos()) && !you_worship(GOD_NEMELEX_XOBEH)
          && !you_worship(GOD_RU))
    {
        talent tal = get_talent(abil.ability, false);
        if (tal.is_invocation)
        {
            if (!quiet)
            {
                mprf("You cannot call out to %s while silenced.",
                     god_name(you.religion).c_str());
            }
            return false;
        }
    }
    // Don't insta-starve the player.
    // (Losing consciousness possible from 400 downward.)
    if (hungerCheck && !you.undead_state())
    {
        const int expected_hunger = you.hunger - abil.food_cost * 2;
        if (!quiet)
        {
            dprf("hunger: %d, max. food_cost: %d, expected hunger: %d",
                 you.hunger, abil.food_cost * 2, expected_hunger);
        }
        // Safety margin for natural hunger, mutations etc.
        if (expected_hunger <= 50)
        {
            if (!quiet)
                canned_msg(MSG_TOO_HUNGRY);
            return false;
        }
    }

    // in case of mp rot ability, check is the player have enough natural MP
    // (avoid use of ring/staf of magical power)
    if ((abil.flags & ABFLAG_PERMANENT_MP)
        && get_real_mp(false) < (int)abil.mp_cost)
    {
        if (!quiet)
            mpr("You don't have enough innate magic capacity to sacrifice.");
        return false;
    }

    switch (abil.ability)
    {
    case ABIL_ZIN_RECITE:
    {
        if (!zin_check_able_to_recite(quiet))
            return false;

        const int result = zin_check_recite_to_monsters(quiet);
        if (result == -1)
        {
            if (!quiet)
                mpr("There's no appreciative audience!");
            return false;
        }
        else if (result == 0)
        {
            if (!quiet)
                mpr("There's no-one here to preach to!");
            return false;
        }
        return true;
    }

    case ABIL_ZIN_CURE_ALL_MUTATIONS:
        return how_mutated();

    case ABIL_ZIN_SANCTUARY:
        if (env.sanctuary_time)
        {
            if (!quiet)
                mpr("There's already a sanctuary in place on this level.");
            return false;
        }
        return true;

    case ABIL_ELYVILON_PURIFICATION:
        if (!you.disease && !you.rotting && !you.duration[DUR_POISONING]
            && !you.duration[DUR_CONF] && !you.duration[DUR_SLOW]
            && !you.petrifying()
            && you.strength(false) == you.max_strength()
            && you.intel(false) == you.max_intel()
            && you.dex(false) == you.max_dex()
            && !player_rotted()
            && !you.duration[DUR_WEAK])
        {
            if (!quiet)
                mpr("Nothing ails you!");
            return false;
        }
        return true;

    case ABIL_MUMMY_RESTORATION:
        if (you.strength(false) == you.max_strength()
            && you.intel(false) == you.max_intel()
            && you.dex(false) == you.max_dex()
            && !player_rotted())
        {
            if (!quiet)
                mpr("You don't need to restore your stats or health!");
            return false;
        }
        return true;

    case ABIL_LUGONU_ABYSS_EXIT:
        if (!player_in_branch(BRANCH_ABYSS))
        {
            if (!quiet)
                mpr("You aren't in the Abyss!");
            return false;
        }
        return true;

    case ABIL_LUGONU_CORRUPT:
        return !is_level_incorruptible(quiet);

    case ABIL_LUGONU_ABYSS_ENTER:
        if (player_in_branch(BRANCH_ABYSS) || brdepth[BRANCH_ABYSS] == -1)
        {
            if (!quiet)
                mpr("You're already here!");
            return false;
        }
        return true;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        if (you.spell_no == 0)
        {
            if (!quiet)
                canned_msg(MSG_NO_SPELLS);
            return false;
        }
        return true;

    case ABIL_ASHENZARI_TRANSFER_KNOWLEDGE:
        if (all_skills_maxed(true))
        {
            if (!quiet)
                mpr("You have nothing more to learn.");
            return false;
        }
        return true;

    case ABIL_OKAWARU_FINESSE:
        if (stasis_blocks_effect(false,
                                 quiet ? NULL : "%s makes your neck tingle."))
        {
            return false;
        }
        return true;

    case ABIL_FEDHAS_EVOLUTION:
        return fedhas_check_evolve_flora(quiet);

    case ABIL_FEDHAS_SPAWN_SPORES:
    {
        const int retval = fedhas_check_corpse_spores(quiet);
        if (retval <= 0)
        {
            if (!quiet)
            {
                if (retval == 0)
                    mpr("No corpses are in range.");
                else
                    canned_msg(MSG_OK);
            }
            return false;
        }
        return true;
    }

    case ABIL_SPIT_POISON:
    case ABIL_BREATHE_FIRE:
    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
    case ABIL_BREATHE_STEAM:
    case ABIL_BREATHE_MEPHITIC:
        if (you.duration[DUR_BREATH_WEAPON])
        {
            if (!quiet)
                canned_msg(MSG_CANNOT_DO_YET);
            return false;
        }
        return true;

    case ABIL_BLINK:
    case ABIL_EVOKE_BLINK:
    {
        const string no_tele_reason = you.no_tele_reason(false, true);
        if (no_tele_reason.empty())
            return true;

        if (!quiet)
             mpr(no_tele_reason);
        return false;
    }

    case ABIL_EVOKE_BERSERK:
    case ABIL_TROG_BERSERK:
        return you.can_go_berserk(true, false, true)
               && (quiet || berserk_check_wielded_weapon());

    case ABIL_EVOKE_FOG:
        if (env.cgrid(you.pos()) != EMPTY_CLOUD)
        {
            if (!quiet)
                mpr("It's too cloudy to do that here.");
            return false;
        }
        return true;

    case ABIL_GOZAG_POTION_PETITION:
        return gozag_setup_potion_petition(quiet);

    case ABIL_GOZAG_CALL_MERCHANT:
        return gozag_setup_call_merchant(quiet);

    case ABIL_GOZAG_BRIBE_BRANCH:
        return gozag_check_bribe_branch(quiet);

    default:
        return true;
    }
}

bool check_ability_possible(const ability_type ability, bool hungerCheck,
                            bool quiet)
{
    return _check_ability_possible(get_ability_def(ability), hungerCheck,
                                   quiet);
}

bool activate_talent(const talent& tal)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        crawl_state.zero_turns_taken();
        return false;
    }

    // Doing these would outright kill the player.
    // (or, in the case of the stat-zeros, they'd at least be extremely
    // dangerous.)
    if (tal.which == ABIL_STOP_FLYING)
    {
        if (is_feat_dangerous(grd(you.pos()), false, true))
        {
            mpr("Stopping flight right now would be fatal!");
            crawl_state.zero_turns_taken();
            return false;
        }
    }
    else if (tal.which == ABIL_TRAN_BAT)
    {
        if (!check_form_stat_safety(TRAN_BAT))
        {
            crawl_state.zero_turns_taken();
            return false;
        }
    }
    else if (tal.which == ABIL_END_TRANSFORMATION)
    {
        if (feat_dangerous_for_form(TRAN_NONE, env.grid(you.pos())))
        {
            mprf("Turning back right now would cause you to %s!",
                 env.grid(you.pos()) == DNGN_LAVA ? "burn" : "drown");

            crawl_state.zero_turns_taken();
            return false;
        }

        if (!check_form_stat_safety(TRAN_NONE))
        {
            crawl_state.zero_turns_taken();
            return false;
        }
    }

    if ((tal.which == ABIL_EVOKE_BERSERK || tal.which == ABIL_TROG_BERSERK)
        && !you.can_go_berserk(true))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    if ((tal.which == ABIL_EVOKE_FLIGHT || tal.which == ABIL_TRAN_BAT || tal.which == ABIL_FLY)
        && !flight_allowed())
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    // Some abilities don't need a hunger check.
    bool hungerCheck = true;
    switch (tal.which)
    {
        case ABIL_RENOUNCE_RELIGION:
        case ABIL_CONVERT_TO_BEOGH:
        case ABIL_STOP_FLYING:
        case ABIL_EVOKE_TURN_VISIBLE:
        case ABIL_END_TRANSFORMATION:
        case ABIL_DELAYED_FIREBALL:
        case ABIL_STOP_SINGING:
        case ABIL_MUMMY_RESTORATION:
        case ABIL_TRAN_BAT:
        case ABIL_ASHENZARI_END_TRANSFER:
        case ABIL_ZIN_VITALISATION:
        case ABIL_GOZAG_POTION_PETITION:
            hungerCheck = false;
            break;
        default:
            break;
    }

    if (hungerCheck && !you.undead_state() && !you_foodless()
        && you.hunger_state == HS_STARVING)
    {
        canned_msg(MSG_TOO_HUNGRY);
        crawl_state.zero_turns_taken();
        return false;
    }

    const ability_def& abil = get_ability_def(tal.which);

    // Check that we can afford to pay the costs.
    // Note that mutation shenanigans might leave us with negative MP,
    // so don't fail in that case if there's no MP cost.
    if (abil.mp_cost > 0 && !enough_mp(abil.mp_cost, false, true))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    const int hpcost = abil.hp_cost.cost(you.hp_max);
    if (hpcost > 0 && !enough_hp(hpcost, false))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    const int zpcost = _zp_cost(abil);
    if (zpcost)
    {
        if (!enough_zp(zpcost, false))
        {
            crawl_state.zero_turns_taken();
            return false;
        }
    }

    if (!_check_ability_possible(abil, hungerCheck))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    bool fail = random2avg(100, 3) < tal.fail;

    const spret_type ability_result = _do_ability(abil, fail);
    switch (ability_result)
    {
        case SPRET_SUCCESS:
            ASSERT(!fail);
            practise(EX_USED_ABIL, abil.ability);
            _pay_ability_costs(abil, zpcost);
            count_action(tal.is_invocation ? CACT_INVOKE : CACT_ABIL, abil.ability);
            return true;
        case SPRET_FAIL:
            mpr("You fail to use your ability.");
            you.turn_is_over = true;
            return false;
        case SPRET_ABORT:
            crawl_state.zero_turns_taken();
            return false;
        case SPRET_NONE:
        default:
            die("Weird ability return type");
            return false;
    }
}

static int _calc_breath_ability_range(ability_type ability)
{
    // Following monster draconian abilities.
    switch (ability)
    {
    case ABIL_BREATHE_FIRE:         return 6;
    case ABIL_BREATHE_FROST:        return 6;
    case ABIL_BREATHE_MEPHITIC:     return 7;
    case ABIL_BREATHE_LIGHTNING:    return 8;
    case ABIL_SPIT_ACID:            return 8;
    case ABIL_BREATHE_POWER:        return 8;
    case ABIL_BREATHE_STICKY_FLAME: return 1;
    case ABIL_BREATHE_STEAM:        return 7;
    case ABIL_BREATHE_POISON:       return 7;
    default:
        die("Bad breath type!");
        break;
    }
    return -2;
}

static bool _sticky_flame_can_hit(const actor *act)
{
    if (act->is_monster())
    {
        const monster* mons = act->as_monster();
        bolt testbeam;
        testbeam.thrower = KILL_YOU;
        zappy(ZAP_BREATHE_STICKY_FLAME, 100, testbeam);

        return !testbeam.ignores_monster(mons);
    }
    else
        return false;
}

/*
 * Use an ability.
 *
 * @param abil The actual ability used.
 * @param fail If true, the ability is doomed to fail, and SPRET_FAIL will
 * be returned if the ability is not SPRET_ABORTed.
 * @returns Whether the spell succeeded (SPRET_SUCCESS), failed (SPRET_FAIL),
 *  or was canceled (SPRET_ABORT). Never returns SPRET_NONE.
 */
static spret_type _do_ability(const ability_def& abil, bool fail)
{
    int power;
    dist abild;
    bolt beam;
    dist spd;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;

    // Note: the costs will not be applied until after this switch
    // statement... it's assumed that only failures have returned! - bwr
    switch (abil.ability)
    {
    case ABIL_MAKE_FUNGUS:
        fail_check();
        if (count_allies() > MAX_MONSTERS / 2)
        {
            mpr("Mushrooms don't grow well in such thickets.");
            return SPRET_ABORT;
        }
        args.top_prompt="Center fungus circle where?";
        direction(abild, args);
        if (!abild.isValid)
        {
            if (abild.isCancel)
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        for (adjacent_iterator ai(abild.target); ai; ++ai)
        {
            place_monster(mgen_data(MONS_FUNGUS, BEH_FRIENDLY, &you, 0, 0, *ai,
                          you.pet_target), true);
        }
        break;

    // Begin ZotDef allies
    case ABIL_MAKE_PLANT:
    case ABIL_MAKE_OKLOB_SAPLING:
    case ABIL_MAKE_OKLOB_PLANT:
    case ABIL_MAKE_BURNING_BUSH:
    case ABIL_MAKE_ICE_STATUE:
    case ABIL_MAKE_OCS:
    case ABIL_MAKE_OBSIDIAN_STATUE:
    case ABIL_MAKE_CURSE_SKULL:
    case ABIL_MAKE_LIGHTNING_SPIRE:
        fail_check();
        if (!create_zotdef_ally(_monster_for_ability(abil),
            _zd_mons_description_for_ability(abil).c_str()))
        {
            return SPRET_ABORT;
        }
        break;
    // End ZotDef Allies

    case ABIL_MAKE_TELEPORT:
        fail_check();
        you_teleport_now(true);
        break;

    // ZotDef traps
    case ABIL_MAKE_ARROW_TRAP:
    case ABIL_MAKE_BOLT_TRAP:
    case ABIL_MAKE_SPEAR_TRAP:
    case ABIL_MAKE_NEEDLE_TRAP:
    case ABIL_MAKE_NET_TRAP:
    case ABIL_MAKE_ALARM_TRAP:
    case ABIL_MAKE_BLADE_TRAP:
        fail_check();
        if (!create_trap(_trap_for_ability(abil)))
            return SPRET_ABORT;
        break;
    // End ZotDef traps

    case ABIL_MAKE_OKLOB_CIRCLE:
        fail_check();
        args.top_prompt = "Center oklob circle where?";
        direction(abild, args);
        if (!abild.isValid)
        {
            if (abild.isCancel)
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        for (adjacent_iterator ai(abild.target); ai; ++ai)
        {
            place_monster(mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, &you, 0, 0,
                          *ai, you.pet_target), true);
        }
        break;

    case ABIL_MAKE_ACQUIRE_GOLD:
        fail_check();
        acquirement(OBJ_GOLD, AQ_SCROLL);
        break;

    case ABIL_MAKE_ACQUIREMENT:
        fail_check();
        acquirement(OBJ_RANDOM, AQ_SCROLL);
        break;

    case ABIL_MAKE_WATER:
        fail_check();
        zotdef_create_pond(you.pos(), 3);
        break;

    case ABIL_MAKE_BAZAAR:
    {
        fail_check();
        // Early exit: don't clobber important features.
        if (feat_is_critical(grd(you.pos())))
        {
            mpr("The dungeon trembles momentarily.");
            return SPRET_ABORT;
        }

        // Generate a portal to something.
        const map_def *mapidx = random_map_for_tag("zotdef_bazaar", false);
        if (mapidx && dgn_safe_place_map(mapidx, false, true, you.pos()))
            mpr("A mystic portal forms.");
        else
        {
            mpr("A buggy portal flickers into view, then vanishes.");
            return SPRET_ABORT;
        }

        break;
    }

    case ABIL_MAKE_ALTAR:
        fail_check();
        if (!zotdef_create_altar())
        {
            mpr("The dungeon dims for a moment.");
            return SPRET_ABORT;
        }
        break;

    case ABIL_MAKE_GRENADES:
        fail_check();
        if (create_monster(
               mgen_data(MONS_GIANT_SPORE, BEH_FRIENDLY, &you, 6, 0,
                         you.pos(), you.pet_target,
                         0)))
        {
            mpr("You create a living grenade.");
        }
        if (create_monster(
               mgen_data(MONS_GIANT_SPORE, BEH_FRIENDLY, &you, 6, 0,
                         you.pos(), you.pet_target,
                         0)))
        {
            mpr("You create a living grenade.");
        }
        break;

    case ABIL_REMOVE_CURSE:
        fail_check();
        remove_curse();
        lose_stat(STAT_RANDOM, 1, true, "zot ability");
        break;

    case ABIL_MUMMY_RESTORATION:
    {
        fail_check();
        mpr("You infuse your body with magical energy.");
        bool did_restore = restore_stat(STAT_ALL, 0, false);

        const int oldhpmax = you.hp_max;
        unrot_hp(9999);
        if (you.hp_max > oldhpmax)
            did_restore = true;

        // If nothing happened, don't take one max MP, don't use a turn.
        if (!did_restore)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return SPRET_ABORT;
        }

        break;
    }

    case ABIL_RECHARGING:
        fail_check();
        if (recharge_wand() <= 0)
            return SPRET_ABORT; // fail message is already given
        break;

    case ABIL_DIG:
        fail_check();
        if (!you.digging)
        {
            you.digging = true;
            mpr("You extend your mandibles.");
        }
        else
        {
            mpr("You are already prepared to dig.");
            return SPRET_ABORT;
        }
        break;

    case ABIL_SHAFT_SELF:
        fail_check();
        if (you.can_do_shaft_ability(false))
        {
            if (yesno("Are you sure you want to shaft yourself?", true, 'n'))
                start_delay(DELAY_SHAFT_SELF, 1);
            else
                return SPRET_ABORT;
        }
        else
            return SPRET_ABORT;
        break;

    case ABIL_DELAYED_FIREBALL:
    {
        fail_check();
        // Note: Power level of ball calculated at release. - bwr
        power = calc_spell_power(SPELL_DELAYED_FIREBALL, true);
        beam.range = spell_range(SPELL_FIREBALL, power);

        targetter_beam tgt(&you, beam.range, ZAP_FIREBALL, power, 1, 1);

        if (!spell_direction(spd, beam, DIR_NONE, TARG_HOSTILE, beam.range,
                             true, true, false, NULL,
                             "Aiming: <white>Delayed Fireball</white>",
                             false, &tgt))
        {
            return SPRET_ABORT;
        }

        if (!zapping(ZAP_FIREBALL, power, beam, true, NULL, false))
            return SPRET_ABORT;

        // Only one allowed, since this is instantaneous. - bwr
        you.attribute[ATTR_DELAYED_FIREBALL] = 0;
        break;
    }

    case ABIL_SPIT_POISON:      // Naga + spit poison mutation
        power = you.experience_level
                + player_mutation_level(MUT_SPIT_POISON) * 5
                + (you.species == SP_NAGA) * 10;
        beam.range = 6;         // following Venom Bolt

        if (!spell_direction(abild, beam)
            || !player_tracer(ZAP_SPIT_POISON, power, beam))
        {
            return SPRET_ABORT;
        }
        else
        {
            fail_check();
            zapping(ZAP_SPIT_POISON, power, beam);
            zin_recite_interrupt();
            you.set_duration(DUR_BREATH_WEAPON, 3 + random2(5));
        }
        break;

    case ABIL_EVOKE_TELEPORTATION:    // ring of teleportation
        fail_check();
        you_teleport();
        break;

    case ABIL_BREATHE_STICKY_FLAME:
    {
        targetter_splash hitfunc(&you);
        beam.range = 1;
        if (!spell_direction(abild, beam,
                             DIR_NONE, TARG_HOSTILE, 0, true, true, false,
                             NULL, NULL, false,
                             &hitfunc))
        {
            return SPRET_ABORT;
        }

        if (stop_attack_prompt(hitfunc, "spit at", _sticky_flame_can_hit))
            return SPRET_ABORT;

        fail_check();
        zapping(ZAP_BREATHE_STICKY_FLAME, (you.form == TRAN_DRAGON) ?
                2 * you.experience_level : you.experience_level,
            beam, false, "You spit a glob of burning liquid.");

        zin_recite_interrupt();
        you.increase_duration(DUR_BREATH_WEAPON,
                      3 + random2(10) + random2(30 - you.experience_level));
        break;
    }

    case ABIL_BREATHE_FIRE:
    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STEAM:
    case ABIL_BREATHE_MEPHITIC:
        beam.range = _calc_breath_ability_range(abil.ability);
        if (!spell_direction(abild, beam))
            return SPRET_ABORT;

        // fallthrough to ABIL_BREATHE_LIGHTNING

    case ABIL_BREATHE_LIGHTNING: // not targeted
        fail_check();

        // TODO: refactor this to use only one call to zapping(), don't
        // duplicate its fail_check(), split out breathe_lightning, etc

        switch (abil.ability)
        {
        case ABIL_BREATHE_FIRE:
            power = you.experience_level
                    + player_mutation_level(MUT_BREATHE_FLAMES) * 4;

            if (you.form == TRAN_DRAGON)
                power += 12;

            snprintf(info, INFO_SIZE, "You breathe a blast of fire%c",
                     (power < 15) ? '.':'!');

            if (!zapping(ZAP_BREATHE_FIRE, power, beam, true, info))
                return SPRET_ABORT;
            break;

        case ABIL_BREATHE_FROST:
            if (!zapping(ZAP_BREATHE_FROST,
                 (you.form == TRAN_DRAGON) ?
                     2 * you.experience_level : you.experience_level,
                 beam, true,
                         "You exhale a wave of freezing cold."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_POISON:
            if (!zapping(ZAP_BREATHE_POISON, you.experience_level, beam, true,
                         "You exhale a blast of poison gas."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_LIGHTNING:
            mpr("You breathe a wild blast of lightning!");
            disc_of_storms(true);
            break;

        case ABIL_SPIT_ACID:
            if (!zapping(ZAP_BREATHE_ACID,
                (you.form == TRAN_DRAGON) ?
                    2 * you.experience_level : you.experience_level,
                beam, true, "You spit a glob of acid."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_POWER:
            if (!zapping(ZAP_BREATHE_POWER,
                (you.form == TRAN_DRAGON) ?
                    2 * you.experience_level : you.experience_level,
                beam, true,
                         "You spit a bolt of dispelling energy."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_STICKY_FLAME:
            if (!zapping(ZAP_BREATHE_STICKY_FLAME,
                (you.form == TRAN_DRAGON) ?
                    2 * you.experience_level : you.experience_level,
                beam, true,
                         "You spit a glob of burning liquid."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_STEAM:
            if (!zapping(ZAP_BREATHE_STEAM,
                (you.form == TRAN_DRAGON) ?
                    2 * you.experience_level : you.experience_level,
                beam, true,
                         "You exhale a blast of scalding steam."))
            {
                return SPRET_ABORT;
            }
            break;

        case ABIL_BREATHE_MEPHITIC:
            if (!zapping(ZAP_BREATHE_MEPHITIC,
                (you.form == TRAN_DRAGON) ?
                    2 * you.experience_level : you.experience_level,
                beam, true,
                         "You exhale a blast of noxious fumes."))
            {
                return SPRET_ABORT;
            }
            break;

        default:
            break;
        }

        zin_recite_interrupt();
        you.increase_duration(DUR_BREATH_WEAPON,
                      3 + random2(10) + random2(30 - you.experience_level));

        if (abil.ability == ABIL_BREATHE_STEAM
            || abil.ability == ABIL_SPIT_ACID)
        {
            you.duration[DUR_BREATH_WEAPON] /= 2;
        }

        break;

    case ABIL_EVOKE_BLINK:      // randarts
    case ABIL_BLINK:            // mutation
        fail_check();
        random_blink(true);
        break;

    case ABIL_EVOKE_BERSERK:    // amulet of rage, randarts
        fail_check();
        you.go_berserk(true);
        break;

    // Fly (tengu/drac) - permanent at high XL
    case ABIL_FLY:
        fail_check();
        if (you.racial_permanent_flight())
        {
            you.attribute[ATTR_PERM_FLIGHT] = 1;
            float_player();
            if (you.species == SP_TENGU)
                mpr("You feel very comfortable in the air.");
        }
        else
            cast_fly(you.experience_level * 4);
        break;

    // DEMONIC POWERS:
    case ABIL_HELLFIRE:
        fail_check();
        if (your_spells(SPELL_HELLFIRE,
                        you.experience_level * 10, false) == SPRET_ABORT)
        {
            return SPRET_ABORT;
        }
        break;

    case ABIL_EVOKE_TURN_INVISIBLE:     // ring, cloaks, randarts
        fail_check();
        potion_effect(POT_INVISIBILITY, you.skill(SK_EVOCATIONS, 2) + 5);
        contaminate_player(1000 + random2(2000), true);
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
        fail_check();
        ASSERT(!you.attribute[ATTR_INVIS_UNCANCELLABLE]);
        mpr("You feel less transparent.");
        you.duration[DUR_INVIS] = 1;
        break;

    case ABIL_EVOKE_FLIGHT:             // ring, boots, randarts
        fail_check();
        ASSERT(!get_form()->forbids_flight());
        if (you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING))
        {
            bool standing = !you.airborne();
            you.attribute[ATTR_PERM_FLIGHT] = 1;
            if (standing)
                float_player();
            else
                mpr("You feel more buoyant.");
        }
        else
            fly_player(you.skill(SK_EVOCATIONS, 2) + 30);
        break;
    case ABIL_EVOKE_FOG:     // cloak of the Thief
        fail_check();
        mpr("With a swish of your cloak, you release a cloud of fog.");
        big_cloud(random_smoke_type(), &you, you.pos(), 50, 8 + random2(8));
        break;

    case ABIL_EVOKE_TELEPORT_CONTROL:
        fail_check();
        cast_teleport_control(30 + you.skill(SK_EVOCATIONS, 2), false);
        break;

    case ABIL_STOP_SINGING:
        fail_check();
        you.duration[DUR_SONG_OF_SLAYING] = 0;
        mpr("You stop singing.");
        break;

    case ABIL_STOP_FLYING:
        fail_check();
        you.duration[DUR_FLIGHT] = 0;
        you.attribute[ATTR_PERM_FLIGHT] = 0;
        land_player();
        break;

    case ABIL_END_TRANSFORMATION:
        fail_check();
        you.time_taken = div_rand_round(you.time_taken * 3, 2);
        untransform();
        break;

    // INVOCATIONS:
    case ABIL_ZIN_RECITE:
    {
        fail_check();
        if (zin_check_recite_to_monsters())
        {
            you.attribute[ATTR_RECITE_TYPE] = (recite_type) random2(NUM_RECITE_TYPES); // This is just flavor
            you.attribute[ATTR_RECITE_SEED] = random2(2187); // 3^7
            you.attribute[ATTR_RECITE_HP]   = you.hp;
            you.duration[DUR_RECITE] = 3 * BASELINE_DELAY;
            mprf("You clear your throat and prepare to recite.");
        }
        else
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        break;
    }
    case ABIL_ZIN_VITALISATION:
        fail_check();
        zin_recite_interrupt();
        zin_vitalisation();
        break;

    case ABIL_ZIN_IMPRISON:
    {
        beam.range = LOS_RADIUS;
        if (!spell_direction(spd, beam, DIR_TARGET, TARG_HOSTILE, 0, false))
            return SPRET_ABORT;

        if (beam.target == you.pos())
        {
            mpr("You cannot imprison yourself!");
            return SPRET_ABORT;
        }

        monster* mons = monster_at(beam.target);

        if (mons == NULL || !you.can_see(mons))
        {
            mpr("There is no monster there to imprison!");
            return SPRET_ABORT;
        }

        if (mons_is_firewood(mons) || mons_is_conjured(mons->type))
        {
            mpr("You cannot imprison that!");
            return SPRET_ABORT;
        }

        if (mons->friendly() || mons->good_neutral())
        {
            mpr("You cannot imprison a law-abiding creature!");
            return SPRET_ABORT;
        }

        fail_check();

        zin_recite_interrupt();
        power = 3 + (roll_dice(5, you.skill(SK_INVOCATIONS, 5) + 12) / 26);

        if (!cast_imprison(power, mons, -GOD_ZIN))
            return SPRET_ABORT;
        break;
    }

    case ABIL_ZIN_SANCTUARY:
        fail_check();
        zin_recite_interrupt();
        if (!zin_sanctuary())
            return SPRET_ABORT;
        break;

    case ABIL_ZIN_CURE_ALL_MUTATIONS:
        fail_check();
        zin_recite_interrupt();
        zin_remove_all_mutations();
        break;

    case ABIL_TSO_DIVINE_SHIELD:
        fail_check();
        tso_divine_shield();
        break;

    case ABIL_TSO_CLEANSING_FLAME:
        fail_check();
        cleansing_flame(10 + you.skill_rdiv(SK_INVOCATIONS, 7, 6),
                        CLEANSING_FLAME_INVOCATION, you.pos(), &you);
        break;

    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
        fail_check();
        summon_holy_warrior(you.skill(SK_INVOCATIONS, 4), false);
        break;

    case ABIL_KIKU_RECEIVE_CORPSES:
        fail_check();
        kiku_receive_corpses(you.skill(SK_NECROMANCY, 4));
        break;

    case ABIL_KIKU_TORMENT:
        fail_check();
        if (!kiku_take_corpse())
        {
            mpr("There are no corpses to sacrifice!");
            return SPRET_ABORT;
        }
        simple_god_message(" torments the living!");
        torment(&you, TORMENT_KIKUBAAQUDGHA, you.pos());
        break;

    case ABIL_YRED_INJURY_MIRROR:
        fail_check();
        if (yred_injury_mirror())
            mpr("Another wave of unholy energy enters you.");
        else
        {
            mprf("You offer yourself to %s, and fill with unholy energy.",
                 god_name(you.religion).c_str());
        }
        you.duration[DUR_MIRROR_DAMAGE] = 9 * BASELINE_DELAY
                     + random2avg(you.piety * BASELINE_DELAY, 2) / 10;
        break;

    case ABIL_YRED_ANIMATE_REMAINS:
    case ABIL_YRED_ANIMATE_DEAD:
        fail_check();
        if (!yred_animate_remains_or_dead())
            return SPRET_ABORT;
        break;

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
        fail_check();
        start_recall(1);
        break;

    case ABIL_YRED_DRAIN_LIFE:
        fail_check();
        cast_los_attack_spell(SPELL_DRAIN_LIFE, you.skill_rdiv(SK_INVOCATIONS),
                              &you, true);
        break;

    case ABIL_YRED_ENSLAVE_SOUL:
    {
        god_acting gdact;
        power = you.skill(SK_INVOCATIONS, 4);
        beam.range = LOS_RADIUS;

        if (!spell_direction(spd, beam))
            return SPRET_ABORT;

        if (beam.target == you.pos())
        {
            mpr("Your soul already belongs to Yredelemnul.");
            return SPRET_ABORT;
        }

        monster* mons = monster_at(beam.target);
        if (mons == NULL || !you.can_see(mons)
            || !ench_flavour_affects_monster(BEAM_ENSLAVE_SOUL, mons))
        {
            mpr("You see nothing there to enslave the soul of!");
            return SPRET_ABORT;
        }

        // The monster can be no more than lightly wounded/damaged.
        if (mons_get_damage_level(mons) > MDAM_LIGHTLY_DAMAGED)
        {
            simple_monster_message(mons, "'s soul is too badly injured.");
            return SPRET_ABORT;
        }
        fail_check();

        return zapping(ZAP_ENSLAVE_SOUL, power, beam, false, NULL, fail);
    }

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        fail_check();
        mpr("You channel some magical energy.");

        inc_mp(1 + random2(you.skill_rdiv(SK_INVOCATIONS, 1, 4) + 2));
        break;

    case ABIL_OKAWARU_HEROISM:
        fail_check();
        mprf(MSGCH_DURATION, you.duration[DUR_HEROISM]
             ? "You feel more confident with your borrowed prowess."
             : "You gain the combat prowess of a mighty hero.");

        you.increase_duration(DUR_HEROISM,
            35 + random2(you.skill(SK_INVOCATIONS, 8)), 80);
        you.redraw_evasion      = true;
        you.redraw_armour_class = true;
        break;

    case ABIL_OKAWARU_FINESSE:
        fail_check();
        if (stasis_blocks_effect(true, "%s emits a piercing whistle.",
                                 20, "%s makes your neck tingle."))
        {
            // Identify the amulet and spend costs - finesse will be aborted
            // for free with an identified amulet.
            break;
        }

        if (you.duration[DUR_FINESSE])
        {
            // "Your [hand(s)] get{s} new energy."
            mprf(MSGCH_DURATION, "%s",
                 you.hands_act("get", "new energy.").c_str());
        }
        else
            mprf(MSGCH_DURATION, "You can now deal lightning-fast blows.");

        you.increase_duration(DUR_FINESSE,
            40 + random2(you.skill(SK_INVOCATIONS, 8)), 80);

        did_god_conduct(DID_HASTY, 8); // Currently irrelevant.
        break;

    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
        beam.range = 8;

        if (!spell_direction(spd, beam))
            return SPRET_ABORT;

        power = you.skill(SK_INVOCATIONS, 1)
                + random2(1 + you.skill(SK_INVOCATIONS, 1))
                + random2(1 + you.skill(SK_INVOCATIONS, 1));

        // Since the actual beam is random, check with BEAM_MMISSILE and the
        // highest range possible.
        if (!player_tracer(ZAP_DEBUGGING_RAY, power, beam, 8))
            return SPRET_ABORT;

        fail_check();

        switch (random2(5))
        {
        case 0: zapping(ZAP_THROW_FLAME, power, beam); break;
        case 1: zapping(ZAP_PAIN,  power, beam); break;
        case 2: zapping(ZAP_STONE_ARROW, power, beam); break;
        case 3: zapping(ZAP_SHOCK, power, beam); break;
        case 4: zapping(ZAP_BREATHE_ACID, power/2, beam); break;
        }
        break;

    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
        fail_check();
        summon_demon_type(random_choose(MONS_HELLWING, MONS_NEQOXEC,
                          MONS_ORANGE_DEMON, MONS_SMOKE_DEMON, MONS_YNOXINUL, -1),
                          20 + you.skill(SK_INVOCATIONS, 3), GOD_MAKHLEB);
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
        beam.range = 6;

        if (!spell_direction(spd, beam))
            return SPRET_ABORT;

        power = you.skill(SK_INVOCATIONS, 3)
                + random2(1 + you.skill(SK_INVOCATIONS, 1))
                + random2(1 + you.skill(SK_INVOCATIONS, 1));

        // Since the actual beam is random, check with BEAM_MMISSILE and the
        // highest range possible.
        if (!player_tracer(ZAP_DEBUGGING_RAY, power, beam, 8))
            return SPRET_ABORT;

        fail_check();

        {
            zap_type ztype =
                random_choose(ZAP_BOLT_OF_FIRE,
                              ZAP_FIREBALL,
                              ZAP_LIGHTNING_BOLT,
                              ZAP_STICKY_FLAME,
                              ZAP_IRON_SHOT,
                              ZAP_BOLT_OF_DRAINING,
                              ZAP_ORB_OF_ELECTRICITY,
                              -1);
            zapping(ztype, power, beam);
        }
        break;

    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        fail_check();
        summon_demon_type(random_choose(MONS_EXECUTIONER, MONS_GREEN_DEATH,
                          MONS_BLIZZARD_DEMON, MONS_BALRUG, MONS_CACODEMON, -1),
                          20 + you.skill(SK_INVOCATIONS, 3), GOD_MAKHLEB);
        break;

    case ABIL_TROG_BURN_SPELLBOOKS:
        fail_check();
        if (!trog_burn_spellbooks())
            return SPRET_ABORT;
        break;

    case ABIL_TROG_BERSERK:
        fail_check();
        // Trog abilities don't use or train invocations.
        you.go_berserk(true);
        break;

    case ABIL_TROG_REGEN_MR:
        fail_check();
        // Trog abilities don't use or train invocations.
        trog_do_trogs_hand(you.piety / 2);
        break;

    case ABIL_TROG_BROTHERS_IN_ARMS:
        fail_check();
        // Trog abilities don't use or train invocations.
        summon_berserker(you.piety +
                         random2(you.piety/4) - random2(you.piety/4),
                         &you);
        break;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        fail_check();
        if (cast_selective_amnesia() <= 0)
            return SPRET_ABORT;
        break;

    case ABIL_ELYVILON_LIFESAVING:
        fail_check();
        if (you.duration[DUR_LIFESAVING])
            mpr("You renew your call for help.");
        else
        {
            mprf("You beseech %s to protect your life.",
                 god_name(you.religion).c_str());
        }
        // Might be a decrease, this is intentional (like Yred).
        you.duration[DUR_LIFESAVING] = 9 * BASELINE_DELAY
                     + random2avg(you.piety * BASELINE_DELAY, 2) / 10;
        break;

    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    {
        fail_check();
        const bool self = (abil.ability == ABIL_ELYVILON_LESSER_HEALING_SELF);
        int pow = 3 + (you.skill_rdiv(SK_INVOCATIONS, 1, 6));
#if TAG_MAJOR_VERSION == 34
        if (self && you.species == SP_DJINNI)
            pow /= 2;
#endif
        if (cast_healing(pow,
                         3 + (int) ceil(you.skill(SK_INVOCATIONS, 1) / 6.0),
                         true, self ? you.pos() : coord_def(0, 0), !self,
                         self ? TARG_NUM_MODES : TARG_INJURED_FRIEND)
                         == SPRET_ABORT)
        {
            return SPRET_ABORT;
        }
        break;
    }

    case ABIL_ELYVILON_PURIFICATION:
        fail_check();
        elyvilon_purification();
        break;

    case ABIL_ELYVILON_GREATER_HEALING_SELF:
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
    {
        fail_check();
        const bool self = (abil.ability == ABIL_ELYVILON_GREATER_HEALING_SELF);

        int pow = 10 + (you.skill_rdiv(SK_INVOCATIONS, 1, 3));
#if TAG_MAJOR_VERSION == 34
        if (self && you.species == SP_DJINNI)
            pow /= 2;
#endif
        if (cast_healing(pow,
                         10 + (int) ceil(you.skill(SK_INVOCATIONS, 1) / 3.0),
                         true, self ? you.pos() : coord_def(0, 0), !self,
                         self ? TARG_NUM_MODES : TARG_INJURED_FRIEND)
                         == SPRET_ABORT)
        {
            return SPRET_ABORT;
        }
        break;
    }

    case ABIL_ELYVILON_DIVINE_VIGOUR:
        fail_check();
        if (!elyvilon_divine_vigour())
            return SPRET_ABORT;
        break;

    case ABIL_LUGONU_ABYSS_EXIT:
        fail_check();
        down_stairs(DNGN_EXIT_ABYSS);
        break;

    case ABIL_LUGONU_BEND_SPACE:
        fail_check();
        lugonu_bend_space();
        break;

    case ABIL_LUGONU_BANISH:
        beam.range = LOS_RADIUS;

        if (!spell_direction(spd, beam))
            return SPRET_ABORT;

        if (beam.target == you.pos())
        {
            mpr("You cannot banish yourself!");
            return SPRET_ABORT;
        }

        return zapping(ZAP_BANISHMENT, 16 + you.skill(SK_INVOCATIONS, 8), beam,
                       true, NULL, fail);

    case ABIL_LUGONU_CORRUPT:
        fail_check();
        if (!lugonu_corrupt_level(300 + you.skill(SK_INVOCATIONS, 15)))
            return SPRET_ABORT;
        break;

    case ABIL_LUGONU_ABYSS_ENTER:
    {
        fail_check();
        // Deflate HP.
        dec_hp(random2avg(you.hp, 2), false);

        // Deflate MP.
        if (you.magic_points)
            dec_mp(random2avg(you.magic_points, 2));

        bool note_status = notes_are_active();
        activate_notes(false);  // This banishment shouldn't be noted.
        banished();
        activate_notes(note_status);
        break;
    }

    case ABIL_NEMELEX_TRIPLE_DRAW:
        fail_check();
        if (!deck_triple_draw())
            return SPRET_ABORT;
        break;

    case ABIL_NEMELEX_DEAL_FOUR:
        fail_check();
        if (!deck_deal())
            return SPRET_ABORT;
        break;

    case ABIL_NEMELEX_STACK_FIVE:
        fail_check();
        if (!deck_stack())
            return SPRET_ABORT;
        break;

    case ABIL_BEOGH_SMITING:
        fail_check();
        if (your_spells(SPELL_SMITING, 12 + skill_bump(SK_INVOCATIONS, 6),
                        false) == SPRET_ABORT)
        {
            return SPRET_ABORT;
        }
        break;

    case ABIL_BEOGH_GIFT_ITEM:
        if (!beogh_gift_item())
            return SPRET_ABORT;
        break;

    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        fail_check();
        start_recall(2);
        break;

    case ABIL_STOP_RECALL:
        fail_check();
        mpr("You stop recalling your allies.");
        end_recall();
        break;

    case ABIL_FEDHAS_SUNLIGHT:
        return fedhas_sunlight(fail);

    case ABIL_FEDHAS_PLANT_RING:
        fail_check();
        if (!fedhas_plant_ring_from_fruit())
            return SPRET_ABORT;
        break;

    case ABIL_FEDHAS_RAIN:
        fail_check();
        if (!fedhas_rain(you.pos()))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return SPRET_ABORT;
        }
        break;

    case ABIL_FEDHAS_SPAWN_SPORES:
        fail_check();
        ASSERT(fedhas_corpse_spores() > 0);
        break;

    case ABIL_FEDHAS_EVOLUTION:
        fail_check();
        fedhas_evolve_flora();
        break;

    case ABIL_TRAN_BAT:
        fail_check();
        if (!transform(100, TRAN_BAT))
        {
            crawl_state.zero_turns_taken();
            return SPRET_ABORT;
        }
        break;

    case ABIL_JIYVA_CALL_JELLY:
    {
        fail_check();
        mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0, you.pos(),
                     MHITNOT, 0, GOD_JIYVA);

        mg.non_actor_summoner = "Jiyva";

        if (!create_monster(mg))
            return SPRET_ABORT;
        break;
    }

    case ABIL_JIYVA_JELLY_PARALYSE:
        fail_check();
        jiyva_paralyse_jellies();
        break;

    case ABIL_JIYVA_SLIMIFY:
    {
        fail_check();
        const item_def* const weapon = you.weapon();
        const string msg = (weapon) ? weapon->name(DESC_YOUR)
                                    : ("your " + you.hand_name(true));
        mprf(MSGCH_DURATION, "A thick mucus forms on %s.", msg.c_str());
        you.increase_duration(DUR_SLIMIFY,
                              random2avg(you.piety / 4, 2) + 3, 100);
        break;
    }

    case ABIL_JIYVA_CURE_BAD_MUTATION:
        fail_check();
        jiyva_remove_bad_mutation();
        break;

    case ABIL_CHEIBRIADOS_TIME_STEP:
        fail_check();
        cheibriados_time_step(you.skill(SK_INVOCATIONS, 10) * you.piety / 100);
        break;

    case ABIL_CHEIBRIADOS_TIME_BEND:
        fail_check();
        cheibriados_time_bend(16 + you.skill(SK_INVOCATIONS, 8));
        break;

    case ABIL_CHEIBRIADOS_DISTORTION:
        fail_check();
        cheibriados_temporal_distortion();
        break;

    case ABIL_CHEIBRIADOS_SLOUCH:
        fail_check();
        if (!cheibriados_slouch(0))
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        break;

    case ABIL_ASHENZARI_SCRYING:
        fail_check();
        if (you.duration[DUR_SCRYING])
            mpr("You extend your astral sight.");
        else
            mpr("You gain astral sight.");
        you.duration[DUR_SCRYING] = 100 + random2avg(you.piety * 2, 2);
        you.xray_vision = true;
        break;

    case ABIL_ASHENZARI_TRANSFER_KNOWLEDGE:
        fail_check();
        if (!ashenzari_transfer_knowledge())
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        break;

    case ABIL_ASHENZARI_END_TRANSFER:
        fail_check();
        ashenzari_end_transfer();
        break;

    case ABIL_DITHMENOS_SHADOW_STEP:
        fail_check();
        if (!dithmenos_shadow_step())
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        break;

    case ABIL_DITHMENOS_SHADOW_FORM:
        fail_check();
        if (!transform(you.skill(SK_INVOCATIONS, 2), TRAN_SHADOW))
        {
            crawl_state.zero_turns_taken();
            return SPRET_ABORT;
        }
        break;

    case ABIL_GOZAG_POTION_PETITION:
        fail_check();
        run_uncancel(UNC_POTION_PETITION, 0);
        break;

    case ABIL_GOZAG_CALL_MERCHANT:
        fail_check();
        run_uncancel(UNC_CALL_MERCHANT, 0);
        break;

    case ABIL_GOZAG_BRIBE_BRANCH:
        fail_check();
        if (!gozag_bribe_branch())
            return SPRET_ABORT;
        break;

    case ABIL_QAZLAL_UPHEAVAL:
        return qazlal_upheaval(coord_def(), false, fail);

    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        fail_check();
        qazlal_elemental_force();
        break;

    case ABIL_QAZLAL_DISASTER_AREA:
        fail_check();
        if (!qazlal_disaster_area())
            return SPRET_ABORT;
        break;

    case ABIL_RU_SACRIFICE_PURITY:
    case ABIL_RU_SACRIFICE_WORDS:
    case ABIL_RU_SACRIFICE_DRINK:
    case ABIL_RU_SACRIFICE_ESSENCE:
    case ABIL_RU_SACRIFICE_HEALTH:
    case ABIL_RU_SACRIFICE_STEALTH:
    case ABIL_RU_SACRIFICE_ARTIFICE:
    case ABIL_RU_SACRIFICE_LOVE:
    case ABIL_RU_SACRIFICE_COURAGE:
    case ABIL_RU_SACRIFICE_ARCANA:
    case ABIL_RU_SACRIFICE_NIMBLENESS:
    case ABIL_RU_SACRIFICE_DURABILITY:
    case ABIL_RU_SACRIFICE_HAND:
        fail_check();
        if (!ru_do_sacrifice(abil.ability))
            return SPRET_ABORT;
        break;

    case ABIL_RU_REJECT_SACRIFICES:
        fail_check();
        if (!ru_reject_sacrifices())
            return SPRET_ABORT;
        break;

    case ABIL_RU_DRAW_OUT_POWER:
        fail_check();
        if (you.duration[DUR_EXHAUSTED])
        {
            mpr("You're too exhausted to draw out your power.");
            return SPRET_ABORT;
        }
        if (you.hp == you.hp_max && you.magic_points == you.max_magic_points
            && !you.duration[DUR_CONF]
            && !you.duration[DUR_SLOW]
            && !you.attribute[ATTR_HELD]
            && !you.petrifying()
            && !you.is_constricted())
        {
            mpr("You have no need to draw out power.");
            return SPRET_ABORT;
        }
        ru_draw_out_power();
        you.increase_duration(DUR_EXHAUSTED, 12 + random2(5));
        break;

    case ABIL_RU_POWER_LEAP:
        fail_check();
        if (you.duration[DUR_EXHAUSTED])
        {
            mpr("You're too exhausted to power leap.");
            return SPRET_ABORT;
        }
        if (!ru_power_leap())
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        you.increase_duration(DUR_EXHAUSTED, 18 + random2(8));
        break;

    case ABIL_RU_APOCALYPSE:
        fail_check();
        if (you.duration[DUR_EXHAUSTED])
        {
            mpr("You're too exhausted to unleash your apocalyptic power.");
            return SPRET_ABORT;
        }
        if (!ru_apocalypse())
            return SPRET_ABORT;
        you.increase_duration(DUR_EXHAUSTED, 30 + random2(20));
        break;

    case ABIL_RENOUNCE_RELIGION:
        fail_check();
        if (yesno("Really renounce your faith, foregoing its fabulous benefits?",
                  false, 'n')
            && yesno("Are you sure you won't change your mind later?",
                     false, 'n'))
        {
            excommunication();
        }
        else
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        break;

    case ABIL_CONVERT_TO_BEOGH:
        fail_check();
        god_pitch(GOD_BEOGH);
        if (you_worship(GOD_BEOGH))
        {
            spare_beogh_convert();
            break;
        }
        return SPRET_ABORT;

    case ABIL_NON_ABILITY:
        fail_check();
        mpr("Sorry, you can't do that.");
        break;

    default:
        die("invalid ability");
    }

    return SPRET_SUCCESS;
}

// [ds] Increase piety cost for god abilities that are particularly
// overpowered in Sprint. Yes, this is a hack. No, I don't care.
static int _scale_piety_cost(ability_type abil, int original_cost)
{
    // Abilities that have aroused our ire earn 2.5x their classic
    // Crawl piety cost.
    return (crawl_state.game_is_sprint()
            && (abil == ABIL_TROG_BROTHERS_IN_ARMS
                || abil == ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB))
           ? div_rand_round(original_cost * 5, 2)
           : original_cost;
}

// We pass in ability ZP cost as it may have changed during the exercise
// of the ability (if the cost is scaled, for example)
static void _pay_ability_costs(const ability_def& abil, int zpcost)
{
    if (abil.flags & ABFLAG_INSTANT)
    {
        you.turn_is_over = false;
        you.elapsed_time_at_last_input = you.elapsed_time;
        update_turn_count();
    }
    else
        you.turn_is_over = true;

    const int food_cost  = abil.food_cost + random2avg(abil.food_cost, 2);
    const int piety_cost =
        _scale_piety_cost(abil.ability, abil.piety_cost.cost());
    const int hp_cost    = abil.hp_cost.cost(you.hp_max);

    dprf("Cost: mp=%d; hp=%d; food=%d; piety=%d",
         abil.mp_cost, hp_cost, food_cost, piety_cost);

    if (abil.mp_cost)
    {
        dec_mp(abil.mp_cost);
        if (abil.flags & ABFLAG_PERMANENT_MP)
            rot_mp(1);
    }

    if (abil.hp_cost)
    {
        dec_hp(hp_cost, false);
        if (abil.flags & ABFLAG_PERMANENT_HP)
            rot_hp(hp_cost);
    }

    if (zpcost)
    {
        you.zot_points -= zpcost;
        you.redraw_experience = true;
    }

    if (abil.flags & ABFLAG_HEX_MISCAST)
    {
        MiscastEffect(&you, NON_MONSTER, SPTYP_HEXES, 10, 90,
                      "power out of control", NH_DEFAULT);
    }
    if (abil.flags & ABFLAG_NECRO_MISCAST)
    {
        MiscastEffect(&you, NON_MONSTER, SPTYP_NECROMANCY, 10, 90,
                      "power out of control");
    }
    if (abil.flags & ABFLAG_NECRO_MISCAST_MINOR)
    {
        MiscastEffect(&you, NON_MONSTER, SPTYP_NECROMANCY, 5, 90,
                      "power out of control");
    }
    if (abil.flags & ABFLAG_TLOC_MISCAST)
    {
        MiscastEffect(&you, NON_MONSTER, SPTYP_TRANSLOCATION, 10, 90,
                      "power out of control");
    }
    if (abil.flags & ABFLAG_TRMT_MISCAST)
    {
        MiscastEffect(&you, NON_MONSTER, SPTYP_TRANSMUTATION, 10, 90,
                      "power out of control");
    }
    if (abil.flags & ABFLAG_LEVEL_DRAIN)
        adjust_level(-1);

    if (food_cost)
        make_hungry(food_cost, false, true);

    if (piety_cost)
        lose_piety(piety_cost);
}

int choose_ability_menu(const vector<talent>& talents)
{
#ifdef USE_TILE_LOCAL
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    ToggleableMenu abil_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                             | MF_TOGGLE_ACTION | MF_ALWAYS_SHOW_MORE,
                             text_only);

    abil_menu.set_highlighter(NULL);
#ifdef USE_TILE_LOCAL
    {
        // Hack like the one in spl-cast.cc:list_spells() to align the title.
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry("  Ability - do what?                 "
                                    "Cost                          Failure",
                                    "  Ability - describe what?           "
                                    "Cost                          Failure",
                                    MEL_ITEM);
        me->colour = BLUE;
        abil_menu.add_entry(me);
    }
#else
    abil_menu.set_title(
        new ToggleableMenuEntry("  Ability - do what?                 "
                                "Cost                          Failure",
                                "  Ability - describe what?           "
                                "Cost                          Failure",
                                MEL_TITLE));
#endif
    abil_menu.set_tag("ability");
    abil_menu.add_toggle_key('!');
    abil_menu.add_toggle_key('?');
    abil_menu.menu_action = Menu::ACT_EXECUTE;

    if (crawl_state.game_is_hints())
    {
        // XXX: This could be buggy if you manage to pick up lots and
        // lots of abilities during hints mode.
        abil_menu.set_more(hints_abilities_info());
    }
    else
    {
        abil_menu.set_more(formatted_string::parse_string(
                           "Press '<w>!</w>' or '<w>?</w>' to toggle "
                           "between ability selection and description."));
    }

    int numbers[52];
    for (int i = 0; i < 52; ++i)
        numbers[i] = i;

    bool found_invocations = false;
    bool found_zotdef = false;

    // First add all non-invocation, non-zotdef abilities.
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        if (talents[i].is_invocation)
            found_invocations = true;
        else if (talents[i].is_zotdef)
            found_zotdef = true;
        else
        {
            ToggleableMenuEntry* me =
                new ToggleableMenuEntry(describe_talent(talents[i]),
                                        describe_talent(talents[i]),
                                        MEL_ITEM, 1, talents[i].hotkey);
            me->data = &numbers[i];
#ifdef USE_TILE
            me->add_tile(tile_def(tileidx_ability(talents[i].which), TEX_GUI));
#endif
            // Only check this here, since your god can't hate its own abilities
            if (god_hates_ability(talents[i].which, you.religion))
                me->colour = COL_FORBIDDEN;
            abil_menu.add_entry(me);
        }
    }

    if (found_zotdef)
    {
#ifdef USE_TILE_LOCAL
        ToggleableMenuEntry* subtitle =
            new ToggleableMenuEntry("    Zot Defence -",
                                    "    Zot Defence -", MEL_ITEM);
        subtitle->colour = BLUE;
        abil_menu.add_entry(subtitle);
#else
        abil_menu.add_entry(
            new ToggleableMenuEntry("    Zot Defence - ",
                                    "    Zot Defence - ", MEL_SUBTITLE));
#endif
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (talents[i].is_zotdef)
            {
                ToggleableMenuEntry* me =
                    new ToggleableMenuEntry(describe_talent(talents[i]),
                                            describe_talent(talents[i]),
                                            MEL_ITEM, 1, talents[i].hotkey);
                me->data = &numbers[i];
#ifdef USE_TILE
                me->add_tile(tile_def(tileidx_ability(talents[i].which),
                                      TEX_GUI));
#endif
                abil_menu.add_entry(me);
            }
        }
    }

    if (found_invocations)
    {
#ifdef USE_TILE_LOCAL
        ToggleableMenuEntry* subtitle =
            new ToggleableMenuEntry("    Invocations - ",
                                    "    Invocations - ", MEL_ITEM);
        subtitle->colour = BLUE;
        abil_menu.add_entry(subtitle);
#else
        abil_menu.add_entry(
            new ToggleableMenuEntry("    Invocations - ",
                                    "    Invocations - ", MEL_SUBTITLE));
#endif
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (talents[i].is_invocation)
            {
                ToggleableMenuEntry* me =
                    new ToggleableMenuEntry(describe_talent(talents[i]),
                                            describe_talent(talents[i]),
                                            MEL_ITEM, 1, talents[i].hotkey);
                me->data = &numbers[i];
#ifdef USE_TILE
                me->add_tile(tile_def(tileidx_ability(talents[i].which),
                                      TEX_GUI));
#endif
                abil_menu.add_entry(me);
            }
        }
    }

    while (true)
    {
        vector<MenuEntry*> sel = abil_menu.show(false);
        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();
        if (sel.empty())
            return -1;

        ASSERT(sel.size() == 1);
        ASSERT(sel[0]->hotkeys.size() == 1);
        int selected = *(static_cast<int*>(sel[0]->data));

        if (abil_menu.menu_action == Menu::ACT_EXAMINE)
            _print_talent_description(talents[selected]);
        else
            return *(static_cast<int*>(sel[0]->data));
    }
}

string describe_talent(const talent& tal)
{
    ASSERT(tal.which != ABIL_NON_ABILITY);

    char* failure = failure_rate_to_string(tal.fail);

    ostringstream desc;
    desc << left
         << chop_string(ability_name(tal.which), 32)
         << chop_string(make_cost_description(tal.which), 30)
         << chop_string(failure, 7);
    free(failure);
    return desc.str();
}

static void _add_talent(vector<talent>& vec, const ability_type ability,
                        bool check_confused)
{
    const talent t = get_talent(ability, check_confused);
    if (t.which != ABIL_NON_ABILITY)
        vec.push_back(t);
}

/**
 * Return all relevant talents that the player has.
 *
 * Currently the only abilities that are affected by include_unusable are god
 * abilities (affect by e.g. penance or silence).
 * @param check_confused If true, abilities that don't work when confused will
 *                       be excluded.
 * @param include_unusable If true, abilities that are currently unusable will
 *                         be excluded.
 * @return  A vector of talent structs.
 */
vector<talent> your_talents(bool check_confused, bool include_unusable)
{
    vector<talent> talents;

    // zot defence abilities; must also be updated in player.cc when these levels are changed
    if (crawl_state.game_is_zotdef())
    {
        if (you.experience_level >= 2)
            _add_talent(talents, ABIL_MAKE_OKLOB_SAPLING, check_confused);
        if (you.experience_level >= 3)
            _add_talent(talents, ABIL_MAKE_ARROW_TRAP, check_confused);
        if (you.experience_level >= 4)
            _add_talent(talents, ABIL_MAKE_PLANT, check_confused);
        if (you.experience_level >= 4)
            _add_talent(talents, ABIL_REMOVE_CURSE, check_confused);
        if (you.experience_level >= 5)
            _add_talent(talents, ABIL_MAKE_BURNING_BUSH, check_confused);
        if (you.experience_level >= 6)
            _add_talent(talents, ABIL_MAKE_ALTAR, check_confused);
        if (you.experience_level >= 6)
            _add_talent(talents, ABIL_MAKE_GRENADES, check_confused);
        if (you.experience_level >= 7)
            _add_talent(talents, ABIL_MAKE_OKLOB_PLANT, check_confused);
        if (you.experience_level >= 8)
            _add_talent(talents, ABIL_MAKE_NET_TRAP, check_confused);
        if (you.experience_level >= 9)
            _add_talent(talents, ABIL_MAKE_ICE_STATUE, check_confused);
        if (you.experience_level >= 10)
            _add_talent(talents, ABIL_MAKE_SPEAR_TRAP, check_confused);
        if (you.experience_level >= 11)
            _add_talent(talents, ABIL_MAKE_ALARM_TRAP, check_confused);
        if (you.experience_level >= 12)
            _add_talent(talents, ABIL_MAKE_FUNGUS, check_confused);
        if (you.experience_level >= 13)
            _add_talent(talents, ABIL_MAKE_BOLT_TRAP, check_confused);
        if (you.experience_level >= 14)
            _add_talent(talents, ABIL_MAKE_OCS, check_confused);
        if (you.experience_level >= 15)
            _add_talent(talents, ABIL_MAKE_NEEDLE_TRAP, check_confused);
        if (you.experience_level >= 16 && !player_has_orb())
            _add_talent(talents, ABIL_MAKE_TELEPORT, check_confused);
        if (you.experience_level >= 17)
            _add_talent(talents, ABIL_MAKE_WATER, check_confused);
        if (you.experience_level >= 19)
            _add_talent(talents, ABIL_MAKE_LIGHTNING_SPIRE, check_confused);
        if (you.experience_level >= 20)
            _add_talent(talents, ABIL_MAKE_OBSIDIAN_STATUE, check_confused);
        // gain bazaar and gold together
        if (you.experience_level >= 21)
            _add_talent(talents, ABIL_MAKE_BAZAAR, check_confused);
        if (you.experience_level >= 21)
            _add_talent(talents, ABIL_MAKE_ACQUIRE_GOLD, check_confused);
        if (you.experience_level >= 22)
            _add_talent(talents, ABIL_MAKE_OKLOB_CIRCLE, check_confused);
        if (you.experience_level >= 24)
            _add_talent(talents, ABIL_MAKE_ACQUIREMENT, check_confused);
        if (you.experience_level >= 25)
            _add_talent(talents, ABIL_MAKE_BLADE_TRAP, check_confused);
        if (you.experience_level >= 26)
            _add_talent(talents, ABIL_MAKE_CURSE_SKULL, check_confused);
        // 27 was: Make teleport trap
    }

    // Species-based abilities.
    if (you.species == SP_MUMMY && you.experience_level >= 13)
        _add_talent(talents, ABIL_MUMMY_RESTORATION, check_confused);

    if (you.species == SP_DEEP_DWARF)
        _add_talent(talents, ABIL_RECHARGING, check_confused);

    if (you.species == SP_FORMICID
        && (you.form != TRAN_TREE || include_unusable))
    {
        _add_talent(talents, ABIL_DIG, check_confused);
        if ((!crawl_state.game_is_sprint() || brdepth[you.where_are_you] > 1)
            && !crawl_state.game_is_zotdef())
        {
            _add_talent(talents, ABIL_SHAFT_SELF, check_confused);
        }
    }

    // Spit Poison. Nagas can upgrade to Breathe Poison.
    if (you.species == SP_NAGA)
    {
        _add_talent(talents, player_mutation_level(MUT_BREATHE_POISON) ?
                    ABIL_BREATHE_POISON : ABIL_SPIT_POISON, check_confused);
    }
    else if (player_mutation_level(MUT_SPIT_POISON))
        _add_talent(talents, ABIL_SPIT_POISON, check_confused);

    if (player_genus(GENPC_DRACONIAN))
    {
        ability_type ability = ABIL_NON_ABILITY;
        switch (you.species)
        {
        case SP_GREEN_DRACONIAN:   ability = ABIL_BREATHE_MEPHITIC;     break;
        case SP_RED_DRACONIAN:     ability = ABIL_BREATHE_FIRE;         break;
        case SP_WHITE_DRACONIAN:   ability = ABIL_BREATHE_FROST;        break;
        case SP_YELLOW_DRACONIAN:  ability = ABIL_SPIT_ACID;            break;
        case SP_BLACK_DRACONIAN:   ability = ABIL_BREATHE_LIGHTNING;    break;
        case SP_PURPLE_DRACONIAN:  ability = ABIL_BREATHE_POWER;        break;
        case SP_PALE_DRACONIAN:    ability = ABIL_BREATHE_STEAM;        break;
        case SP_MOTTLED_DRACONIAN: ability = ABIL_BREATHE_STICKY_FLAME; break;
        default: break;
        }

        // Draconians don't maintain their original breath weapons
        // if shapechanged into a non-dragon form.
        if (form_changed_physiology() && you.form != TRAN_DRAGON)
            ability = ABIL_NON_ABILITY;

        if (ability != ABIL_NON_ABILITY)
            _add_talent(talents, ability, check_confused);
    }

    if (you.species == SP_VAMPIRE && you.experience_level >= 3
        && you.hunger_state <= HS_SATIATED
        && you.form != TRAN_BAT)
    {
        _add_talent(talents, ABIL_TRAN_BAT, check_confused);
    }

    if ((you.species == SP_TENGU && you.experience_level >= 5
         || player_mutation_level(MUT_BIG_WINGS)) && !you.airborne()
        || you.racial_permanent_flight() && !you.attribute[ATTR_PERM_FLIGHT]
#if TAG_MAJOR_VERSION == 34
           && you.species != SP_DJINNI
#endif
           )
    {
        // Tengu can fly, but only from the ground
        // (until level 15, when it becomes permanent until revoked).
        // Black draconians and gargoyles get permaflight at XL 14, but they
        // don't get the tengu movement/evasion bonuses and they don't get
        // temporary flight before then.
        // Other dracs can mutate big wings whenever for temporary flight.
        _add_talent(talents, ABIL_FLY, check_confused);
    }

    if (you.attribute[ATTR_PERM_FLIGHT] && you.racial_permanent_flight())
        _add_talent(talents, ABIL_STOP_FLYING, check_confused);

    // Mutations
    if (player_mutation_level(MUT_HURL_HELLFIRE))
        _add_talent(talents, ABIL_HELLFIRE, check_confused);

    if (you.duration[DUR_TRANSFORMATION] && !you.transform_uncancellable)
        _add_talent(talents, ABIL_END_TRANSFORMATION, check_confused);

    if (player_mutation_level(MUT_BLINK))
        _add_talent(talents, ABIL_BLINK, check_confused);

    // Religious abilities.
    vector<ability_type> abilities = get_god_abilities(include_unusable);
    for (unsigned int i = 0; i < abilities.size(); ++i)
        _add_talent(talents, abilities[i], check_confused);

    // And finally, the ability to opt-out of your faith {dlb}:
    if (!you_worship(GOD_NO_GOD))
        _add_talent(talents, ABIL_RENOUNCE_RELIGION, check_confused);

    if (env.level_state & LSTATE_BEOGH && can_convert_to_beogh())
        _add_talent(talents, ABIL_CONVERT_TO_BEOGH, check_confused);

    //jmf: Check for breath weapons - they're exclusive of each other, I hope!
    //     Make better ones come first.
    if (you.species != SP_RED_DRACONIAN &&
        ((you.form == TRAN_DRAGON
         && dragon_form_dragon_type() == MONS_FIRE_DRAGON)
         || player_mutation_level(MUT_BREATHE_FLAMES)))
    {
        _add_talent(talents, ABIL_BREATHE_FIRE, check_confused);
    }

    // Checking for unreleased Delayed Fireball.
    if (you.attribute[ ATTR_DELAYED_FIREBALL ])
        _add_talent(talents, ABIL_DELAYED_FIREBALL, check_confused);

    if (you.duration[DUR_SONG_OF_SLAYING])
        _add_talent(talents, ABIL_STOP_SINGING, check_confused);

    // Evocations from items.
    if (you.scan_artefacts(ARTP_BLINK)
        && !player_mutation_level(MUT_NO_ARTIFICE))
    {
        _add_talent(talents, ABIL_EVOKE_BLINK, check_confused);
    }

    if (you.scan_artefacts(ARTP_FOG)
        && !player_mutation_level(MUT_NO_ARTIFICE))
    {
        _add_talent(talents, ABIL_EVOKE_FOG, check_confused);
    }

    if (you.evokable_berserk() && !player_mutation_level(MUT_NO_ARTIFICE))
        _add_talent(talents, ABIL_EVOKE_BERSERK, check_confused);

    if (you.evokable_invis() && !you.attribute[ATTR_INVIS_UNCANCELLABLE]
        && !player_mutation_level(MUT_NO_ARTIFICE))
    {
        // Now you can only turn invisibility off if you have an
        // activatable item.  Wands and potions will have to time
        // out. -- bwr
        if (you.duration[DUR_INVIS])
            _add_talent(talents, ABIL_EVOKE_TURN_VISIBLE, check_confused);
        else
            _add_talent(talents, ABIL_EVOKE_TURN_INVISIBLE, check_confused);
    }

    if (you.evokable_flight() && !player_mutation_level(MUT_NO_ARTIFICE))
    {
        // Has no effect on permanently flying Tengu.
        if (!you.permanent_flight() || !you.racial_permanent_flight())
        {
            // You can still evoke perm flight if you have temporary one.
            if (!you.flight_mode()
                || !you.attribute[ATTR_PERM_FLIGHT]
                   && you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING))
            {
                _add_talent(talents, ABIL_EVOKE_FLIGHT, check_confused);
            }
            // Now you can only turn flight off if you have an
            // activatable item.  Potions and spells will have to time
            // out.
            if (you.flight_mode() && !you.attribute[ATTR_FLIGHT_UNCANCELLABLE])
                _add_talent(talents, ABIL_STOP_FLYING, check_confused);
        }
    }

    if (you.wearing(EQ_RINGS, RING_TELEPORTATION)
        && !player_mutation_level(MUT_NO_ARTIFICE)
        && !crawl_state.game_is_sprint())
    {
        _add_talent(talents, ABIL_EVOKE_TELEPORTATION, check_confused);
    }

    if (you.wearing(EQ_RINGS, RING_TELEPORT_CONTROL)
        && !player_mutation_level(MUT_NO_ARTIFICE))
    {
        _add_talent(talents, ABIL_EVOKE_TELEPORT_CONTROL, check_confused);
    }

    // Find hotkeys for the non-hotkeyed talents.
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        // Skip preassigned hotkeys.
        if (talents[i].hotkey != 0)
            continue;

        // Try to find a free hotkey for i, starting from Z.
        for (int k = 51; k >= 0; ++k)
        {
            const int kkey = index_to_letter(k);
            bool good_key = true;

            // Check that it doesn't conflict with other hotkeys.
            for (unsigned int j = 0; j < talents.size(); ++j)
            {
                if (talents[j].hotkey == kkey)
                {
                    good_key = false;
                    break;
                }
            }

            if (good_key)
            {
                talents[i].hotkey = k;
                you.ability_letter_table[k] = talents[i].which;
                break;
            }
        }
        // In theory, we could be left with an unreachable ability
        // here (if you have 53 or more abilities simultaneously).
    }

    return talents;
}

// Note: we're trying for a behaviour where the player gets
// to keep their assigned invocation slots if they get excommunicated
// and then rejoin (but if they spend time with another god we consider
// the old invocation slots void and erase them).  We also try to
// protect any bindings the character might have made into the
// traditional invocation slots (A-E and X). -- bwr
static void _set_god_ability_helper(ability_type abil, char letter)
{
    int i;
    const int index = letter_to_index(letter);

    for (i = 0; i < 52; i++)
        if (you.ability_letter_table[i] == abil)
            break;

    if (i == 52)    // Ability is not already assigned.
    {
        // If slot is unoccupied, move in.
        if (you.ability_letter_table[index] == ABIL_NON_ABILITY)
            you.ability_letter_table[index] = abil;
    }
}

// Return GOD_NO_GOD if it isn't a god ability, otherwise return
// the index of the god.
static int _is_god_ability(ability_type abil)
{
    if (abil == ABIL_NON_ABILITY)
        return GOD_NO_GOD;

    // Not in god_abilities because players get them at 0*
    // TODO: Fix that and remove the following.
    if (abil == ABIL_CHEIBRIADOS_TIME_BEND)
        return GOD_CHEIBRIADOS;
    if (abil == ABIL_ELYVILON_LESSER_HEALING_OTHERS)
        return GOD_ELYVILON;
    if (abil == ABIL_TROG_BURN_SPELLBOOKS)
        return GOD_TROG;

    for (int i = 0; i < NUM_GODS; ++i)
        for (int j = 0; j < MAX_GOD_ABILITIES; ++j)
        {
            if (god_abilities[i][j] == abil)
                return i;
        }

    return GOD_NO_GOD;
}

void set_god_ability_slots()
{
    ASSERT(!you_worship(GOD_NO_GOD));

    _set_god_ability_helper(ABIL_RENOUNCE_RELIGION, 'X');

    // Clear out other god invocations.
    for (int i = 0; i < 52; i++)
    {
        const int god = _is_god_ability(you.ability_letter_table[i]);
        if (god != GOD_NO_GOD && god != you.religion)
            you.ability_letter_table[i] = ABIL_NON_ABILITY;
    }

    // Finally, add in current god's invocations in traditional slots.
    int num = 0;
    if (you_worship(GOD_ELYVILON))
    {
        _set_god_ability_helper(ABIL_ELYVILON_LESSER_HEALING_OTHERS,
                                'a' + num++);
    }
    if (you_worship(GOD_CHEIBRIADOS))
    {
        _set_god_ability_helper(ABIL_CHEIBRIADOS_TIME_BEND,
                                'a' + num++);
    }

    for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (god_abilities[you.religion][i] != ABIL_NON_ABILITY)
        {
            _set_god_ability_helper(god_abilities[you.religion][i],
                                    'a' + num++);

            if (you_worship(GOD_ELYVILON))
            {
                if (god_abilities[you.religion][i]
                        == ABIL_ELYVILON_LESSER_HEALING_SELF)
                {
                    _set_god_ability_helper(ABIL_ELYVILON_LIFESAVING, 'p');
                }
                else if (god_abilities[you.religion][i]
                            == ABIL_ELYVILON_GREATER_HEALING_OTHERS)
                {
                    _set_god_ability_helper(ABIL_ELYVILON_GREATER_HEALING_SELF,
                                            'a' + num++);
                }
            }
            else if (you_worship(GOD_YREDELEMNUL))
            {
                if (god_abilities[you.religion][i]
                        == ABIL_YRED_RECALL_UNDEAD_SLAVES)
                {
                    _set_god_ability_helper(ABIL_YRED_INJURY_MIRROR,
                                            'a' + num++);
                }
            }
        }
    }
}

// Returns an index (0-51) if successful, -1 if you should
// just use the next one.
static int _find_ability_slot(const ability_def &abil)
{
    for (int slot = 0; slot < 52; slot++)
        // Placeholder handling, part 2: The ability we have might
        // correspond to a placeholder, in which case the ability letter
        // table will contain that placeholder.  Convert the latter to
        // its corresponding ability before comparing the two, so that
        // we'll find the placeholder's index properly.
        if (_fixup_ability(you.ability_letter_table[slot]) == abil.ability)
            return slot;

    // No requested slot, find new one and make it preferred.

    // Skip over a-e (invocations), a-g for Elyvilon, a-E for ZotDef
    int first_slot = letter_to_index('f');
    if (you_worship(GOD_ELYVILON))
        first_slot = letter_to_index('h');
    if (abil.flags & ABFLAG_ZOTDEF)
        first_slot = letter_to_index('F'); // for *some* memory compat.

    if (abil.ability == ABIL_ZIN_CURE_ALL_MUTATIONS)
        first_slot = letter_to_index('W');
    if (abil.ability == ABIL_CONVERT_TO_BEOGH)
        first_slot = letter_to_index('Y');
    if (abil.ability == ABIL_RU_SACRIFICE_PURITY
      || abil.ability == ABIL_RU_SACRIFICE_WORDS
      || abil.ability == ABIL_RU_SACRIFICE_DRINK
      || abil.ability == ABIL_RU_SACRIFICE_ESSENCE
      || abil.ability == ABIL_RU_SACRIFICE_HEALTH
      || abil.ability == ABIL_RU_SACRIFICE_STEALTH
      || abil.ability == ABIL_RU_SACRIFICE_ARTIFICE
      || abil.ability == ABIL_RU_SACRIFICE_LOVE
      || abil.ability == ABIL_RU_SACRIFICE_COURAGE
      || abil.ability == ABIL_RU_SACRIFICE_ARCANA
      || abil.ability == ABIL_RU_SACRIFICE_NIMBLENESS
      || abil.ability == ABIL_RU_SACRIFICE_DURABILITY
      || abil.ability == ABIL_RU_SACRIFICE_HAND
      || abil.ability == ABIL_RU_REJECT_SACRIFICES)
    {
        first_slot = letter_to_index('P');
    }


    for (int slot = first_slot; slot < 52; ++slot)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = abil.ability;
            return slot;
        }
    }

    // If we can't find anything else, try a-e.
    for (int slot = first_slot - 1; slot >= 0; --slot)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = abil.ability;
            return slot;
        }
    }

    // All letters are assigned.
    return -1;
}

vector<ability_type> get_god_abilities(bool include_unusable, bool ignore_piety)
{
    vector<ability_type> abilities;
    if (you_worship(GOD_TROG) && (include_unusable || !silenced(you.pos())))
        abilities.push_back(ABIL_TROG_BURN_SPELLBOOKS);
    else if (you_worship(GOD_ELYVILON) && (include_unusable || !silenced(you.pos())))
        abilities.push_back(ABIL_ELYVILON_LESSER_HEALING_OTHERS);
    else if (you_worship(GOD_CHEIBRIADOS) && (include_unusable
                                              || !(silenced(you.pos())
                                                   || player_under_penance())))
    {
        abilities.push_back(ABIL_CHEIBRIADOS_TIME_BEND);
    }
    else if (you_worship(GOD_RU))
    {

        ASSERT(you.props.exists("available_sacrifices"));
        CrawlVector &available_sacrifices
            = you.props["available_sacrifices"].get_vector();

        int num_sacrifices = available_sacrifices.size();
        for (int i = 0; i < num_sacrifices; ++i)
        {
            abilities.push_back(
                static_cast<ability_type>(available_sacrifices[i].get_int()));
        }
        if (num_sacrifices > 0)
            abilities.push_back(ABIL_RU_REJECT_SACRIFICES);
    }
    else if (you.transfer_skill_points > 0)
        abilities.push_back(ABIL_ASHENZARI_END_TRANSFER);

    // Remaining abilities are unusable if under penance, or if silenced if not
    // Nemelex abilities.
    if (!include_unusable && (player_under_penance()
                              || silenced(you.pos())
                              && !you_worship(GOD_NEMELEX_XOBEH)
                              && !you_worship(GOD_RU)))
    {
        return abilities;
    }

    for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (!you_worship(GOD_GOZAG) && you.piety < piety_breakpoint(i)
            && !ignore_piety)
        {
            continue;
        }

        const ability_type abil =
            _fixup_ability(god_abilities[you.religion][i]);
        if (abil == ABIL_NON_ABILITY
            || brdepth[BRANCH_ABYSS] == -1
               && (abil == ABIL_LUGONU_ABYSS_EXIT
                   || abil == ABIL_LUGONU_ABYSS_ENTER))
        {
            continue;
        }

        abilities.push_back(abil);
        if (abil == ABIL_ELYVILON_LESSER_HEALING_SELF)
            abilities.push_back(ABIL_ELYVILON_LIFESAVING);
        else if (abil == ABIL_ELYVILON_GREATER_HEALING_OTHERS)
            abilities.push_back(ABIL_ELYVILON_GREATER_HEALING_SELF);
        else if (abil == ABIL_YRED_RECALL_UNDEAD_SLAVES
                 || abil == ABIL_STOP_RECALL && you_worship(GOD_YREDELEMNUL))
        {
            abilities.push_back(ABIL_YRED_INJURY_MIRROR);
        }
    }

    if (can_do_capstone_ability(GOD_ZIN))
        abilities.push_back(ABIL_ZIN_CURE_ALL_MUTATIONS);

    return abilities;
}

////////////////////////////////////////////////////////////////////////
// generic_cost

int generic_cost::cost() const
{
    return base + (add > 0 ? random2avg(add, rolls) : 0);
}

int scaling_cost::cost(int max) const
{
    return (value < 0) ? (-value) : ((value * max + 500) / 1000);
}

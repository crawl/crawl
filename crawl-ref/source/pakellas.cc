/**
 * @file
 * @brief Pakellas new rod source file
**/

#include "AppHdr.h"
#include "pakellas.h"

#include <string>

#include "env.h"
#include "actor.h"
#include "cloud.h"
#include "coord.h"
#include "debug.h"
#include "defines.h"
#include "fight.h"
#include "god-abil.h"
#include "items.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "level-state-type.h"
#include "libutil.h"
#include "invent.h"
#include "orb.h"
#include "player.h"
#include "player-stats.h"
#include "prompt.h"
#include "sprint.h"
#include "spl-book.h"
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "random.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-place.h"
#include "misc.h"
#include "viewchar.h"



static pakellas_blueprint_struct _base_blueprint(const char* name, const char* desc, const char* abbr)
{
    return {
        name,
        desc,
        abbr,
        1,
        0,
        100,
        vector<int>(),
        vector<int>()
    };
}
static pakellas_blueprint_struct _base2_blueprint(const char* name, const char* desc, const char* abbr,
    int max_level, int additional_mana, int percent)
{
    return {
        name,
        desc,
        abbr,
        max_level,
        additional_mana,
        percent,
        vector<int>(),
        vector<int>()
    };
}
static pakellas_blueprint_struct _prerequire_blueprint(const char* name, const char* desc, const char* abbr, int percent, vector<int> pre, vector<int> exclude)
{
    return {
        name,
        desc,
        abbr,
        1,
        0,
        percent,
        pre,
        exclude
    };
}
static pakellas_blueprint_struct _prerequire2_blueprint(const char* name, const char* desc, const char* abbr, int max_level, int additional_mana, int percent, vector<int> pre, vector<int> exclude)
{
    return {
        name,
        desc,
        abbr,
        max_level,
        additional_mana,
        percent,
        pre,
        exclude
    };
}




/// a per-god map of conducts to piety rewards given by that god.
map<pakellas_blueprint_type, pakellas_blueprint_struct> blueprint_list =
{
    //DESTRUCTION
    { BLUEPRINT_RANGE, _base2_blueprint("Range increase", "Range increased by 1.", "range+",
                                2,
                                0,
                                100)},
    { BLUEPRINT_PENTAN, _prerequire2_blueprint("Penetration", "Adds penetration to missiles. Increase Cost 1", "penet",
                                1,
                                1,   
                                100,
                                {},
                                {BLUEPRINT_BOME, BLUEPRINT_SPREAD})},
    { BLUEPRINT_BOME, _prerequire2_blueprint("Explosion", "3x3 explodes at the landing site. Increase Cost 1", "bomb+",
                                1,
                                1,   
                                100,
                                {},
                                {BLUEPRINT_PENTAN, BLUEPRINT_SPREAD})},
    { BLUEPRINT_ELEMENTAL_FIRE, _prerequire_blueprint("Fire Enchantment", "Turn 30% into a fire damage. Damage 1.2x increase.", "fire",
                                100,
                                {}, 
                                {BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON})},
    { BLUEPRINT_ELEMENTAL_COLD, _prerequire_blueprint("Cold Enchantment", "Turn 30% into a cold damage. Damage 1.2x increase.", "cold",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON})},
    { BLUEPRINT_ELEMENTAL_ELEC, _prerequire_blueprint("Elec Enchantment", "Turn 30% into a elec damage. Damage 1.2x increase.", "elec",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON})},
    { BLUEPRINT_ELEMENTAL_EARTH, _prerequire_blueprint("Earth Enchantment", "Damage 1.1x increase.", "earth",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON})},
    { BLUEPRINT_PERFECT_SHOT, _base_blueprint("Deadly Accuracy", "Missiles hit 100%.", "acc+" )},
    { BLUEPRINT_CLOUD, _prerequire_blueprint("Cloud Trail", "Missile leaves a cloud trail.", "cloud",
                                100,
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON},
                                {})},
    { BLUEPRINT_DEBUF_SLOW, _base_blueprint("Slow", "Slow target with magic resistance check", "slow" )},
    { BLUEPRINT_DEBUF_BLIND, _base_blueprint("Blind", "Blind target with Hit Dice check.", "blind" )},
    { BLUEPRINT_KNOCKBACK, _base2_blueprint("Knockback", "Enemy hit is knocked back.", "knockback",
                                1,
                                0,
                                0)},
    { BLUEPRINT_STICKY_FLAME, _prerequire_blueprint("Sticky flame", "Sticky flames to the hit enemy.", "Innerflame",
                                100,
                                {BLUEPRINT_ELEMENTAL_FIRE},
                                {})},
    { BLUEPRINT_FROZEN, _prerequire_blueprint("Frozen", "Fronzen to the hit enemy.", "frozen",
                                100,
                                {BLUEPRINT_ELEMENTAL_COLD},
                                {})},
    { BLUEPRINT_CHAIN_LIGHTNING, _prerequire_blueprint("lightening arc", "Lightning Arc splashes from the hit enemy.", "shock",
                                100,
                                {BLUEPRINT_ELEMENTAL_ELEC},
                                {})},
    { BLUEPRINT_DEFORM, _base2_blueprint("Radiation", "deformed to the hit enemy, Increase Cost 1", "rad",
                                1,
                                1,
                                100)},
    { BLUEPRINT_CHAOS, _prerequire_blueprint("Chaos", "Random effect to the hit enemy.  Damage 1.3x increase", "chaos",
                                50,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_ELEMENTAL_POISON})},
    { BLUEPRINT_SPREAD, _prerequire2_blueprint("Scatter", "Cone-shaped area. Range 1 reduced, Increase Cost 1", "cone",
                                1,
                                1,
                                100,
                                {},
                                {BLUEPRINT_BOME, BLUEPRINT_PENTAN})},
    { BLUEPRINT_ELEMENTAL_POISON, _prerequire_blueprint("Poison Enchantment", "Turn 30% into a poison damage. Damage 1.2x increase.", "poison",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS})},
    { BLUEPRINT_CURARE, _prerequire_blueprint("Curare", "Poison causes slow.", "curare",
                                0,
                                {BLUEPRINT_ELEMENTAL_POISON},
                                {})},
                                
    //SUMMON
    { BLUEPRINT_SIZEUP, _base2_blueprint("Size up", "Max hp increased by 30 % and size increased.", "size+",
                                3,
                                0,
                                100)},
    { BLUEPRINT_SUMMON_CAPACITY, _base2_blueprint("Multiple summon", "Summon limit increased by 1.", "multi+",
                                4,
                                0,
                                100)},
    { BLUEPRINT_MAGIC_ENERGY_BOLT, _prerequire2_blueprint("Destruction Magic", "Learn weak destruction magic.|Destruction magic is improved.", "destruction",
                                2,
                                0,
                                100,
                                {},
                                {BLUEPRINT_ANTIMAGIC_AURA})},
    { BLUEPRINT_FIRE_SUMMON, _prerequire_blueprint("Fire Enchantment", "Adds flame to attack. add fire resistance.", "fire",
                                100,
                                {},
                                {BLUEPRINT_COLD_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_CHOATIC_SUMMON, BLUEPRINT_EARTH_SUMMON})},
    { BLUEPRINT_COLD_SUMMON, _prerequire_blueprint("Cold Enchantment", "Adds cold to attack. add cold resistance.", "cold",
                                100,
                                {},
                                {BLUEPRINT_FIRE_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_CHOATIC_SUMMON, BLUEPRINT_EARTH_SUMMON})},
    { BLUEPRINT_ELEC_SUMMON, _prerequire_blueprint("Elec Enchantment", "Adds elec to attack. add elec resistance.", "elec",
                                100,
                                {},
                                {BLUEPRINT_FIRE_SUMMON, BLUEPRINT_COLD_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_CHOATIC_SUMMON, BLUEPRINT_EARTH_SUMMON})},
    { BLUEPRINT_POISON_SUMMON, _prerequire_blueprint("Poison Enchantment", "Adds a paralyzing poison to attack.", "poison",
                                100,
                                {},
                                {BLUEPRINT_FIRE_SUMMON, BLUEPRINT_COLD_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_CHOATIC_SUMMON, BLUEPRINT_EARTH_SUMMON})},
    { BLUEPRINT_FLIGHT, _base2_blueprint("Jetpack", "Gain flight, and movement speed increases.", "fly",
                                1,
                                0,
                                100)},
    { BLUEPRINT_SUMMON_SHOT, _base2_blueprint("Launcher", "Fires at the enemy when summoned.", "bolt",
                                1,
                                0,
                                0)},
    { BLUEPRINT_CLEAVING, _prerequire2_blueprint("Cleaving", "Gain cleaving.", "cleave",
                                1,
                                0,
                                100,
                                {},
                                {BLUEPRINT_LEGION})},
    { BLUEPRINT_HALO, _prerequire_blueprint("Halo", "Get a 3-tile halo.", "halo",
                                100,
                                {},
                                {BLUEPRINT_ANTIMAGIC_AURA}) },
    { BLUEPRINT_ANTIMAGIC_AURA, _prerequire_blueprint("Antimagic aura", "Get a 2-tile antimagic aura.", "anitmagic",
                                100,
                                {},
                                {BLUEPRINT_MAGIC_ENERGY_BOLT, BLUEPRINT_HALO})},
    { BLUEPRINT_CHOATIC_SUMMON, _prerequire_blueprint("Choas", "Adds Choas to attack.", "choas",
                                100,
                                {},
                                {BLUEPRINT_FIRE_SUMMON, BLUEPRINT_COLD_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_EARTH_SUMMON})},
    { BLUEPRINT_EARTH_SUMMON, _prerequire_blueprint("Earth Enchantment", "Adds 5 ac and Max hp increased by 20%.", "earth",
                                100,
                                {},
                                {BLUEPRINT_FIRE_SUMMON, BLUEPRINT_COLD_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_CHOATIC_SUMMON})},
    { BLUEPRINT_SUMMON_TIME, _base2_blueprint("Power Efficiency", "Summon time is Increased", "time+",
                                1,
                                0,
                                100)},
    { BLUEPRINT_LEGION, _prerequire2_blueprint("Legion", "Summons two weak golems at a time. Increase Cost 1|Summons three weak golems at a time. Increase Cost 1", "legion+",
                                2,
                                1,
                                100,
                                {},
                                {BLUEPRINT_CLEAVING}) },

                                                                       
    //ASSIST
    { BLUEPRINT_STATUP_STR, _base2_blueprint("Buff Unit-Str", "When cast Buff Unit, increase str by 5|When Cast buff unit, increase str by 10|When Cast buff unit, increase str by 15", "str+",
                                3,
                                0,
                                33) },
    { BLUEPRINT_STATUP_DEX, _base2_blueprint("Buff Unit-Dex", "When cast Buff Unit, increase dex by 5|When Cast buff unit, increase dex by 10|When Cast buff unit, increase dex by 15", "dex+",
                                3,
                                0,
                                33) },
    { BLUEPRINT_STATUP_INT, _base2_blueprint("Buff Unit-Int", "When cast Buff Unit, increase int by 5|When Cast buff unit, increase int by 10|When Cast buff unit, increase int by 15", "int+",
                                3,
                                0,
                                33) },
    { BLUEPRINT_EVASION, _base2_blueprint("Buff Unit-Ev Up", "When cast Buff Unit, increase evasion by 5|When Cast buff unit, increase evasion by 10|When Cast buff unit, increase evasion by 15", "eva+",
                                3,
                                0,
                                33) },
    { BLUEPRINT_SWIFT, _base2_blueprint("Buff Unit-Swift", "", "swift",
                                1,
                                0,
                                0) },
    { BLUEPRINT_SWIFT_2, _prerequire_blueprint("Buff Unit-Swift Up", "", "swift+",
                                200,
                                {BLUEPRINT_SWIFT},
                                {}) },
    { BLUEPRINT_HASTE, _prerequire_blueprint("Buff Unit-Haste", "", "haste",
                                300,
                                {BLUEPRINT_SWIFT_2},
                                {}) },
    { BLUEPRINT_CLEAVE, _prerequire_blueprint("Buff Unit-Cleave", "When cast Buff Unit, gain a cleaving attack.", "cleave",
                                100,
                                {},
                                {}) },

    { BLUEPRINT_BLINK, _base2_blueprint("Warp Unit", "Gain new abilities to cast blink.", "Warp",
                                1,
                                0,
                                100) },
    { BLUEPRINT_BLINK_STRONG, _prerequire_blueprint("Warp Unit-Range UP", "Warp Unit range increased.", "Wrange",
                                150,
                                {BLUEPRINT_BLINK},
                                {BLUEPRINT_WARF_MANA}) },
    { BLUEPRINT_WARF_MANA, _prerequire_blueprint("Warp Unit-Mana", "Warp Unit mana cost reduced by 1", "Wmana",
                                150,
                                {BLUEPRINT_BLINK},
                                {BLUEPRINT_BLINK_STRONG}) },
    { BLUEPRINT_SWAP_BLINK, _prerequire_blueprint("Warp Unit-Swap", "Warp Unit fire replace to position swap bolt. cost increased to 5", "Wswap",
                                200,
                                {BLUEPRINT_WARF_MANA, BLUEPRINT_BLINK_STRONG},
                                {BLUEPRINT_TELEPORT}) },
    { BLUEPRINT_TELEPORT, _prerequire_blueprint("Warp Unit-Teleport", "Warp Unit is replace to teleport. cost increased to 6", "Wtele",
                                200,
                                {BLUEPRINT_WARF_MANA, BLUEPRINT_BLINK_STRONG},
                                {BLUEPRINT_SWAP_BLINK}) },
    { BLUEPRINT_CONTROL_BLINK, _prerequire_blueprint("Warp Unit-ControlBlink", "Warp Unit is replace to controlled blink. cost increased to 7", "Wcblink",
                                300,
                                {BLUEPRINT_SWAP_BLINK},
                                {}) },

    { BLUEPRINT_BARRICADE, _base2_blueprint("Barrier Unit", "Gains new abilities to construct barricade.", "Barr",
                                1,
                                0,
                                100) },
    { BLUEPRINT_BARRICADE_STRONG, _prerequire2_blueprint("Barrier Unit-Hard", "Barrier Unit health increases.", "Bhard",
                                3,
                                0,
                                150,
                                {BLUEPRINT_BARRICADE},
                                {}) },
    { BLUEPRINT_BARRICADE_RANGE, _prerequire_blueprint("Barrier Unit-Range up", "Increases the range of Barrier Unit by 2.", "Brange",
                                150,
                                {BLUEPRINT_BARRICADE},
                                {}) },
    { BLUEPRINT_BARRICADE_SPIKE, _prerequire_blueprint("Barrier Unit-Spike", "Spikes are installed on the Barrier Unit.", "Bspike",
                                150,
                                {BLUEPRINT_BARRICADE},
                                {}) },
    { BLUEPRINT_BARRICADE_TIME, _prerequire_blueprint("Barrier Unit-Duration", "Barrier Unit duration increased.", "Btime",
                                150,
                                {BLUEPRINT_BARRICADE},
                                {}) },
                               
    { BLUEPRINT_REGEN, _base2_blueprint("Heal Unit", "Gain new abilities to cast weak regeneration.", "Heal",
                                1,
                                0,
                                100) },
    { BLUEPRINT_REGEN_STRONG, _prerequire_blueprint("Heal Unit-Power Up", "Heal Unit more powerful", "Hpower",
                                150,
                                {BLUEPRINT_REGEN},
                                {BLUEPRINT_HEAL_PURIFICATION}) },
    { BLUEPRINT_HEAL_PURIFICATION, _prerequire_blueprint("Heal Unit-Purification", "Heal Unit restore slow, poison, and petrifying.", "Hpurfi",
                                150,
                                {BLUEPRINT_REGEN},
                                {BLUEPRINT_REGEN_STRONG}) },
    { BLUEPRINT_SMALL_HEAL, _prerequire_blueprint("Heal Unit-Minor Heal", "Heal Unit is replace to minor heal. cost increased to 5", "Hminor",
                                200,
                                {BLUEPRINT_REGEN_STRONG, BLUEPRINT_HEAL_PURIFICATION},
                                {}) },
    { BLUEPRINT_LARGE_HEAL, _prerequire_blueprint("Heal Unit-Major Heal", "Heal Unit is replace to major heal. cost increased to 6", "Hmajor",
                                300,
                                {BLUEPRINT_SMALL_HEAL},
                                {}) },

    { BLUEPRINT_CLOUD_UNIT, _base2_blueprint("Cloud Unit", "Gain new abilities to produce steam cloud.", "Cloud",
                                1,
                                0,
                                100) }, 
    { BLUEPRINT_CLOUD_CONFUSE, _prerequire_blueprint("Cloud Unit-Confusion", "Cloud Unit produces mephitic cloud", "Cconf",
                                150,
                                {BLUEPRINT_CLOUD_UNIT},
                                {BLUEPRINT_CLOUD_FOG}) },
    { BLUEPRINT_CLOUD_FOG, _prerequire_blueprint("Cloud Unit-Fog", "Cloud Unit produces fog cloud", "Cfog",
                                150,
                                {BLUEPRINT_CLOUD_UNIT},
                                {BLUEPRINT_CLOUD_CONFUSE}) },
    { BLUEPRINT_CLOUD_FIRE, _prerequire_blueprint("Cloud Unit-Fire", "Cloud Unit produces fire cloud", "Cfire",
                                150,
                                {BLUEPRINT_CLOUD_FOG, BLUEPRINT_CLOUD_CONFUSE},
                                {BLUEPRINT_CLOUD_COLD}) },
    { BLUEPRINT_CLOUD_COLD, _prerequire_blueprint("Cloud Unit-Cold", "Cloud Unit produces cold cloud", "Ccold",
                                150,
                                {BLUEPRINT_CLOUD_FOG, BLUEPRINT_CLOUD_CONFUSE},
                                {BLUEPRINT_CLOUD_FIRE}) },
    { BLUEPRINT_CLOUD_ACID, _prerequire_blueprint("Cloud Unit-Acid", "Cloud Unit produces acid cloud", "Cacid",
                                150,
                                {BLUEPRINT_CLOUD_COLD, BLUEPRINT_CLOUD_FIRE},
                                {}) },
    { BLUEPRINT_CLOUD_TIME, _prerequire_blueprint("Cloud Unit-Duration", "Increases Cloud Unit duration", "Cdur",
                                100,
                                {BLUEPRINT_CLOUD_UNIT},
                                {}) },

   //COMMON
    { BLUEPRINT_BATTERY_UP, _base2_blueprint("Battery Up", "Increase 3 rod capacity.", "capa+",
                                6,
                                0,
                                150)},
    { BLUEPRINT_LIGHT, _prerequire_blueprint("Lightweight", "Can be used without holding.", "light",
                                100,
                                {},
                                {BLUEPRINT_BATTLEMAGE}) },
    { BLUEPRINT_BATTLEMAGE, _prerequire_blueprint("Battle Mage", "Increases the damage the rod deals in melee combat.", "melee+",
                                150,
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS, BLUEPRINT_ELEMENTAL_POISON,
                                BLUEPRINT_FIRE_SUMMON, BLUEPRINT_COLD_SUMMON, BLUEPRINT_ELEC_SUMMON, BLUEPRINT_POISON_SUMMON, BLUEPRINT_CHOATIC_SUMMON, BLUEPRINT_EARTH_SUMMON},
                                {BLUEPRINT_LIGHT})},
    { BLUEPRINT_MORE_ENCHANT, _base2_blueprint("More Enchant", "Increase 1 enchant, Increase 1 rod capacity.", "statup",
                                6,
                                0,
                                150)},
    { BLUEPRINT_HEAVILY_MODIFIED, _base2_blueprint("Heavily Modified", "Apply two random blueprints.", "random",
                                6,
                                0,
                                50)},
};

static vector<string> _split_desc(const char* str, char c = ' ')
{
    vector<string> result;

    do
    {
        const char* begin = str;

        while (*str != c && *str)
            str++;

        result.push_back(string(begin, str));
    } while (0 != *str++);

    return result;
}



static bool _enchant_rod(item_def* rod_, bool only_mana = false, bool slient_ = false) {
    const string orig_name = rod_->name(DESC_YOUR);
    bool work = false;
    {
        rod_->charge_cap += ROD_CHARGE_MULT * 1;

        work = true;
    }

    if (rod_->charges < rod_->charge_cap)
    {
        rod_->charges = min<int>(rod_->charge_cap, rod_->charges + 1);
        work = true;
    }
    if(!only_mana)
       rod_->rod_plus++;
    if (!work)
        return false;

    if(!slient_)
        mprf("%s glows for a moment.", orig_name.c_str());

    return true;
}




static void _pakellas_expire_upgrade()
{
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ON));
    you.props[PAKELLAS_UPGRADE_ON].get_vector().clear();
}

bool pakellas_prototype()
{
    ASSERT(you.props[PAKELLAS_PROTOTYPE].get_int() == 0);

    int keyin = 0;
    while (true)
    {
        if (crawl_state.seen_hups)
            return false;

        clear_messages();

        mpr_nojoin(MSGCH_PLAIN, "  [a] - destruction : A rod with destructive power");
        mpr_nojoin(MSGCH_PLAIN, "  [b] - summoning : A rod that summons powerful allies");
        mpr_nojoin(MSGCH_PLAIN, "  [c] - assist : A rod that provides various assistive abilities");
        mprf(MSGCH_PROMPT, "which one?");

        keyin = toalower(get_ch()) - 'a';


        if (keyin < 0 || keyin > 2)
            continue;

        break;
    }

    int thing_created = items(true, OBJ_RODS, ROD_PAKELLAS, 1, 0,
        you.religion);
    if (thing_created == NON_ITEM
        || !move_item_to_grid(&thing_created, you.pos()))
    {
        mprf("You cannot receive item here.");
        return false;
    }

    mitm[thing_created].charge_cap = 10 * ROD_CHARGE_MULT;
    mitm[thing_created].charges = mitm[thing_created].charge_cap;
    mitm[thing_created].rod_plus = 0;

    do_uncurse_item(mitm[thing_created], false);
    you.props[PAKELLAS_PROTOTYPE].get_int() = keyin + 1;
    _pakellas_expire_upgrade();
    pakellas_reset_upgrade_timer(true);

    set_ident_type(mitm[thing_created], true);
    set_ident_flags(mitm[thing_created], ISFLAG_KNOW_TYPE);
    set_ident_flags(mitm[thing_created], ISFLAG_KNOW_PLUSES);
    simple_god_message(" grants you a gift!");

    if (you.props[PAKELLAS_PROTOTYPE].get_int() == 3 || you.species == SP_FELID) {
        CrawlVector& available_sacrifices = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();
        available_sacrifices.push_back(BLUEPRINT_LIGHT);
    }


    return true;
}

static bool _check_able_upgrade(pakellas_blueprint_type type) {
    ASSERT(you.props.exists(AVAILABLE_ROD_UPGRADE_KEY));
    CrawlVector& available_upgrade = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();

    if (blueprint_list[type].prerequire.empty() == false) {
        bool ok_ = false;
        for (int prerequire_upgrade : blueprint_list[type].prerequire) {
            for (auto current_upgrade : available_upgrade) {
                if (prerequire_upgrade == current_upgrade.get_int()) {
                    ok_ = true;
                    break;
                }
            }
        }
        if (!ok_)
            return false;
    }

    for (int exclude_upgrade : blueprint_list[type].exclude) {
        for (auto current_upgrade : available_upgrade) {
            if (exclude_upgrade == current_upgrade.get_int()) {
                return false;
            }
        }
    }

    if (type == BLUEPRINT_BLINK && you.species == SP_FORMICID)
        return false;


    int max_level = blueprint_list[type].max_level;
    if (max_level > 0) {
        for (auto current_upgrade : available_upgrade) {
            if (current_upgrade.get_int() == type && max_level > 0) {
                max_level--;
            }
        }
    }
    if (max_level > 0 || max_level == -1) {
        return true;
    }
    return false;
}

static int _get_upgrade_modify(pakellas_blueprint_type type) {
    CrawlVector& upgrade_modify = you.props[AVAILABLE_ROD_UPGRADE_MODIFY].get_vector();

    int num = 1;
    for (auto upgrade_ : upgrade_modify) {

        if (upgrade_.get_int() == (int)type) {
            num++;
        }
    }
    return num;
}

static void _erase_upgrade_modify(pakellas_blueprint_type type) {
    CrawlVector& upgrade_modify = you.props[AVAILABLE_ROD_UPGRADE_MODIFY].get_vector();

    int i = 0;
    for (auto upgrade_ : upgrade_modify) {

        if (upgrade_.get_int() == (int)type) {
            upgrade_modify.erase(i);
            break;
        }
        i++;
    }
}

/**
 * Which upgrade are valid for Pakellas to potentially present to the player?
 *
 * @return      A weights list of potential upgrade
 */
static vector<pair<pakellas_blueprint_type, int>> _get_possible_upgrade()
{
    int prototype = you.props[PAKELLAS_PROTOTYPE].get_int();     
    vector<pair<pakellas_blueprint_type, int>> possible_upgrade;

    if (prototype == 1) {
        for (int i = BLUEPRINT_DESTRUCTION_START; i < BLUEPRINT_DESTRUCTION_END; i++) {
            pakellas_blueprint_type type = (pakellas_blueprint_type)i;
            if (_check_able_upgrade(type)) {
                possible_upgrade.emplace_back(type, blueprint_list[type].percent/_get_upgrade_modify(type));
                

            }
        }
    }

    if (prototype == 2) {
        for (int i = BLUEPRINT_SUMMON_START; i < BLUEPRINT_SUMMON_END; i++) {
            pakellas_blueprint_type type = (pakellas_blueprint_type)i;
            if (_check_able_upgrade(type)) {
                possible_upgrade.emplace_back(type, blueprint_list[type].percent / _get_upgrade_modify(type));
            }
        }
    }

    if (prototype == 3) {
        for (int i = BLUEPRINT_CHARM_START; i < BLUEPRINT_CHARM_END; i++) {
            pakellas_blueprint_type type = (pakellas_blueprint_type)i;
            if (_check_able_upgrade(type)) {
                possible_upgrade.emplace_back(type, blueprint_list[type].percent / _get_upgrade_modify(type));
            }
        }
    }

    for (int i = BLUEPRINT_PUBLIC_START; i < BLUEPRINT_PUBLIC_END; i++) {
        pakellas_blueprint_type type = (pakellas_blueprint_type)i;
        if (_check_able_upgrade(type)) {
            possible_upgrade.emplace_back(type, blueprint_list[type].percent / _get_upgrade_modify(type));
        }
    }
    return possible_upgrade;
}


static item_def* _get_pakellas_rod() {
    for (item_def& i : you.inv)
    {
        if (i.is_type(OBJ_RODS, ROD_PAKELLAS))
        {
            return &i;
            break;
        }
    }
    return nullptr;
}


bool pakellas_upgrade()
{
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ON));
    ASSERT(you.props.exists(AVAILABLE_ROD_UPGRADE_KEY));


    item_def* rod_ = _get_pakellas_rod();

    if (rod_ == nullptr)
    {
        mpr("You don't have pakellas rod.");
        return false;
    }


    int keyin = 0;

    while (true)
    {
        if (crawl_state.seen_hups)
            return false;

        clear_messages();

        int i = 0;
        for (const auto& store : you.props[PAKELLAS_UPGRADE_ON].get_vector())
        {
            string line = make_stringf("  [%c] - ", i + 'a');

            vector<string> descs = _split_desc(blueprint_list[(pakellas_blueprint_type)store.get_int()].desc, '|');

            int prev_level = is_blueprint_exist((pakellas_blueprint_type)store.get_int());

            line += make_stringf("%s : %s", blueprint_list[(pakellas_blueprint_type)store.get_int()].name,
                descs[(prev_level>(int)(descs.size()-1))? 0: prev_level].c_str());
            mpr_nojoin(MSGCH_PLAIN, line);
            i++;
        }
        mpr_nojoin(MSGCH_PLAIN, "  [d] - reject upgrade");
        mprf(MSGCH_PROMPT, "Purchase which upgrade?");

        keyin = toalower(get_ch()) - 'a';


        if (keyin < 0 || keyin > 3)
            continue;

        break;
    }

    if (keyin == 3 &&
        !yesno("Do you really want to reject the upgrade?",
            false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (keyin == 3) {
        pakellas_reset_upgrade_timer(false);
        simple_god_message(" has delayed your rod upgrade.");
    }
    else {
        if (keyin <= 2) {
            CrawlVector& available_sacrifices = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();
            int _upgrade = you.props[PAKELLAS_UPGRADE_ON].get_vector().get_value(keyin).get_int();
            
            bool heavily_mod_ = (_upgrade == BLUEPRINT_HEAVILY_MODIFIED);
            vector<string> add_string;
            int num_ = heavily_mod_ ? 2 : 1;

            while (num_ > 0) {
                if (_upgrade == BLUEPRINT_MORE_ENCHANT) {
                    _enchant_rod(rod_, false, true);
                    add_string.push_back(blueprint_list[(pakellas_blueprint_type)_upgrade].name);
                    num_--;
                }
                else if (_upgrade == BLUEPRINT_BATTERY_UP) {
                    _enchant_rod(rod_, true, true);
                    _enchant_rod(rod_, true, true);
                    _enchant_rod(rod_, true, true);
                    add_string.push_back(blueprint_list[(pakellas_blueprint_type)_upgrade].name);
                    num_--;
                }
                else if (_upgrade != BLUEPRINT_HEAVILY_MODIFIED) {
                    available_sacrifices.push_back(_upgrade);
                    add_string.push_back(blueprint_list[(pakellas_blueprint_type)_upgrade].name);
                    num_--;
                }
                _erase_upgrade_modify((pakellas_blueprint_type)_upgrade);

                if (num_ > 0) {
                    auto upgrades = _get_possible_upgrade();
                    _upgrade = *random_choose_weighted(upgrades);
                }
            }
            if (heavily_mod_) {
                mprf("upgrade random buleprint : %s, %s", add_string[0].c_str(), add_string[1].c_str());
            }

        }

        pakellas_reset_upgrade_timer(true);
        you.props[AVAILABLE_UPGRADE_NUM_KEY].get_int()++;
        you.wield_change = true;
        _enchant_rod(rod_);
    }
    _pakellas_expire_upgrade();

    if (you.props[AVAILABLE_UPGRADE_NUM_KEY].get_int() == 6) {
        simple_god_message(" has completed testing the prototype.");
    }
    return true;
}




/**
 * Chooses three distinct upgrade to offer the player, store them in
 * PAKELLAS_UPGRADE_ON, and print a message to the player letting them
 * know that their new upgrades are ready.
 */
void pakellas_offer_new_upgrade()
{
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ON));
    you.props[PAKELLAS_UPGRADE_ON].get_vector().clear();

    if (!you.props.exists(AVAILABLE_ROD_UPGRADE_MODIFY)) {
        you.props[AVAILABLE_ROD_UPGRADE_MODIFY].new_vector(SV_INT);
    }


    vector<pair<pakellas_blueprint_type, int>> possible_upgrade = _get_possible_upgrade();
    CrawlVector& available_upgrade = you.props[PAKELLAS_UPGRADE_ON].get_vector();
    CrawlVector& upgrade_modify = you.props[AVAILABLE_ROD_UPGRADE_MODIFY].get_vector();



    for (int i = 0; i < 3; i++) {
        pakellas_blueprint_type get_upgrade = *random_choose_weighted(possible_upgrade);
        bool vaild = true;
        for (auto current_upgrade : available_upgrade) {
            if (current_upgrade.get_int() == (int)get_upgrade) {
                vaild = false;
                break;
            }
        }
        if (vaild == false) {
            i--;
            continue;
        }
        available_upgrade.push_back((int)get_upgrade);
        upgrade_modify.push_back((int)get_upgrade);
    }
    simple_god_message(" believes you are ready to make a new upgrade.");
    // included in default force_more_message
}


/**
 * Reset the time until the next set of Pakellas upgrade are offered.
 *
 * @param clear_timer       Whether to reset the timer to the base time-between-
 *                          upgrades, rather than adding to it.
 */
void pakellas_reset_upgrade_timer(bool clear_timer)
{
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ROD_PROGRESS_KEY));
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ROD_DELAY_KEY));
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ROD_PENALTY_KEY));

    const int base_delay = 80;
    int delay = you.props[PAKELLAS_UPGRADE_ROD_DELAY_KEY].get_int();
    int added_delay;
    if (clear_timer)
    {
        added_delay = 0;
        delay = base_delay;
        you.props[PAKELLAS_UPGRADE_ROD_PENALTY_KEY] = 0;
    }
    else
    {
        added_delay = you.props[PAKELLAS_UPGRADE_ROD_PENALTY_KEY].get_int();
        const int new_penalty = 50;
        added_delay += new_penalty;

        you.props[PAKELLAS_UPGRADE_ROD_PENALTY_KEY] = added_delay;
    }

    delay = div_rand_round((delay + added_delay) * (3 - you.faith()), 3);
    if (crawl_state.game_is_sprint())
        delay /= SPRINT_MULTIPLIER;

    you.props[PAKELLAS_UPGRADE_ROD_DELAY_KEY] = delay;
    you.props[PAKELLAS_UPGRADE_ROD_PROGRESS_KEY] = 0;
}


int is_blueprint_exist(pakellas_blueprint_type blueprint)
{
    int level = 0;
    if (you.props.exists(AVAILABLE_ROD_UPGRADE_KEY)) {
        CrawlVector& available_upgrade = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();
        for (auto upgrade : available_upgrade) {
            if((pakellas_blueprint_type)upgrade.get_int() == blueprint) {
                level++;
            }
        }
    }
    return level;
}

int get_blueprint_element()
{
    if (you.props.exists(AVAILABLE_ROD_UPGRADE_KEY)) {
        CrawlVector& available_upgrade = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();
        for (auto upgrade : available_upgrade) {
            switch (upgrade.get_int()) {
            case BLUEPRINT_ELEMENTAL_FIRE:
            case BLUEPRINT_FIRE_SUMMON:
                return BLUEPRINT_ELEMENTAL_FIRE;
            case BLUEPRINT_ELEMENTAL_COLD:
            case BLUEPRINT_COLD_SUMMON:
                return BLUEPRINT_ELEMENTAL_COLD;
            case BLUEPRINT_ELEMENTAL_ELEC:
            case BLUEPRINT_ELEC_SUMMON:
                return BLUEPRINT_ELEMENTAL_ELEC;
            case BLUEPRINT_ELEMENTAL_EARTH:
            case BLUEPRINT_EARTH_SUMMON:
                return BLUEPRINT_ELEMENTAL_EARTH;
            case BLUEPRINT_CHAOS:
            case BLUEPRINT_CHOATIC_SUMMON:
                return BLUEPRINT_CHAOS;
            case BLUEPRINT_ELEMENTAL_POISON:
            case BLUEPRINT_POISON_SUMMON:
                return BLUEPRINT_ELEMENTAL_POISON;
            default:
                break;
            }
        }
    }
    return 0;
}

int quick_charge_pakellas() {
    int powers = you.piety / 2;

    short inc_ = 30 + calc_dice(2, powers * 2 / 5).roll();
    int item_slot = -1;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Charge which item?", menu_type::invlist,
                OSEL_DIVINE_RECHARGE);
        }

        if (item_slot == PROMPT_NOTHING)
            return -1;

        if (item_slot == PROMPT_ABORT)
        {
             canned_msg(MSG_OK);
             return -1;
        }

        item_def& items_ = you.inv[item_slot];

        if (!item_is_quickrechargeable(items_))
        {
            mpr("Choose an item to recharge, or Esc to abort.");
            more();

            // Try again.
            item_slot = -1;
            continue;
        }

        if (items_.base_type != OBJ_MISCELLANY && items_.base_type != OBJ_RODS)
            return 0;

        if (items_.base_type == OBJ_RODS)
        {
            bool work = false;
            const string orig_name = items_.name(DESC_YOUR);


            if (items_.charges < items_.charge_cap)
            {
                short charge_ = items_.charge_cap * inc_ / 100;
                items_.charges = items_.charges + charge_;
                if (items_.charge_cap < items_.charges) {
                    items_.charges = items_.charge_cap;
                }
                work = true;
            }

            if (!work) {
                mprf("%s already fully charged.", orig_name.c_str());
                return 0;
            }

            mprf("%s has recharged.", orig_name.c_str());
        }
        else if (items_.base_type == OBJ_MISCELLANY) {
            //not yet

            const string orig_name = items_.name(DESC_YOUR);
            int& debt = evoker_debt(items_.sub_type);

            if (debt == 0) {
                mprf("%s already charged.", orig_name.c_str());
                return 0;
            }

            int max_debt_ = evoker_charge_xp_debt(items_.sub_type) * evoker_max_charges(items_.sub_type);

            int charge_ = max_debt_ * inc_ / 100;
            debt = max(0, debt - charge_);
            
            if (debt == 0) {
                mprf("%s has fully recharged.", orig_name.c_str());
            }
            else {
                mprf("%s has slightly recharged.", orig_name.c_str());
            }
        }

        you.wield_change = true;
        return 1;
    } while (true);

    return 0;
}

vector<spell_type> list_of_rod_spell()
{
    vector<spell_type> _spells;

    if (you.props[PAKELLAS_PROTOTYPE].get_int() != 3) {
        return _spells;
    }

    _spells.emplace_back(SPELL_PAKELLAS_ROD_SELFBUFF);
    if (is_blueprint_exist(BLUEPRINT_BLINK)) {
        if(is_blueprint_exist(BLUEPRINT_CONTROL_BLINK))
            _spells.emplace_back(SPELL_PAKELLAS_ROD_CONTROLL_BLINK);
        else if (is_blueprint_exist(BLUEPRINT_SWAP_BLINK))
            _spells.emplace_back(SPELL_PAKELLAS_ROD_SWAP_BOLT);
        else 
            _spells.emplace_back(SPELL_PAKELLAS_ROD_BLINKTELE);

    }

    if (is_blueprint_exist(BLUEPRINT_BARRICADE)) {
        _spells.emplace_back(SPELL_PAKELLAS_ROD_BARRIAR);
    }
    if (is_blueprint_exist(BLUEPRINT_REGEN)) {
        _spells.emplace_back(SPELL_PAKELLAS_ROD_REGEN);
    }
    if (is_blueprint_exist(BLUEPRINT_CLOUD_UNIT)) {
        _spells.emplace_back(SPELL_PAKELLAS_ROD_CLOUD);
        
    }
    return _spells;
}


spell_type evoke_support_pakellas_rod()
{
    item_def* rod_ = _get_pakellas_rod();

    if (rod_ == nullptr)
    {
        mpr("You don't have pakellas rod.");
        return SPELL_NO_SPELL;
    }


    vector<spell_type> list_spells = list_of_rod_spell();

    int keyin = 0;
    if (list_spells.size() == 0) {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPELL_NO_SPELL;
    }
    else if (list_spells.size() == 1) {
        keyin = 'a';
    }
    else {
        mprf(MSGCH_PROMPT,
            "Evoke which spell from the rod ([a-%c] spell [?*] list)? ",
            (char)('a' + list_spells.size()- 1));
        keyin = get_ch();


        if (keyin == '?' || keyin == '*')
        {
            keyin = read_book(*rod_, true);
            // [ds] read_book sets turn_is_over.
            you.turn_is_over = false;
        }
    }


    if (!isaalpha(keyin))
    {
        canned_msg(MSG_HUH);
        return SPELL_NO_SPELL;
    }

    if (keyin - 'a' >= (int)list_spells.size()) {
        return SPELL_NO_SPELL;
    }

    return list_spells[keyin - 'a'];
}

spret cast_pakellas_selfbuff(int powc, bolt&, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    fail_check();
    mpr("Pakellas buff is in your body.");
    int time_ = 7 + roll_dice(3, powc) / 2;
    time_ = min(100, time_);
    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, time_, 100);


    const int str_up = is_blueprint_exist(BLUEPRINT_STATUP_STR) * 5;
    const int dex_up = is_blueprint_exist(BLUEPRINT_STATUP_DEX) * 5;
    const int int_up = is_blueprint_exist(BLUEPRINT_STATUP_INT) * 5;
    const int ev_up = is_blueprint_exist(BLUEPRINT_EVASION) * 5;

    if (str_up > 0 || dex_up > 0 || int_up > 0 || ev_up) {
        if (!you.duration[DUR_PAKELLAS_DURATION]) {
            you.attribute[ATTR_PAKELLAS_STR] = str_up;
            notify_stat_change(STAT_STR, str_up, true);
            you.attribute[ATTR_PAKELLAS_DEX] = dex_up;
            notify_stat_change(STAT_DEX, dex_up, true);
            you.attribute[ATTR_PAKELLAS_INT] = int_up;
            notify_stat_change(STAT_INT, int_up, true);
            you.attribute[ATTR_PAKELLAS_EV] = ev_up;
            //notify_stat_change(STAT_EV, ev_up, true);
            you.redraw_evasion = true;
            you.set_duration(DUR_PAKELLAS_DURATION, time_);
        }
        else {
            if (str_up > you.attribute[ATTR_PAKELLAS_STR]) {
                int offset_ = str_up - you.attribute[ATTR_PAKELLAS_STR];
                you.attribute[ATTR_PAKELLAS_STR] += offset_;
                notify_stat_change(STAT_STR, offset_, true);
            }
            if (dex_up > you.attribute[ATTR_PAKELLAS_DEX]) {
                int offset_ = dex_up - you.attribute[ATTR_PAKELLAS_DEX];
                you.attribute[ATTR_PAKELLAS_STR] += offset_;
                notify_stat_change(STAT_DEX, offset_, true);
            }
            if (int_up > you.attribute[ATTR_PAKELLAS_INT]) {
                int offset_ = int_up - you.attribute[ATTR_PAKELLAS_INT];
                you.attribute[ATTR_PAKELLAS_STR] += offset_;
                notify_stat_change(STAT_INT, offset_, true);
            }
            if (ev_up > you.attribute[ATTR_PAKELLAS_EV]) {
                int offset_ = ev_up - you.attribute[ATTR_PAKELLAS_EV];
                you.attribute[ATTR_PAKELLAS_EV] += offset_;
                you.redraw_evasion = true;
                //notify_stat_change(STAT_EV, offset_, true);
            }
            you.set_duration(DUR_PAKELLAS_DURATION, time_);
        }
    }

    if (is_blueprint_exist(BLUEPRINT_CLEAVE)) {
        you.increase_duration(DUR_CLEAVE, time_, 100);
    }

    return spret::success;
}


void pakellas_remove_self_buff()
{
    mprf(MSGCH_DURATION, "Your stat buff fades away.");
    notify_stat_change(STAT_STR, -you.attribute[ATTR_PAKELLAS_STR], true);
    notify_stat_change(STAT_INT, -you.attribute[ATTR_PAKELLAS_INT], true);
    notify_stat_change(STAT_DEX, -you.attribute[ATTR_PAKELLAS_DEX], true);
    you.duration[DUR_PAKELLAS_DURATION] = 0;
    you.attribute[ATTR_PAKELLAS_STR] = 0;
    you.attribute[ATTR_PAKELLAS_INT] = 0;
    you.attribute[ATTR_PAKELLAS_DEX] = 0;
    you.attribute[ATTR_PAKELLAS_EV] = 0;
    you.redraw_evasion = true;
}

spret cast_pakellas_blinktele(int, bolt&, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    if (you.no_tele(false, false, true))
        return fail ? spret::fail : spret::success;

    fail_check();
    if (is_blueprint_exist(BLUEPRINT_TELEPORT)) {
        you_teleport();
        if (is_blueprint_exist(BLUEPRINT_BLINK_STRONG)) {
            if (you.duration[DUR_TELEPORT] > 1)
                you.duration[DUR_TELEPORT]--;
        }
    }
    else {
        uncontrolled_blink();
        if (is_blueprint_exist(BLUEPRINT_BLINK_STRONG))
            uncontrolled_blink(); //more blink
    }
    return spret::success;
}
spret cast_pakellas_swap_bolt(int powc, bolt& beam, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    bolt tracer = beam;
    if (!player_tracer(ZAP_RANDOM_BOLT_TRACER, 200, tracer))
        return spret::abort;

    fail_check();

    bolt pbolt = beam;
    pbolt.name = "swap bolt";
    pbolt.thrower = KILL_YOU_MISSILE;
    pbolt.flavour = BEAM_MMISSILE;
    pbolt.real_flavour = BEAM_MMISSILE;
    pbolt.colour = LIGHTMAGENTA;
    pbolt.glyph = dchar_glyph(DCHAR_FIRED_ZAP);

    pbolt.hit_func = [](monster* mon, bool) {
        if (mon == nullptr)
            return;

        swap_with_monster(mon);
    };

    pbolt.obvious_effect = true;
    pbolt.range = spell_range(SPELL_PAKELLAS_ROD_SWAP_BOLT, powc);
    pbolt.hit = AUTOMATIC_HIT;
    pbolt.damage = calc_dice(1, 0);
    pbolt.origin_spell = SPELL_PAKELLAS_ROD_SWAP_BOLT;
    pbolt.fire();


    return spret::success;
}
spret cast_pakellas_controll_blink(int, bolt&, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    if (you.no_tele(true, true, true))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return spret::abort;
    }

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return spret::abort;
    }

    if (orb_limits_translocation())
    {
        if (!yesno("Your blink will be uncontrolled - continue anyway?",
            false, 'n'))
        {
            return spret::abort;
        }

        mprf(MSGCH_ORB, "The Orb prevents control of your translocation!");
        return cast_blink(fail);
    }
    int add_range_ = is_blueprint_exist(BLUEPRINT_BLINK_STRONG);

    return controlled_blink(fail, true, 6 + add_range_);
}
spret cast_pakellas_barriar(int powc, bolt& beam, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    const int dur = 2 + 2 * is_blueprint_exist(BLUEPRINT_BARRICADE_TIME);

    if (grid_distance(beam.target, you.pos()) > spell_range(SPELL_PAKELLAS_ROD_BARRIAR,
        powc)
        || !in_bounds(beam.target))
    {
        mpr("That's too far away.");
        return spret::abort;
    }

    if (!monster_habitable_grid(MONS_HUMAN, grd(beam.target)))
    {
        mpr("You can't construct there.");
        return spret::abort;
    }

    monster* mons = monster_at(beam.target);
    if (mons)
    {
        if (you.can_see(*mons))
        {
            mpr("That space is already occupied.");
            return spret::abort;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see is blocking your construction!");
        return spret::success;
    }

    fail_check();
    bool multi = false;//is_blueprint_exist(BLUEPRINT_BARRICADE_CROSS);

    mgen_data barricade(MONS_BARRICADE, BEH_FRIENDLY, beam.target, MHITYOU,
        MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);
    barricade.set_summoned(&you, dur, SPELL_PAKELLAS_ROD_BARRIAR, GOD_PAKELLAS);
    barricade.hp += is_blueprint_exist(BLUEPRINT_BARRICADE_STRONG) * 30;


    if (create_monster(barricade))
    {
        mprf("The structure of Barricade%s built.",
            multi ? "s were" : " was");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}
spret cast_pakellas_regen(int powc, bolt&, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    fail_check();
    if (is_blueprint_exist(BLUEPRINT_LARGE_HEAL) || is_blueprint_exist(BLUEPRINT_SMALL_HEAL)) {
        int heal_pow = 1;

        if (is_blueprint_exist(BLUEPRINT_LARGE_HEAL))
        {
            heal_pow = 9 + powc / 25;
        } 
        else if (is_blueprint_exist(BLUEPRINT_SMALL_HEAL))
        {
            heal_pow = 4 + powc / 40;
        }
        if (is_blueprint_exist(BLUEPRINT_REGEN_STRONG)) {
            heal_pow += 2;
        }

        heal_pow = min(50, heal_pow);
        const int healed = heal_pow + roll_dice(2, heal_pow) - 2;
        mpr("You are healed.");
        inc_hp(healed);
    }
    else {
        int max_time_ = powc / 6 + 1;

        if (is_blueprint_exist(BLUEPRINT_REGEN_STRONG)) {
            max_time_ *= 2;
        }

        you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, max_time_), 100,
            "Your skin crawls.");
    }

    if (is_blueprint_exist(BLUEPRINT_HEAL_PURIFICATION)) {
        you.duration[DUR_POISONING] = 0;
        you.duration[DUR_SLOW] = 0;
        you.duration[DUR_PETRIFYING] = 0;
    }

    return spret::success;
}
spret cast_pakellas_cloud(int powc, bolt& beam, bool fail)
{
    if (you.religion != GOD_PAKELLAS) {
        mprf("You cannot use it if you do not believe pakellas.");
        return spret::success;
    }
    if (env.level_state & LSTATE_STILL_WINDS)
    {
        mpr("The air is too still to form clouds.");
        return spret::abort;
    }

    int max_time_ = 6 + div_rand_round(powc * 3, 6);

    if (is_blueprint_exist(BLUEPRINT_CLOUD_TIME)) {
        max_time_ *= 2;
    }

    const int range = spell_range(SPELL_PAKELLAS_ROD_CLOUD, powc);

    targeter_shotgun hitfunc(&you, CLOUD_CONE_BEAM_COUNT, range);

    hitfunc.set_aim(beam.target);

    if (stop_attack_prompt(hitfunc, "cloud"))
        return spret::abort;

    fail_check();


    cloud_type cloud = CLOUD_STEAM;

    if (is_blueprint_exist(BLUEPRINT_CLOUD_ACID))
    {
        cloud = CLOUD_ACID;
    }
    else if (is_blueprint_exist(BLUEPRINT_CLOUD_FIRE))
    {
        cloud = CLOUD_FIRE;
    }
    else if (is_blueprint_exist(BLUEPRINT_CLOUD_COLD))
    {
        cloud = CLOUD_COLD;
    }
    else if (is_blueprint_exist(BLUEPRINT_CLOUD_FOG))
    {
        cloud = CLOUD_PURPLE_SMOKE;
    }
    else if (is_blueprint_exist(BLUEPRINT_CLOUD_CONFUSE))
    {
        cloud = CLOUD_MEPHITIC;
    }
    else
    {
        max_time_ /= 3;
    }

    for (const auto& entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;
        place_cloud(cloud, entry.first,
            4 + random2avg(max_time_, 3),
            &you);
    }
    mprf("%s %s a blast of %s!",
        you.name(DESC_THE).c_str(),
        you.conj_verb("create").c_str(),
        cloud_type_name(cloud).c_str());

    return spret::success;
}


int pakellas_addtional_difficult(spell_type which_spell) {

    if (which_spell == SPELL_PAKELLAS_ROD || which_spell == SPELL_PAKELLAS_ROD_SUMMON) {
        int add_ = 0;
        if (you.props.exists(AVAILABLE_ROD_UPGRADE_KEY)) {
            CrawlVector& available_upgrade = you.props[AVAILABLE_ROD_UPGRADE_KEY].get_vector();
            for (auto upgrade : available_upgrade)             
            {
                add_ += blueprint_list[(pakellas_blueprint_type)upgrade.get_int()].additional_mana;
            }
        }
        return add_;
    }
    else if (which_spell == SPELL_PAKELLAS_ROD_BLINKTELE) {
        if (is_blueprint_exist(BLUEPRINT_TELEPORT))
        {
            return 3;
        }
    }
    else if (which_spell == SPELL_PAKELLAS_ROD_REGEN) {
        if (is_blueprint_exist(BLUEPRINT_LARGE_HEAL))
        {
            return 2;
        }
        if (is_blueprint_exist(BLUEPRINT_SMALL_HEAL))
        {
            return 1;
        }
    }
    return 0;
}
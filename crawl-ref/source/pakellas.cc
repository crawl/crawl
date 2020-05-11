/**
 * @file
 * @brief Pakellas new rod source file
**/

#include "AppHdr.h"
#include "pakellas.h"

#include <string>

#include "env.h"
#include "actor.h"
#include "coord.h"
#include "debug.h"
#include "defines.h"
#include "god-abil.h"
#include "items.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "libutil.h"
#include "invent.h"
#include "player.h"
#include "prompt.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"



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



/// a per-god map of conducts to piety rewards given by that god.
map<pakellas_blueprint_type, pakellas_blueprint_struct> blueprint_list =
{
    //DESTRUCTION
    { BLUEPRINT_RANGE, _base_blueprint("Range increase", "Range increased by 1.", "range+")},
    { BLUEPRINT_PENTAN, _prerequire_blueprint("Penetration", "Adds penetration to missiles.", "penet",
                                100,
                                {},
                                {BLUEPRINT_BOME})},
    { BLUEPRINT_BOME, _prerequire_blueprint("Explosion", "Missile 3x3 explodes at the landing site.", "bomb+",
                                100,
                                {},
                                {BLUEPRINT_PENTAN})},
    { BLUEPRINT_ELEMENTAL_FIRE, _prerequire_blueprint("Fire Enchantment", "Missiles have a fire enchantmentand damage is slightly increased.", "fire",
                                100,
                                {}, 
                                {BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS})},
    { BLUEPRINT_ELEMENTAL_COLD, _prerequire_blueprint("Cold Enchantment", "Missiles have a clod enchantmentand damage is slightly increased.", "cold",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS})},
    { BLUEPRINT_ELEMENTAL_ELEC, _prerequire_blueprint("Elec Enchantment", "Missiles have a elec enchantmentand damage is slightly increased.", "elec",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS})},
    { BLUEPRINT_ELEMENTAL_EARTH, _prerequire_blueprint("Earth Enchantment", "Missiles damage is slightly increased.", "earth",
                                100,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_CHAOS})},
    { BLUEPRINT_PERFECT_SHOT, _base_blueprint("Deadly Accuracy", "Missiles hit 100%.", "acc+¡Ä" )},
    { BLUEPRINT_CLOUD, _prerequire_blueprint("Cloud Trail", "Missile leaves a cloud trail.", "cloud",
                                100,
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH, BLUEPRINT_CHAOS},
                                {})},
    { BLUEPRINT_DEBUF_SLOW, _base_blueprint("Debuf Slow", "The opponent hit by the missile is slow.", "slow" )},
    { BLUEPRINT_DEBUF_BLIND, _base_blueprint("Debuf Blind", "The opponent hit by the missile is blind.", "blind" )},
    { BLUEPRINT_KNOCKBACK, _base2_blueprint("Knockback", "Enemy hit is knocked back.", "knockback",
                                1,
                                0,
                                0)},
    { BLUEPRINT_STICKY_FLAME, _prerequire_blueprint("Sticky flame", "Sticky flames take place on the right enemy.", "Innerflame",
                                50,
                                {BLUEPRINT_ELEMENTAL_FIRE},
                                {})},
    { BLUEPRINT_FROZEN, _prerequire_blueprint("Frozen", "Fronzen take place on the right enemy.", "frozen",
                                50,
                                {BLUEPRINT_ELEMENTAL_COLD},
                                {})},
    { BLUEPRINT_CHAIN_LIGHTNING, _prerequire_blueprint("Chain lightening", "Chain Lightning splashes from the hit enemy.", "chain",
                                50,
                                {BLUEPRINT_ELEMENTAL_ELEC},
                                {})},
    { BLUEPRINT_DEFORM, _base_blueprint("Radiation", "Enemies hit by the missile are deformed.", "rad")},
    { BLUEPRINT_CHAOS, _prerequire_blueprint("Choas", "Enemies hit by missiles have an unknown effect.", "choas",
                                20,
                                {},
                                {BLUEPRINT_ELEMENTAL_FIRE, BLUEPRINT_ELEMENTAL_COLD, BLUEPRINT_ELEMENTAL_ELEC, BLUEPRINT_ELEMENTAL_EARTH})},
    //SUMMON
    { BLUEPRINT_SIZEUP, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_SUMMON_CAPACITY, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_MAGIC_ENERGY_BOLT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_FIRE_SUMMON, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_COLD_SUMMON, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_ELEC_SUMMON, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_POISON_SUMMON, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_FLIGHT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_SUMMON_SHOT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_CLEAVING, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_HALO, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_ANTIMAGIC_AURA, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_CHOATIC_SUMMON, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    //ASSIST
    { BLUEPRINT_STATUP_STR, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_STATUP_DEX, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_STATUP_INT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_SWIFT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_HASTE, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_BLING, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_DIRECTION_BLINK, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_CONTROL_BLINK, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_TELEPORT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_EMERGENCY_TELEPORT, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_REGEN, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_SMALL_HEAL, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    { BLUEPRINT_LARGE_HEAL, _base_blueprint("Penetration", "Adds penetration to missiles.", "penet+")},
    //COMMON
    { BLUEPRINT_BATTERY_UP, _base2_blueprint("Battery Up", "The maximum rod capacity is increased by 3.", "capa+",
                                6,
                                0,
                                150)},
    { BLUEPRINT_LIGHT, _base2_blueprint("Lightweight", "Can be used without holding.", "light",
                                6,
                                0,
                                0)},
    { BLUEPRINT_BATTLEMAGE, _base2_blueprint("Battle Mage", "Increases the damage the rod deals in melee combat.", "melee+",
                                1,
                                0,
                                0)},
    { BLUEPRINT_MORE_ENCHANT, _base2_blueprint("More Enchant", "Add enchant value by 1 without special ability.", "statup",
                                6,
                                0,
                                150)},
    { BLUEPRINT_HEAVILY_MODIFIED, _base2_blueprint("Heavily Modified", "Apply two random blueprints.", "random",
                                6,
                                0,
                                50)},
};


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

        mpr_nojoin(MSGCH_PLAIN, "  [a] - destruction");
        mpr_nojoin(MSGCH_PLAIN, "  [b] - summoning (not yet)");
        mpr_nojoin(MSGCH_PLAIN, "  [c] - assist (not yet)");
        mprf(MSGCH_PROMPT, "which one?");

        keyin = toalower(get_ch()) - 'a';


        if (keyin == 1 || keyin == 2) {
            continue;
        }

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

    you.props[PAKELLAS_PROTOTYPE].get_int() = keyin + 1;
    _pakellas_expire_upgrade();
    pakellas_reset_upgrade_timer(true);

    set_ident_type(mitm[thing_created], true);
    set_ident_flags(mitm[thing_created], ISFLAG_KNOW_TYPE);
    set_ident_flags(mitm[thing_created], ISFLAG_KNOW_PLUSES);
    simple_god_message(" grants you a gift!");
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
                possible_upgrade.emplace_back(type, blueprint_list[type].percent);
            }
        }
    }

    for (int i = BLUEPRINT_PUBLIC_START; i < BLUEPRINT_PUBLIC_END; i++) {
        pakellas_blueprint_type type = (pakellas_blueprint_type)i;
        if (_check_able_upgrade(type)) {
            possible_upgrade.emplace_back(type, blueprint_list[type].percent);
        }
    }
    return possible_upgrade;
}




bool pakellas_upgrade()
{
    ASSERT(you.props.exists(PAKELLAS_UPGRADE_ON));
    ASSERT(you.props.exists(AVAILABLE_ROD_UPGRADE_KEY));

    item_def* rod_ = nullptr;
    for (item_def& i : you.inv)
    {
        if (i.is_type(OBJ_RODS, ROD_PAKELLAS))
        {
            rod_ = &i;
            break;
        }
    }

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

            line += make_stringf("%s : %s", blueprint_list[(pakellas_blueprint_type)store.get_int()].name,
                blueprint_list[(pakellas_blueprint_type)store.get_int()].desc);
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
            
            int num_ = (_upgrade == BLUEPRINT_HEAVILY_MODIFIED) ? 2 : 1;


            while (num_ > 0) {
                if (_upgrade == BLUEPRINT_MORE_ENCHANT) {
                    _enchant_rod(rod_, false, true);
                    num_--;
                }
                else if (_upgrade == BLUEPRINT_BATTERY_UP) {
                    _enchant_rod(rod_, true, true);
                    _enchant_rod(rod_, true, true);
                    _enchant_rod(rod_, true, true);
                    num_--;
                }
                else if (_upgrade != BLUEPRINT_HEAVILY_MODIFIED) {
                    available_sacrifices.push_back(_upgrade);
                    num_--;
                }

                if (num_ > 0) {
                    auto upgrades = _get_possible_upgrade();
                    _upgrade = *random_choose_weighted(upgrades);
                }
            }
            

        }

        pakellas_reset_upgrade_timer(true);
        you.props[AVAILABLE_UPGRADE_NUM_KEY].get_int()++;
        you.wield_change = true;
        _enchant_rod(rod_);
    }
    _pakellas_expire_upgrade();
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

    vector<pair<pakellas_blueprint_type, int>> possible_upgrade = _get_possible_upgrade();
    CrawlVector& available_upgrade = you.props[PAKELLAS_UPGRADE_ON].get_vector();

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

int quick_charge_pakellas() {

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
                items_.charges = items_.charge_cap;
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
            mprf("debt %d.", debt);

            if (debt == 0) {
                mprf("%s already charged.", orig_name.c_str());
                return 0;
            }

            debt = 0;

            mprf("%s has recharged.", orig_name.c_str());
        }

        you.wield_change = true;
        return 1;
    } while (true);

    return 0;
}
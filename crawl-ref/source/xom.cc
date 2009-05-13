/*
 *  File:       xom.cc
 *  Summary:    All things Xomly
 *  Written by: Zooko
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>

#include "beam.h"
#include "branch.h"
#include "database.h"
#include "delay.h"
#include "effects.h"
#include "it_use2.h"
#include "items.h"
#include "Kills.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monplace.h"
#include "monstuff.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

#ifdef DEBUG_XOM
#    define DEBUG_RELIGION    1
#    define NOTE_DEBUG_XOM    1
#endif

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS 1
#    define DEBUG_GIFTS       1
#endif

// Which spells?  First I copied all spells from your_spells(), and then
// I filtered some out, especially conjurations.  Then I sorted them in
// roughly ascending order of power.

// Spells to be cast at tension 0 (no or only low-level monsters around),
// mostly flavour.
static const spell_type _xom_nontension_spells[] =
{
    SPELL_MAGIC_MAPPING, SPELL_DETECT_ITEMS, SPELL_SUMMON_BUTTERFLIES,
    SPELL_DETECT_CREATURES, SPELL_FLY, SPELL_SPIDER_FORM,
    SPELL_STATUE_FORM, SPELL_ICE_FORM, SPELL_DRAGON_FORM, SPELL_NECROMUTATION
};

// Spells to be cast at tension > 0, i.e. usually in battle situations.
static const spell_type _xom_tension_spells[] =
{
    SPELL_BLINK, SPELL_CONFUSING_TOUCH, SPELL_CAUSE_FEAR,
    SPELL_MASS_SLEEP, SPELL_DISPERSAL, SPELL_STONESKIN, SPELL_RING_OF_FLAMES,
    SPELL_OLGREBS_TOXIC_RADIANCE, SPELL_TUKIMAS_VORPAL_BLADE,
    SPELL_MAXWELLS_SILVER_HAMMER, SPELL_FIRE_BRAND, SPELL_FREEZING_AURA,
    SPELL_POISON_WEAPON, SPELL_STONEMAIL, SPELL_LETHAL_INFUSION,
    SPELL_EXCRUCIATING_WOUNDS, SPELL_WARP_BRAND, SPELL_TUKIMAS_DANCE,
    SPELL_RECALL, SPELL_SUMMON_BUTTERFLIES, SPELL_SUMMON_SMALL_MAMMALS,
    SPELL_SUMMON_SCORPIONS, SPELL_SUMMON_SWARM, SPELL_FLY, SPELL_SPIDER_FORM,
    SPELL_STATUE_FORM, SPELL_ICE_FORM, SPELL_DRAGON_FORM, SPELL_ANIMATE_DEAD,
    SPELL_SUMMON_WRAITHS, SPELL_SHADOW_CREATURES, SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_CALL_CANINE_FAMILIAR, SPELL_SUMMON_ICE_BEAST, SPELL_SUMMON_UGLY_THING,
    SPELL_CONJURE_BALL_LIGHTNING, SPELL_SUMMON_DRAGON, SPELL_DEATH_CHANNEL,
    SPELL_NECROMUTATION
};

static const char *_xom_message_arrays[NUM_XOM_MESSAGE_TYPES][6] =
{
    // XM_NORMAL
    {
        "Xom is interested.",
        "Xom is mildly amused.",
        "Xom is amused.",
        "Xom is highly amused!",
        "Xom thinks this is hilarious!",
        "Xom roars with laughter!"
    },

    // XM_INTRIGUED
    {
        "Xom is interested.",
        "Xom is very interested.",
        "Xom is extremely interested.",
        "Xom is intrigued!",
        "Xom is very intrigued!",
        "Xom is fascinated!"
    }
};

const char *describe_xom_favour(bool upper)
{
    std::string favour;
    if (you.religion != GOD_XOM)
        favour = "a very buggy toy of Xom.";
    else if (you.gift_timeout < 1)
        favour = "a BORING thing.";
    else
    {
        favour = (you.piety > 180) ? "Xom's teddy bear." :
                 (you.piety > 150) ? "a beloved toy of Xom." :
                 (you.piety > 120) ? "a favourite toy of Xom." :
                 (you.piety >  80) ? "a toy of Xom." :
                 (you.piety >  50) ? "a plaything of Xom." :
                 (you.piety >  20) ? "a special plaything of Xom."
                                   : "a very special plaything of Xom.";
    }

    if (upper)
        favour = uppercase_first(favour);

    return (favour.c_str());
}

static std::string _get_xom_speech(const std::string key)
{
    std::string result = getSpeakString("Xom " + key);

    if (result.empty())
        result = getSpeakString("Xom general effect");

    if (result.empty())
        return ("Xom makes something happen.");

    return (result);
}

static bool _xom_is_bored()
{
    return (you.religion == GOD_XOM && you.gift_timeout == 0);
}

static bool _xom_feels_nasty()
{
    // Xom will only directly kill you with a bad effect if you're under
    // penance from him, or if he's bored.
    return (you.penance[GOD_XOM] || _xom_is_bored());
}

bool xom_is_nice(int tension)
{
    if (you.penance[GOD_XOM])
        return (false);

    if (you.religion == GOD_XOM)
    {
        // If you.gift_timeout was 0, then Xom was BORED.  He HATES that.
        if (you.gift_timeout == 0)
            return (false);

        // At high tension Xom is more likely to be nice,
        // at zero tension the opposite.
        const int tension_bonus
            = (tension == -1 ? 0 :
               tension ==  0 ? -std::min(abs(HALF_MAX_PIETY - you.piety) / 2,
                                         you.piety / 10)
                             : std::min((MAX_PIETY - you.piety) / 2,
                                        random2(tension)));

        const int effective_piety = you.piety + tension_bonus;
        ASSERT(effective_piety >= 0 && effective_piety <= MAX_PIETY);

#ifdef DEBUG_XOM
        mprf(MSGCH_DIAGNOSTICS,
             "Xom: tension: %d, piety: %d -> tension bonus = %d, eff. piety: %d",
             tension, you.piety, tension_bonus, effective_piety);
#endif

        // Whether Xom is nice depends largely on his mood (== piety).
        return (x_chance_in_y(effective_piety, MAX_PIETY));
    }
    else // CARD_XOM
        return coinflip();
}

static void _xom_is_stimulated(int maxinterestingness,
                               const char *message_array[],
                               bool force_message)
{
    if (you.religion != GOD_XOM || maxinterestingness <= 0)
        return;

    // Xom is not directly stimulated by his own acts.
    if (crawl_state.which_god_acting() == GOD_XOM)
        return;

    int interestingness = random2(maxinterestingness);

    interestingness = std::min(255, interestingness);

#if defined(DEBUG_RELIGION) || defined(DEBUG_GIFTS) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "Xom: gift_timeout: %d, maxinterestingness = %d, interestingness = %d",
         you.gift_timeout, maxinterestingness, interestingness);
#endif

    bool was_stimulated = false;
    if (interestingness > you.gift_timeout && interestingness >= 12)
    {
        you.gift_timeout = interestingness;
        was_stimulated = true;
    }

    if (was_stimulated || force_message)
    {
        god_speaks(GOD_XOM,
                   ((interestingness > 200) ? message_array[5] :
                    (interestingness > 100) ? message_array[4] :
                    (interestingness >  75) ? message_array[3] :
                    (interestingness >  50) ? message_array[2] :
                    (interestingness >  25) ? message_array[1]
                                            : message_array[0]));
    }
}

void xom_is_stimulated(int maxinterestingness, xom_message_type message_type,
                       bool force_message)
{
    _xom_is_stimulated(maxinterestingness, _xom_message_arrays[message_type],
                       force_message);
}

void xom_is_stimulated(int maxinterestingness, const std::string& message,
                       bool force_message)
{
    if (you.religion != GOD_XOM)
        return;

    const char *message_array[6];

    for (int i = 0; i < 6; ++i)
        message_array[i] = message.c_str();

    _xom_is_stimulated(maxinterestingness, message_array, force_message);
}

void xom_tick()
{
    // Xom semi-randomly drifts your piety.
    const std::string old_xom_favour = describe_xom_favour();
    const bool good = (you.piety == HALF_MAX_PIETY? coinflip()
                                                  : you.piety > HALF_MAX_PIETY);
    int size = abs(you.piety - HALF_MAX_PIETY);

    // Piety slowly drifts towards the extremes.
    int delta = (x_chance_in_y(511, 1000) ? 1 : -1);
    size += delta;
    if (size > HALF_MAX_PIETY)
        size = HALF_MAX_PIETY;

    you.piety = HALF_MAX_PIETY + (good ? size : -size);
    std::string new_xom_favour = describe_xom_favour();
    if (old_xom_favour != new_xom_favour)
    {
        // If we entered another favour state, take a big step into
        // the new territory to avoid oscillating favour announcements
        // every few turns.
        size += delta * 8;
        if (size > HALF_MAX_PIETY)
            size = HALF_MAX_PIETY;

        // If size was 0 to begin with it may become negative but that
        // doesn't really matter.
        you.piety = HALF_MAX_PIETY + (good ? size : -size);
    }

#ifdef DEBUG_DIAGNOSTICS
    snprintf(info, INFO_SIZE, "xom_tick(), delta: %d, piety: %d",
             delta, you.piety);
    take_note(Note(NOTE_MESSAGE, 0, 0, info), true);
#endif

    // ...but he gets bored...
    if (you.gift_timeout > 0 && coinflip())
        you.gift_timeout--;

    new_xom_favour = describe_xom_favour();
    if (old_xom_favour != new_xom_favour)
    {
        const std::string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
    }

    if (you.gift_timeout == 1)
        simple_god_message(" is getting BORED.");

    if (one_chance_in(3))
    {
        const int tension = get_tension(GOD_XOM);
        const int chance = (tension ==  0 ? 1 :
                            tension <=  5 ? 2 :
                            tension <= 10 ? 3 :
                            tension <= 20 ? 4
                                          : 5);

        // If Xom is bored the chances for Xom acting are reversed.
        if (you.gift_timeout == 0 && x_chance_in_y(5-chance,5))
        {
            xom_acts(abs(you.piety - HALF_MAX_PIETY), tension);
            return;
        }
        else if (you.gift_timeout <= 1 && chance > 0
                 && x_chance_in_y(chance-1, 4))
        {
            // During tension, Xom may briefly forget about being bored.
            const int interest = random2(chance*15);
            if (interest > 0)
            {
                if (interest < 25)
                    simple_god_message(" is interested.");
                else
                    simple_god_message(" is intrigued.");

                you.gift_timeout += interest;
#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
                mprf(MSGCH_DIAGNOSTICS,
                     "tension %d (chance: %d) -> increase interest to %d",
                     tension, chance, you.gift_timeout);
#endif
            }
        }

        if (x_chance_in_y(chance, 5))
            xom_acts(abs(you.piety - HALF_MAX_PIETY), tension);
    }
}

// Picks 100 random grids from the level and checks whether they've been
// marked as seen (explored) or known (mapped). If seen_only is true
// grids only "seen" via magic mapping don't count.
// Returns the estimated percentage value of exploration.
static int _exploration_estimate(bool seen_only = false)
{
    int seen  = 0;
    int total = 0;
    int tries = 0;
    do
    {
        tries++;
        coord_def pos = random_in_bounds();
        if (!seen_only && is_terrain_known(pos) || is_terrain_seen(pos))
        {
            seen++;
            total++;
            continue;
        }

        bool open = true;
        if (grid_is_solid(grd(pos)) && grd(pos) != DNGN_CLOSED_DOOR)
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (map_bounds(*ai) && (!grid_is_opaque(grd(*ai))
                                        || grd(*ai) == DNGN_CLOSED_DOOR))
                {
                    open = true;
                    break;
                }
            }
        }
        if (open)
            total++;
    }
    while (total < 100 && tries < 1000);

#ifdef DEBUG_XOM
    mprf(MSGCH_DIAGNOSTICS,
         "exploration estimate (%s): %d out of %d grids seen",
         seen_only ? "explored" : "mapped", seen, total);
#endif

    // If we didn't get any qualifying grids, there are probably so few
    // of them you've already seen them all.
    if (total == 0)
        return 100;

    if (total < 100)
        seen *= 100/total;

    return (seen);
}

static bool _spell_weapon_check(const spell_type spell)
{
    switch (spell)
    {
    case SPELL_TUKIMAS_DANCE:
        // Requires a wielded weapon.
        return player_weapon_wielded();
    case SPELL_TUKIMAS_VORPAL_BLADE:
    case SPELL_MAXWELLS_SILVER_HAMMER:
    case SPELL_FIRE_BRAND:
    case SPELL_FREEZING_AURA:
    case SPELL_POISON_WEAPON:
    case SPELL_LETHAL_INFUSION:
    case SPELL_EXCRUCIATING_WOUNDS:
    case SPELL_WARP_BRAND:
    {
        if (!player_weapon_wielded())
            return (false);

        // The wielded weapon must be a non-branded non-launcher non-artefact!
        const item_def& weapon = *you.weapon();
        return (!is_artefact(weapon) && !is_range_weapon(weapon)
                && get_weapon_brand(weapon) == SPWPN_NORMAL);
    }
    default:
        return (true);
    }
}

static bool _transformation_check(const spell_type spell)
{
    transformation_type tran = TRAN_NONE;
    switch (spell)
    {
    case SPELL_SPIDER_FORM:
        tran = TRAN_SPIDER;
        break;
    case SPELL_STATUE_FORM:
        tran = TRAN_STATUE;
        break;
    case SPELL_ICE_FORM:
        tran = TRAN_ICE_BEAST;
        break;
    case SPELL_DRAGON_FORM:
        tran = TRAN_DRAGON;
        break;
    case SPELL_NECROMUTATION:
        tran = TRAN_LICH;
        break;
    default:
        break;
    }

    if (tran == TRAN_NONE)
        return (true);

    // Check whether existing enchantments/transformations, cursed equipment,
    // or potential stat loss interfere with this transformation.
    return transform(0, tran, true, true);
}

static bool _xom_makes_you_cast_random_spell(int sever, int tension)
{
    int spellenum = std::max(1, sever);

    god_acting gdact(GOD_XOM);

    spell_type spell;
    if (tension > 0)
    {
        const int nxomspells = ARRAYSZ(_xom_tension_spells);
        spellenum = std::min(nxomspells, spellenum);
        spell     = _xom_tension_spells[random2(spellenum)];

        // Don't attempt to cast spells that are guaranteed to fail.
        // You may still get results such as "The spell fizzles" or
        // "Nothing appears to happen", but those should be rarer now.
        if (!_spell_weapon_check(spell))
            return (false);
    }
    else
    {
        const int nxomspells = ARRAYSZ(_xom_nontension_spells);
        // spellenum will be at least 3, so we don't run into infinite loops
        // for Detect Creatures/Magic Mapping in fully explored levels.
        spellenum = std::min(nxomspells, std::max(3 + coinflip(), spellenum));
        spell     = _xom_nontension_spells[random2(spellenum)];

        if (spell == SPELL_MAGIC_MAPPING || spell == SPELL_DETECT_ITEMS)
        {
            // If the level is already mostly explored, there's
            // a chance we might try something else.
            const int explored = _exploration_estimate();
            if (explored > 80 && x_chance_in_y(explored, 100))
                return (false);
        }
    }
    // Don't attempt to cast spells the undead cannot memorise.
    if (you_cannot_memorise(spell))
        return (false);

    // Don't attempt to transform the player if the transformation will fail.
    if (!_transformation_check(spell))
        return (false);

    god_speaks(GOD_XOM, _get_xom_speech("spell effect").c_str());

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "_xom_makes_you_cast_random_spell(); spell: %d, spellenum: %d",
         spell, spellenum);
#endif

    static char spell_buf[100];
    snprintf(spell_buf, sizeof(spell_buf), "cast spell '%s'",
             spell_title(spell));
    take_note(Note(NOTE_XOM_EFFECT, you.piety, tension, spell_buf), true);

    your_spells(spell, sever, false);
    return (true);
}

static void _try_brand_switch(const int item_index)
{
    if (item_index == NON_ITEM)
        return;

    item_def &item(mitm[item_index]);

    // Only apply it to melee weapons for the player.
    if (item.base_type != OBJ_WEAPONS || is_range_weapon(item))
        return;

    if (is_unrandom_artefact(item) || is_fixed_artefact(item))
        return;

    // Only do it some of the time.
    if (one_chance_in(5))
        return;

    int brand;
    if (item.base_type == OBJ_WEAPONS)
    {
        // Only switch already branded items.
        if (get_weapon_brand(item) == SPWPN_NORMAL)
            return;

        brand = (int) SPWPN_CHAOS;
    }
    else
    {
        // Only switch already branded items.
        if (get_ammo_brand(item) == SPWPN_NORMAL)
            return;

        brand = (int) SPMSL_CHAOS;
    }

    if (is_random_artefact(item))
        randart_set_property(item, RAP_BRAND, brand);
    else
        item.special = brand;
}

static void _xom_make_item(object_class_type base, int subtype, int power)
{
    god_acting gdact(GOD_XOM);

    int thing_created =
        items(true, base, subtype, true, power, MAKE_ITEM_RANDOM_RACE,
              0, 0, GOD_XOM);

    if (thing_created == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }

    _try_brand_switch(thing_created);

    static char gift_buf[100];
    snprintf(gift_buf, sizeof(gift_buf), "god gift: %s",
             mitm[thing_created].name(DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, gift_buf), true);

    move_item_to_grid(&thing_created, you.pos());
    mitm[thing_created].inscription = "god gift";
    canned_msg(MSG_SOMETHING_APPEARS);

    stop_running();
}

static void _xom_acquirement(object_class_type force_class)
{
    god_acting gdact(GOD_XOM);

    int item_index = NON_ITEM;

    if (!acquirement(force_class, GOD_XOM, false, &item_index)
        || item_index == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }

    _try_brand_switch(item_index);

    static char gift_buf[100];
    snprintf(gift_buf, sizeof(gift_buf), "god gift: %s",
             mitm[item_index].name(DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, gift_buf), true);

    stop_running();
}

static object_class_type _get_unrelated_wield_class(object_class_type ref)
{
    object_class_type objtype = OBJ_WEAPONS;
    if (ref == OBJ_WEAPONS)
    {
        if (one_chance_in(10))
            objtype = OBJ_MISCELLANY;
        else
            objtype = OBJ_STAVES;
    }
    else if (ref == OBJ_STAVES)
    {
        if (one_chance_in(10))
            objtype = OBJ_MISCELLANY;
        else
            objtype = OBJ_WEAPONS;
    }
    else
    {
        const int temp_rand = random2(3);
        objtype = (temp_rand == 0) ? OBJ_WEAPONS :
                  (temp_rand == 1) ? OBJ_STAVES
                                   : OBJ_MISCELLANY;
    }

    return (objtype);
}

static bool _xom_annoyance_gift(int power)
{
    god_acting gdact(GOD_XOM);

    if (coinflip() && player_in_a_dangerous_place())
    {
        const item_def *weapon = you.weapon();

        // Xom has a sense of humour.
        if (coinflip() && weapon && weapon->cursed())
        {
            // If you are wielding a cursed item then Xom will give you
            // an item of that same type.  Ha ha!
            god_speaks(GOD_XOM, _get_xom_speech("cursed gift").c_str());
            if (coinflip())
                // For added humour, give the same sub-type.
                _xom_make_item(weapon->base_type, weapon->sub_type, power * 3);
            else
                _xom_acquirement(weapon->base_type);
            return (true);
        }

        const item_def *gloves = you.slot_item(EQ_GLOVES);
        if (coinflip() && gloves && gloves->cursed())
        {
            // If you are wearing cursed gloves, then Xom will give you
            // a ring.  Ha ha!
            god_speaks(GOD_XOM, _get_xom_speech("cursed gift").c_str());
            _xom_make_item(OBJ_JEWELLERY, get_random_ring_type(), power * 3);
            return (true);
        };

        const item_def *amulet = you.slot_item(EQ_AMULET);
        if (coinflip() && amulet && amulet->cursed())
        {
            // If you are wearing a cursed amulet, then Xom will give
            // you an amulet.  Ha ha!
            god_speaks(GOD_XOM, _get_xom_speech("cursed gift").c_str());
            _xom_make_item(OBJ_JEWELLERY, get_random_amulet_type(), power * 3);
            return (true);
        };

        const item_def *left_ring = you.slot_item(EQ_LEFT_RING);
        const item_def *right_ring = you.slot_item(EQ_RIGHT_RING);
        if (coinflip() && ((left_ring && left_ring->cursed())
                           || (right_ring && right_ring->cursed())))
        {
            // If you are wearing a cursed ring, then Xom will give you
            // a ring.  Ha ha!
            god_speaks(GOD_XOM, _get_xom_speech("ring gift").c_str());
            _xom_make_item(OBJ_JEWELLERY, get_random_ring_type(), power * 3);
            return (true);
        }

        if (one_chance_in(5) && weapon)
        {
            // Xom will give you a wielded item of a type different from
            // what you are currently wielding.
            god_speaks(GOD_XOM, _get_xom_speech("weapon gift").c_str());

            const object_class_type objtype =
                _get_unrelated_wield_class(weapon->base_type);

            if (x_chance_in_y(power, 256))
                _xom_acquirement(objtype);
            else
                _xom_make_item(objtype, OBJ_RANDOM, power * 3);
            return (true);
        }
    }

    return (false);
}

static bool _xom_give_item(int power)
{
    if (_xom_annoyance_gift(power))
        return (true);

    const item_def *cloak = you.slot_item(EQ_CLOAK);
    if (coinflip() && cloak && cloak->cursed())
    {
        // If you are wearing a cursed cloak, then Xom will give you a
        // cloak or body armour.  Ha ha!
        god_speaks(GOD_XOM, _get_xom_speech("armour gift").c_str());
        _xom_make_item(OBJ_ARMOUR,
                       one_chance_in(10) ? ARM_CLOAK :
                                get_random_body_armour_type(power * 2),
                       power * 3);
        return (true);
    }

    god_speaks(GOD_XOM, _get_xom_speech("general gift").c_str());

    // There are two kinds of Xom gifts: acquirement and random object.
    // The result from acquirement is very good (usually as good or
    // better than random object), and it is sometimes tuned to the
    // player's skills and nature.  Being tuned to the player's skills
    // and nature is not very Xomlike...
    if (x_chance_in_y(power, 256))
    {
        const object_class_type types[] = {
            OBJ_WEAPONS, OBJ_ARMOUR, OBJ_JEWELLERY,  OBJ_BOOKS,
            OBJ_STAVES,  OBJ_FOOD,   OBJ_MISCELLANY, OBJ_GOLD
        };
        god_acting gdact(GOD_XOM);
        _xom_acquirement(RANDOM_ELEMENT(types));
    }
    else
    {
        // Random-type random object.
        _xom_make_item(OBJ_RANDOM, OBJ_RANDOM, power * 3);
    }

    more();
    return (true);
}

static bool _choose_mutatable_monster(const monsters* mon)
{
    return (mon->alive() && mon->can_safely_mutate()
            && !mons_is_submerged(mon));
}

static bool _is_chaos_upgradeable(const item_def &item,
                                  const monsters* mon)
{
    // Since Xom is a god he is capable of changing randarts, but not
    // other artifacts.
    if (is_artefact(item) && !is_random_artefact(item))
       return (false);

    // Only upgrade permanent items, since the player should get a
    // chance to use the item if s/he can defeat the monster.
    if (item.flags & ISFLAG_SUMMONED)
        return (false);

    // Blessed blades are protected, being gifts from good gods.
    if (is_blessed_blade(item))
        return (false);

    // God gifts from good gods are protected.  Also, Beogh hates all
    // the other gods, so he'll protect his gifts as well.
    if (item.orig_monnum < 0)
    {
        god_type iorig = static_cast<god_type>(-item.orig_monnum);
        if ((iorig > GOD_NO_GOD && iorig < NUM_GODS)
            && (is_good_god(iorig) || iorig == GOD_BEOGH))
        {
            return (false);
        }
    }

    // Leave branded items alone, since this is supposed to be an
    // upgrade.
    if (item.base_type == OBJ_MISSILES)
    {
        // Don't make boulders or throwing nets of chaos.
        if (item.sub_type == MI_LARGE_ROCK
            || item.sub_type == MI_THROWING_NET)
        {
            return (false);
        }

        if (get_ammo_brand(item) == SPMSL_NORMAL)
            return (true);
    }
    else
    {
        // If the weapon is a launcher, and the monster is either out
        // of ammo or is carrying javelins, then don't bother upgrading
        // the launcher.
        if (is_range_weapon(item)
            && (mon->inv[MSLOT_MISSILE] == NON_ITEM
                || !has_launcher(mitm[mon->inv[MSLOT_MISSILE]])))
        {
            return (false);
        }

        if (get_weapon_brand(item) == SPWPN_NORMAL)
            return (true);
    }

    return (false);
}

static bool _choose_chaos_upgrade(const monsters* mon)
{
    // Only choose monsters that will attack.
    if (!mon->alive() || mons_attitude(mon) != ATT_HOSTILE
        || mons_is_fleeing(mon) || mons_is_panicking(mon))
    {
       return (false);
    }

    if (mons_itemuse(mon) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    // Holy beings are presumably protected by another god, unless
    // they're gifts from a chaotic god.
    if (mons_is_holy(mon) && !is_chaotic_god(mon->god))
        return (false);

    // God gifts from good gods will be protected by their god from
    // being given chaos weapons, while other gods won't mind the help
    // in their servants' killing the player.
    if (is_good_god(mon->god))
       return (false);

    // Beogh presumably doesn't want Xom messing with his orcs, even if
    // it would give them a better weapon.
    if (mons_species(mon->type) == MONS_ORC
        && (mons_class_flag(mon->type, M_PRIEST) || coinflip()))
    {
        return (false);
    }

    mon_inv_type slots[] = {MSLOT_WEAPON, MSLOT_ALT_WEAPON, MSLOT_MISSILE};

    // NOTE: Code assumes that the monster will only be carrying one
    // missile launcher at a time.
    bool special_launcher = false;
    for (int i = 0; i < 3; ++i)
    {
        const mon_inv_type slot = slots[i];
        const int          midx = mon->inv[slot];

        if (midx == NON_ITEM)
            continue;
        const item_def &item(mitm[midx]);

        // The monster already has a chaos weapon.  Give the upgrade to
        // a different monster.
        if (is_chaotic_item(item))
            return (false);

        if (_is_chaos_upgradeable(item, mon))
        {
            if (item.base_type != OBJ_MISSILES)
                return (true);

            // If, for some weird reason, a monster is carrying a bow
            // and javelins, then branding the javelins is okay, since
            // they won't be fired by the bow.
            if (!special_launcher || !has_launcher(item))
                return (true);
        }

        if (is_range_weapon(item))
        {
            // If the launcher alters its ammo, then branding the
            // monster's ammo won't be an upgrade.
            int brand = get_weapon_brand(item);
            if (brand == SPWPN_FLAME || brand == SPWPN_FROST
                || brand == SPWPN_VENOM)
            {
                special_launcher = true;
            }
        }
    }

    return (false);
}

static void _do_chaos_upgrade(item_def &item, const monsters* mon)
{
    ASSERT(item.base_type == OBJ_MISSILES
           || item.base_type == OBJ_WEAPONS);
    ASSERT(!is_unrandom_artefact(item) && !is_fixed_artefact(item));

    bool seen = false;
    if (mon && you.can_see(mon) && item.base_type == OBJ_WEAPONS)
    {
        seen = true;

        description_level_type desc = mons_friendly(mon) ? DESC_CAP_YOUR :
                                                           DESC_CAP_THE;
        std::string msg = apostrophise(mon->name(desc));

        msg += " ";

        msg += item.name(DESC_PLAIN, false, false, false);

        msg += " is briefly surrounded by a scintillating aura of "
               "random colours.";

        mpr(msg.c_str());
    }

    const int brand = (item.base_type == OBJ_WEAPONS) ? (int) SPWPN_CHAOS
                                                      : (int) SPMSL_CHAOS;

    if (is_random_artefact(item))
    {
        randart_set_property(item, RAP_BRAND, brand);

        if (seen)
            randart_wpn_learn_prop(item, RAP_BRAND);
    }
    else
    {
        item.special = brand;

        if (seen)
            set_ident_flags(item, ISFLAG_KNOW_TYPE);

        // Make sure it's visibly special.
        if (!(item.flags & ISFLAG_COSMETIC_MASK))
            item.flags |= ISFLAG_GLOWING;

        // Make the pluses more like a randomly generated ego item.
        item.plus  += random2(5);
        item.plus2 += random2(5);
    }
}

static monster_type _xom_random_demon(int sever, bool use_greater_demons = true)
{
    const int roll = random2(1000 - (MAX_PIETY - sever) * 3);
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "_xom_random_demon(); sever = %d, roll: %d",
         sever, roll);
#endif
    const demon_class_type dct =
        (roll >= 850) ? DEMON_GREATER :
        (roll >= 340) ? DEMON_COMMON
                      : DEMON_LESSER;

    monster_type demon = MONS_PROGRAM_BUG;

    // Sometimes, send a holy warrior instead.
    if (dct == DEMON_GREATER && coinflip())
        demon = summon_any_holy_being(HOLY_BEING_WARRIOR);
    else
    {
        const demon_class_type dct2 =
            (!use_greater_demons && dct == DEMON_GREATER) ? DEMON_COMMON : dct;

        if (dct2 == DEMON_COMMON && one_chance_in(10))
            demon = MONS_CHAOS_SPAWN;
        else
            demon = summon_any_demon(dct2);
    }

    return (demon);
}

static bool _feat_is_deadly(dungeon_feature_type feat)
{
    if (you.airborne())
        return (false);

    return (feat == DNGN_LAVA || feat == DNGN_DEEP_WATER && !you.can_swim());
}

static bool _player_is_dead()
{
    return (you.hp <= 0 || you.strength <= 0 || you.dex <= 0 || you.intel <= 0
            || _feat_is_deadly(grd(you.pos()))
            || you.did_escape_death());
}

static bool _xom_do_potion()
{
    potion_type pot = POT_HEALING;
    while (true)
    {
        pot = static_cast<potion_type>(
                random_choose(POT_HEALING, POT_HEAL_WOUNDS, POT_MAGIC,
                              POT_SPEED, POT_MIGHT, POT_INVISIBILITY,
                              POT_BERSERK_RAGE, POT_EXPERIENCE, -1));

        if (pot == POT_EXPERIENCE && !one_chance_in(6))
            pot = POT_BERSERK_RAGE;

        bool has_effect = true;
        // Don't pick something that won't have an effect.
        // Extending an existing effect is okay, though.
        switch (pot)
        {
        case POT_HEALING:
            if (you.rotting || you.disease || you.duration[DUR_CONF]
                || you.duration[DUR_POISONING])
            {
                break;
            }
            // else fall through
        case POT_HEAL_WOUNDS:
            if (you.hp == you.hp_max && player_rotted() == 0)
                has_effect = false;
            break;
        case POT_MAGIC:
            if (you.magic_points == you.max_magic_points)
                has_effect = false;
            break;
        case POT_BERSERK_RAGE:
            if (!you.can_go_berserk(false))
                has_effect = false;
            break;
        case POT_EXPERIENCE:
            if (you.experience_level == 27)
                has_effect = false;
            break;
        default:
            break;
        }
        if (has_effect)
            break;
    }

    god_speaks(GOD_XOM, _get_xom_speech("potion effect").c_str());

    if (pot == POT_BERSERK_RAGE)
        you.berserk_penalty = NO_BERSERK_PENALTY;

    // Take a note.
    std::string potion_msg = "potion effect ";
    switch (pot)
    {
    case POT_HEALING:       potion_msg += "(healing)"; break;
    case POT_HEAL_WOUNDS:   potion_msg += "(heal wounds)"; break;
    case POT_MAGIC:         potion_msg += "(magic)"; break;
    case POT_SPEED:         potion_msg += "(speed)"; break;
    case POT_MIGHT:         potion_msg += "(might)"; break;
    case POT_INVISIBILITY:  potion_msg += "(invisibility)"; break;
    case POT_BERSERK_RAGE:  potion_msg += "(berserk)"; break;
    case POT_EXPERIENCE:    potion_msg += "(experience)"; break;
    default:                potion_msg += "(other)"; break;
    }
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, potion_msg.c_str()), true);

    potion_effect(pot, 150);

    return (true);
}

static bool _xom_confuse_monsters(int sever)
{
    bool rc = false;
    monsters *monster;
    for (unsigned i = 0; i < MAX_MONSTERS; ++i)
    {
        monster = &menv[i];

        if (monster->type == -1 || !mons_near(monster)
            || mons_wont_attack(monster)
            || !mons_class_is_confusable(monster->type)
            || one_chance_in(20))
        {
            continue;
        }

        if (monster->add_ench(mon_enchant(ENCH_CONFUSION, 0,
                                          KC_FRIENDLY, random2(sever))))
        {
            // Only give this message once.
            if (!rc)
                god_speaks(GOD_XOM, _get_xom_speech("confusion").c_str());

            simple_monster_message(monster, " looks rather confused.");
            rc = true;
        }
    }

    if (rc == true)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "confuse monster(s)"),
                  true);
    }
    return (rc);
}

static bool _xom_send_allies(int sever)
{
    bool rc = false;
    // The number of allies is dependent on severity, though heavily
    // randomized.
    int numdemons = sever;
    for (int i = 0; i < 3; i++)
        numdemons = random2(numdemons + 1);
    numdemons = std::min(numdemons + 2, 16);

    int numdifferent = 0;

    // If we have a mix of demons and non-demons, there's a chance
    // that one or both of the factions may be hostile.
    int hostiletype = random_choose_weighted(3, 0,  // both friendly
                                             4, 1,  // one hostile
                                             4, 2,  // other hostile
                                             1, 3,  // both hostile
                                             0);

    std::vector<bool> is_demonic(numdemons, false);
    std::vector<int> summons(numdemons);

    int num_actually_summoned = 0;

    for (int i = 0; i < numdemons; ++i)
    {
        monster_type monster = _xom_random_demon(sever);

        summons[i] =
            create_monster(
                mgen_data(monster, BEH_FRIENDLY,
                          3, MON_SUMM_AID,
                          you.pos(), MHITYOU,
                          MG_FORCE_BEH, GOD_XOM));

        if (summons[i] != -1)
        {
            num_actually_summoned++;
            is_demonic[i] = (mons_class_holiness(monster) == MH_DEMONIC);

            // If it's not a demon, Xom got it someplace else, so use
            // different messages below.
            if (!is_demonic[i])
                numdifferent++;
        }
    }

    if (num_actually_summoned)
    {
        const bool only_holy    = (numdifferent == num_actually_summoned);
        const bool only_demonic = (numdifferent == 0);

        if (only_holy)
        {
            god_speaks(GOD_XOM,
                       _get_xom_speech("multiple holy summons").c_str());
        }
        else if (only_demonic)
        {
            god_speaks(GOD_XOM,
                       _get_xom_speech("multiple summons").c_str());
        }
        else
        {
            god_speaks(GOD_XOM,
                       _get_xom_speech("multiple mixed summons").c_str());
        }

        // If we have only non-demons, there's a chance that they
        // may be hostile.
        if (only_holy && one_chance_in(4))
            hostiletype = 2;
        // If we have only demons, they'll always be friendly.
        else if (only_demonic)
            hostiletype = 0;

        for (int i = 0; i < numdemons; ++i)
        {
            if (summons[i] == -1)
                continue;

            monsters *mon = &menv[summons[i]];

            if (hostiletype != 0)
            {
                // Mark factions hostile as appropriate.
                if (hostiletype == 3
                    || (is_demonic[i] && hostiletype == 1)
                    || (!is_demonic[i] && hostiletype == 2))
                {
                    mon->attitude = ATT_HOSTILE;
                    behaviour_event(mon, ME_ALERT, MHITYOU);
                }
            }

            player_angers_monster(mon);
        }

        // Take a note.
        static char summ_buf[80];
        snprintf(summ_buf, sizeof(summ_buf), "summons %d %s%s%s",
                 num_actually_summoned,
                 hostiletype == 0 ? "friendly " :
                 hostiletype == 3 ? "hostile " : "",
                 only_demonic ? "demon" : "monster",
                 num_actually_summoned > 1 ? "s" : "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, summ_buf), true);

        rc = true;
    }

    return (rc);
}

static bool _xom_send_one_ally(int sever)
{
    bool rc = false;

    const monster_type mon = _xom_random_demon(sever);
    const bool is_demonic = (mons_class_holiness(mon) == MH_DEMONIC);

    // If we have a non-demon, Xom got it someplace else, so use
    // different messages below.
    bool different = !is_demonic;

    beh_type beha = BEH_FRIENDLY;

    // There's a chance that a non-demon may be hostile.
    if (different && one_chance_in(4))
        beha = BEH_HOSTILE;

    const int summons =
        create_monster(
            mgen_data(mon, beha, 6, MON_SUMM_AID, you.pos(), MHITYOU,
                      MG_FORCE_BEH, GOD_XOM));

    if (summons != -1)
    {
        if (different)
            god_speaks(GOD_XOM, _get_xom_speech("single holy summon").c_str());
        else
            god_speaks(GOD_XOM, _get_xom_speech("single summon").c_str());

        player_angers_monster(&menv[summons]);

        // Take a note.
        static char summ_buf[80];
        snprintf(summ_buf, sizeof(summ_buf), "summons %s %s",
                 beha == BEH_FRIENDLY ? "friendly" : "hostile",
                 menv[summons].name(DESC_PLAIN).c_str());
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, summ_buf), true);

        rc = true;
    }

    return (rc);
}

static bool _xom_polymorph_nearby_monster(bool helpful)
{
    bool rc = false;
    if (there_are_monsters_nearby(false, false))
    {
        monsters *mon = choose_random_nearby_monster(0,
                                                     _choose_mutatable_monster);
        if (mon)
        {
            const char* lookup = (helpful ? "good monster polymorph"
                                          : "bad monster polymorph");
            god_speaks(GOD_XOM, _get_xom_speech(lookup).c_str());

            bool see_old = you.can_see(mon);
            std::string old_name = mon->full_name(DESC_PLAIN);

            if (one_chance_in(8) && !mons_is_shapeshifter(mon))
            {
                mon->add_ench(one_chance_in(3) ? ENCH_GLOWING_SHAPESHIFTER
                                               : ENCH_SHAPESHIFTER);
            }

            const bool powerup = !(mons_wont_attack(mon) ^ helpful);
            monster_polymorph(mon, RANDOM_MONSTER,
                              powerup ? PPT_MORE : PPT_LESS);

            bool see_new = you.can_see(mon);

            if (see_old || see_new)
            {
                std::string new_name = mon->full_name(DESC_PLAIN);
                if (!see_old)
                    old_name = "something unseen";
                else if (!see_new)
                    new_name = "something unseen";

                // Take a note.
                static char poly_buf[120];
                snprintf(poly_buf, sizeof(poly_buf), "polymorph %s -> %s",
                         old_name.c_str(), new_name.c_str());

                std::string poly = poly_buf;
#ifdef NOTE_DEBUG_XOM
                poly += " (";
                poly += (powerup ? "upgrade" : "downgrade");
                poly += ")";
#endif
                take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, poly.c_str()),
                          true);
            }
            rc = true;
        }
    }

    return (rc);
}

static void _confuse_monster(monsters mons, int sever)
{
    if (!mons_class_is_confusable(mons.type))
        return;

    const bool was_confused = mons.confused();
    if (mons.add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_FRIENDLY,
                                  random2(sever))))
    {
        if (was_confused)
            simple_monster_message(&mons, " looks rather more confused.");
        else
            simple_monster_message(&mons, " looks rather confused.");
    }
}

static bool _swap_monsters(monsters *m1, monsters *m2)
{
    monsters& mon1(*m1);
    monsters& mon2(*m2);

    const bool mon1_caught = mons_is_caught(&mon1);
    const bool mon2_caught = mons_is_caught(&mon2);

    const coord_def mon1_pos = mon1.pos();
    const coord_def mon2_pos = mon2.pos();

    if (!mon2.is_habitable(mon1_pos) || !mon1.is_habitable(mon2_pos))
        return (false);

    // Make submerged monsters unsubmerge.
    mon1.del_ench(ENCH_SUBMERGED);
    mon2.del_ench(ENCH_SUBMERGED);

    mgrd(mon1_pos) = mon2.mindex();
    mon1.moveto(mon2_pos);
    mgrd(mon2_pos) = mon1.mindex();
    mon2.moveto(mon1_pos);

    if (mon1_caught && !mon2_caught)
    {
        check_net_will_hold_monster(&mon2);
        mon1.del_ench(ENCH_HELD, true);

    }
    else if (mon2_caught && !mon1_caught)
    {
        check_net_will_hold_monster(&mon1);
        mon2.del_ench(ENCH_HELD, true);
    }

    return (true);
}

// Swap places with a random monster and, depending on severity, also
// between monsters. This can be pretty bad if there are a lot of
// hostile monsters around.
static bool _xom_rearrange_pieces(int sever)
{
    if (player_stair_delay())
        return (false);

    std::vector<monsters *> mons;
    for (unsigned i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters* m = &menv[i];

        if (!m->alive())
            continue;

        if (!see_grid(m->pos()))
            continue;

        mons.push_back(m);
    }
    if (mons.empty())
        return (false);

    god_speaks(GOD_XOM, _get_xom_speech("rearrange the pieces").c_str());

    const int num_mons = mons.size();

    // Swap places with a random monster.
    monsters *mon = mons[random2(num_mons)];
    swap_with_monster(mon);

    // Sometimes confuse said monster.
    if (coinflip())
        _confuse_monster(*mon, sever);

    if (num_mons > 1 && x_chance_in_y(sever, 70))
    {
        bool did_message = false;
        const int max_repeats = std::min(num_mons / 2, 8);
        const int repeats     = std::min(random2(sever / 10) + 1, max_repeats);
        for (int i = 0; i < repeats; ++i)
        {
            const int mon1 = random2(num_mons);
            int mon2 = mon1;
            while (mon1 == mon2)
                mon2 = random2(num_mons);

            if (_swap_monsters(mons[mon1], mons[mon2]))
            {
                if (!did_message)
                {
                    mpr("Some monsters swap places.");
                    did_message = true;
                }
                if (one_chance_in(3))
                    _confuse_monster(*mons[mon1], sever);
                if (one_chance_in(3))
                    _confuse_monster(*mons[mon2], sever);
            }
        }
    }
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "swap monsters"), true);

    return (true);
}

static bool _xom_give_mutations(bool good)
{
    bool rc = false;

    if (you.can_safely_mutate()
        && player_mutation_level(MUT_MUTATION_RESISTANCE) < 3)
    {
        const char* lookup = (good ? "good mutations" : "random mutations");
        god_speaks(GOD_XOM, _get_xom_speech(lookup).c_str());

        const int num_tries = random2(4) + 1;

        static char mut_buf[80];
        snprintf(mut_buf, sizeof(mut_buf), "give %smutation%s",
#ifdef NOTE_DEBUG_XOM
                 good ? "good " : "random ",
#else
                 "",
#endif
                 num_tries > 1 ? "s" : "");

        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, mut_buf), true);

        mpr("Your body is suffused with distortional energy.");

        set_hp(1 + random2(you.hp), false);
        deflate_hp(you.hp_max / 2, true);

        bool failMsg = true;

        for (int i = num_tries; i > 0; --i)
        {
            if (mutate(good ? RANDOM_GOOD_MUTATION : RANDOM_XOM_MUTATION,
                       failMsg, false, true, false, false,
                       good || !_xom_feels_nasty()))
            {
                rc = true;
            }
            else
                failMsg = false;
        }
    }

    return (rc);
}

// Summons a permanent ally.
static bool _xom_send_major_ally(int sever)
{
    bool rc = false;
    const monster_type mon = _xom_random_demon(sever);
    const bool is_demonic = (mons_class_holiness(mon) == MH_DEMONIC);

    beh_type beha = BEH_FRIENDLY;

    // There's a chance that a non-demon may be hostile.
    if (!is_demonic && one_chance_in(4))
        beha = BEH_HOSTILE;

    const int summons =
        create_monster(
            mgen_data(_xom_random_demon(sever, one_chance_in(8)), beha,
                      0, 0, you.pos(), MHITYOU, MG_FORCE_BEH, GOD_XOM));

    if (summons != -1)
    {
        if (is_demonic)
        {
            god_speaks(GOD_XOM,
                       _get_xom_speech("single major demon summon").c_str());
        }
        else
        {
            god_speaks(GOD_XOM,
                       _get_xom_speech("single major holy summon").c_str());
        }

        player_angers_monster(&menv[summons]);

        // Take a note.
        static char summ_buf[80];
        snprintf(summ_buf, sizeof(summ_buf), "sends permanent %s %s",
                 beha == BEH_FRIENDLY ? "friendly" : "hostile",
                 menv[summons].name(DESC_PLAIN).c_str());
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, summ_buf), true);

        rc = true;
    }

    return (rc);
}

static bool _xom_throw_divine_lightning()
{
    if (!player_in_a_dangerous_place())
        return (false);

    // Make sure there's at least one enemy within the lightning radius.
    bool found_hostile = false;
    for (radius_iterator ri(you.pos(), 2, true, true, true); ri; ++ri)
    {
        if (monsters* mon = monster_at(*ri))
        {
            if (!mons_wont_attack(mon))
            {
                found_hostile = true;
                break;
            }
        }
    }

    // No hostiles within radius.
    if (!found_hostile)
        return (false);

    bool protection = false;
    if (you.hp <= random2(201))
    {
        you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 1;
        protection = true;
    }

    god_speaks(GOD_XOM, "The area is suffused with divine lightning!");

    bolt beam;

    beam.flavour      = BEAM_ELECTRICITY;
    beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(3, 30);
    beam.target       = you.pos();
    beam.name         = "blast of lightning";
    beam.colour       = LIGHTCYAN;
    beam.thrower      = KILL_MISC;
    beam.beam_source  = NON_MONSTER;
    beam.aux_source   = "Xom's lightning strike";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    beam.explode();

    if (you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION])
    {
        mpr("Your divine protection wanes.");
        you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 0;
    }

    // Don't accidentally kill the player when doing a good act.
    if (you.escaped_death_cause == KILLED_BY_WILD_MAGIC
        && you.escaped_death_aux == "Xom's lightning strike")
    {
        you.hp = 1;
        you.reset_escaped_death();
    }

    // Take a note.
    static char lightning_buf[80];
    snprintf(lightning_buf, sizeof(lightning_buf),
             "divine lightning%s", protection ? " (protected)" : "");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, lightning_buf), true);

    return (true);
}

// The nicer stuff.  Note: these things are not necessarily nice.
static bool _xom_is_good(int sever, int tension)
{
    bool done = false;

    // Did Xom (already) kill the player?
    if (_player_is_dead())
        return (true);

    god_acting gdact(GOD_XOM);

    // This series of random calls produces a poisson-looking
    // distribution: initial hump, plus a long-ish tail.

    // Don't make the player go berserk etc. if there's no danger.
    if (tension > random2(3) && x_chance_in_y(2, sever))
        done = _xom_do_potion();
    else if (x_chance_in_y(3, sever))
    {
        // There are a lot less non-tension spells than tension ones,
        // so use them more rarely.
        if (tension > 0 || one_chance_in(3))
            done = _xom_makes_you_cast_random_spell(sever, tension);
    }
    else if (x_chance_in_y(4, sever))
        done = _xom_confuse_monsters(sever);
    // It's pointless to send in help if there's no danger.
    else if (tension > random2(5) && x_chance_in_y(5, sever))
        done = _xom_send_one_ally(sever);
    else if (x_chance_in_y(6, sever))
    {
        _xom_give_item(sever);
        done = true;
    }
    // It's pointless to send in help if there's no danger.
    else if (tension > random2(10) && x_chance_in_y(7, sever))
        done = _xom_send_allies(sever);
    else if (x_chance_in_y(8, sever))
        done = _xom_polymorph_nearby_monster(true);
    else if (tension > 0 && x_chance_in_y(9, sever))
        done = _xom_rearrange_pieces(sever);
    else if (random2(tension) < 15 && x_chance_in_y(10, sever))
    {
        _xom_give_item(sever);
        done = true;
    }
    else if (x_chance_in_y(11, sever) && you.level_type != LEVEL_ABYSS)
    {
        // Try something else if teleportation is impossible.
        if (scan_randarts(RAP_PREVENT_TELEPORTATION))
            return (false);

        // This is not very interesting if the level is already fully
        // explored (presumably cleared). Even then, it may occasionally
        // happen.
        const int explored = _exploration_estimate(true);
        if (explored >= 80 && x_chance_in_y(explored, 120))
            return (false);

        // The Xom teleportation train takes you on instant teleportation
        // to a few random areas, stopping randomly but most likely in
        // an area that is not dangerous to you.
        god_speaks(GOD_XOM, _get_xom_speech("teleportation journey").c_str());
        int count = 0;
        do
        {
            count++;
            you_teleport_now(false);
            more();
            if (one_chance_in(10))
                break;
        }
        while (x_chance_in_y(3, 4) || player_in_a_dangerous_place());
        maybe_update_stashes();

        // Take a note.
        static char tele_buf[80];
        snprintf(tele_buf, sizeof(tele_buf),
                 "XOM: %d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
                 player_in_a_dangerous_place() ? " (dangerous)" : // see below
#endif
                 "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, tension, tele_buf), true);
        done = true;
    }
    else if (random2(tension) < 5 && x_chance_in_y(12, sever))
    {
        // This can fail with radius 1, or in open areas.
        if (vitrify_area(random2avg(sever/4,2) + 1))
        {
            god_speaks(GOD_XOM, _get_xom_speech("vitrification").c_str());
            take_note(Note(NOTE_XOM_EFFECT, you.piety, tension,
                           "vitrification"), true);
            done = true;
        }
    }
    else if (random2(tension) < 5 && x_chance_in_y(13, sever)
             && x_chance_in_y(16, how_mutated()))
    {
        done = _xom_give_mutations(true);
    }
    // It's pointless to send in help if there's no danger.
    else if (tension > random2(15) && x_chance_in_y(14, sever))
        done = _xom_send_major_ally(sever);
    else if (tension > 0 && x_chance_in_y(15, sever))
        done = _xom_throw_divine_lightning();

    return (done);
}

// Is the equipment type usable, and is the slot empty?
static bool _could_wear_eq(equipment_type eq)
{
    if (!you_tran_can_wear(eq, true))
        return (false);

    return (you.slot_item(eq) == NULL);
}

static item_def* _tran_get_eq(equipment_type eq)
{
    if (you_tran_can_wear(eq, true))
        return (you.slot_item(eq));

    return (NULL);
}

// Which types of dungeon features are in view?
static void _get_in_view(FixedVector<bool, NUM_REAL_FEATURES>& in_view)
{
    in_view.init(false);

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        in_view[grd(*ri)] = true;
}

static void _xom_zero_miscast()
{
    std::vector<std::string> messages;
    std::vector<std::string> priority;

    std::vector<int> inv_items;
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        const item_def &item(you.inv[i]);
        if (is_valid_item(item) && !item_is_equipped(item)
            && !item_is_critical(item))
        {
            inv_items.push_back(i);
        }
    }

    // Assure that the messages vector has at least one element.
    messages.push_back("Nothing appears to happen... Ominous!");

    ///////////////////////////////////
    // Dungeon feature dependent stuff.

    FixedVector<bool, NUM_REAL_FEATURES> in_view;
    _get_in_view(in_view);

    if (in_view[DNGN_LAVA])
        messages.push_back("The lava spits out sparks!");

    if (in_view[DNGN_SHALLOW_WATER] || in_view[DNGN_DEEP_WATER])
    {
        messages.push_back("The water briefly bubbles.");
        messages.push_back("The water briefly swirls.");
        messages.push_back("The water briefly glows.");
    }

    if (in_view[DNGN_DEEP_WATER])
    {
        messages.push_back("From the corner of your eye you spot something "
                           "lurking in the deep water.");
    }

    if (in_view[DNGN_ORCISH_IDOL])
    {
        if (you.species == SP_HILL_ORC)
            priority.push_back("The idol of Beogh turns to glare at you.");
        else
            priority.push_back("The orcish idol turns to glare at you.");
    }

    if (in_view[DNGN_GRANITE_STATUE])
        priority.push_back("The granite statue turns to stare at you.");

    if (in_view[DNGN_WAX_WALL])
        priority.push_back("The wax wall pulsates ominously.");

    if (in_view[DNGN_CLEAR_ROCK_WALL] || in_view[DNGN_CLEAR_STONE_WALL]
        || in_view[DNGN_CLEAR_PERMAROCK_WALL])
    {
        messages.push_back("Dim shapes swim through the translucent wall.");
    }

    if (in_view[DNGN_GREEN_CRYSTAL_WALL])
        messages.push_back("Dim shapes swim through the green crystal wall.");

    if (in_view[DNGN_METAL_WALL])
        messages.push_back("Tendrils of electricity crawl over the metal "
                           "wall!");

    if (in_view[DNGN_FOUNTAIN_BLUE] || in_view[DNGN_FOUNTAIN_SPARKLING])
    {
        priority.push_back("The water in the fountain briefly bubbles.");
        priority.push_back("The water in the fountain briefly swirls.");
        priority.push_back("The water in the fountain briefly glows.");
    }

    if (in_view[DNGN_DRY_FOUNTAIN_BLUE]
        || in_view[DNGN_DRY_FOUNTAIN_SPARKLING]
        || in_view[DNGN_PERMADRY_FOUNTAIN])
    {
        priority.push_back("Water briefly sprays from the dry fountain.");
        priority.push_back("Dust puffs up from the dry fountain.");
    }

    if (in_view[DNGN_STONE_ARCH])
        priority.push_back("The stone arch briefly shows a sunny meadow on "
                           "the other side.");

    const dungeon_feature_type feat = grd(you.pos());

    if (!grid_is_solid(feat) && grid_stair_direction(feat) == CMD_NO_CMD
        && !grid_is_trap(feat) && feat != DNGN_STONE_ARCH
        && feat != DNGN_OPEN_DOOR && feat != DNGN_ABANDONED_SHOP)
    {
        const std::string feat_name =
            feature_description(you.pos(), false, DESC_CAP_THE, false);

        if (you.airborne())
        {
            // Kenku fly a lot, so don't put airborne messages into the
            // priority vector for them.
            std::vector<std::string>* vec;
            if (you.species == SP_KENKU)
                vec = &messages;
            else
                vec = &priority;

            vec->push_back(feat_name
                           + " seems to fall away from under you!");
            vec->push_back(feat_name
                           + " seems to rush up at you!");

            if (grid_is_water(feat))
            {
                priority.push_back("Something invisible splashes into the "
                                   "water beneath you!");
            }
        }
        else if (grid_is_water(feat))
        {
            priority.push_back("The water briefly recedes away from you.");
            priority.push_back("Something invisible splashes into the water "
                               "beside you!");
        }
    }

    if (!grid_destroys_items(feat) && !grid_is_solid(feat)
        && inv_items.size() > 0)
    {
        int idx = inv_items[random2(inv_items.size())];

        const item_def &item(you.inv[idx]);

        std::string name;
        if (item.quantity == 1)
            name = item.name(DESC_CAP_YOUR, false, false, false);
        else
        {
            name  = "One of ";
            name += item.name(DESC_NOCAP_YOUR, false, false, false);
        }
        messages.push_back(name + " falls out of your pack, then "
                           "immediately jumps back in!");
    }

    ////////////////////////////////////////////
    // Body, player spcies, transformations, etc

    const int transform = you.attribute[ATTR_TRANSFORMATION];

    if (you.species == SP_MUMMY && you_tran_can_wear(EQ_BODY_ARMOUR))
    {
        messages.push_back("You briefly get tangled in your bandages.");
        if (!you.airborne() && !you.swimming())
            messages.push_back("You trip over your bandages.");
    }

    if (transform != TRAN_SPIDER)
    {
        std::string str = "A monocle briefly appears over your ";
        str += coinflip() ? "right" : "left";
        str += " eye.";
        messages.push_back(str);
    }

    if (!player_genus(GENPC_DRACONIAN) && you.species != SP_MUMMY
        && (transform == TRAN_NONE || transform == TRAN_BLADE_HANDS))
    {
        messages.push_back("Your eyebrows briefly feel incredibly bushy.");
        messages.push_back("Your eyebrows wriggle.");
    }

    if (you.species != SP_NAGA
        && (you.species != SP_MERFOLK || !player_is_swimming())
        && !you.airborne())
    {
        messages.push_back("You do an impromptu tapdance.");
    }

    ///////////////////////////
    // Equipment related stuff.
    item_def* item;

    if (_could_wear_eq(EQ_WEAPON))
    {
        std::string str = "A fancy cane briefly appears in your ";
        str += you.hand_name(false);
        str += ".";

        messages.push_back(str);
    }

    if (_tran_get_eq(EQ_CLOAK) != NULL)
        messages.push_back("Your cloak billows in an unfelt wind.");

    if ((item = _tran_get_eq(EQ_HELMET)))
    {
        std::string str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " leaps into the air, briefly spins, then lands back on "
               "your head!";

        messages.push_back(str);
    }

    if ((item = _tran_get_eq(EQ_BOOTS)) && item->sub_type == ARM_BOOTS
        && !you.cannot_act())
    {
        std::string name = item->name(DESC_BASENAME, false, false, false);
        name = replace_all(name, "pair of ", "");

        std::string str = "You compulsively click the heels of your ";
        str += name;
        str += " together three times.";
    }

    if ((item = _tran_get_eq(EQ_SHIELD)))
    {
        std::string str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " spins!";

        messages.push_back(str);

        str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " briefly flashes a lurid colour!";
        messages.push_back(str);
    }

    if ((item = _tran_get_eq(EQ_BODY_ARMOUR)))
    {
        std::string str;
        std::string name = item->name(DESC_BASENAME, false, false, false);

        if (name.find("dragon") != std::string::npos)
        {
            str  = "The scales on your ";
            str += name;
            str += " wiggle briefly.";
        }
        else if (item->sub_type == ARM_ANIMAL_SKIN)
        {
            str  = "The fur on your ";
            str += name;
            str += " grows longer at an alarming rate, then retracts back "
                   "to normal.";
        }
        else if (item->sub_type == ARM_LEATHER_ARMOUR)
        {
            str  = "Your ";
            str += name;
            str += " briefly grows fur, then returns to normal.";
        }
        else if (item->sub_type == ARM_ROBE)
        {
            str  = "You briefly become tangled in your ";
            str += pluralise(name);
            str += ".";
        }
        else if (item->sub_type >= ARM_RING_MAIL
                 && item->sub_type <= ARM_PLATE_MAIL)
        {
            str  = "Your ";
            str += name;
            str += " briefly appears rusty.";
        }

        if (!str.empty())
            messages.push_back(str);
    }

    ////////
    // Misc.
    if (inv_items.size() > 0)
    {
        int idx = inv_items[random2(inv_items.size())];

        item = &you.inv[idx];

        std::string name = item->name(DESC_CAP_YOUR, false, false, false);
        std::string verb = coinflip() ? "glow" : "vibrate";

        if (item->quantity == 1)
            verb += "s";

        messages.push_back(name + " briefly " + verb + ".");
    }

    if (!priority.empty() && coinflip())
        mpr(priority[random2(priority.size())].c_str());
    else
        mpr(messages[random2(messages.size())].c_str());
}

static void _get_hand_type(std::string &hand, bool &can_plural)
{
    hand       = "";
    can_plural = true;

    const int transform = you.attribute[ATTR_TRANSFORMATION];

    std::vector<std::string> hand_vec;
    std::vector<bool>        plural_vec;
    bool                     plural;

    hand_vec.push_back(you.hand_name(false, &plural));
    plural_vec.push_back(plural);

    if (you.species != SP_NAGA || transform_changed_physiology())
    {
        item_def* item;
        if ((item = _tran_get_eq(EQ_BOOTS)) && item->sub_type == ARM_BOOTS)
        {
            hand_vec.push_back("boot");
            plural = true;
        }
        else
            hand_vec.push_back(you.foot_name(false, &plural));
        plural_vec.push_back(plural);
    }

    if (transform == TRAN_SPIDER)
    {
        hand_vec.push_back("mandible");
        plural_vec.push_back(true);
    }
    else if (you.species != SP_MUMMY && !player_mutation_level(MUT_BEAK)
             || transform_changed_physiology())
    {
        hand_vec.push_back("nose");
        plural_vec.push_back(false);
    }

    if (transform == TRAN_BAT
        || you.species != SP_MUMMY && !transform_changed_physiology())
    {
        hand_vec.push_back("ear");
        plural_vec.push_back(true);
    }

    if (!transform_changed_physiology())
    {
        hand_vec.push_back("elbow");
        plural_vec.push_back(true);
    }

    ASSERT(hand_vec.size() == plural_vec.size());
    ASSERT(hand_vec.size() > 0);

    const unsigned int choice = random2(hand_vec.size());

    hand       = hand_vec[choice];
    can_plural = plural_vec[choice];
}

static void _xom_miscast(const int max_level, const bool nasty)
{
    ASSERT(max_level >= 0 && max_level <= 3);

    const char* speeches[4] = {
        "zero miscast effect",
        "minor miscast effect",
        "medium miscast effect",
        "major miscast effect"
    };

    const char* causes[4] = {
        "the mischief of Xom",
        "the capriciousness of Xom",
        "the capriciousness of Xom",
        "the severe capriciousness of Xom"
    };

    const char* speech_str = speeches[max_level];
    const char* cause_str  = causes[max_level];

    const int level = random2(max_level + 1);

    // Take a note.
    std::string desc = "miscast effect";
#ifdef NOTE_DEBUG_XOM
    static char level_buf[20];
    snprintf(level_buf, sizeof(level_buf), " level %d%s",
             level, (nasty ? " (nasty)" : ""));
    desc += level_buf;
#endif
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, desc.c_str()), true);

    if (level == 0 && one_chance_in(20))
    {
        god_speaks(GOD_XOM, _get_xom_speech(speech_str).c_str());
        _xom_zero_miscast();
        return;
    }

    std::string hand_str;
    bool        can_plural;

    _get_hand_type(hand_str, can_plural);

    // If Xom's not being nasty, then prevent spell miscasts from
    // killing the player.
    const int lethality_margin  = nasty ? 0 : random_range(1, 4);

    god_speaks(GOD_XOM, _get_xom_speech(speech_str).c_str());

    MiscastEffect(&you, -GOD_XOM, SPTYP_RANDOM, level, cause_str, NH_DEFAULT,
                  lethality_margin, hand_str, can_plural);
}

static bool _xom_lose_stats()
{
    stat_type stat = STAT_RANDOM;
    int       max  = 3;

    // Don't kill the player unless Xom is being nasty.
    if (!_xom_feels_nasty())
    {
        // Make sure not to lower strength so much that the player
        // will die once might wears off.
        char      vals[3] =
            {you.strength - (you.duration[DUR_MIGHT] ? 5 : 0),
             you.dex, you.intel};
        stat_type types[3] = {STAT_STRENGTH, STAT_DEXTERITY,
                              STAT_INTELLIGENCE};
        int tries = 0;
        do
        {
            int idx = random2(3);
            stat = types[idx];
            max  = std::min(3, vals[idx] - 1);
        }
        while (max < 2 && (++tries < 30));

        if (tries >= 30)
            return (false);
    }

    god_speaks(GOD_XOM, _get_xom_speech("lose stats").c_str());
    const int loss = 1 + random2(max);
    lose_stat(stat, loss, true, "the vengeance of Xom" );

    // Take a note.
    static char stat_buf[80];
    snprintf(stat_buf, sizeof(stat_buf), "stat loss: -%d %s (%d/%d)",
             loss, (stat == STAT_STRENGTH  ? "Str" :
                    stat == STAT_DEXTERITY ? "Dex" : "Int"),
             (stat == STAT_STRENGTH  ? you.strength :
              stat == STAT_DEXTERITY ? you.dex : you.intel),
             (stat == STAT_STRENGTH  ? you.max_strength :
              stat == STAT_DEXTERITY ? you.max_dex : you.max_intel));

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, stat_buf), true);

    return (true);
}

static bool _xom_chaos_upgrade_nearby_monster()
{
    bool rc = false;

    monsters *mon = choose_random_nearby_monster(0, _choose_chaos_upgrade);

    if (!mon)
        return (false);

    god_speaks(GOD_XOM, _get_xom_speech("chaos upgrade").c_str());

    mon_inv_type slots[] = {MSLOT_WEAPON, MSLOT_ALT_WEAPON, MSLOT_MISSILE};

    for (int i = 0; i < 3 && !rc; ++i)
    {
        item_def* const item = mon->mslot_item(slots[i]);
        if (item && _is_chaos_upgradeable(*item, mon))
        {
            _do_chaos_upgrade(*item, mon);
            rc = true;
        }
    }
    ASSERT(rc);

    // Wake the monster up.
    behaviour_event(mon, ME_ALERT, MHITYOU);

    if (rc)
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "chaos upgrade"), true);

    return (rc);
}

static bool _xom_player_confusion_effect(int sever)
{
    bool rc = false;

    // Looks like this will *always* succeed?
    if (confuse_player(random2(sever) + 1, false))
    {
        // FIXME: Message order is a bit off here.
        god_speaks(GOD_XOM, _get_xom_speech("confusion").c_str());
        rc = true;

        // Sometimes Xom gets carried away and starts confusing
        // other creatures too.
        bool mons_too = false;
        if (coinflip())
        {
            for (unsigned i = 0; i < MAX_MONSTERS; ++i)
            {
                monsters* const monster = &menv[i];

                if (!monster->alive()
                    || !mons_near(monster)
                    || !mons_class_is_confusable(monster->type)
                    || one_chance_in(20))
                {
                    continue;
                }

                if (monster->add_ench(mon_enchant(ENCH_CONFUSION, 0,
                                                  KC_FRIENDLY, random2(sever))))
                {
                    simple_monster_message(monster,
                                           " looks rather confused.");
                }
                mons_too = true;
            }
        }

        // Take a note.
        std::string conf_msg = "confusion";
        if (mons_too)
            conf_msg += " (+ monsters)";
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, conf_msg.c_str()), true);
    }

    return (rc);
}

static bool _valid_floor_grid(coord_def pos)
{
    if (!in_bounds(pos))
        return (false);

    return (grd(pos) == DNGN_FLOOR);
}

static bool _move_stair(coord_def stair_pos, bool away)
{
    dungeon_feature_type feat = grd(stair_pos);
    ASSERT(grid_stair_direction(feat) != CMD_NO_CMD);

    coord_def begin, towards;

    bool stairs_moved = false;
    if (away)
    {
        // If the staircase starts out under the player, first shove it
        // onto a neighbouring grid.
        if (stair_pos == you.pos())
        {
            coord_def new_pos(stair_pos);
            // Loop twice through all adjacent grids. In the first round,
            // only consider grids whose next neighbour in the direction
            // away from the player is also of type floor. If we didn't
            // find any matching grid, try again without that restriction.
            for (int tries = 0; tries < 2; ++tries)
            {
                int adj_count = 0;
                for (adjacent_iterator ai(stair_pos); ai; ++ai)
                    if (grd(*ai) == DNGN_FLOOR
                        && (tries || _valid_floor_grid(*ai + *ai - stair_pos))
                        && one_chance_in(++adj_count))
                    {
                        new_pos = *ai;
                    }

                if (!tries && new_pos != stair_pos)
                    break;
            }

            if (new_pos == stair_pos)
                return (false);

            if (!slide_feature_over(stair_pos, new_pos))
                return (false);

            stair_pos = new_pos;
            stairs_moved = true;
        }

        begin   = you.pos();
        towards = stair_pos;
    }
    else
    {
        // Can't move towards player if it's already adjacent.
        if (adjacent(you.pos(), stair_pos))
            return (false);

        begin   = stair_pos;
        towards = you.pos();
    }

    ray_def ray;
    if (!find_ray(begin, towards, true, ray, 0, true))
    {
        mpr("Couldn't find ray between player and stairs.", MSGCH_ERROR);
        return (stairs_moved);
    }

    // Don't start off under the player.
    if (away)
        ray.advance();

    bool found_stairs = false;
    int  past_stairs  = 0;
    while (in_bounds(ray.pos()) && see_grid(ray.pos())
           && !grid_is_solid(ray.pos()) && ray.pos() != you.pos())
    {
        if (ray.pos() == stair_pos)
            found_stairs = true;
        if (found_stairs)
            past_stairs++;
        ray.advance();
    }
    past_stairs--;

    if (!away && grid_is_solid(ray.pos()))
    {
        // Transparent wall between stair and player.
        return (stairs_moved);
    }

    if (away && !found_stairs)
    {
        if (grid_is_solid(ray.pos()))
        {
            // Transparent wall between stair and player.
            return (stairs_moved);
        }

        mpr("Ray didn't cross stairs.", MSGCH_ERROR);
    }

    if (away && past_stairs <= 0)
    {
        // Stairs already at edge, can't move further away.
        return (stairs_moved);
    }

    if (!in_bounds(ray.pos()) || ray.pos() == you.pos())
        ray.regress();

    while (!see_grid(ray.pos()) || grd(ray.pos()) != DNGN_FLOOR)
    {
        ray.regress();
        if (!in_bounds(ray.pos()) || ray.pos() == you.pos()
            || ray.pos() == stair_pos)
        {
            // No squares in path are a plain floor.
            return (stairs_moved);
        }
    }

    ASSERT(stair_pos != ray.pos());

    std::string stair_str =
        feature_description(stair_pos, false, DESC_CAP_THE, false);

    mprf("%s slides %s you!", stair_str.c_str(),
         away ? "away from" : "towards");

    // Animate stair moving.
    const feature_def &feat_def = get_feature_def(feat);

    bolt beam;

    beam.range   = INFINITE_DISTANCE;
    beam.flavour = BEAM_VISUAL;
    beam.type    = feat_def.symbol;
    beam.colour  = feat_def.colour;
    beam.source  = stair_pos;
    beam.target  = ray.pos();
    beam.name    = "STAIR BEAM";
    beam.draw_delay = 50; // Make beam animation slower than normal.

    beam.aimed_at_spot = true;
    beam.fire();

    // Clear out "missile trails"
    viewwindow(true, false);

    if (!swap_features(stair_pos, ray.pos(), false, false))
    {
        mprf(MSGCH_ERROR, "_move_stair(): failed to move %s",
             stair_str.c_str());
        return (stairs_moved);
    }

    return (true);
}

static bool _repel_stairs()
{
    // Repeating the effect while it's still active is boring.
    if (you.duration[DUR_REPEL_STAIRS_MOVE]
        || you.duration[DUR_REPEL_STAIRS_CLIMB])
    {
        return (false);
    }

    std::vector<coord_def> stairs_avail;
    bool real_stairs = false;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, false, true); ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (grid_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
            if (grid_is_staircase(feat))
                real_stairs = true;
        }
    }

    // Should only happen if there are stairs in view.
    if (stairs_avail.empty())
        return (false);

    // Don't mention staircases if there aren't any nearby.
    std::string stair_msg = _get_xom_speech("repel stairs");
    if (stair_msg.find("@staircase@") != std::string::npos)
    {
        std::string feat_name;
        if (!real_stairs)
        {
            if (grid_is_escape_hatch(grd(stairs_avail[0])))
                feat_name = "escape hatch";
            else
                feat_name = "gate";
        }
        else
            feat_name = "staircase";
        stair_msg = replace_all(stair_msg, "@staircase@", feat_name);
    }

    god_speaks(GOD_XOM, stair_msg.c_str());

    you.duration[DUR_REPEL_STAIRS_MOVE] = 1000;

    if (one_chance_in(5)
        || grid_stair_direction(grd(you.pos())) != CMD_NO_CMD
           && grd(you.pos()) != DNGN_ENTER_SHOP)
    {
        you.duration[DUR_REPEL_STAIRS_CLIMB] = 500;
    }

    std::random_shuffle(stairs_avail.begin(), stairs_avail.end());
    int count_moved = 0;
    for (unsigned int i = 0; i < stairs_avail.size(); i++)
        if (_move_stair(stairs_avail[i], true))
            count_moved++;

    if (!count_moved)
    {
        if (one_chance_in(8))
            mpr("Nothing appears to happen... Ominous!");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }
    else
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "repel stairs"), true);

    return (true);
}

static bool _xom_draining_torment_effect(int sever)
{
    const std::string speech = _get_xom_speech("draining or torment");
    const bool nasty = _xom_feels_nasty();
    bool rc = false;

    if (one_chance_in(4))
    {
        // XP drain effect (25%).
        if (player_prot_life() < 3 && (nasty || you.experience > 0))
        {
            god_speaks(GOD_XOM, speech.c_str());

            drain_exp();
            if (random2(sever) > 3 && (nasty || you.experience > 0))
                drain_exp();
            if (random2(sever) > 3 && (nasty || you.experience > 0))
                drain_exp();

            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "draining"), true);
            rc = true;
        }
    }
    else
    {
        // Torment effect (75%).
        if (!player_res_torment())
        {
            god_speaks(GOD_XOM, speech.c_str());
            torment_player(0, TORMENT_XOM);

            // Take a note.
            static char torment_buf[80];
            snprintf(torment_buf, sizeof(torment_buf),
                     "torment (%d/%d hp)", you.hp, you.hp_max);
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, torment_buf), true);

            rc = true;
        }
    }
    return (rc);
}

static bool _xom_summon_hostiles(int sever)
{
    bool rc = false;
    const std::string speech = _get_xom_speech("hostile monster");

    // Nasty, but fun.
    if (player_weapon_wielded() && one_chance_in(4))
    {
        const item_def& weapon = *you.weapon();
        const std::string wep_name = weapon.name(DESC_PLAIN);
        rc = cast_tukimas_dance(100, GOD_XOM, true);

        if (rc)
        {
            static char wpn_buf[80];
            snprintf(wpn_buf, sizeof(wpn_buf),
                     "animates weapon (%s)", wep_name.c_str());
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, wpn_buf), true);
        }
    }
    else
    {
        // The number of demons is dependent on severity, though heavily
        // randomized.
        int numdemons = sever;
        for (int i = 0; i < 3; i++)
            numdemons = random2(numdemons+1);
        numdemons = std::min(numdemons+1,14);

        int num_summoned = 0;
        for (int i = 0; i < numdemons; ++i)
        {
            if (create_monster(
                    mgen_data::hostile_at(
                        _xom_random_demon(sever),
                        you.pos(), 4, 0, true, GOD_XOM,
                        MON_SUMM_WRATH)) != -1)
            {
                num_summoned++;
            }
        }

        if (num_summoned > 0)
        {
            static char summ_buf[80];
            snprintf(summ_buf, sizeof(summ_buf),
                     "summons %d hostile demon%s",
                     num_summoned, num_summoned > 1 ? "s" : "");
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, summ_buf), true);

            rc = true;
        }
    }

    if (rc)
        god_speaks(GOD_XOM, speech.c_str());

    return (rc);
}

static bool _xom_is_bad(int sever, int tension)
{
    bool done = false;
    bool nasty = (sever >= 5 && _xom_feels_nasty());

    god_acting gdact(GOD_XOM);

    // Rough estimate of how bad a Xom effect hits the player,
    // scaled between 1 (harmless) and 5 (disastrous).
    int badness = 1;
    while (!done)
    {
        // Did Xom kill the player?
        if (_player_is_dead())
            return (true);

        if (!nasty && x_chance_in_y(3, sever))
        {
            _xom_miscast(0, nasty);
            done = true;
        }
        else if (!nasty && x_chance_in_y(4, sever))
        {
            _xom_miscast(1, nasty);
            done = true;
        }
        else if (x_chance_in_y(5, sever))
        {
            done = _xom_lose_stats();
            badness = 2;
        }
        else if (x_chance_in_y(6, sever))
        {
            _xom_miscast(2, nasty);
            badness = 2;
            done = true;
        }
        else if (x_chance_in_y(7, sever) && you.level_type != LEVEL_ABYSS)
        {
            // Try something else if teleportation is impossible.
            if (scan_randarts(RAP_PREVENT_TELEPORTATION))
                return (false);

            // This is not particularly exciting if the level is already
            // fully explored (presumably cleared).  If Xom is feeling
            // nasty, this is likelier to happen if the level is
            // unexplored.
            const int explored = _exploration_estimate(true);
            if (nasty && explored >= 50 && coinflip()
                || explored >= 80 + random2(20))
            {
                done = false;
                continue;
            }

            // The Xom teleportation train takes you on instant
            // teleportation to a few random areas, stopping if either
            // an area is dangerous to you or randomly.
            god_speaks(GOD_XOM,
                       _get_xom_speech("teleportation journey").c_str());
            int count = 0;
            do
            {
                count++;
                you_teleport_now(false);
                more();
            }
            while (x_chance_in_y(3, 4) && !player_in_a_dangerous_place());
            badness = player_in_a_dangerous_place() ? 3 : 1;
            maybe_update_stashes();

            // Take a note.
            static char tele_buf[80];
            snprintf(tele_buf, sizeof(tele_buf),
                     "%d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
                     badness == 3 ? " (dangerous)" : "");
#else
                     "");
#endif
            take_note(Note(NOTE_XOM_EFFECT, you.piety, tension, tele_buf),
                      true);
            done = true;
        }
        else if (x_chance_in_y(8, sever))
        {
            done = _xom_chaos_upgrade_nearby_monster();
            badness = 2 + coinflip();
        }
        else if (random2(tension) < 10 && x_chance_in_y(9, sever))
        {
            done = _xom_give_mutations(false);
            badness = 3;
        }
        else if (x_chance_in_y(10, sever))
        {
            done = _xom_polymorph_nearby_monster(false);
            badness = 3;
        }
        else if (tension > 0 && x_chance_in_y(11, sever))
        {
            done = _repel_stairs();
            badness = (you.duration[DUR_REPEL_STAIRS_CLIMB] ? 3 : 2);
        }
        // It's pointless to confuse player if there's no danger nearby.
        else if (tension > 0 && x_chance_in_y(12, sever))
        {
            done = _xom_player_confusion_effect(sever);
            badness = (random2(tension) > 5 ? 2 : 1);
        }
        else if (x_chance_in_y(13, sever))
        {
            done = _xom_draining_torment_effect(sever);
            badness = (random2(tension) > 5 ? 3 : 2);
        }
        else if (x_chance_in_y(14, sever))
        {
            done = _xom_summon_hostiles(sever);
            badness = 3 + coinflip();
        }
        else if (x_chance_in_y(15, sever))
        {
            _xom_miscast(3, nasty);
            badness = 4 + coinflip();
            done = true;
        }
        else if (one_chance_in(sever) && you.level_type != LEVEL_ABYSS)
        {
            god_speaks(GOD_XOM, _get_xom_speech("banishment").c_str());
            // handles note taking
            banished(DNGN_ENTER_ABYSS, "Xom");
            badness = 5;
            done = true;
        }
    }

    // If we got here because Xom was bored, reset gift timeout according
    // to the badness of the effect.
    if (done && _xom_is_bored())
    {
        const int interest = random2avg(badness*60, 2);
        you.gift_timeout   = std::min(interest, 255);
#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
        mprf(MSGCH_DIAGNOSTICS, "badness: %d, new interest: %d",
             badness, you.gift_timeout);
#endif
    }
    return (done);
}

static char* _stat_ptrs[3] = {&you.strength, &you.intel, &you.dex};

static void _handle_accidental_death(const int orig_hp,
    const char orig_stats[],
    const FixedVector<unsigned char, NUM_MUTATIONS> &orig_mutation)
{
    // Did ouch() return early because the player died from the Xom
    // effect, even though neither is the player under penance nor is
    // Xom bored?
    if (!you.did_escape_death()
        && you.escaped_death_aux.empty()
        && !_player_is_dead())
    {
        // The player is fine.
        return;
    }

    std::string speech_type = "accidental homicide";

    const dungeon_feature_type feat = grd(you.pos());

    switch (you.escaped_death_cause)
    {
        case NUM_KILLBY:
        case KILLED_BY_LEAVING:
        case KILLED_BY_WINNING:
        case KILLED_BY_QUITTING:
            speech_type = "weird death";
            break;

        case KILLED_BY_LAVA:
        case KILLED_BY_WATER:
            if (!_feat_is_deadly(feat))
                speech_type = "weird death";
            break;

        case KILLED_BY_STUPIDITY:
            if (you.intel > 0)
                speech_type = "weird death";
            break;

        case KILLED_BY_WEAKNESS:
            if (you.strength > 0)
                speech_type = "weird death";
            break;

        case KILLED_BY_CLUMSINESS:
            if (you.dex > 0)
                speech_type = "weird death";
            break;

        default:
            if (_feat_is_deadly(feat))
                speech_type = "weird death";
            if (you.strength <= 0 || you.intel <= 0 || you.dex <= 0)
                speech_type = "weird death";
        break;
    }

    mpr("You die...");
    god_speaks(GOD_XOM, _get_xom_speech(speech_type).c_str());
    god_speaks(GOD_XOM, _get_xom_speech("resurrection").c_str());

    if (you.hp <= 0)
        you.hp = std::min(orig_hp, you.hp_max);

    mutation_type dex_muts[5] = {MUT_GREY2_SCALES, MUT_METALLIC_SCALES,
                                 MUT_YELLOW_SCALES, MUT_RED2_SCALES,
                                 MUT_STRONG_STIFF};

    for (int i = 0; i < 5; ++i)
    {
        mutation_type bad = dex_muts[i];

        while (you.dex <= 0 && you.mutation[bad] > orig_mutation[bad])
            delete_mutation(bad, true, true);
    }
    while (you.dex <= 0
           && you.mutation[MUT_FLEXIBLE_WEAK] <
                  orig_mutation[MUT_FLEXIBLE_WEAK])
    {
        mutate(MUT_FLEXIBLE_WEAK, true, true, true);
    }

    while (you.strength <= 0
           && you.mutation[MUT_FLEXIBLE_WEAK] >
                  orig_mutation[MUT_FLEXIBLE_WEAK])
    {
        delete_mutation(MUT_FLEXIBLE_WEAK, true, true);
    }
    while (you.strength <= 0
           && you.mutation[MUT_STRONG_STIFF] <
                  orig_mutation[MUT_STRONG_STIFF])
    {
        mutate(MUT_STRONG_STIFF, true, true, true);
    }

    mutation_type bad_muts[3]  = {MUT_WEAK, MUT_DOPEY, MUT_CLUMSY};
    mutation_type good_muts[3] = {MUT_STRONG, MUT_CLEVER, MUT_AGILE};

    for (int i = 0; i < 3; ++i)
    {
        while (*(_stat_ptrs[i]) <= 0)
        {
            mutation_type good = good_muts[i];
            mutation_type bad  = bad_muts[i];
            if (you.mutation[bad] > orig_mutation[bad]
                || you.mutation[good] < orig_mutation[good])
            {
                mutate(good, true, true, true);
            }
            else
            {
                *(_stat_ptrs[i]) = orig_stats[i];
                break;
            }
        }
    }

    you.max_strength = std::max(you.max_strength, you.strength);
    you.max_intel    = std::max(you.max_intel, you.intel);
    you.max_dex      = std::max(you.max_dex, you.dex);

    if (_feat_is_deadly(feat))
        you.teleport(true);
}

void xom_acts(bool niceness, int sever, int tension)
{
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS, "xom_acts(%u, %d, %d); piety: %u, interest: %u\n",
         niceness, sever, tension, you.piety, you.gift_timeout);
#endif

#ifdef WIZARD
    if (_player_is_dead())
    {
        // This should only happen if the player used wizard mode to
        // escape from death via stat loss, or if the player used wizard
        // mode to escape death from deep water or lava.
        ASSERT(you.wizard && !you.did_escape_death());
        if (_feat_is_deadly(grd(you.pos())))
        {
            mpr("Player is standing in deadly terrain, skipping Xom act.",
                MSGCH_DIAGNOSTICS);
        }
        else
        {
            mpr("Player is already dead, skipping Xom act.",
                MSGCH_DIAGNOSTICS);
        }
        return;
    }
#else
    ASSERT(!_player_is_dead());
#endif

    entry_cause_type old_entry_cause = you.entry_cause;

    sever = std::max(1, sever);

    god_type which_god = GOD_XOM;
    // Drawing the Xom card from Nemelex's decks of oddities or punishment.
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() != GOD_XOM)
    {
        which_god = crawl_state.which_god_acting();

        if (crawl_state.is_god_retribution())
        {
            niceness = false;
            simple_god_message(" asks Xom for help in punishing you, and "
                               "Xom happily agrees.", which_god);
        }
        else
        {
            niceness = true;
            simple_god_message(" calls in a favour from Xom.", which_god);
        }
    }

    if (tension == -1)
        tension = get_tension(which_god);

#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM) || defined(DEBUG_TENSION)
    mprf(MSGCH_DIAGNOSTICS, "Xom tension: %d", tension);
#endif

    const int  orig_hp       = you.hp;
    const char orig_stats[3] = {*(_stat_ptrs[0]), *(_stat_ptrs[1]),
                                *(_stat_ptrs[2])};

    const FixedVector<unsigned char, NUM_MUTATIONS> orig_mutation
        = you.mutation;

#ifdef NOTE_DEBUG_XOM
    static char xom_buf[100];
    snprintf(xom_buf, sizeof(xom_buf), "xom_acts(%s, %d, %d), mood: %d",
             (niceness ? "true" : "false"), sever, tension, you.piety);
    take_note(Note(NOTE_MESSAGE, 0, 0, xom_buf), true);
#endif

    const bool was_bored = _xom_is_bored();
    if (niceness && !one_chance_in(20))
    {
        // Good stuff.
        while (!_xom_is_good(sever, tension))
            ;
    }
    else
    {
        if (was_bored && Options.note_xom_effects)
            take_note(Note(NOTE_MESSAGE, 0, 0, "XOM is BORED!"), true);
#ifdef NOTE_DEBUG_XOM
        else if (niceness)
        {
            take_note(Note(NOTE_MESSAGE, 0, 0, "good act randomly turned bad"),
                      true);
        }
#endif

        // Bad mojo.
        while (!_xom_is_bad(sever, tension))
            ;
    }

    _handle_accidental_death(orig_hp, orig_stats, orig_mutation);

    // Drawing the Xom card from Nemelex's decks of oddities or punishment.
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() != GOD_XOM)
    {
        if (old_entry_cause != you.entry_cause
            && you.entry_cause_god == GOD_XOM)
        {
            you.entry_cause_god = crawl_state.which_god_acting();
        }
    }

    if (you.religion == GOD_XOM && one_chance_in(5))
    {
        const std::string old_xom_favour = describe_xom_favour();
        you.piety = random2(MAX_PIETY + 1);
        const std::string new_xom_favour = describe_xom_favour();
        if (was_bored || old_xom_favour != new_xom_favour)
        {
            const std::string msg = "You are now " + new_xom_favour;
            god_speaks(you.religion, msg.c_str());
        }
#ifdef NOTE_DEBUG_XOM
        snprintf(info, INFO_SIZE, "reroll piety: %d", you.piety);
        take_note(Note(NOTE_MESSAGE, 0, 0, info), true);
#endif
    }
    else if (was_bored)
    {
        // If we didn't reroll at least mention the new favour
        // now it's not "BORING thing" anymore.
        const std::string new_xom_favour = describe_xom_favour();
        const std::string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
    }
}

static void _xom_check_less_runes(int runes_gone)
{
    if (player_in_branch(BRANCH_HALL_OF_ZOT)
        || !(branches[BRANCH_HALL_OF_ZOT].branch_flags & BFLAG_HAS_ORB))
    {
        return;
    }

    int runes_avail = you.attribute[ATTR_UNIQUE_RUNES]
                       + you.attribute[ATTR_DEMONIC_RUNES]
                       + you.attribute[ATTR_ABYSSAL_RUNES]
                       - you.attribute[ATTR_RUNES_IN_ZOT];
    int was_avail = runes_avail + runes_gone;

    // No longer enough available runes to get into Zot.
    if (was_avail >= NUMBER_OF_RUNES_NEEDED
        && runes_avail < NUMBER_OF_RUNES_NEEDED)
    {
        xom_is_stimulated(128, "Xom snickers.", true);
    }
}

void xom_check_lost_item(const item_def& item)
{
    if (item.base_type == OBJ_ORBS)
        xom_is_stimulated(255, "Xom laughs nastily.", true);
    else if (is_fixed_artefact(item))
        xom_is_stimulated(128, "Xom snickers.", true);
    else if (is_rune(item))
    {
        // If you'd dropped it, check if that means you'd dropped your
        // third rune, and now you don't have enough to get into Zot.
        if (item.flags & ISFLAG_BEEN_IN_INV)
            _xom_check_less_runes(item.quantity);

        if (is_unique_rune(item))
            xom_is_stimulated(255, "Xom snickers loudly.", true);
        else if (you.entry_cause == EC_SELF_EXPLICIT
                 && !(item.flags & ISFLAG_BEEN_IN_INV))
        {
            // Player voluntarily entered Pan or the Abyss looking for
            // runes, yet never found them.
            if (item.plus == RUNE_ABYSSAL
                && you.attribute[ATTR_ABYSSAL_RUNES] == 0)
            {
                // Ignore Abyss area shifts.
                if (you.level_type != LEVEL_ABYSS)
                {
                    // Abyssal runes are a lot more trouble to find than
                    // demonic runes, so they get twice the stimulation.
                    xom_is_stimulated(128, "Xom snickers.", true);
                }
            }
            else if (item.plus == RUNE_DEMONIC
                     && you.attribute[ATTR_DEMONIC_RUNES] == 0)
            {
                xom_is_stimulated(64, "Xom snickers softly.", true);
            }
        }
    }
}

void xom_check_destroyed_item(const item_def& item, int cause)
{
    int amusement = 0;

    if (item.base_type == OBJ_ORBS)
    {
        xom_is_stimulated(255, "Xom laughs nastily.", true);
        return;
    }
    else if (is_fixed_artefact(item))
        xom_is_stimulated(128, "Xom snickers.", true);
    else if (is_rune(item))
    {
        _xom_check_less_runes(item.quantity);

        if (is_unique_rune(item) || item.plus == RUNE_ABYSSAL)
            amusement = 255;
        else
            amusement = 64 * item.quantity;
    }

    xom_is_stimulated(amusement,
                      (amusement > 128) ? "Xom snickers loudly." :
                      (amusement > 64)  ? "Xom snickers."
                                        : "Xom snickers softly.",
                      true);
}

static bool _death_is_funny(const kill_method_type killed_by)
{
    switch (killed_by)
    {
    // The less original deaths are considered boring.
    case KILLED_BY_MONSTER:
    case KILLED_BY_BEAM:
    case KILLED_BY_CLOUD:
    case KILLED_BY_FREEZING:
    case KILLED_BY_BURNING:
    case KILLED_BY_SELF_AIMED:
    case KILLED_BY_SOMETHING:
    case KILLED_BY_TRAP:
        return (false);
    default:
        // All others are fun (says Xom).
        return (true);
    }
}

void xom_death_message(const kill_method_type killed_by)
{
    if (you.religion != GOD_XOM && (!you.worshipped[GOD_XOM] || coinflip()))
        return;

    const int death_tension = get_tension(GOD_XOM, false);

    // "Normal" deaths with only down to -2 hp and comparatively low tension
    // are considered particularly boring.
    if (!_death_is_funny(killed_by) && you.hp >= -1 * random2(3)
        && death_tension <= random2(10))
    {
        god_speaks(GOD_XOM, _get_xom_speech("boring death").c_str());
    }
    // Unusual methods of dying, really low hp, or high tension make
    // for funny deaths.
    else if (_death_is_funny(killed_by) || you.hp <= -10
             || death_tension >= 20)
    {
        god_speaks(GOD_XOM, _get_xom_speech("laughter").c_str());
    }

    // All others just get ignored by Xom.
}

/**
 * @file
 * @brief All things Xomly
**/

#include "AppHdr.h"

#include "xom.h"

#include <algorithm>
#include <functional>

#include "abyss.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "corpse.h"
#include "coordit.h"
#include "database.h"
#ifdef WIZARD
#include "dbg-util.h"
#endif
#include "delay.h"
#include "directn.h"
#include "dlua.h"
#include "english.h"
#include "env.h"
#include "errors.h"
#include "god-item.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "losglobal.h"
#include "makeitem.h"
#include "map-knowledge.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random-pick.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-transloc.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "viewchar.h"
#include "view.h"

#ifdef DEBUG_XOM
#    define DEBUG_RELIGION
#    define NOTE_DEBUG_XOM
#endif

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS
#    define DEBUG_GIFTS
#endif

static void _do_xom_event(xom_event_type event_type, int sever);
static int _xom_event_badness(xom_event_type event_type);

static bool _action_is_bad(xom_event_type action)
{
    return action > XOM_LAST_GOOD_ACT && action <= XOM_LAST_BAD_ACT;
}

// Spells to be cast at tension > 0, i.e. usually in battle situations.
// Spells later in the list require higher severity to have a chance of being
// selected.
static const vector<spell_type> _xom_random_spells =
{
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_DAZZLING_FLASH,
    SPELL_FUGUE_OF_THE_FALLEN,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_ANIMATE_ARMOUR,
    SPELL_MARTYRS_KNELL,
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_SUMMON_BLAZEHEART_GOLEM,
    SPELL_BATTLESPHERE,
    SPELL_INTOXICATE,
    SPELL_ANIMATE_DEAD,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_SUMMON_CACTUS,
    SPELL_DISPERSAL,
    SPELL_DEATH_CHANNEL,
    SPELL_SUMMON_HYDRA,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_MALIGN_GATEWAY,
    SPELL_DISCORD,
    SPELL_DISJUNCTION,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_SUMMON_DRAGON,
    SPELL_FULSOME_FUSILLADE,
    SPELL_CHAIN_OF_CHAOS
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

/**
 * How much does Xom like you right now?
 *
 * Doesn't account for boredom, or whether or not you actually worship Xom.
 *
 * @return An index mapping to an entry in xom_moods.
 */
int xom_favour_rank()
{
    static const int breakpoints[] = { 20, 50, 80, 120, 150, 180};
    for (unsigned int i = 0; i < ARRAYSZ(breakpoints); ++i)
        if (you.piety <= breakpoints[i])
            return i;
    return ARRAYSZ(breakpoints);
}

static const char* xom_moods[] = {
    "a very special plaything of Xom.",
    "a special plaything of Xom.",
    "a plaything of Xom.",
    "a toy of Xom.",
    "a favourite toy of Xom.",
    "a beloved toy of Xom.",
    "Xom's teddy bear."
};

static const char *describe_xom_mood()
{
    const int mood = xom_favour_rank();
    ASSERT(mood >= 0);
    ASSERT((size_t) mood < ARRAYSZ(xom_moods));
    return xom_moods[mood];
}

const string describe_xom_favour()
{
    string favour;
    if (!you_worship(GOD_XOM))
        favour = "a very buggy toy of Xom.";
    else if (you.gift_timeout < 1)
        favour = "a BORING thing.";
    else
        favour = describe_xom_mood();

    return favour;
}

#define XOM_SPEECH(x) x
static string _get_xom_speech(const string &key)
{
    string result = getSpeakString("Xom " + key);

    if (result.empty())
        result = getSpeakString("Xom " XOM_SPEECH("general effect"));

    if (result.empty())
        return "Xom makes something happen.";

    return result;
}

static bool _xom_is_bored()
{
    return you_worship(GOD_XOM) && !you.gift_timeout;
}

static bool _xom_feels_nasty()
{
    // Xom will only directly kill you with a bad effect if you're under
    // penance from him, or if he's bored.
    return you.penance[GOD_XOM] || _xom_is_bored();
}

bool xom_is_nice(int tension)
{
    if (player_under_penance(GOD_XOM))
        return false;

    if (you_worship(GOD_XOM))
    {
        // If you.gift_timeout is 0, then Xom is BORED. He HATES that.
        if (!you.gift_timeout)
            return false;

        // At high tension Xom is more likely to be nice, at zero
        // tension the opposite.
        const int tension_bonus
            = (tension == -1 ? 0 // :
// Xom needs to be less negative
//              : tension ==  0 ? -min(abs(HALF_MAX_PIETY - you.piety) / 2,
//                                     you.piety / 10)
                             : min((MAX_PIETY - you.piety) / 2,
                                   random2(tension)));

        const int effective_piety = you.piety + tension_bonus;
        ASSERT_RANGE(effective_piety, 0, MAX_PIETY + 1);

#ifdef DEBUG_XOM
        mprf(MSGCH_DIAGNOSTICS,
             "Xom: tension: %d, piety: %d -> tension bonus = %d, eff. piety: %d",
             tension, you.piety, tension_bonus, effective_piety);
#endif

        // Whether Xom is nice depends largely on his mood (== piety).
        return x_chance_in_y(effective_piety, MAX_PIETY);
    }
    else // CARD_XOM
        return coinflip();
}

static void _xom_is_stimulated(int maxinterestingness,
                               const char *message_array[],
                               bool force_message)
{
    if (!you_worship(GOD_XOM) || maxinterestingness <= 0)
        return;

    // Xom is not directly stimulated by his own acts.
    if (crawl_state.which_god_acting() == GOD_XOM)
        return;

    int interestingness = random2(piety_scale(maxinterestingness));

    interestingness = min(200, interestingness);

#if defined(DEBUG_RELIGION) || defined(DEBUG_GIFTS) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "Xom: gift_timeout: %d, maxinterestingness = %d, interestingness = %d",
         you.gift_timeout, maxinterestingness, interestingness);
#endif

    bool was_stimulated = false;
    if (interestingness > you.gift_timeout && interestingness >= 10)
    {
        you.gift_timeout = interestingness;
        was_stimulated = true;
    }

    if (was_stimulated || force_message)
    {
        god_speaks(GOD_XOM,
                   ((interestingness > 160) ? message_array[5] :
                    (interestingness >  80) ? message_array[4] :
                    (interestingness >  60) ? message_array[3] :
                    (interestingness >  40) ? message_array[2] :
                    (interestingness >  20) ? message_array[1]
                                            : message_array[0]));
        //updating piety status line
        you.redraw_title = true;
    }
}

void xom_is_stimulated(int maxinterestingness, xom_message_type message_type,
                       bool force_message)
{
    _xom_is_stimulated(maxinterestingness, _xom_message_arrays[message_type],
                       force_message);
}

void xom_is_stimulated(int maxinterestingness, const string& message,
                       bool force_message)
{
    if (!you_worship(GOD_XOM))
        return;

    const char *message_array[6];

    for (int i = 0; i < 6; ++i)
        message_array[i] = message.c_str();

    _xom_is_stimulated(maxinterestingness, message_array, force_message);
}

void xom_tick()
{
    // Xom now ticks every action, not every 20 turns.
    if (one_chance_in(20))
    {
        // Xom semi-randomly drifts your piety.
        const string old_xom_favour = describe_xom_favour();
        const bool good = (you.piety == HALF_MAX_PIETY ? coinflip()
                                                       : you.piety > HALF_MAX_PIETY);
        int size = abs(you.piety - HALF_MAX_PIETY);

        // Piety slowly drifts towards the extremes.
        const int delta = piety_scale(x_chance_in_y(511, 1000) ? 1 : -1);
        size += delta;
        if (size > HALF_MAX_PIETY)
            size = HALF_MAX_PIETY;

        you.piety = HALF_MAX_PIETY + (good ? size : -size);
        string new_xom_favour = describe_xom_favour();
        you.redraw_title = true; // redraw piety/boredom display
        if (old_xom_favour != new_xom_favour)
        {
            // If we entered another favour state, take a big step into
            // the new territory, to avoid oscillating favour announcements
            // every few turns.
            size += delta * 8;
            if (size > HALF_MAX_PIETY)
                size = HALF_MAX_PIETY;

            // If size was 0 to begin with, it may become negative, but that
            // doesn't really matter.
            you.piety = HALF_MAX_PIETY + (good ? size : -size);
        }
#ifdef DEBUG_XOM
        const string note = make_stringf("xom_tick(), delta: %d, piety: %d",
                                         delta, you.piety);
        take_note(Note(NOTE_MESSAGE, 0, 0, note), true);
#endif

        // ...but he gets bored...
        if (you.gift_timeout > 0 && coinflip())
           you.gift_timeout--;

        new_xom_favour = describe_xom_favour();
        if (old_xom_favour != new_xom_favour)
        {
            const string msg = "You are now " + new_xom_favour;
            god_speaks(you.religion, msg.c_str());
        }

        if (you.gift_timeout == 1)
            simple_god_message(" is getting BORED.");
    }

    if (x_chance_in_y(2 + you.faith(), 6))
    {
        const int tension = get_tension(GOD_XOM);
        const int chance = (tension ==  0 ? 1 :
                            tension <=  5 ? 2 :
                            tension <= 10 ? 3 :
                            tension <= 20 ? 4
                                          : 5);

        // If Xom is bored, the chances for Xom acting are sort of reversed.
        if (!you.gift_timeout && x_chance_in_y(25 - chance*chance, 100))
        {
            xom_acts(abs(you.piety - HALF_MAX_PIETY), maybe_bool::maybe, tension);
            return;
        }
        else if (you.gift_timeout <= 1 && chance > 0
                 && x_chance_in_y(chance - 1, 80))
        {
            // During tension, Xom may briefly forget about being bored.
            const int interest = random2(chance * 15);
            if (interest > 0)
            {
                if (interest < 25)
                    simple_god_message(" is interested.");
                else
                    simple_god_message(" is intrigued.");

                you.gift_timeout += interest;
                //updating piety status line
                you.redraw_title = true;
#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
                mprf(MSGCH_DIAGNOSTICS,
                     "tension %d (chance: %d) -> increase interest to %d",
                     tension, chance, you.gift_timeout);
#endif
            }
        }

        if (x_chance_in_y(chance*chance, 100))
            xom_acts(abs(you.piety - HALF_MAX_PIETY), maybe_bool::maybe, tension);
    }
}

static bool mon_nearby(function<bool(monster&)> filter)
{
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (filter(**mi))
            return true;
    return false;
}

// Picks 100 random grids from the level and checks whether they've been
// marked as seen (explored) or known (mapped). If seen_only is true,
// grids only "seen" via magic mapping don't count. Returns the
// estimated percentage value of exploration.
static int _exploration_estimate(bool seen_only = false)
{
    int seen  = 0;
    int total = 0;
    int tries = 0;

    do
    {
        tries++;

        coord_def pos = random_in_bounds();
        if (!seen_only && env.map_knowledge(pos).known() || env.map_knowledge(pos).seen())
        {
            seen++;
            total++;
            continue;
        }

        bool open = true;
        if (cell_is_solid(pos) && !feat_is_closed_door(env.grid(pos)))
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(env.grid(*ai))
                                        || feat_is_closed_door(env.grid(*ai))))
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

    // If we didn't get any qualifying grids, there are probably so few
    // of them you've already seen them all.
    if (total == 0)
        return 100;

    if (total < 100)
        seen *= 100 / total;

    return seen;
}

static bool _teleportation_check()
{
    if (crawl_state.game_is_sprint())
        return false;

    return !you.no_tele();
}

/// Try to choose a random player-castable spell.
static spell_type _choose_random_spell(int sever)
{
    const int spellenum = max(1, sever);
    vector<spell_type> ok_spells;
    const vector<spell_type> &spell_list = _xom_random_spells;
    for (int i = 0; i < min(spellenum, (int)spell_list.size()); ++i)
    {
        const spell_type spell = spell_list[i];
        if (!spell_is_useless(spell, true, true, true))
            ok_spells.push_back(spell);
    }

    if (!ok_spells.size())
        return SPELL_NO_SPELL;
    return ok_spells[random2(ok_spells.size())];
}

/// Cast a random spell 'through' the player.
static void _xom_random_spell(int sever)
{
    const spell_type spell = _choose_random_spell(sever);
    int power = sever + you.experience_level * 2
                + get_tension() + you.runes.count() * 2;

    if (spell == SPELL_NO_SPELL)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("spell effect").c_str());

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "_xom_makes_you_cast_random_spell(); spell: %d",
         spell);
#endif

    your_spells(spell, power, false);
    const string note = make_stringf("cast spell '%s'", spell_title(spell));
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

// Map out the level, detect items across the level, and detect creatures.
static void _xom_divination(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("divination").c_str());

    magic_mapping(5 + sever * 2, 50 + random2avg(sever * 2, 2), false);

    const int prev_detected = count_detected_mons();
    const int num_creatures = detect_creatures(sever);
    const int num_items = detect_items(sever);

    if (num_creatures == 0 && num_items == 0)
        canned_msg(MSG_DETECT_NOTHING);
    else if (num_creatures == prev_detected)
    {
        // This is not strictly true. You could have cast Detect
        // Creatures with a big enough fuzz that the detected glyph is
        // still on the map when the original one has been killed. Then
        // another one is spawned, so the number is the same as before.
        // There's no way we can check this, however.
        mpr("You detect items, but no nearby creatures.");
    }
    else
    {
        if (num_items > 0)
            mpr("You detect items and creatures!");
        else
            mpr("You detect creatures, but no further items.");
    }

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "divination: all"), true);
}

static void _try_brand_switch(const int item_index)
{
    if (item_index == NON_ITEM)
        return;

    item_def &item(env.item[item_index]);

    if (item.base_type != OBJ_WEAPONS)
        return;

    if (is_unrandom_artefact(item))
        return;

    // Only do it some of the time.
    if (one_chance_in(2))
        return;

    if (get_weapon_brand(item) == SPWPN_NORMAL)
        return;

    // TODO: shared code with _do_chaos_upgrade
    mprf("%s erupts in a glittering mayhem of colour.",
                            item.name(DESC_THE, false, false, false).c_str());
    if (is_random_artefact(item))
        artefact_set_property(item, ARTP_BRAND, SPWPN_CHAOS);
    else
        item.brand = SPWPN_CHAOS;
}

static void _xom_make_item(object_class_type base, int subtype, int power)
{
    god_acting gdact(GOD_XOM);

    int thing_created = items(true, base, subtype, power, 0, GOD_XOM);

    if (thing_created == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }
    else if (base == OBJ_ARMOUR && subtype == ARM_ORB && one_chance_in(4))
        god_speaks(GOD_XOM, _get_xom_speech("orb gift").c_str());

    _try_brand_switch(thing_created);

    static char gift_buf[100];
    snprintf(gift_buf, sizeof(gift_buf), "god gift: %s",
             env.item[thing_created].name(DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, gift_buf), true);

    canned_msg(MSG_SOMETHING_APPEARS);
    move_item_to_grid(&thing_created, you.pos());

    if (thing_created == NON_ITEM) // if it fell into lava
        simple_god_message(" snickers.", false, GOD_XOM);

    stop_running();
}

/// Xom's 'acquirement'. A gift for the player, of a sort...
static void _xom_acquirement(int /*sever*/)
{
    god_speaks(GOD_XOM, _get_xom_speech("general gift").c_str());

    const object_class_type types[] =
    {
        OBJ_WEAPONS,  OBJ_ARMOUR,   OBJ_JEWELLERY,  OBJ_BOOKS,
        OBJ_STAVES,   OBJ_WANDS,    OBJ_MISCELLANY, OBJ_GOLD,
        OBJ_MISSILES, OBJ_TALISMANS
    };
    const object_class_type force_class = RANDOM_ELEMENT(types);

    const int item_index = acquirement_create_item(force_class, GOD_XOM,
            false, you.pos());
    if (item_index == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }

    _try_brand_switch(item_index);

    const string note = make_stringf("god gift: %s",
                                     env.item[item_index].name(DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);

    stop_running();
    more();
}

/// Create a random item and give it to the player.
static void _xom_random_item(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("general gift").c_str());
    _xom_make_item(OBJ_RANDOM, OBJ_RANDOM, sever * 3);
    more();
}

static bool _choose_mutatable_monster(const monster& mon)
{
    return mon.alive() && mon.can_safely_mutate();
}

static bool _choose_enchantable_monster(const monster& mon)
{
    return mon.alive() && !mon.wont_attack()
           && !mons_invuln_will(mon);
}

static bool _is_chaos_upgradeable(const item_def &item)
{
    // Change randarts, but not other artefacts.
    if (is_unrandom_artefact(item))
        return false;

    // Staves can't be changed either, since they don't have brands in the way
    // other weapons do.
    if (item.base_type == OBJ_STAVES
#if TAG_MAJOR_VERSION == 34
        || item.base_type == OBJ_RODS
#endif
       )
{
        return false;
}

    // Only upgrade permanent items, since the player should get a
    // chance to use the item if he or she can defeat the monster.
    if (item.flags & ISFLAG_SUMMONED)
        return false;

    // Blessed weapons are protected, being gifts from good gods.
    if (is_blessed(item))
        return false;

    // God gifts are protected -- but not his own!
    if (item.orig_monnum < 0)
    {
        god_type iorig = static_cast<god_type>(-item.orig_monnum);
        if (iorig > GOD_NO_GOD && iorig < NUM_GODS && iorig != GOD_XOM)
            return false;
    }

    // Don't stuff player inventory slots with chaos throwables.
    if (item.base_type == OBJ_MISSILES)
        return false;

    return true;
}

static bool _choose_chaos_upgrade(const monster& mon)
{
    // Only choose monsters that will attack.
    if (!mon.alive() || mons_attitude(mon) != ATT_HOSTILE
        || mons_is_fleeing(mon))
    {
        return false;
    }

    if (mons_itemuse(mon) < MONUSE_STARTING_EQUIPMENT)
        return false;

    // Holy beings are presumably protected by another god, unless
    // they're gifts from a chaotic god.
    if (mon.is_holy() && !is_chaotic_god(mon.god))
        return false;

    // God gifts from good gods will be protected by their god from
    // being given chaos weapons, while other gods won't mind the help
    // in their servants' killing the player.
    if (is_good_god(mon.god))
        return false;

    mon_inv_type slots[] = {MSLOT_WEAPON, MSLOT_ALT_WEAPON, MSLOT_MISSILE};

    // NOTE: Code assumes that the monster will only be carrying one
    // missile launcher at a time.
    bool special_launcher = false;
    for (int i = 0; i < 3; ++i)
    {
        const mon_inv_type slot = slots[i];
        const int          midx = mon.inv[slot];

        if (midx == NON_ITEM)
            continue;
        const item_def &item(env.item[midx]);

        // The monster already has a chaos weapon. Give the upgrade to
        // a different monster.
        if (is_chaotic_item(item))
            return false;

        if (_is_chaos_upgradeable(item))
        {
            if (item.base_type != OBJ_MISSILES)
                return true;

            // If, for some weird reason, a monster is carrying a bow
            // and javelins, then branding the javelins is okay, since
            // they won't be fired by the bow.
            if (!special_launcher || is_throwable(&mon, item))
                return true;
        }

        if (is_range_weapon(item))
        {
            // If the launcher alters its ammo, then branding the
            // monster's ammo won't be an upgrade.
            int brand = get_weapon_brand(item);
            if (brand == SPWPN_FLAMING || brand == SPWPN_FREEZING
                || brand == SPWPN_VENOM)
            {
                special_launcher = true;
            }
        }
    }

    return false;
}

static void _do_chaos_upgrade(item_def &item, const monster* mon)
{
    ASSERT(item.base_type == OBJ_MISSILES
           || item.base_type == OBJ_WEAPONS);
    ASSERT(!is_unrandom_artefact(item));

    bool seen = false;
    if (mon && you.can_see(*mon) && item.base_type == OBJ_WEAPONS)
    {
        seen = true;

        const description_level_type desc = mon->friendly() ? DESC_YOUR
                                                            : DESC_THE;
        mprf("%s %s erupts in a glittering mayhem of colour.",
            apostrophise(mon->name(desc)).c_str(),
            item.name(DESC_PLAIN, false, false, false).c_str());
    }

    const int brand = (item.base_type == OBJ_WEAPONS) ? (int) SPWPN_CHAOS
                                                      : (int) SPMSL_CHAOS;

    if (is_random_artefact(item))
    {
        artefact_set_property(item, ARTP_BRAND, brand);

        if (seen)
            artefact_learn_prop(item, ARTP_BRAND);
    }
    else
    {
        item.brand = brand;

        if (seen)
            set_ident_flags(item, ISFLAG_KNOW_TYPE);

        // Make sure it's visibly special.
        if (!(item.flags & ISFLAG_COSMETIC_MASK))
            item.flags |= ISFLAG_GLOWING;

        // Give some extra enchantments to tempt players into using chaos brand.
        if (item.base_type == OBJ_WEAPONS)
            item.plus  += random_range(2, 4);
    }
}

// Xom forcibly sends you to a special bazaar,
// with visuals pretending it's banishment.
static void _xom_bazaar_trip(int /*sever*/)
{
    stop_delay(true);
    god_speaks(GOD_XOM, _get_xom_speech("bazaar trip").c_str());
    run_animation(ANIMATION_BANISH, UA_BRANCH_ENTRY, false);
    dlua.callfn("dgn_set_persistent_var", "sb", "xom_bazaar", true);
    down_stairs(DNGN_ENTER_BAZAAR);
    you.props[XOM_BAZAAR_TRIP_COUNT].get_int()++;
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "banished to a bazaar"),
                  true);
}

static const vector<random_pick_entry<monster_type>> _xom_summons =
{
  { -4, 26,   5, FLAT, MONS_BUTTERFLY },
  { -4, -1,   5, FLAT, MONS_QUOKKA },
  { -3,  6,  60, SEMI, MONS_CRIMSON_IMP },
  { -1,  6,  40, SEMI, MONS_IRON_IMP },
  {  2,  8,  60, SEMI, MONS_QUASIT },
  {  2,  8,  40, SEMI, MONS_SHADOW_IMP },
  {  3,  8,  50, SEMI, MONS_UFETUBUS },
  {  3,  8,  40, SEMI, MONS_WHITE_IMP },
  {  4,  8,   5, SEMI, MONS_MUMMY },
  {  4, 10,  75, SEMI, MONS_PHANTOM },
  {  5, 10,  10, SEMI, MONS_BOMBARDIER_BEETLE },
  {  5, 12,  90, SEMI, MONS_SWAMP_DRAKE },
  {  5, 12,  25, SEMI, MONS_WEEPING_SKULL },
  {  7, 14,  50, SEMI, MONS_ORANGE_DEMON },
  {  7, 15,  95, SEMI, MONS_SHAPESHIFTER },
  {  8, 14,  40, SEMI, MONS_ICE_DEVIL },
  {  8, 14,  40, SEMI, MONS_RED_DEVIL },
  {  8, 15,  60, SEMI, MONS_BOGGART },
  {  8, 20, 155, SEMI, MONS_CHAOS_SPAWN },
  {  9, 14,  60, SEMI, MONS_HELLWING },
  {  9, 14,   5, FLAT, MONS_TOENAIL_GOLEM },
  {  9, 14,  30, SEMI, MONS_VAMPIRE },
  {  9, 16,  50, SEMI, MONS_YNOXINUL },
  { 10, 14,  40, SEMI, MONS_HELL_RAT },
  { 10, 14,  30, SEMI, MONS_KOBOLD_DEMONOLOGIST },
  { 10, 15,  90, SEMI, MONS_ABOMINATION_SMALL },
  { 10, 16,  50, SEMI, MONS_UGLY_THING },
  { 10, 17,  50, SEMI, MONS_SOUL_EATER },
  { 10, 18,  85, SEMI, MONS_RUST_DEVIL },
  { 11, 17,  50, SEMI, MONS_SMOKE_DEMON },
  { 11, 17,  30, SEMI, MONS_DREAM_SHEEP },
  { 11, 20,  80, SEMI, MONS_WORLDBINDER },
  { 11, 22,  50, SEMI, MONS_NEQOXEC },
  { 12, 16,  30, SEMI, MONS_OBSIDIAN_BAT },
  { 12, 18,  30, SEMI, MONS_TARANTELLA },
  { 12, 18,  15, SEMI, MONS_DEMONSPAWN },
  { 13, 19,  75, SEMI, MONS_SUN_DEMON },
  { 13, 20,  75, SEMI, MONS_SIXFIRHY },
  { 13, 20,  15, SEMI, MONS_GREAT_ORB_OF_EYES },
  { 13, 21, 105, SEMI, MONS_LAUGHING_SKULL },
  { 14, 22,  90, SEMI, MONS_ABOMINATION_LARGE },
  { 14, 22, 105, SEMI, MONS_GLOWING_SHAPESHIFTER },
  { 15, 22,  50, SEMI, MONS_HELL_HOG },
  { 15, 23,   1, FLAT, MONS_OBSIDIAN_STATUE },
  { 16, 22,  10, SEMI, MONS_BUNYIP },
  { 16, 23,  50, SEMI, MONS_RADROACH },
  { 16, 25,  75, SEMI, MONS_VERY_UGLY_THING },
  { 17, 24,  35, SEMI, MONS_GLOWING_ORANGE_BRAIN },
  { 17, 25,  50, SEMI, MONS_SPHINX },
  { 17, 33,  35, SEMI, MONS_SHADOW_DEMON },
  { 17, 33,  65, SEMI, MONS_SIN_BEAST },
  { 17, 33,  15, SEMI, MONS_CACODEMON },
  { 18, 33,  35, SEMI, MONS_REAPER },
  { 18, 24,  30, SEMI, MONS_DANCING_WEAPON },
  { 19, 26,   1, FLAT, MONS_ORANGE_STATUE },
  { 20, 33,  30, SEMI, MONS_APOCALYPSE_CRAB },
  { 21, 33,  30, SEMI, MONS_TENTACLED_MONSTROSITY },
  { 22, 33,   1, FLAT, MONS_STARFLOWER },
  { 23, 33,  30, SEMI, MONS_HELLEPHANT },
  { 24, 33,   5, SEMI, MONS_MOTH_OF_WRATH },
  { 25, 33,   5, SEMI, MONS_NEKOMATA },
};

// Whenever choosing a monster that obviously comes in bands, spawn a few more,
// which is then used later on to not count for the summon count range of the
// power tier the given Xom summon calls for.
static int _xom_pal_minibands(monster_type mtype)
{
    int count = 1;

    if (mtype == MONS_BUTTERFLY || mtype == MONS_LAUGHING_SKULL ||
        mtype == MONS_DREAM_SHEEP)
    {
        count = x_chance_in_y(you.experience_level, 27) ? 3 : 2;
    }
    else if (mtype == MONS_HELL_RAT || mtype == MONS_BOGGART ||
             mtype == MONS_UGLY_THING || mtype == MONS_TARANTELLA ||
             mtype == MONS_HELL_HOG || mtype == MONS_VERY_UGLY_THING)
    {
        count = 2;
    }

    return count;
}

// Don't let later summoners double-up later on in summon calls;
// it gets too messy too quickly to tell what's happening.
static bool _xom_pal_summonercheck(monster_type mtype)
{
     return mtype == MONS_OBSIDIAN_STATUE || mtype == MONS_ORANGE_STATUE ||
            mtype == MONS_GLOWING_ORANGE_BRAIN || mtype == MONS_SHADOW_DEMON;
}

// This and _xom_random_pal keep three tiers of enemies that are each scaled
// to three tiers of XL, spawning either more of weaker monsters or less
// of stronger monsters whenever Xom summons allies or enemies.
static int _xom_pal_counting(int roll, bool isFriendly)
{
    int count = 0;

    if (roll <= 150)
    {
        if (you.experience_level < 4)
            count = random_range(2, 3);
        else if (you.experience_level < 10)
            count = random_range(3, 4);
        else if (you.experience_level < 19)
            count = random_range(3, 5);
        else
            count = random_range(4, 5);
    }
    else if (roll <= 300)
    {
        if (you.experience_level < 10)
            count = random_range(1, 2);
        else if (you.experience_level < 19)
            count = random_range(2, 3);
        else
            count = random_range(2, 4);
    }
    else
    {
        if (you.experience_level < 10)
            count = 1;
        else if (you.experience_level < 19)
            count = random_range(1, 2);
        else
            count = random_range(2, 3);
    }

    if (you.runes.count() > 4)
        count += div_rand_round(you.runes.count(), 5);

    if (!isFriendly && _xom_feels_nasty())
        count *= 1.5;

    return count;
}

static monster_type _xom_random_pal(int roll, bool isFriendly)
{
    monster_picker xom_picker;
    int variance = you.experience_level;

    // Tiers here match _xom_pal_counting's tiers of strength related
    // inversely to summon count but scaling up versus one's XL regardless.
    if (roll <= 130)
        if (you.experience_level < 19)
           variance += -4;
        else
           variance += random_range(-4, -3);
    else if (roll <= 300)
        if (you.experience_level < 19)
            variance += random_range(-2, -1);
        else
            variance += random_range(-2, 0);
    else
        if (you.experience_level < 10)
            variance += random_range(-1, 1);
        else if (you.experience_level < 19)
            variance += random_range(0, 2);
        else
            variance += random_range(1, 3);

    // Make it a little flashier if it's allied or if Xom's quite dissatisfied.
    if (isFriendly)
        variance += random_range(1, 4);
    else if (_xom_is_bored())
        variance += random_range(2, 4);
    else if (you.penance[GOD_XOM])
        variance += random_range(4, 6);

    variance = min(33, variance);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "_xom_random_pal(); xl variance roll: %d", roll);
#endif

    monster_type mon_type = xom_picker.pick(_xom_summons, variance, MONS_CRIMSON_IMP);

    // Endgame and extended monsters Xom has some fondness for (in being jokes
    // or highly chaotic), but which are mostly meant to stay in their branches,
    // get a small extra chance to be picked in the appropriate branch or Zigs.
    // Makes it a little more dramatic.
    if ((player_in_branch(BRANCH_ZOT) || player_in_branch(BRANCH_ZIGGURAT))
         && one_chance_in(13))
    {
        mon_type = random_choose_weighted(5, MONS_DEATH_COB,
                                          4, MONS_KILLER_KLOWN,
                                          3, MONS_PROTEAN_PROGENITOR,
                                          2, MONS_CURSE_TOE);
    }
    else if ((player_in_branch(BRANCH_PANDEMONIUM) ||
              player_in_branch(BRANCH_ZIGGURAT)) && one_chance_in(13))
    {
        mon_type = x_chance_in_y(3, 5) ? MONS_DEMONSPAWN_BLOOD_SAINT :
                                         MONS_DEMONSPAWN_CORRUPTER;
    }
    else if (player_in_branch(BRANCH_ZIGGURAT) && one_chance_in(4))
    {
        mon_type = x_chance_in_y(5, 7) ? MONS_PANDEMONIUM_LORD :
                                         MONS_PLAYER_GHOST;
    }

    return mon_type;
}

static bool _player_is_dead()
{
    return you.hp <= 0
        || you.did_escape_death();
}

static void _note_potion_effect(potion_type pot)
{
    string potion_name = potion_type_name(static_cast<int>(pot));

    string potion_msg = "potion effect ";

    potion_msg += ("(" + potion_name + ")");

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, potion_msg), true);
}


/// Feed the player a notionally-good potion effect.
static void _xom_do_potion(int /*sever*/)
{
    potion_type pot = POT_CURING;
    do
    {
        pot = random_choose_weighted(10, POT_CURING,
                                     10, POT_HEAL_WOUNDS,
                                     10, POT_MAGIC,
                                     10, POT_HASTE,
                                     10, POT_MIGHT,
                                     10, POT_BRILLIANCE,
                                     10, POT_INVISIBILITY,
                                     5,  POT_AMBROSIA,
                                     5,  POT_ATTRACTION,
                                     5,  POT_BERSERK_RAGE,
                                     1,  POT_EXPERIENCE);
    }
    while (!get_potion_effect(pot)->can_quaff()); // ugh

    // Experience uses default power, other potions get bonus power.
    // Curing, heal wounds, magic and berserk rage ignore power.
    const int pow = pot == POT_EXPERIENCE ? 40 : 150;

    god_speaks(GOD_XOM, _get_xom_speech("potion effect").c_str());

    _note_potion_effect(pot);

    get_potion_effect(pot)->effect(true, pow);

    level_change(); // need this for !xp - see mantis #3245
}

static void _confuse_monster(monster* mons, int sever)
{
    if (mons->clarity())
        return;
    if (mons->holiness() & (MH_NONLIVING | MH_PLANT))
        return;

    const bool was_confused = mons->confused();
    if (mons->add_ench(mon_enchant(ENCH_CONFUSION, 0,
          &env.mons[ANON_FRIENDLY_MONSTER], random2(sever) * 10)))
    {
        if (was_confused)
            simple_monster_message(*mons, " looks rather more confused.");
        else
            simple_monster_message(*mons, " looks rather confused.");
    }
}

static void _xom_confuse_monsters(int sever)
{
    bool spoke = false;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->wont_attack() || one_chance_in(20))
            continue;

        // Only give this message once.
        if (!spoke)
            god_speaks(GOD_XOM, _get_xom_speech("confusion").c_str());
        spoke = true;

        _confuse_monster(*mi, sever);
    }

    if (spoke)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "confuse monster(s)"),
                  true);
    }
}

/// Post a passel of pals to the player.
static void _xom_send_allies(int sever)
{
    int strengthRoll = random2(1000 - (MAX_PIETY + sever) * 5);
    int count = _xom_pal_counting(strengthRoll, true);
    int num_actually_summoned = 0;

    for (int i = 0; i < count; ++i)
    {
        monster_type mon_type = _xom_random_pal(strengthRoll, true);

        mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH);
        mg.set_summoned(&you, 3, MON_SUMM_AID, GOD_XOM);

        // Even though the friendlies are charged to you for accounting,
        // they should still show as Xom's fault if one of them kills you.
        mg.non_actor_summoner = "Xom";

        // Banding monsters don't count against the overall summon roll range,
        // and are restricted themselves in _xom_pal_minibands.
        int miniband = _xom_pal_minibands(mon_type);

        for (int j = 0; j < miniband; ++j)
        {
            monster* made = create_monster(mg);

            if (made)
            {
                num_actually_summoned++;
                if (made->type == MONS_REAPER)
                    _do_chaos_upgrade(*made->weapon(), made);
            }
        }

        // To make given random monster summonings more coherent, have a good
        // chance to jump forward and make the next summon the same as the last,
        // but only if it's not already a banding monster or a late summoner.
        if (x_chance_in_y(2, 3) && miniband == 1 &&
            !_xom_pal_summonercheck(mon_type) && i < count - 1)
        {
            i += 1;
            monster* made = create_monster(mg);

            if (made)
            {
                num_actually_summoned++;
                if (made->type == MONS_REAPER)
                    _do_chaos_upgrade(*made->weapon(), made);
            }
        }
    }

    if (num_actually_summoned)
    {
        god_speaks(GOD_XOM, _get_xom_speech("multiple summons").c_str());

        const string note = make_stringf("summons %d friend%s",
                                         num_actually_summoned,
                                         num_actually_summoned > 1 ? "s" : "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
}

/// Send a single pal to the player's aid, hopefully.
static void _xom_send_one_ally(int sever)
{
    int strengthRoll = random2(1000 - (MAX_PIETY + sever) * 5);
    const monster_type mon_type = _xom_random_pal(strengthRoll, true);

    mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH);
    mg.set_summoned(&you, 6, MON_SUMM_AID, GOD_XOM);

    mg.non_actor_summoner = "Xom";

    if (monster *summons = create_monster(mg))
    {
        // Add a little extra length and regen. Make friends with your new pal.
        int extra = random_range(100, 200);
        summons->add_ench(mon_enchant(ENCH_ABJ, MON_SUMM_AID, nullptr, extra));
        summons->add_ench(mon_enchant(ENCH_REGENERATION, MON_SUMM_AID,
                                       nullptr, 2000));
        god_speaks(GOD_XOM, _get_xom_speech("single summon").c_str());

        if (summons->type == MONS_REAPER)
            _do_chaos_upgrade(*summons->weapon(), summons);

        const string note = make_stringf("summons friendly %s",
                                         summons->name(DESC_PLAIN).c_str());
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
}

/**
 * Try to polymorph the given monster. If 'helpful', hostile monsters will
 * (try to) turn into weaker ones, and friendly monsters into stronger ones;
 * if (!helpful), the reverse is true.
 *
 * @param mons      The monster in question.
 * @param helpful   Whether to try to be helpful.
 */
static void _xom_polymorph_monster(monster &mons, bool helpful)
{
    god_speaks(GOD_XOM,
               helpful ? _get_xom_speech("good monster polymorph").c_str()
                       : _get_xom_speech("bad monster polymorph").c_str());

    const bool see_old = you.can_see(mons);
    const string old_name = see_old ? mons.full_name(DESC_PLAIN)
                                    : "something unseen";

    if (one_chance_in(8)
        && !mons_is_ghost_demon(mons.type)
        && !mons.is_shapeshifter()
        && mons.holiness() & MH_NATURAL
        && (you.experience_level > 4 || _xom_feels_nasty()))
    {
        mons.add_ench(one_chance_in(3) ? ENCH_GLOWING_SHAPESHIFTER
                                       : ENCH_SHAPESHIFTER);
    }

    const bool powerup = !(mons.wont_attack() ^ helpful);

    if (powerup)
    {
        if (you.experience_level > 4 && !helpful)
            mons.polymorph(PPT_MORE);
        else
            mons.polymorph(PPT_SAME);

        if (you.experience_level < 10 && !helpful)
            mons.malmutate("");
    }

    else
        mons.polymorph(PPT_LESS);

    const bool see_new = you.can_see(mons);

    if (see_old || see_new)
    {
        const string new_name = see_new ? mons.full_name(DESC_PLAIN)
                                        : "something unseen";

        string note = make_stringf("%s polymorph %s -> %s",
                                   helpful ? "good" : "bad",
                                   old_name.c_str(), new_name.c_str());

#ifdef NOTE_DEBUG_XOM
        note += " (";
        note += (powerup ? "upgrade" : "downgrade");
        note += ")";
#endif
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
}

/// Find a monster to poly.
static monster* _xom_mons_poly_target()
{
    vector<monster*> polymorphable;

    // XXX: Polymorphing early bats over liquids turn into D:1 steam dragons
    //      and killer bees, so skip them while polymorphing at low xls.
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (_choose_mutatable_monster(**mi) && !mons_is_firewood(**mi)
           && (you.experience_level < 4 && (env.grid(mi->pos()) != DNGN_DEEP_WATER)
               && env.grid(mi->pos()) != DNGN_LAVA) || you.experience_level > 3 )
        {
            polymorphable.push_back(*mi);
        }
    }

    shuffle_array(polymorphable);

    if (polymorphable.empty())
        return nullptr;
    else
        return polymorphable[0];
}

/// Try to polymporph a nearby monster into something weaker... or stronger.
static void _xom_polymorph_nearby_monster(bool helpful)
{
    monster* mon = _xom_mons_poly_target();
    if (mon)
        _xom_polymorph_monster(*mon, helpful);
}

/// Try to polymporph a nearby monster into something weaker.
static void _xom_good_polymorph(int /*sever*/)
{
    _xom_polymorph_nearby_monster(true);
}

/// Try to polymporph a nearby monster into something stronger.
static void _xom_bad_polymorph(int /*sever*/)
{
    _xom_polymorph_nearby_monster(false);
}

bool swap_monsters(monster* m1, monster* m2)
{
    monster& mon1(*m1);
    monster& mon2(*m2);

    const bool mon1_caught = mon1.caught();
    const bool mon2_caught = mon2.caught();

    mon1.swap_with(m2);

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

    return true;
}

/// Which monsters, if any, can Xom currently swap with the player?
static vector<monster*> _rearrangeable_pieces()
{
    vector<monster* > mons;
    if (player_stair_delay() || monster_at(you.pos()))
        return mons;

    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_is_tentacle_or_tentacle_segment(mi->type))
            mons.push_back(*mi);
    }
    return mons;
}

// Swap places with a random monster and, depending on severity, also
// between monsters. This can be pretty bad if there are a lot of
// hostile monsters around.
static void _xom_rearrange_pieces(int sever)
{
    vector<monster*> mons = _rearrangeable_pieces();
    if (mons.empty())
        return;

    god_speaks(GOD_XOM, _get_xom_speech("rearrange the pieces").c_str());

    const int num_mons = mons.size();

    // Swap places with a random monster.
    monster* mon = mons[random2(num_mons)];
    swap_with_monster(mon);

    // Sometimes confuse said monster.
    if (coinflip())
        _confuse_monster(mon, sever);

    if (num_mons > 1 && x_chance_in_y(sever, 70))
    {
        bool did_message = false;
        const int max_repeats = min(num_mons / 2, 8);
        const int repeats     = min(random2(sever / 10) + 1, max_repeats);
        for (int i = 0; i < repeats; ++i)
        {
            const int mon1 = random2(num_mons);
            int mon2 = mon1;
            while (mon1 == mon2)
                mon2 = random2(num_mons);

            if (swap_monsters(mons[mon1], mons[mon2]))
            {
                if (!did_message)
                {
                    mpr("Some monsters swap places.");
                    did_message = true;
                }
                if (one_chance_in(4))
                    _confuse_monster(mons[mon1], sever);
                if (one_chance_in(4))
                    _confuse_monster(mons[mon2], sever);
            }
        }
    }
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "swap monsters"), true);
}

static int _xom_random_stickable(const int HD)
{
    unsigned int c;

    static const int arr[] =
    {
        WPN_CLUB, WPN_SPEAR, WPN_TRIDENT, WPN_HALBERD,
        WPN_GLAIVE, WPN_QUARTERSTAFF, WPN_SHORTBOW, WPN_ORCBOW,
        WPN_LONGBOW, WPN_GIANT_CLUB, WPN_GIANT_SPIKED_CLUB
    };

    // Maximum snake hd is 11 (anaconda) so random2(hd) gives us 0-10, and
    // weapon_rarity also gives us 1-10.
    do
    {
        c = random2(HD);
    }
    while (c >= ARRAYSZ(arr)
           || random2(HD) > weapon_rarity(arr[c]) && x_chance_in_y(c, HD));

    return arr[c];
}

static bool _hostile_snake(monster& mon)
{
    return mon.attitude == ATT_HOSTILE
            && mons_genus(mon.type) == MONS_SNAKE;
}

// An effect similar to old sticks to snakes (which worked on "sticks" other
// than arrows)
//  * Transformations are permanent.
//  * Weapons are always non-cursed.
//  * HD influences the enchantment and type of the weapon.
//  * Weapon is not guaranteed to be useful.
//  * Weapon will never be branded.
static void _xom_snakes_to_sticks(int /*sever*/)
{
    bool action = false;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!_hostile_snake(**mi))
            continue;

        if (!action)
        {
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                           "snakes to sticks"), true);
            god_speaks(GOD_XOM, _get_xom_speech("snakes to sticks").c_str());
            action = true;
        }

        const object_class_type base_type = coinflip() ? OBJ_MISSILES
                                                       : OBJ_WEAPONS;

        const int sub_type =
            (base_type == OBJ_MISSILES ? MI_JAVELIN
             : _xom_random_stickable(mi->get_experience_level()));

        int item_slot = items(false, base_type, sub_type,
                              mi->get_experience_level() / 3 - 1,
                              0, -1);

        if (item_slot == NON_ITEM)
            continue;

        item_def &item(env.item[item_slot]);

        // Always limit the quantity to 1.
        item.quantity = 1;

        // Output some text since otherwise snakes will disappear silently.
        mprf("%s reforms as %s.", mi->name(DESC_THE).c_str(),
             item.name(DESC_A).c_str());

        // Dismiss monster silently.
        move_item_to_grid(&item_slot, mi->pos());
        monster_die(**mi, KILL_DISMISSED, NON_MONSTER, true, false);
    }
}

/// Try to find a nearby hostile monster with an animateable weapon.
static monster* _find_monster_with_animateable_weapon()
{
    vector<monster* > mons_wpn;
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->wont_attack() || mi->is_summoned()
            || mons_itemuse(**mi) < MONUSE_STARTING_EQUIPMENT
            || (mi->flags & MF_HARD_RESET))
        {
            continue;
        }

        const int mweap = mi->inv[MSLOT_WEAPON];
        if (mweap == NON_ITEM)
            continue;

        const item_def weapon = env.item[mweap];

        if (weapon.base_type == OBJ_WEAPONS
            && !(weapon.flags & ISFLAG_SUMMONED)
            && weapon.quantity == 1
            && !is_range_weapon(weapon)
            && !is_special_unrandom_artefact(weapon)
            && get_weapon_brand(weapon) != SPWPN_DISTORTION)
        {
            mons_wpn.push_back(*mi);
        }
    }
    if (mons_wpn.empty())
        return nullptr;
    return mons_wpn[random2(mons_wpn.size())];
}

static void _xom_animate_monster_weapon(int sever)
{
    // Pick a random monster...
    monster* mon = _find_monster_with_animateable_weapon();
    if (!mon)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("animate monster weapon").c_str());

    // ...and get its weapon.
    const int wpn = mon->inv[MSLOT_WEAPON];
    ASSERT(wpn != NON_ITEM);

    const int dur = min(2 + (random2(sever) / 5), 6);

    mgen_data mg(MONS_DANCING_WEAPON, BEH_FRIENDLY, mon->pos(), mon->mindex());
    mg.set_summoned(&you, dur, SPELL_TUKIMAS_DANCE, GOD_XOM);

    mg.non_actor_summoner = "Xom";

    monster *dancing = create_monster(mg);

    if (!dancing)
        return;

    // Make the monster unwield its weapon.
    mon->unequip(*(mon->mslot_item(MSLOT_WEAPON)), false, true);
    mon->inv[MSLOT_WEAPON] = NON_ITEM;

    mprf("%s %s dances into the air!",
         apostrophise(mon->name(DESC_THE)).c_str(),
         env.item[wpn].name(DESC_PLAIN).c_str());

    destroy_item(dancing->inv[MSLOT_WEAPON]);

    dancing->inv[MSLOT_WEAPON] = wpn;
    env.item[wpn].set_holding_monster(*dancing);
    dancing->colour = env.item[wpn].get_colour();
}

// Have Xom make a big ring of temporary harmless plantlife around the player.
static void _xom_harmless_flora(int /*sever*/)
{
    bool created = false;
    bool perfectRing = coinflip();
    int radius = random_choose_weighted(54 - you.experience_level, 2,
                                        27, 3,
                                        13 + you.experience_level, 4);

    monster_type mon_type = x_chance_in_y(13 + you.experience_level, 54) ?
                            MONS_DEMONIC_PLANT : MONS_TOADSTOOL;

    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        // Half of the time, make it imperfect on the inner parts.
        if (ri->distance_from(you.pos()) != radius
            && !perfectRing && x_chance_in_y(1, 3))
        {
            continue;
        }

        if (!actor_at(*ri) && monster_habitable_grid(MONS_PLANT, env.grid(*ri)))
        {
            mgen_data mg(mon_type, BEH_HOSTILE, *ri, MHITYOU, MG_FORCE_BEH | MG_FORCE_PLACE);
            mg.set_summoned(&you, 5, MON_SUMM_AID, GOD_XOM);

            mg.non_actor_summoner = "Xom";

            if (create_monster(mg))
                created = true;
        }
    }

    if (created)
    {
        god_speaks(GOD_XOM, _get_xom_speech("flora ring").c_str());
        if (mon_type == MONS_DEMONIC_PLANT)
            mpr("Demonic plants sprout up around you!");
        else
            mpr("Toadstools sprout up around you!");

        const string note = make_stringf("made a garden");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS); // shouldn't be reached, and yet
}

// Can Xom reasonably convert a feature into interconnected doors?
// Avoids altars, stairs, portals, and runed doors. Allow anywhere
// diggable or otherwise walkable, and glassifies solid doors.
static bool _xom_door_replaceable(dungeon_feature_type feat)
{
    return !feat_is_critical(feat)
            && ((feat_has_solid_floor(feat)) || feat_is_diggable(feat)
                || (feat_is_door(feat) && !feat_is_runed(feat)));
}

// Find a group of monsters in a certain range of the player,
// then move them towards or away from the target, according to a cap.
// Return how many were moved for further assessing how many more may be moved.
static int _xom_count_and_move_group(int min_range, int max_range,
                                      bool inwards, int cap = INT_MAX)
{
    int moved = 0;
    vector<monster*> collectable;

    for (radius_iterator ri(you.pos(), max_range, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (grid_distance(*ri, you.pos()) < min_range
            || !monster_at(*ri)
            || monster_at(*ri)->wont_attack())
        {
            continue;
        }

        collectable.push_back(monster_at(*ri));
    }

    shuffle_array(collectable);

    for (monster *moving_mons : collectable)
    {
        if (moved == cap)
            break;

        coord_def empty;

        if (inwards)
        {
            // Blink close a limited number of hostile enemies
            // adjacent or one tile away.
            if (!find_habitable_spot_near(you.pos(), mons_base_type(*moving_mons),
                                          2, false, empty))
            {
                continue;
            }

            if (moving_mons->blink_to(empty, true))
            {
                simple_monster_message(*moving_mons, " is shoved forward by the hand of Xom!");
                moving_mons->drain_action_energy();
                behaviour_event(moving_mons, ME_DISTURB, nullptr, you.pos());
                ++moved;
            }
        }
        else
        {
            int placeable_count = 0;
            coord_def spot;

            // Xom can heavily relocate enemies as desired to make this ring,
            // though teleporting them away by default would be more boring.

            // First try to blink the monster somewhere the player can still see.
            for (radius_iterator ri(moving_mons->pos(), 9, C_SQUARE, LOS_NO_TRANS, true);
                 ri; ++ri)
            {
                // Only look for unoccupied viable spaces
                // outside the entire door ring.
                if (actor_at(*ri) || !monster_habitable_grid(moving_mons, env.grid(*ri))
                    || grid_distance(*ri, you.pos()) < 6)
                {
                    continue;
                }

                if (one_chance_in(++placeable_count))
                    spot = *ri;
            }

            // If that didn't work, try somewhere the player can't see.
            if (spot.origin())
            {
                for (radius_iterator ri(moving_mons->pos(), 9, C_SQUARE, LOS_NO_TRANS, true);
                    ri; ++ri)
                {
                    if (actor_at(*ri) || !monster_habitable_grid(moving_mons, env.grid(*ri))
                        || grid_distance(*ri, you.pos()) < 6)
                    {
                        continue;
                    }

                    if (one_chance_in(++placeable_count))
                        spot = *ri;
                }
            }

            // If that still didn't work, just teleport the monster.
            if (spot.origin())
                moving_mons->teleport(true);
            else if (moving_mons->blink_to(spot, true))
            {
                moving_mons->drain_action_energy();
                ++moved;
            }
        }
    }

    return moved;
}

// Have Xom make a huge, slightly distant ring of clear, disconnected doors,
// and move enemies in or out according to Xom's mood.
static void _xom_door_ring(bool good)
{
    bool created = false;
    int total_moved = 0;
    int dug = 0;
    const int min_radius = 3;
    const int max_radius = 5;

    dungeon_feature_type feat = DNGN_CLOSED_CLEAR_DOOR;

    if (good)
    {
        // If meant to be good, shove out all adjacent hostile enemies.
        // Split to prioritize inside the ring are moved first
        // before moving those where the doors will appear.
        total_moved += _xom_count_and_move_group(1, min_radius - 1, false);
        total_moved += _xom_count_and_move_group(min_radius, max_radius, false);
    }
    else
    {
        // If meant to be bad, shove in visible enemies closer to the player,
        // capped by XL and how many are already adjacent.
        // Once more, prioritize monsters in the closer ring's range first.
        int soft_cap = max(1, div_rand_round(you.experience_level, 6));
        int inner_cap = _xom_feels_nasty() ? 8 : 2 + random_range(1, soft_cap);

        int moved = _xom_count_and_move_group(min_radius, max_radius,
                                              true, inner_cap);
        total_moved += moved;
        inner_cap -= moved;

        if (you.normal_vision >= max_radius + 1 && inner_cap > 0)
        {
            total_moved += _xom_count_and_move_group(max_radius + 1, you.normal_vision,
                                                     true, inner_cap);
        }

        total_moved += _xom_count_and_move_group(min_radius, max_radius, false);
    }

    // Either way, place the door ring round the player in a radius of 3 to 5.
    // It won't be perfect- skipping altars and stairs, rarely failing to
    // find a better spot for a monster- but it's more than enough for Xom.
    for (radius_iterator ri(you.pos(), max_radius, C_SQUARE, LOS_NONE); ri; ++ri)
    {
        if (in_bounds(*ri) && !actor_at(*ri)
            && grid_distance(*ri, you.pos()) >= min_radius
            && grid_distance(*ri, you.pos()) <= max_radius
            && _xom_door_replaceable(env.grid(*ri)))
        {
            if (cloud_at(*ri))
                delete_cloud(*ri);

            // For later messaging's sake, track how much is being dug out.
            if (feat_is_diggable(env.grid(*ri)))
                dug++;

            dungeon_terrain_changed(*ri, feat, false, true);

            map_wiz_props_marker *marker = new map_wiz_props_marker(*ri);
            marker->set_property("connected_exclude", "true");
            env.markers.add(marker);

            created = true;
        }
    }

    if (created)
    {
        env.markers.clear_need_activate();
        string message;

        if (dug > 60)
            message = "The dungeon churns and warps with monstrous intensity.";
        else if (dug > 30)
            message = "The dungeon churns and warps violently around you.";
        else
            message = "The dungeon churns and shimmers intensely around you.";

        mprf(good ? MSGCH_GOD : MSGCH_WARN, "%s", message.c_str());

        string note = "";
        if (good)
        {
            god_speaks(GOD_XOM, _get_xom_speech("kind door ring").c_str());
            note = make_stringf("made a ring of doors, pushed %d %s out",
                                total_moved, total_moved != 1 ? "others"
                                                              : "other");
        }
        else
        {
            god_speaks(GOD_XOM, _get_xom_speech("mean door ring").c_str());
            note = make_stringf("made a ring of doors, pulled %d %s in",
                                total_moved, total_moved != 1 ? "others"
                                                              : "other");
        }

        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _xom_good_door_ring(int /*sever*/)
{
    _xom_door_ring(true);
}

static void _xom_bad_door_ring(int /*sever*/)
{
    _xom_door_ring(false);
}

static const map<dungeon_feature_type, int> terrain_fake_shatter_chances = {
    { DNGN_CLOSED_DOOR,      90 },
    { DNGN_GRATE,            90 },
    { DNGN_ORCISH_IDOL,      90 },
    { DNGN_GRANITE_STATUE,   90 },
    { DNGN_CLEAR_ROCK_WALL,  33 },
    { DNGN_ROCK_WALL,        33 },
    { DNGN_SLIMY_WALL,       25 },
    { DNGN_CRYSTAL_WALL,     25 },
    { DNGN_TREE,             25 },
    { DNGN_CLEAR_STONE_WALL, 15 },
    { DNGN_STONE_WALL,       15 },
    { DNGN_METAL_STATUE,      5 },
    { DNGN_METAL_WALL,        5 },
};

static int _xom_shatter_walls(coord_def where, bool more_than_dig)
{
    dungeon_feature_type feat = env.grid(where);

    if (!in_bounds(where)
         || env.markers.property_at(where, MAT_ANY, "veto_destroy") == "veto")
    {
        return 0;
    }

    if (feat_is_tree(feat))
        feat = DNGN_TREE;
    else if (feat_is_door(feat))
        feat = DNGN_CLOSED_DOOR;

    auto chance = terrain_fake_shatter_chances.find(feat);

    if (chance == terrain_fake_shatter_chances.end()
        || ((!feat_is_diggable(feat) || feat_is_door(feat)) && !more_than_dig)
        || !x_chance_in_y(chance->second, 100))
    {
        return 0;
    }

    if (you.see_cell(where))
    {
        if (feat_is_door(feat))
            mpr("A door shatters!");
        else if (feat == DNGN_GRATE)
            mpr("An iron grate is ripped into pieces!");
    }

    noisy(spell_effect_noise(SPELL_SHATTER), where);
    destroy_wall(where);

    if (feat == DNGN_ROCK_WALL || feat == DNGN_STONE_WALL
        || feat == DNGN_GRANITE_STATUE)
    {
        mgen_data mg(MONS_PILE_OF_DEBRIS, BEH_HOSTILE, where, MHITYOU,
                     MG_FORCE_BEH | MG_FORCE_PLACE);

        create_monster(mg);
    }

    return 1;
}

// Xom produces Shatter level noise, demolishes random features, and also
// fails to actually do any actual damage. Unlike actual Shatter, this both
// leaves behind debris and usually has very little chance to do more than dig.
static void _xom_fake_shatter(int /*sever*/)
{
    bool more_than_dig = one_chance_in(5);

    god_speaks(GOD_XOM, _get_xom_speech("fake shatter").c_str());

    if (silenced(you.pos()))
        mpr("The dungeon shakes... harmlessly?");
    else
    {
        noisy(spell_effect_noise(SPELL_SHATTER), you.pos());
        mprf(MSGCH_SOUND, "The dungeon rumbles... harmlessly?");
    }

    run_animation(ANIMATION_SHAKE_VIEWPORT, UA_PLAYER);

    int dest = 0;
    int rocks = 0;

    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        if (!cell_see_cell(you.pos(), *di, LOS_SOLID))
            continue;

        dest += _xom_shatter_walls(*di, more_than_dig);
    }

    for (distance_iterator di(you.pos(), true, true, LOS_NO_TRANS); di; ++di)
    {
        if (one_chance_in(5) && rocks <= dest / 2 && !monster_at(*di)
              && !cell_is_solid(*di))
        {
            int rock_spot = items(true, OBJ_MISSILES, MI_LARGE_ROCK, 0, 0, GOD_XOM);
            env.item[rock_spot].quantity = 1;
            move_item_to_grid(&rock_spot, *di);
            rocks++;
        }
    }

    if (rocks)
        mpr("Some rocks are dislodged from the ceiling.");

    if (dest)
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "fake shatter"), true);

}

static void _xom_give_mutations(bool good)
{
    if (!you.can_safely_mutate())
        return;

    god_speaks(GOD_XOM, good ? _get_xom_speech("good mutations").c_str()
                             : _get_xom_speech("random mutations").c_str());

    const int num_tries = random2(4) + 1;

    const string note = make_stringf("give %smutation%s",
#ifdef NOTE_DEBUG_XOM
             good ? "good " : "random ",
#else
             "",
#endif
             num_tries > 1 ? "s" : "");

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    mpr("Your body is suffused with distortional energy.");

    bool failMsg = true;

    for (int i = num_tries; i > 0; --i)
    {
        if (you.penance[GOD_XOM] && i == num_tries && !good)
        {
            if (!mutate(RANDOM_BAD_MUTATION, "Xom's mischief",
                        failMsg, false, true, false, MUTCLASS_NORMAL))
            {
                failMsg = false;
            }
        }
        else if (!mutate(good ? RANDOM_GOOD_MUTATION : RANDOM_XOM_MUTATION,
                    good ? "Xom's grace" : "Xom's mischief",
                    failMsg, false, true, false, MUTCLASS_NORMAL))
        {
            failMsg = false;
        }
    }
}

static void _xom_give_good_mutations(int) { _xom_give_mutations(true); }
static void _xom_give_bad_mutations(int) { _xom_give_mutations(false); }

static void _xom_drop_lightning()
{
    bolt beam;

    beam.flavour      = BEAM_ELECTRICITY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(3, 30);
    beam.target       = you.pos();
    beam.name         = "blast of lightning";
    beam.colour       = LIGHTCYAN;
    beam.thrower      = KILL_MISC;
    beam.source_id    = MID_NOBODY;
    beam.aux_source   = "Xom's lightning strike";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    beam.explode(true, true);
}

static void _xom_spray_lightning(coord_def position)
{
    bolt beam;

    // range has no tracer, so randomness is ok
    beam.range        = 7;
    beam.source       = you.pos();
    beam.target       = position;
    beam.target.x     += random_range(-1, 1);
    beam.target.y     += random_range(-1, 1);
    while (beam.target == you.pos())
    {
        beam.target.x     += random_range(-1, 1);
        beam.target.y     += random_range(-1, 1);
    }
    beam.thrower      = KILL_MISC;
    beam.source_id    = MID_NOBODY;
    beam.aux_source   = "Xom's lightning strike";

    int power = 20 + you.experience_level * 5;
    if (you.runes.count() > 4)
        power += you.runes.count() * 3;

    // uncontrolled, so no player tracer.
    zappy(ZAP_LIGHTNING_BOLT, power, true, beam);
    beam.fire();
}

// Have Xom throw down divine lightning in a 5x5 explosion, then fire random
// lightning bolts in random directions potentially fudged towards, like
// old black draconian breath.
static void _xom_throw_divine_lightning(int /*sever*/)
{
    god_speaks(GOD_XOM, _get_xom_speech("divine lightning").c_str());

    _xom_drop_lightning();

    int spray_count = random_range(4, max(5, (min(27, get_tension() / 5))));
    int fire_count = 0;

    // Have a chance to actually aim lightning bolts near each present enemy.
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (fire_count < spray_count && one_chance_in(3))
        {
            _xom_spray_lightning(mi->pos());
            fire_count++;
        }
    }

    // Fire spare random bolts at random spots.
    if (fire_count < spray_count)
    {
        for (int i = 0; i < spray_count - fire_count; ++i)
        {
            coord_def randspot = you.pos();
            randspot.x += random_range(-6, 6);
            randspot.y += random_range(-6, 6);
            _xom_spray_lightning(randspot);
            fire_count++;
        }
    }

    string note = make_stringf("divine lightning + %d bolts", spray_count);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

// What scenery nearby would Xom like to mess with, if any?
static vector<coord_def> _xom_scenery_candidates()
{
    vector<coord_def> candidates;
    for (vision_iterator ri(you); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        if (feat_is_fountain(feat) && feat != DNGN_FOUNTAIN_BLOOD
            && feat != DNGN_FOUNTAIN_EYES)
        {
            candidates.push_back(*ri);
        }
        else if (feat_is_tree(feat))
            candidates.push_back(*ri);
        else if (feat_is_food(feat))
            candidates.push_back(*ri);
    }

    return candidates;
}

// What doors nearby would Xom like to mess with, if any?
static vector<coord_def> _xom_door_candidates()
{
    vector<coord_def> candidates;
    vector<coord_def> closed_doors;
    vector<coord_def> open_doors;
    for (vision_iterator ri(you); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        if (feat_is_closed_door(feat))
        {
            // Check whether this door is already included in a gate.
            if (find(begin(closed_doors), end(closed_doors), *ri)
                == end(closed_doors))
            {
                // If it's a gate, add all doors belonging to the gate.
                set<coord_def> all_door;
                find_connected_identical(*ri, all_door);
                for (auto dc : all_door)
                    closed_doors.push_back(dc);
            }
        }
        else if (feat_is_open_door(feat) && !actor_at(*ri)
                 && env.igrid(*ri) == NON_ITEM)
        {
            // Check whether this door is already included in a gate.
            if (find(begin(open_doors), end(open_doors), *ri)
                == end(open_doors))
            {
                // Check whether any of the doors belonging to a gate is
                // blocked by an item or monster.
                set<coord_def> all_door;
                find_connected_identical(*ri, all_door);
                bool is_blocked = false;
                for (auto dc : all_door)
                {
                    if (actor_at(dc) || env.igrid(dc) != NON_ITEM)
                    {
                        is_blocked = true;
                        break;
                    }
                }

                // If the doorway isn't blocked, add all doors
                // belonging to the gate.
                if (!is_blocked)
                {
                    for (auto dc : all_door)
                        open_doors.push_back(dc);
                }
            }
        }
    }
    // Order needs to be the same as messaging below, else the messages might
    // not make sense.
    candidates.insert(end(candidates), begin(open_doors), end(open_doors));
    candidates.insert(end(candidates), begin(closed_doors), end(closed_doors));

    return candidates;
}

// Place one or more decorative* features nearish the player.
static void _xom_place_decor()
{
    coord_def place;
    bool success = false;
    int aby = player_in_branch(BRANCH_ABYSS) ? 0 : 1;
    dungeon_feature_type decor = random_choose_weighted(10, DNGN_ALTAR_XOM,
                                                        7, DNGN_TRAP_TELEPORT,
                                                        2, DNGN_CACHE_OF_FRUIT,
                                                        2, DNGN_CACHE_OF_MEAT,
                                                        2, DNGN_CACHE_OF_BAKED_GOODS,
                                                        1, DNGN_CLOSED_DOOR,
                                                        1, DNGN_OPEN_DOOR,
                                                        aby, DNGN_ENTER_ABYSS);

    const int featuresCount = max(2, random2(random2(16)));
    for (int tries = featuresCount; tries > 0; --tries)
    {
        if ((random_near_space(&you, you.pos(), place, false)
             || random_near_space(&you, you.pos(), place, true))
            && env.grid(place) == DNGN_FLOOR && !(env.igrid(place) != NON_ITEM))
        {
            dungeon_terrain_changed(place, decor);
            success = true;
        }
    }

    if (success)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                       "scenery: changed the scenery"), true);
        const string key = make_stringf("scenery %s", dungeon_feature_name(decor));
        god_speaks(GOD_XOM, _get_xom_speech(key).c_str());
    }
}

static void _xom_summon_butterflies()
{
    bool success = false;
    const int how_many = random_range(10, 20);

    for (int i = 0; i < how_many; ++i)
    {
        mgen_data mg(MONS_BUTTERFLY, BEH_FRIENDLY, you.pos(), MHITYOU,
                     MG_FORCE_BEH);
        mg.set_summoned(&you, 3, MON_SUMM_AID, GOD_XOM);
        if (create_monster(mg))
            success = true;
    }

    if (success)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                       "scenery: summon butterflies"), true);
        god_speaks(GOD_XOM, _get_xom_speech("scenery").c_str());
    }
}

// Mess with nearby terrain features, mostly harmlessly.
static void _xom_change_scenery(int /*sever*/)
{
    vector<coord_def> candidates = _xom_scenery_candidates();

    if (candidates.empty() || one_chance_in(6))
    {
        if (x_chance_in_y(2, 3))
            _xom_place_decor();
        else
            _xom_summon_butterflies();
        return;
    }

    int fountains_blood    = 0;
    int food_swapped       = 0;
    int trees_polymorphed  = 0;

    dungeon_feature_type wtree = random_choose_weighted(4, DNGN_DEMONIC_TREE,
                                                        1, DNGN_PETRIFIED_TREE);
    dungeon_feature_type btree = random_choose_weighted(1, DNGN_TREE,
                                                        3, DNGN_MANGROVE);

    for (coord_def pos : candidates)
    {
        switch (env.grid(pos))
        {
        case DNGN_DRY_FOUNTAIN:
        case DNGN_FOUNTAIN_BLUE:
        case DNGN_FOUNTAIN_SPARKLING:
            if (x_chance_in_y(fountains_blood, 3))
                continue;

            env.grid(pos) = DNGN_FOUNTAIN_BLOOD;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                fountains_blood++;
            break;
        case DNGN_CACHE_OF_FRUIT:
        case DNGN_CACHE_OF_MEAT:
        case DNGN_CACHE_OF_BAKED_GOODS:
            if (x_chance_in_y(food_swapped, 3))
                continue;

            if (env.grid(pos) == DNGN_CACHE_OF_FRUIT)
                env.grid(pos) = DNGN_CACHE_OF_MEAT;
            else if (env.grid(pos) == DNGN_CACHE_OF_MEAT)
                env.grid(pos) = DNGN_CACHE_OF_BAKED_GOODS;
            else
                env.grid(pos) = DNGN_CACHE_OF_FRUIT;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                food_swapped++;
            break;
        case DNGN_TREE:
        case DNGN_MANGROVE:
            if (x_chance_in_y(trees_polymorphed, 3))
                continue;

            env.grid(pos) = wtree;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                trees_polymorphed++;
            break;
        case DNGN_DEMONIC_TREE:
        case DNGN_PETRIFIED_TREE:
            if (x_chance_in_y(trees_polymorphed, 3))
                continue;

            env.grid(pos) = btree;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                trees_polymorphed++;
            break;
        default:
            break;
        }
    }

    if (!fountains_blood && !food_swapped && !trees_polymorphed)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("scenery").c_str());

    vector<string> effects, terse;
    if (fountains_blood > 0)
    {
        string fountains = make_stringf(
                 "%s fountain%s start%s gushing blood",
                 fountains_blood == 1 ? "a" : "some",
                 fountains_blood == 1 ? ""  : "s",
                 fountains_blood == 1 ? "s" : "");

        if (effects.empty())
            fountains = uppercase_first(fountains);
        effects.push_back(fountains);
        terse.push_back(make_stringf("%d fountains blood", fountains_blood));
    }

    if (food_swapped > 0)
    {
        string snacks = "some nearby snacks are swapped around";

        if (effects.empty())
            snacks = uppercase_first(snacks);
        effects.push_back(snacks);
        terse.push_back(make_stringf("%d snacks swapped", food_swapped));
    }

    if (trees_polymorphed > 0)
    {
        string trees = "some trees are warped by chaos";
        if (effects.empty())
            trees = uppercase_first(trees);
        effects.push_back(trees);
        terse.push_back(make_stringf("%d trees warped", trees_polymorphed));
    }

    if (!effects.empty())
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, ("scenery: "
            + comma_separated_line(terse.begin(), terse.end(), ", ", ", ")).c_str()),
            true);
        mprf("%s!",
             comma_separated_line(effects.begin(), effects.end(),
                                  ", and ").c_str());
    }
}

static void _xom_open_and_close_doors(int /* sever */)
{
    vector<coord_def> candidates = _xom_door_candidates();

    if (candidates.empty())
        return;

    int doors_open      = 0;
    int doors_close     = 0;
    for (coord_def pos : candidates)
    {
        switch (env.grid(pos))
        {
        case DNGN_CLOSED_DOOR:
        case DNGN_CLOSED_CLEAR_DOOR:
        case DNGN_RUNED_DOOR:
        case DNGN_RUNED_CLEAR_DOOR:
            dgn_open_door(pos);
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                doors_open++;
            break;
        case DNGN_OPEN_DOOR:
        case DNGN_OPEN_CLEAR_DOOR:
            dgn_close_door(pos);
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                doors_close++;
            break;
        default:
            break;
        }
    }
    if (!doors_open && !doors_close)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("scenery").c_str());

    vector<string> effects, terse;

    if (doors_open > 0)
    {
        effects.push_back(make_stringf("%s door%s burst%s open",
                                       doors_open == 1 ? "A"    :
                                       doors_open == 2 ? "Two"
                                                       : "Several",
                                       doors_open == 1 ? ""  : "s",
                                       doors_open == 1 ? "s" : ""));
        terse.push_back(make_stringf("%d doors open", doors_open));
    }
    if (doors_close > 0)
    {
        string closed = make_stringf("%s%s door%s slam%s shut",
                 doors_close == 1 ? "a"    :
                 doors_close == 2 ? "two"
                                  : "several",
                 doors_open > 0   ? (doors_close == 1 ? "nother" : " other")
                                  : "",
                 doors_close == 1 ? ""  : "s",
                 doors_close == 1 ? "s" : "");
        if (effects.empty())
            closed = uppercase_first(closed);
        effects.push_back(closed);
        terse.push_back(make_stringf("%d doors close", doors_close));
    }
    if (!effects.empty())
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, ("scenery: "
            + comma_separated_line(terse.begin(), terse.end(), ", ", ", ")).c_str()),
            true);
        mprf("%s!",
             comma_separated_line(effects.begin(), effects.end(),
                                  ", and ").c_str());
    }

    if (doors_open || doors_close)
        noisy(10, you.pos());
}

/// Xom hurls liquid fireballs at your foes! Or, possibly, 'fireballs'.
static void _xom_destruction(int sever, bool real)
{
    bool rc = false;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_is_projectile(**mi)
            || mons_is_tentacle_or_tentacle_segment(mi->type)
            || one_chance_in(3))
        {
            continue;
        }

        // Skip adjacent monsters, and skip non-hostile monsters if not feeling nasty.
        if (real
            && (adjacent(you.pos(), mi->pos())
                || mi->wont_attack() && !_xom_feels_nasty()))
        {
            continue;
        }

        if (!real)
        {
            if (!rc)
                god_speaks(GOD_XOM, _get_xom_speech("fake destruction").c_str());
            rc = true;
            backlight_monster(*mi);
            continue;
        }

        int dice = 2 + div_rand_round(you.experience_level, 13);
        if (you.runes.count() > 4)
            dice += div_rand_round(you.runes.count(), 3);

        bolt beam;

        beam.flavour      = BEAM_STICKY_FLAME;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(dice, 4 + sever / 12);
        beam.target       = mi->pos();
        beam.name         = "sticky fireball";
        beam.colour       = RED;
        beam.thrower      = KILL_MISC;
        beam.source_id    = MID_NOBODY;
        beam.aux_source   = "Xom's destruction";
        beam.ex_size      = 1;
        beam.is_explosion = true;

        // Only give this message once.
        if (!rc)
            god_speaks(GOD_XOM, _get_xom_speech("destruction").c_str());
        rc = true;

        beam.explode();
    }

    if (rc)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                       real ? "destruction" : "fake destruction"), true);
    }
}

static void _xom_real_destruction(int sever) { _xom_destruction(sever, true); }
static void _xom_fake_destruction(int sever) { _xom_destruction(sever, false); }

// A crunched down copy of the scroll of butterflies knockback.
static void _xom_harmless_knockback(coord_def p)
{
    monster* mon = monster_at(p);
    if (mon)
    {
        const int dist = random_range(2, 3);
        mon->knockback(you, dist, 0, "hand of Xom");
        behaviour_event(mon, ME_ALERT, &you);
    }
}

// Knock back any nearby monsters without doing damage, then summon an
// increasing amount of living spell force lances to all rocket fire against
// whatever doesn't clear them away the next turn.
static void _xom_force_lances(int /* sever */)
{
    int xl = _xom_feels_nasty() ? you.experience_level / 3
                                : you.experience_level + you.runes.count() / 4;
    int count = 2 + (xl / 2) + random_range(0, div_rand_round(xl, 5));
    int strength = max(1, xl / 2);
    int created = 0;

    // Clear up space for summoning Force Lances, if possible.
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
        if (grid_distance(*ri, you.pos()) == 2)
            _xom_harmless_knockback(*ri);

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        _xom_harmless_knockback(*ai);

    for (int i = 0; i < count; ++i)
    {
        mgen_data mg(MONS_LIVING_SPELL, BEH_FRIENDLY, you.pos(),
                     MHITYOU, MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);

        mg.set_summoned(&you, 2, MON_SUMM_AID, GOD_XOM);
        mg.hd = strength;
        mg.props[CUSTOM_SPELL_LIST_KEY].get_vector().push_back(SPELL_FORCE_LANCE);
        mg.non_actor_summoner = "Xom";

        if (create_monster(mg))
            created++;
    }

    if (created > 0)
    {
        const string note = make_stringf("summons %d living force lance%s",
                                         created,
                                         created > 1 ? "s" : "");

        god_speaks(GOD_XOM,  _get_xom_speech("force lance fleet").c_str());
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _xom_enchant_monster(int sever, bool helpful)
{
    vector<monster*> targetable;
    enchant_type ench;
    string ench_name = "";
    int xl = you.experience_level;
    int affected = 0;
    int cap = 0;
    int time = 0;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (_choose_enchantable_monster(**mi))
            targetable.push_back(*mi);
    }

    if (helpful) // To the player, not the monster.
    {
        cap = 1 + div_rand_round(xl, 5);
        cap = random_range(1, cap);
        ench = random_choose_weighted(200 + xl * 4, ENCH_PETRIFYING,
                                      300 - xl * 5, ENCH_PARALYSIS,
                                      300 - xl * 5, ENCH_VITRIFIED,
                                      200 + xl * 4, ENCH_SLOW);
    }
    else
    {
        cap = 1 + div_rand_round(xl, 7);
        cap = random_range(1, cap);
        ench = random_choose_weighted(200 + xl * 4, ENCH_HASTE,
                                      300, ENCH_MIGHT,
                                      200 + xl * 2, ENCH_REGENERATION,
                                      300 - xl * 5, ENCH_RESISTANCE);
    }

    ench_name = description_for_ench(ench);

    if (ench == ENCH_PETRIFYING || ench == ENCH_PARALYSIS)
        time = 30 + random2(sever / 10);
    else if (ench == ENCH_VITRIFIED || ench == ENCH_SLOW)
        time = 200 + random2(sever);
    else if (ench == ENCH_HASTE || ench == ENCH_MIGHT)
        time = 400 + random2(sever * 4);
    else if (ench == ENCH_REGENERATION || ench == ENCH_RESISTANCE)
        time = 800 + random2(sever * 4);

    god_speaks(GOD_XOM,
               helpful ? _get_xom_speech("good enchant monster").c_str()
                       : _get_xom_speech("bad enchant monster").c_str());

    shuffle_array(targetable);

    for (monster *application : targetable)
    {
        if (affected == cap)
            break;

        mprf("%s suddenly %s %s!",
              application->name(DESC_THE).c_str(),
              (ench == ENCH_PETRIFYING || ench == ENCH_REGENERATION) ? "starts" : "looks",
              ench_name.c_str());

        application->add_ench(mon_enchant(ench, 0, nullptr, time));
        affected++;
    }

    // Take a note.
    const string note = make_stringf("enchant monster%s (%s, %s)",
                                     affected >= 1 ? "s" : "",
                                     helpful ? "good" : "bad",
                                     ench_name.c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static void _xom_good_enchant_monster(int sever)
{
    _xom_enchant_monster(sever, true);
}
static void _xom_bad_enchant_monster(int sever)
{
    _xom_enchant_monster(sever, false);
}

// Look for monsters sufficiently weak enough for Xom to buff.
static vector<monster*> _xom_find_weak_monsters(bool range)
{
    vector<monster*> targetable;
    int runes = (you.runes.count() > 3) ? div_rand_round(you.runes.count(), 3) : 0;
    int maximum_hd = 3 + you.experience_level * 7 / 20 + runes;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        // No counting battlespheres or orbs of destruction. Try not to buff
        // the same target multiple times by checking the most prominent ones.
        // Fuzz the HD range to make it harder to deliberately plan around.
        if (mons_attitude(**mi) == ATT_FRIENDLY
            && !mons_is_conjured(mi->type)
            && !mons_is_tentacle_or_tentacle_segment(mi->type)
            && !(mi->has_ench(ENCH_HASTE) && mi->has_ench(ENCH_INVIS)
                 && mi->has_ench(ENCH_EMPOWERED_SPELLS) && mi->has_ench(ENCH_MIGHT))
            && mi->get_hit_dice() < maximum_hd + (range ? random_range(-2, 1) : 0))
        {
            targetable.push_back(*mi);
        }
    }

    return targetable;
}

// Throw nearly every single monster buff in the game on a random ally in sight.
// Xom will only buff very low HD monsters this way, and will summon weaklings
// if you don't have anything weak enough to get a buff.
static void _xom_hyper_enchant_monster(int sever)
{
    vector<enchant_type> buff_list { ENCH_MIGHT, ENCH_HASTE, ENCH_INVIS,
                                     ENCH_EMPOWERED_SPELLS, ENCH_REPEL_MISSILES,
                                     ENCH_RESISTANCE, ENCH_REGENERATION,
                                     ENCH_STRONG_WILLED, ENCH_TOXIC_RADIANCE,
                                     ENCH_DOUBLED_VIGOUR, ENCH_MIRROR_DAMAGE,
                                     ENCH_SWIFT };
    vector<monster*> targetable = _xom_find_weak_monsters(true);
    int time = random_range(200, 200 + sever * 2);
    int xl = you.experience_level;
    int good_god = you_worship(GOD_ELYVILON) || you_worship(GOD_ZIN) ||
                   you_worship(GOD_SHINING_ONE);
    int buff_count = 0;

    if (targetable.empty())
    {
        monster_type mon_type;

        // Mostly more mundane choices than usual Xom summons.
        if (xl < 7)
        {
            mon_type = (good_god || one_chance_in(4)) ? MONS_IGUANA
                                                      : MONS_CERULEAN_IMP;
        }
        else if (xl < 14 + random_range(-1, 1))
        {
            mon_type = (good_god || one_chance_in(4)) ? MONS_BLACK_BEAR
                                                      : MONS_HELL_RAT;
        }
        else if (xl < 21 + random_range(-1, 1))
        {
            mon_type = (good_god || one_chance_in(4)) ? MONS_WYVERN
                                                      : MONS_HELL_HOUND;
        }
        else
        {
            mon_type = (good_god || one_chance_in(3)) ? MONS_ELEPHANT
                                                      : MONS_TOENAIL_GOLEM;
        }

        mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(),
                    MHITYOU, MG_FORCE_BEH | MG_FORCE_PLACE);
        mg.set_summoned(&you, 3, MON_SUMM_AID, GOD_XOM);
        mg.non_actor_summoner = "Xom";
        monster* mon = create_monster(mg);

        if (mon)
        {
            targetable.insert(targetable.begin(), mon);
            string summ = make_stringf("%s pulls itself out of thin air.",
                                        targetable[0]->name(DESC_A, true).c_str());
            god_speaks(GOD_XOM, summ.c_str());
        }
    }
    else
    {
        shuffle_array(targetable);

        if (_xom_feels_nasty())
        {
            sort(targetable.begin(), targetable.end(),
            [](const monster* a, const monster* b)
            {return a->get_hit_dice() < b->get_hit_dice();});
        }
    }

    if (!targetable.empty())
    {
        string lines = "";

        for (enchant_type apply : buff_list)
        {
            string ench_name = description_for_ench(apply).c_str();

            // Avoid repeats or completely useless effects.
            if ((targetable[0]->has_ench(apply)
                || apply == ENCH_MIGHT && !mons_has_attacks(*(targetable[0])))
                || (apply == ENCH_EMPOWERED_SPELLS && !targetable[0]->is_actual_spellcaster())
                || (apply == ENCH_SWIFT && targetable[0]->is_stationary())
                || (apply == ENCH_STRONG_WILLED && targetable[0]->willpower() == WILL_INVULN))
            {
                continue;
            }

            if (apply == ENCH_REPEL_MISSILES || apply == ENCH_REGENERATION
                || apply == ENCH_TOXIC_RADIANCE || apply == ENCH_MIRROR_DAMAGE
                || apply == ENCH_SWIFT)
            {
                lines += make_stringf("starts %s, ", ench_name.c_str());
            }
            else if (apply == ENCH_EMPOWERED_SPELLS)
                lines += make_stringf("has its spells empowered, ");
            else
                lines += make_stringf("looks %s, ", ench_name.c_str());

            targetable[0]->add_ench(mon_enchant(apply, 0, nullptr, time));
            buff_count++;
        }

    if (buff_count > 0)
    {
        if (targetable[0]->hit_points < targetable[0]->max_hit_points)
        {
            targetable[0]->heal(targetable[0]->max_hit_points);
            lines += make_stringf("is healed, ");
        }

        god_speaks(GOD_XOM, _get_xom_speech("good hyper enchant monster").c_str());

        // Rather than figuring out sentence structure from the above list,
        // just staple on a line from casting Cantrip onto the end instead.
        mprf("%s suddenly %sand looks braver for a moment!",
              targetable[0]->name(DESC_THE, true).c_str(), lines.c_str());
    }

    const string note = make_stringf("buffed friendly %s %d %s",
                                     targetable[0]->name(DESC_PLAIN, true).c_str(),
                                     buff_count, buff_count == 1 ? "time" : "times" );
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
}

static void _xom_mass_charm(int sever)
{
    vector<monster*> targetable;
    int affected = 0;
    int iters = 0;
    int hd_target = 0;
    int target_count = 0;
    int time = 500 + random2(sever);

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        // While Xom won't care, other gods of blasphemer chaos knights might.
        if (!god_hates_monster(**mi) && _choose_enchantable_monster(**mi))
        {
            targetable.push_back(*mi);
            hd_target += mi->get_hit_dice();
            target_count++;
        }
    }

    hd_target /= target_count;
    shuffle_array(targetable);

    god_speaks(GOD_XOM, _get_xom_speech("mass charm").c_str());

    for (monster *application : targetable)
    {
        // Always guarantee one is affected and one is not affected, regardless
        // of HD. Otherwise, mostly try to get the weaker half.
        if (iters == 0 || (iters > 1 && affected <= target_count / 2
            && application->get_hit_dice() + random_range(-1, 1) <= hd_target))
        {
            simple_monster_message(*application, " is charmed.");
            application->add_ench(mon_enchant(ENCH_CHARM, 0, nullptr, time));
            affected++;
        }

        iters++;
    }

    const string note = make_stringf("charmed %d monster%s",
                                     affected,  affected != 1 ? "s" : "");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

// Xom makes some scary skeletons pop out.
// This halves enemy willpower, drains them, and fears them.
static void _xom_wave_of_despair(int sever)
{
    int skeleton_count = 0;
    god_speaks(GOD_XOM, _get_xom_speech("wave of despair").c_str());

    // As is done in wiz-item.cc L#154, apparently the simplest way we have for
    // making decorative skeletons is to make a monster and then kill it. Sure.
    for (distance_iterator di(you.pos(), true, true, 2); di; ++di)
    {
        if (!monster_at(*di) && !cell_is_solid(*di)
            && env.grid(*di) != DNGN_ORB_DAIS)
        {
            monster dummy;
            dummy.type = MONS_HUMAN; // maybe random floor monsters? player genus?
            dummy.position = *di;

            item_def* corpse = place_monster_corpse(dummy, true);
            if (corpse)
            {
                turn_corpse_into_skeleton(*corpse);
                skeleton_count++;
            }
        }
    }

    if (skeleton_count)
        mpr("Skeletons, inanimate yet cursed, drop down from the ceiling.");

    for (int i = 0; i <= you.current_vision; ++i)
    {
        for (distance_iterator di(you.pos(), false, false, i); di; ++di)
        {
            if (grid_distance(you.pos(), *di) == i && !feat_is_solid(env.grid(*di))
                && you.see_cell_no_trans(*di))
            {
                flash_tile(*di, random_choose(DARKGRAY, MAGENTA), 0);
            }
        }

        animation_delay(35, true);
        view_clear_overlays();
    }

    mprf(MSGCH_DANGER, "A draining tide of despair and horror washes over you and your surroundings!");

    const int pow = 50 + random_range(sever / 2, sever);

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            mon->strip_willpower(&you, pow, true);

            if (mon->holiness() & (MH_NATURAL | MH_PLANT))
                mon->add_ench(mon_enchant(ENCH_DRAINED, 2, &you, pow));

            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.strip_willpower(&you, pow, true);
    mass_enchantment(ENCH_FEAR, pow * 5);

    const string note = make_stringf("spooky wave of despair");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

// Xom hastes, slows, or paralyzes the player and everything else in sight
// for the same length of time (give or take time slices and base speed).
// Mostly screws with whatever else might join the fight afterwards,
// ally creation, and also with damage-over-time effects.
static void _xom_time_control(int sever)
{
    duration_type dur;
    enchant_type ench;
    string xomline;
    string message;
    string note;
    int time;
    bool bad = true;

    if (x_chance_in_y(1, 3))
    {
        dur = DUR_HASTE;
        ench = ENCH_HASTE;
        xomline = "fast forward";
        if (you.stasis())
        {
            message = "Your stasis prevents you from being hasted, but everything else in sight speeds up!";
            note = "hasted everything in sight";
        }
        else
        {
            message = "You and everything else in sight speeds up!";
            note = "hasted player and everything else in sight";
        }
        time = random_range(100, 200) + sever / 3;
    }
    else if (coinflip())
    {
        dur = DUR_SLOW;
        ench = ENCH_SLOW;
        xomline = "slow motion";
        if (you.stasis())
        {
            message = "Your stasis prevents you from being slowed, but everything else in sight slows down!";
            note = "slowed everything in sight";
            bad = false;
        }
        else
        {
            message = "You and everything else in sight slows down!";
            note = "slowed player and everything else";
        }
        time = random_range(100, 200) + sever / 3;
    }
    else
    {
        dur = DUR_PARALYSIS;
        ench = ENCH_PARALYSIS;
        xomline = "pause";
        if (you.stasis())
        {
            message = "Your stasis prevents you from being paralysed, but everything else in sight stops moving!";
            note = "paralysed everything in sight";
            bad = false;
        }
        else
        {
            message = "You and everything else in sight suddenly stops moving!";
            note = "paralysed player and everything else";

            // Less of a decent joke if it directly kills, so.
            if (cloud_at(you.pos()) && !is_harmless_cloud(cloud_at(you.pos())->type))
                delete_cloud(you.pos());
        }
        time = random_range(30, 50);
    }

    god_speaks(GOD_XOM, _get_xom_speech(xomline).c_str());

    mprf(bad ? MSGCH_WARN : MSGCH_GOD, "%s", message.c_str());

    if (!you.stasis())
        you.increase_duration(dur, time / 10);

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if ((!mons_has_attacks(**mi) && ench != ENCH_PARALYSIS) || mi->stasis())
            continue;

        mi->add_ench(mon_enchant(ench, 0, nullptr, time));
    }

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

/// Toss some fog around the player. Helping...?
static void _xom_fog(int /*sever*/)
{
    big_cloud(CLOUD_RANDOM_SMOKE, &you, you.pos(), 50, 8 + random2(8));
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "fog"), true);
    god_speaks(GOD_XOM, _get_xom_speech("cloud").c_str());
}

static item_def* _xom_get_random_worn_ring()
{
    item_def* item;
    vector<item_def*> worn_rings;

    for (int slots = EQ_FIRST_JEWELLERY; slots <= EQ_LAST_JEWELLERY; slots++)
    {
        if (slots == EQ_AMULET)
            continue;

        if (item = you.slot_item(static_cast<equipment_type>(slots)))
            worn_rings.push_back(item);
    }

    if (worn_rings.empty())
        return nullptr;

    return worn_rings[random2(worn_rings.size())];
}

static void _xom_pseudo_miscast(int /*sever*/)
{
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "silly message"), true);
    god_speaks(GOD_XOM, _get_xom_speech("zero miscast effect").c_str());

    vector<string> messages;
    vector<string> priority;

    vector<item_def *> inv_items;
    for (auto &item : you.inv)
    {
        if (item.defined() && !item_is_equipped(item))
            inv_items.push_back(&item);
    }

    // Assure that the messages vector has at least one element.
    messages.emplace_back("Nothing appears to happen... Ominous!");

    ///////////////////////////////////
    // Dungeon feature dependent stuff.

    vector<dungeon_feature_type> in_view;
    vector<string> in_view_name;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const dungeon_feature_type feat = env.grid(*ri);
        const string feat_name = feature_description_at(*ri, false,
                                                        DESC_THE);
        in_view.push_back(feat);
        in_view_name.push_back(feat_name);
    }

    for (size_t iv = 0; iv < in_view.size(); ++iv)
    {
        string str;

        if (in_view[iv] == DNGN_LAVA)
            str = _get_xom_speech("feature lava");
        else if (in_view[iv] == DNGN_SHALLOW_WATER
                 || in_view[iv] == DNGN_FOUNTAIN_BLUE
                 || in_view[iv] == DNGN_FOUNTAIN_SPARKLING)
        {
            str = _get_xom_speech("feature shallow water");
        }
        else if (in_view[iv] == DNGN_DEEP_WATER)
            str = _get_xom_speech("feature deep water");
        else if (in_view[iv] == DNGN_FOUNTAIN_BLOOD)
            str = _get_xom_speech("feature blood");
        else if (in_view[iv] == DNGN_DRY_FOUNTAIN)
            str = _get_xom_speech("feature dry");
        else if (feat_is_statuelike(in_view[iv]))
            str = _get_xom_speech("feature statuelike");
        else if (feat_is_tree(in_view[iv]))
            str = _get_xom_speech("feature tree");
        else if (in_view[iv] == DNGN_CLEAR_ROCK_WALL
                 || in_view[iv] == DNGN_CLEAR_STONE_WALL
                 || in_view[iv] == DNGN_CLEAR_PERMAROCK_WALL
                 || in_view[iv] == DNGN_CRYSTAL_WALL)
        {
            str = _get_xom_speech("feature translucent wall");
        }
        else if (in_view[iv] == DNGN_METAL_WALL)
            str = _get_xom_speech("feature metal wall");
        else if (in_view[iv] == DNGN_STONE_ARCH)
            str = _get_xom_speech("feature stone arch");

        if (!str.empty())
        {
            str = replace_all(str, "@the_feature@", in_view_name[iv]);
            str = replace_all(str, "@The_feature@",
                              uppercase_first(in_view_name[iv]));

            messages.push_back(str);
        }
    }

    const dungeon_feature_type feat = env.grid(you.pos());

    if (!feat_is_solid(feat) && feat_stair_direction(feat) == CMD_NO_CMD
        && !feat_is_trap(feat) && feat != DNGN_STONE_ARCH
        && !feat_is_open_door(feat) && feat != DNGN_ABANDONED_SHOP)
    {
        const string feat_name = feature_description_at(you.pos(), false,
                                                        DESC_THE);
        string str;

        if (you.airborne())
        {
            str = _get_xom_speech(
                      feat_is_water(feat) ? "underfoot airborne water"
                                          : "underfoot airborne general");
        }
        else
        {
            str = _get_xom_speech(
                      feat_is_water(feat) ? "underfoot water"
                                          : "underfoot general");
        }

        str = replace_all(str, "@the_feature@", feat_name);
        str = replace_all(str, "@The_feature@", uppercase_first(feat_name));

        // Don't put airborne messages into the priority vector for
        // anyone who can fly a lot.
        if (you.racial_permanent_flight())
            messages.push_back(str);
        else
            priority.push_back(str);
    }

    if (!inv_items.empty())
    {
        const item_def &item = **random_iterator(inv_items);
        string name = item.name(DESC_YOUR, false, false, false);
        string str;

        if (feat_has_solid_floor(feat))
        {
            str = _get_xom_speech(
                      item.quantity == 1 ? "floor inventory singular"
                                         : "floor inventory plural");
        }
        else
            str = _get_xom_speech(
                      item.quantity == 1 ? "inventory singular"
                                         : "inventory plural");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    //////////////////////////////////////////////
    // Body, player species, transformations, etc.

    if (get_form()->flesh_equivalent.empty()
        && starts_with(species::skin_name(you.species), "bandage")
        && you_can_wear(EQ_BODY_ARMOUR, true) != false)
    {
        string str = _get_xom_speech(
                (!you.airborne() && !you.swimming()) ? "floor bandages"
                                                     : "bandages");
        messages.push_back(str);
    }

    if (player_has_ears())
    {
        string str = _get_xom_speech("ears");
        messages.push_back(str);
    }

    {
        string str = _get_xom_speech(
                you.get_mutation_level(MUT_MISSING_EYE) ? "one eye"
                                                        : "eyes");
        messages.push_back(str);
    }

    {
        string str = _get_xom_speech("mouth");
        messages.push_back(str);
    }

    if (player_has_hair())
    {
        string str = _get_xom_speech("hair");
        messages.push_back(str);
    }

    if (player_has_feet() && !you.airborne() && !you.cannot_act())
    {
        string str = _get_xom_speech("impromptu dance");

        str = replace_all(str, "@hand@", you.hand_name(false));
        str = replace_all(str, "@hands@", you.hand_name(true));

        str = replace_all(str, "@foot@", you.foot_name(false));
        str = replace_all(str, "@feet@", you.foot_name(true));

        messages.push_back(str);
    }

    if (you.has_tail())
    {
        string str = _get_xom_speech("tail");
        messages.push_back(str);
    }

    {
        string str = _get_xom_speech("random body part singular");

        str = replace_all(str, "@random_body_part_any_singular@",
                          random_body_part_name(false, BPART_ANY));
        str = replace_all(str, "@random_body_part_internal_singular@",
                          random_body_part_name(false, BPART_INTERNAL));
        str = replace_all(str, "@random_body_part_external_singular@",
                          random_body_part_name(false, BPART_EXTERNAL));

        messages.push_back(str);
    }

    {
        string str = _get_xom_speech("random body part plural");

        str = replace_all(str, "@random_body_part_any_plural@",
                          random_body_part_name(true, BPART_ANY));
        str = replace_all(str, "@random_body_part_internal_plural@",
                          random_body_part_name(true, BPART_INTERNAL));
        str = replace_all(str, "@random_body_part_external_plural@",
                          random_body_part_name(true, BPART_EXTERNAL));

        messages.push_back(str);
    }

    ///////////////////////////
    // Equipment related stuff.

    if (you_can_wear(EQ_WEAPON, true) != false
        && !you.slot_item(EQ_WEAPON))
    {
        const bool one_handed = you.slot_item(EQ_OFFHAND)
                                || you.get_mutation_level(MUT_MISSING_HAND);
        string str = _get_xom_speech(one_handed ? "unarmed one hand"
                                                : "unarmed two hands");

        str = replace_all(str, "@hand@", you.hand_name(false));
        str = replace_all(str, "@hands@", you.hand_name(true));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_CLOAK))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("cloak slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_HELMET))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("helmet slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_OFFHAND))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("offhand slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_GLOVES))
    {
        string gloves_name = item->name(DESC_BASENAME, false, false, false);
        // XXX: If the gloves' name doesn't start with "pair of", make it do so,
        // so that it always comes out as singular for grammar purposes. This
        // happens with the Mad Mage's Maulers and Delatra's gloves; see
        // item_def::name_aux().
        if (gloves_name.find("pair of ") != 0)
            gloves_name = "pair of " + gloves_name;

        string name = "your " + gloves_name;
        string str = _get_xom_speech("gloves slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_BOOTS))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("boots slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_GIZMO))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("gizmo slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_ring())
    {
        // Don't just say "your ring" here. We want to know which one.
        string name = "your " + item->name(DESC_QUALNAME, false, false, false);
        string str = _get_xom_speech("ring slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        string ring_holder_singular;
        string ring_holder_plural;

        // The ring on the amulet slot is on a neck/mantle, not a hand.
        if (item == you.slot_item(EQ_RING_AMULET))
        {
            // XXX: Logic duplicated from item_def::name().
            if (you.species == SP_OCTOPODE && form_keeps_mutations())
            {
                ring_holder_singular = "mantle";
                ring_holder_plural = "mantles";
            }
            else
            {
                ring_holder_singular = "neck";
                ring_holder_plural = "necks";
            }
        }
        else
        {
            ring_holder_singular = you.hand_name(false);
            ring_holder_plural = you.hand_name(true);
        }

        str = replace_all(str, "@hand@", ring_holder_singular);
        str = replace_all(str, "@hands@", ring_holder_plural);

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_BODY_ARMOUR))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str;

        if (name.find("dragon") != string::npos)
            str = _get_xom_speech("dragon armour");
        else if (item->sub_type == ARM_ANIMAL_SKIN)
            str = _get_xom_speech("animal skin");
        else if (item->sub_type == ARM_LEATHER_ARMOUR)
            str = _get_xom_speech("leather armour");
        else if (item->sub_type == ARM_ROBE)
            str = _get_xom_speech("robe");
        else if (item->sub_type >= ARM_RING_MAIL
                 && item->sub_type <= ARM_PLATE_ARMOUR)
        {
            str = _get_xom_speech("metal armour");
        }

        if (!str.empty())
        {
            str = replace_all(str, "@your_item@", name);
            str = replace_all(str, "@Your_item@", uppercase_first(name));

            messages.push_back(str);
        }
    }

    string str;

    if (!priority.empty() && coinflip())
        str = priority[random2(priority.size())];
    else
        str = messages[random2(messages.size())];

    str = maybe_pick_random_substring(str);
    mpr(str);
}

static bool _miscast_is_nasty(int sever)
{
    return sever >= 5 && _xom_feels_nasty();
}

static void _xom_chaos_upgrade(int /*sever*/)
{
    monster* mon = choose_random_nearby_monster(_choose_chaos_upgrade);

    if (!mon)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("chaos upgrade").c_str());

    mon_inv_type slots[] = {MSLOT_WEAPON, MSLOT_ALT_WEAPON, MSLOT_MISSILE};

    bool rc = false;
    for (int i = 0; i < 3 && !rc; ++i)
    {
        item_def* const item = mon->mslot_item(slots[i]);
        if (item && _is_chaos_upgradeable(*item))
        {
            _do_chaos_upgrade(*item, mon);
            rc = true;
        }
    }
    ASSERT(rc);

    // Wake the monster up.
    behaviour_event(mon, ME_ALERT, &you);

    if (rc)
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "chaos upgrade"), true);
}

static void _xom_player_confusion_effect(int sever)
{
    const bool conf = you.confused();

    if (!confuse_player(5 + random2(3), true))
        return;

    god_speaks(GOD_XOM, _get_xom_speech("confusion").c_str());
    mprf(MSGCH_WARN, "You are %sconfused.",
         conf ? "more " : "");

    // At higher severities, Xom is less likely to confuse surrounding
    // creatures.
    bool mons_too = false;
    if (random2(sever) < 30)
    {
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (random2(sever) > 30)
                continue;
            _confuse_monster(*mi, sever);
            mons_too = true;
        }
    }

    // Take a note.
    string conf_msg = "confusion";
    if (mons_too)
        conf_msg += " (+ monsters)";
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, conf_msg), true);
}

static bool _valid_floor_grid(coord_def pos)
{
    if (!in_bounds(pos))
        return false;

    return env.grid(pos) == DNGN_FLOOR;
}

bool move_stair(coord_def stair_pos, bool away, bool allow_under)
{
    if (!allow_under)
        ASSERT(stair_pos != you.pos());

    dungeon_feature_type feat = env.grid(stair_pos);
    ASSERT(feat_stair_direction(feat) != CMD_NO_CMD);

    coord_def begin, towards;

    bool stairs_moved = false;
    if (away)
    {
        // If the staircase starts out under the player, first shove it
        // onto a neighbouring grid.
        if (allow_under && stair_pos == you.pos())
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
                    if (env.grid(*ai) == DNGN_FLOOR
                        && (tries || _valid_floor_grid(*ai + *ai - stair_pos))
                        && one_chance_in(++adj_count))
                    {
                        new_pos = *ai;
                    }

                if (!tries && new_pos != stair_pos)
                    break;
            }

            if (new_pos == stair_pos)
                return false;

            if (!slide_feature_over(stair_pos, new_pos, true))
                return false;

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
            return false;

        begin   = stair_pos;
        towards = you.pos();
    }

    ray_def ray;
    if (!find_ray(begin, towards, ray, opc_solid_see))
    {
        mprf(MSGCH_ERROR, "Couldn't find ray between player and stairs.");
        return stairs_moved;
    }

    // Don't start off under the player.
    if (away)
        ray.advance();

    bool found_stairs = false;
    int  past_stairs  = 0;
    while (in_bounds(ray.pos()) && you.see_cell(ray.pos())
           && !cell_is_solid(ray.pos()) && ray.pos() != you.pos())
    {
        if (ray.pos() == stair_pos)
            found_stairs = true;
        if (found_stairs)
            past_stairs++;
        ray.advance();
    }
    past_stairs--;

    if (!away && cell_is_solid(ray.pos()))
    {
        // Transparent wall between stair and player.
        return stairs_moved;
    }

    if (away && !found_stairs)
    {
        if (cell_is_solid(ray.pos()))
        {
            // Transparent wall between stair and player.
            return stairs_moved;
        }

        mprf(MSGCH_ERROR, "Ray didn't cross stairs.");
    }

    if (away && past_stairs <= 0)
    {
        // Stairs already at edge, can't move further away.
        return stairs_moved;
    }

    if (!in_bounds(ray.pos()) || ray.pos() == you.pos())
        ray.regress();

    while (!you.see_cell(ray.pos()) || env.grid(ray.pos()) != DNGN_FLOOR)
    {
        ray.regress();
        if (!in_bounds(ray.pos()) || ray.pos() == you.pos()
            || ray.pos() == stair_pos)
        {
            // No squares in path are a plain floor.
            return stairs_moved;
        }
    }

    ASSERT(stair_pos != ray.pos());

    string stair_str = feature_description_at(stair_pos, false, DESC_THE);

    mprf("%s slides %s you!", stair_str.c_str(),
         away ? "away from" : "towards");

    // Animate stair moving.
    const feature_def &feat_def = get_feature_def(feat);

    bolt beam;

    beam.range   = INFINITE_DISTANCE;
    beam.flavour = BEAM_VISUAL;
    beam.glyph   = feat_def.symbol();
    beam.colour  = feat_def.colour();
    beam.source  = stair_pos;
    beam.target  = ray.pos();
    beam.name    = "STAIR BEAM";
    beam.draw_delay = 50; // Make beam animation slower than normal.

    beam.aimed_at_spot = true;
    beam.fire();

    // Clear out "missile trails"
    viewwindow();
    update_screen();

    if (!swap_features(stair_pos, ray.pos(), false, false))
    {
        mprf(MSGCH_ERROR, "_move_stair(): failed to move %s",
             stair_str.c_str());
        return stairs_moved;
    }

    return true;
}

static vector<coord_def> _nearby_stairs()
{
    vector<coord_def> stairs_avail;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
    {
        if (!cell_see_cell(you.pos(), *ri, LOS_SOLID_SEE))
            continue;

        dungeon_feature_type feat = env.grid(*ri);
        if (feat_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
        }
    }

    return stairs_avail;
}

static void _xom_repel_stairs(bool unclimbable)
{
    vector<coord_def> stairs_avail  = _nearby_stairs();

    // Only works if there are stairs in view.
    if (stairs_avail.empty())
        return;

    bool real_stairs = false;
    for (auto loc : stairs_avail)
        if (feat_is_staircase(env.grid(loc)))
            real_stairs = true;

    // Don't mention staircases if there aren't any nearby.
    string stair_msg = _get_xom_speech("repel stairs");
    string feat_name;

    if (!real_stairs)
    {
        feat_name =
            feat_is_escape_hatch(env.grid(stairs_avail[0])) ? "escape hatch"
                                                            : "gate";
    }
    else
        feat_name = "staircase";

    stair_msg = replace_all(stair_msg, "@staircase@", feat_name);

    god_speaks(GOD_XOM, stair_msg.c_str());

    you.duration[DUR_REPEL_STAIRS_MOVE] = 1000; // 100 turns
    if (unclimbable)
        you.duration[DUR_REPEL_STAIRS_CLIMB] = 500; // 50 turns

    shuffle_array(stairs_avail);
    int count_moved = 0;
    for (coord_def stair : stairs_avail)
        if (move_stair(stair, true, true))
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
}

static void _xom_moving_stairs(int) { _xom_repel_stairs(false); }
static void _xom_unclimbable_stairs(int) { _xom_repel_stairs(true); }

static void _xom_cloud_trail(int /*sever*/)
{
    you.duration[DUR_CLOUD_TRAIL] = random_range(600, 1200);
    // 80% chance of a useful trail
    cloud_type ctype = random_choose_weighted(20, CLOUD_CHAOS,
                                              9,  CLOUD_MAGIC_TRAIL,
                                              5,  CLOUD_MIASMA,
                                              5,  CLOUD_PETRIFY,
                                              5,  CLOUD_MUTAGENIC,
                                              4,  CLOUD_MISERY,
                                              1,  CLOUD_SALT,
                                              1,  CLOUD_BLASTMOTES);

    bool suppressed = false;
    if (you_worship(GOD_ZIN) && (ctype == CLOUD_CHAOS || ctype == CLOUD_MUTAGENIC))
        suppressed = true;
    else if (is_good_god(you.religion) && (ctype == CLOUD_MIASMA || ctype == CLOUD_MISERY))
        suppressed = true;

    if (suppressed)
        ctype = CLOUD_SALT;

    you.props[XOM_CLOUD_TRAIL_TYPE_KEY] = ctype;

    // Need to explicitly set as non-zero. Use a clean half of the power cap.
    if (you.props[XOM_CLOUD_TRAIL_TYPE_KEY].get_int() == CLOUD_BLASTMOTES)
        you.props[BLASTMOTE_POWER_KEY] = 25;

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "cloud trail"), true);

    const string speech = _get_xom_speech("cloud trail");
    god_speaks(GOD_XOM, speech.c_str());

    if (suppressed)
        simple_god_message(" purifies the foul vapours!");
}

static void _xom_statloss(int /*sever*/)
{
    const string speech = _get_xom_speech("draining or torment");

    const stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
    int loss = 1;

    // Don't set the player to statzero unless Xom is being nasty.
    if (_xom_feels_nasty())
        loss = 1 + random2(3);
    else if (you.stat(stat) <= loss)
        return;

    god_speaks(GOD_XOM, speech.c_str());
    lose_stat(stat, loss);

    const char* sstr[3] = { "Str", "Int", "Dex" };
    const string note = make_stringf("stat loss: -%d %s (%d/%d)",
                                     loss, sstr[stat], you.stat(stat),
                                     you.max_stat(stat));

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static void _xom_draining(int /*sever*/)
{
    int power = 100;
    const string speech = _get_xom_speech("draining or torment");
    god_speaks(GOD_XOM, speech.c_str());

    if (you.experience_level < 4
        || (you.hp_max_adj_temp < 0 && !_xom_feels_nasty()))
    {
        power /= 2;
    }

    drain_player(power, true);

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "draining"), true);
}

static void _xom_torment(int /*sever*/)
{
    if (_xom_feels_nasty())
    {
        god_speaks(GOD_XOM, _get_xom_speech("draining or torment").c_str());
        torment_player(0, TORMENT_XOM);
    }
    else
    {
        god_speaks(GOD_XOM, _get_xom_speech("torment all").c_str());
        torment(nullptr, TORMENT_XOM, you.pos());
    }

    const string note = make_stringf("torment%s(%d/%d hp)",
                                      _xom_feels_nasty() ? " all (player " : " (",
                                      you.hp, you.hp_max);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static monster* _xom_summon_hostile(monster_type hostile)
{
    // Fuzz the monster's placement.
    int placeable_count = 0;
    int distance = you.penance[GOD_XOM] ? 5 : 3;
    coord_def spot = you.pos();

    for (radius_iterator ri(you.pos(), distance, C_SQUARE, LOS_NO_TRANS, true);
             ri; ++ri)
    {
        if ((!feat_has_solid_floor(env.grid(*ri))) || cell_is_solid(*ri)
            || actor_at(*ri) || grid_distance(*ri, you.pos()) <= 1)
        {
            continue;
        }

        if (one_chance_in(++placeable_count))
            spot = *ri;
    }

    monster* mon = create_monster(mgen_data::hostile_at(hostile, true, spot)
                          .set_summoned(nullptr, 4, MON_SUMM_WRATH, GOD_XOM)
                          .set_non_actor_summoner("Xom"));

    // To prevent high-stealth players from just ducking around a corner
    // and losing some chunk of hostile summons, give them a helping hand.
    if (mon)
    {
        mon->target = you.pos();
        mon->foe = MHITYOU;
        mon->behaviour = BEH_SEEK;

        if (mon->type == MONS_REAPER)
            _do_chaos_upgrade(*mon->weapon(), mon);
    }

    return mon;
}

static void _xom_summon_hostiles(int sever)
{
    int strengthRoll = random2(1000 - (MAX_PIETY- sever) * 5);
    int num_summoned = 0;
    const bool shadow_creatures = one_chance_in(4);

    if (shadow_creatures)
    {
        // Small number of shadow creatures, but still a little touch of chaos.
        int multiplier = 1;
        int range_cap = div_rand_round(you.experience_level, 8);

        if (_xom_is_bored())
            multiplier = 2;
        else if (you.penance[GOD_XOM])
        {
            multiplier = 3;
            range_cap = 4;
        }

        int count = random_range(1, max(1, range_cap)) * multiplier;

        for (int i = 0; i < multiplier; ++i)
            if (_xom_summon_hostile(_xom_random_pal(strengthRoll, false)))
                num_summoned++;

        for (int i = 0; i < count; ++i)
            if (_xom_summon_hostile(RANDOM_MOBILE_MONSTER))
                num_summoned++;
    }
    else
    {
        int count = _xom_pal_counting(strengthRoll, false);

        for (int i = 0; i < count; ++i)
        {
            monster_type mon_type = _xom_random_pal(strengthRoll, false);

            // As seen in _xom_send_allies, add more of given banding monsters
            // without adding to the overall summon cap.
            int miniband = _xom_pal_minibands(mon_type);

            for (int j = 0; j < miniband; ++j)
                if (_xom_summon_hostile(mon_type))
                    num_summoned++;

            // Again as seen in _xom_send_allies, make summons likely to come in
            // pairs of the same type if neither a band nor later summoners,
            // while still respecting the overall summon cap.
            if (x_chance_in_y(2, 3) && miniband == 1 &&
                !_xom_pal_summonercheck(mon_type) && i < count - 1)
            {
                i += 1;
                if (_xom_summon_hostile(_xom_random_pal(strengthRoll, false)))
                    num_summoned++;
            }
        }
    }

    if (num_summoned > 0)
    {
        const string note = make_stringf("summons %d hostile %s%s",
                                         num_summoned,
                                         shadow_creatures ? "shadow creature"
                                                          : "chaos creature",
                                         num_summoned > 1 ? "s" : "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);

        const string speech = _get_xom_speech("hostile monster");
        god_speaks(GOD_XOM, speech.c_str());
    }
}

// Make two hostile clones and one friendly clone. Net danger, but maybe
// the player can make something of it anyway.
static void _xom_send_in_clones(int /*sever*/)
{
    const int friendly_count = you.allies_forbidden() ? 0 : 1;
    const int hostile_count = 2;
    int hostiles_summon_count = 0;
    int friendly_summon_count = 0;
    int power = 0;

    const string speech = _get_xom_speech("send in the clones");
    god_speaks(GOD_XOM, speech.c_str());

    for (int i = 0; i < friendly_count + hostile_count; i++)
    {
        monster* mon = get_free_monster();

        if (!mon || monster_at(you.pos()))
            return;

        mon->type = MONS_PLAYER;
        mon->behaviour = BEH_SEEK;
        mon->set_position(you.pos());
        mon->mid = MID_PLAYER;
        env.mgrid(you.pos()) = mon->mindex();

        if (hostiles_summon_count < hostile_count)
        {
            mon->attitude = ATT_HOSTILE;
            power = -1;
        }
        else
        {
            mon->attitude = ATT_FRIENDLY;
            power = 0;
        }

        if (mons_summon_illusion_from(mon, (actor *)&you, SPELL_NO_SPELL, power, true))
        {
            if (hostiles_summon_count < hostile_count)
                hostiles_summon_count++;
            else
                friendly_summon_count++;
        }
        mon->reset();
    }

    const string note = make_stringf("summoned %d hostile %s + %d friendly %s",
                                     hostiles_summon_count,
                                     hostiles_summon_count == 1 ? "illusion"
                                                                : "illusions",
                                     friendly_summon_count,
                                     friendly_summon_count == 1 ? "illusion"
                                                                : "illusions");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

// Roll per-monster whether they're neutral or hostile.
static bool _xom_maybe_neutral_summon(int sever, bool threat,
                                      monster_type mon_type)
{
    beh_type setting = (x_chance_in_y(sever, 300) || threat) ? BEH_HOSTILE
                                                             : BEH_NEUTRAL;

    mgen_data mg(mon_type, setting, you.pos(), MHITYOU, MG_FORCE_BEH);
    mg.set_summoned(&you, 4, MON_SUMM_AID, GOD_XOM);
    mg.non_actor_summoner = "Xom";

    return create_monster(mg);
}

// Drain you for 3/4ths of your mp, then create several brain worms, mana
// vipers, and / or quicksilver elementals from your mp based on your xl. If
// Xom's not bored or wrathful, severity gives a chance of some being neutral.
static void _xom_brain_drain(int sever)
{
    bool created = false;
    bool upgrade = you.penance[GOD_XOM];
    int xl = upgrade ? you.experience_level + 6 : you.experience_level;
    int worm_count = 0;
    int viper_count = 0;
    int quicksilver_count = 0;
    int drain = 1;

    drain = upgrade ? you.magic_points :
            min(you.magic_points, max(1, (you.magic_points * 3 / 4)));

    if (xl < 15)
        worm_count = xl < 5 ? 1 : min(4, div_rand_round(xl - 1, 3));

    if (xl > 11 && xl < 22)
        viper_count = xl < 14 ? 1 : min(4, div_rand_round(xl - 10, 3));

    if (xl > 19)
        quicksilver_count = xl < 22 ? 1 : min(3, div_rand_round(xl - 18, 3));

    // Xom won't do this anyway if you have no MP, so...
    if (drain > 0)
    {
        drain_mp(drain);

        for (int i = 0; i < worm_count; ++i)
            if (_xom_maybe_neutral_summon(sever, upgrade, MONS_BRAIN_WORM))
                created = true;

        for (int i = 0; i < viper_count; ++i)
            if (_xom_maybe_neutral_summon(sever, upgrade, MONS_MANA_VIPER))
                created = true;

        for (int i = 0; i < quicksilver_count; ++i)
            if (_xom_maybe_neutral_summon(sever, upgrade, MONS_QUICKSILVER_ELEMENTAL))
                created = true;

        const string speech = _get_xom_speech("brain drain");
        god_speaks(GOD_XOM, speech.c_str());

        if (created)
        {
            string react = _get_xom_speech("drained brain");

            react = replace_all(react, "@random_body_part_any_singular@",
                                random_body_part_name(false, BPART_ANY));
            react = replace_all(react, "@random_body_part_internal_singular@",
                                random_body_part_name(false, BPART_INTERNAL));
            react = replace_all(react, "@random_body_part_external_singular@",
                                random_body_part_name(false, BPART_EXTERNAL));
            react = replace_all(react, "@random_body_part_any_plural@",
                                random_body_part_name(true, BPART_ANY));
            react = replace_all(react, "@random_body_part_internal_plural@",
                                random_body_part_name(true, BPART_INTERNAL));
            react = replace_all(react, "@random_body_part_external_plural@",
                                random_body_part_name(true, BPART_EXTERNAL));

            react = maybe_pick_random_substring(react);

            const string note = make_stringf("drained mp, created monsters");
            mprf(MSGCH_WARN, "%s", react.c_str());
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
        }
        else
        {
            mprf(MSGCH_WARN, "You feel nearly all of your power leaking away!");
            const string note = make_stringf("drained mp");
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
        }
    }
}

// XXX: Possibly this could be used elsewhere, like defaults for wizard spells
// for those without speech, but we generally try to just give those custom
// messages per-monster anyway currently.
static const map<shout_type, string> speaking_keys = {
    { S_SILENT,         "dramatically intoning" },
    { S_SHOUT,          "shouting" },
    { S_BARK,           "barking" },
    { S_HOWL,           "howling" },
    { S_SHOUT2,         "shouting twice-over" },
    { S_ROAR,           "roaring" },
    { S_SCREAM,         "screaming" },
    { S_BELLOW,         "bellowing" },
    { S_BLEAT,          "bleating" },
    { S_TRUMPET,        "trumpeting" },
    { S_SCREECH,        "screeching" },
    { S_BUZZ,           "buzzing" },
    { S_MOAN,           "chillingly moaning" },
    { S_GURGLE,         "gurgling" },
    { S_CROAK,          "croaking" },
    { S_GROWL,          "growling" },
    { S_HISS,           "hissing" },
    { S_SKITTER,        "weaving" },
    { S_FAINT_SKITTER,  "faintly weaving" },
    { S_DEMON_TAUNT,    "sneering" },
    { S_CHERUB,         "speaking" },
    { S_SQUEAL,         "squealing" },
    { S_LOUD_ROAR,      "roaring" },
    { S_RUSTLE,         "scribing" },
    { S_SQUEAK,         "squeaking" },
};

static bool _has_min_recall_level()
{
    int min = you.penance[GOD_XOM] ? 3 : 5;
    return you.experience_level > min;
}

// As intense as these checks are, it basically just avoids monsters who
// would otherwise be preoccupied or aren't "real monsters" to kill.
static bool _valid_speaker_of_recall(monster* mon)
{
    return mon->alive() && !mon->wont_attack() && _should_recall(mon)
            && !mon->berserk_or_frenzied() && you.can_see(*mon)
            && !mons_is_tentacle_or_tentacle_segment(mon->type)
            && !mons_is_firewood(*mon) && !mons_is_object(mon->type)
            && !mon->is_summoned() && !mons_is_confused(*mon)
            && !mon->petrifying() && !mon->cannot_act() && !mon->asleep()
            && !mon->is_silenced() && !mon->has_ench(ENCH_WORD_OF_RECALL);
}

// Xom forces a random monster in sight to start reciting a Word of Recall.
// Since this is much rarer and less anticipated compared to normal sightings
// of ironbound convokers, it takes extra long to cast.
static void _xom_grants_word_of_recall(int /*sever*/)
{
    int duration = 90 - div_rand_round(you.experience_level, 9) * 10;
    string note = "";

    vector<monster*> targetable;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_speaker_of_recall(*mi))
            targetable.push_back(*mi);
    }

    // Shouldn't be reached outside of wizmode due to
    // having checked for a valid speaker first.
    if (targetable.empty())
        return;

    shuffle_array(targetable);
    string phrasing = "reciting";
    string xom_speech = "grant word of recall";

    shout_type shouting = get_monster_data(targetable[0]->type)->shouts;
    phrasing = speaking_keys.find(shouting)->second;
    if (phrasing == "dramatically intoning")
        xom_speech = "grant voiceless word of recall";

    god_speaks(GOD_XOM, _get_xom_speech(xom_speech).c_str());
    mprf(MSGCH_WARN, "%s is forced to slowly start %s a word of recall!",
                     targetable[0]->name(DESC_A, true, false).c_str(),
                     phrasing.c_str());
    mon_enchant chant_timer = mon_enchant(ENCH_WORD_OF_RECALL, 1,
                                          targetable[0], duration);
    targetable[0]->add_ench(chant_timer);

    note = make_stringf("made %s speak a word of recall",
                        targetable[0]->name(DESC_A, true, false).c_str());

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static bool _has_min_banishment_level()
{
    int min = you.penance[GOD_XOM] ? 6 : 9;
    return you.experience_level >= min;
}

// Rolls whether banishment will be averted.
static bool _will_not_banish()
{
    return x_chance_in_y(5, you.experience_level);
}

// Disallow early banishment and make it much rarer later-on.
// While Xom is bored, the chance is increased.
static bool _allow_xom_banishment()
{
    // Always allowed if under penance.
    if (player_under_penance(GOD_XOM))
        return true;

    // If Xom is bored or wrathful, banishment becomes viable earlier.
    if (_xom_feels_nasty())
        return !_will_not_banish();

    // Below the minimum experience level, only fake banishment is allowed.
    if (!_has_min_banishment_level())
    {
        // Allow banishment; it will be retracted right away.
        if (one_chance_in(5) && x_chance_in_y(you.piety, 1000))
            return true;
        else
            return false;
    }
    else if (_will_not_banish())
        return false;

    return true;
}

static void _revert_banishment(bool xom_banished = true)
{
    more();
    god_speaks(GOD_XOM, xom_banished
               ? _get_xom_speech("revert own banishment").c_str()
               : _get_xom_speech("revert other banishment").c_str());
    down_stairs(DNGN_EXIT_ABYSS);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                   "revert banishment"), true);
}

xom_event_type xom_maybe_reverts_banishment(bool xom_banished, bool debug)
{
    // Never revert if Xom is bored or the player is under penance.
    if (_xom_feels_nasty())
        return XOM_BAD_BANISHMENT;

    // Sometimes Xom will immediately revert banishment.
    // Always if the banishment happened below the minimum exp level and Xom was responsible.
    if (xom_banished && !_has_min_banishment_level() || x_chance_in_y(you.piety, 1000))
    {
        if (!debug)
            _revert_banishment(xom_banished);
        return XOM_BAD_PSEUDO_BANISHMENT;
    }
    return XOM_BAD_BANISHMENT;
}

static void _xom_do_banishment(bool real)
{
    god_speaks(GOD_XOM, _get_xom_speech("banishment").c_str());

    int power = _xom_feels_nasty() ? you.experience_level * 3 / 2 - 10
                                   : you.experience_level * 5 / 4 - 13;
    // Handles note taking, scales depth by XL
    banished("Xom", max(1, power));
    if (!real)
        _revert_banishment();
}

static void _xom_banishment(int /*sever*/) { _xom_do_banishment(true); }
static void _xom_pseudo_banishment(int) { _xom_do_banishment(false); }

static void _xom_noise(int /*sever*/)
{
    // Ranges from shout to shatter volume.
    const int noisiness = 12 + random2(19);

    god_speaks(GOD_XOM, _get_xom_speech("noise").c_str());
    // Xom isn't subject to silence.
    fake_noisy(noisiness, you.pos());

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "noise"), true);
}

static bool _mon_valid_blink_victim(const monster& mon)
{
    return !mon.wont_attack()
            && !mon.no_tele()
            && !mons_is_projectile(mon);
}

static void _xom_blink_monsters(int /*sever*/)
{
    int blinks = 0;
    // Sometimes blink towards the player, sometimes randomly. It might
    // end up being helpful instead of dangerous, but Xom doesn't mind.
    const bool blink_to_player = _xom_feels_nasty() || coinflip();
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (blinks >= 5)
            break;

        if (!_mon_valid_blink_victim(**mi) || coinflip())
            continue;

        // Only give this message once.
        if (!blinks)
            god_speaks(GOD_XOM, _get_xom_speech("blink monsters").c_str());

        if (blink_to_player)
            blink_other_close(*mi, you.pos());
        else
            monster_blink(*mi, false);

        blinks++;
    }

    if (blinks)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "blink monster(s)"),
                  true);
    }
}

static void _xom_cleaving(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("cleaving").c_str());

    you.increase_duration(DUR_CLEAVE, 10 + random2(sever));

    if (const item_def* const weapon = you.weapon())
    {
        const bool axe = item_attack_skill(*weapon) == SK_AXES;
        mprf(MSGCH_DURATION,
             "%s %s sharp%s", weapon->name(DESC_YOUR).c_str(),
             conjugate_verb("look", weapon->quantity > 1).c_str(),
             (axe) ? " (like it always does)." : ".");
    }
    else
    {
        mprf(MSGCH_DURATION, "%s",
             you.hands_act("look", "sharp.").c_str());
    }

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "cleaving"), true);
}


static void _handle_accidental_death(const int orig_hp,
    const FixedVector<uint8_t, NUM_MUTATIONS> &orig_mutation,
    const transformation orig_form)
{
    // Did ouch() return early because the player died from the Xom
    // effect, even though neither is the player under penance nor is
    // Xom bored?
    if ((!you.did_escape_death()
         && you.escaped_death_aux.empty()
         && !_player_is_dead())
        || you.pending_revival) // don't let xom take credit for felid revival
    {
        // The player is fine.
        return;
    }

    string speech_type = XOM_SPEECH("accidental homicide");

    const dungeon_feature_type feat = env.grid(you.pos());

    switch (you.escaped_death_cause)
    {
        case NUM_KILLBY:
        case KILLED_BY_LEAVING:
        case KILLED_BY_WINNING:
        case KILLED_BY_QUITTING:
            speech_type = XOM_SPEECH("weird death");
            break;

        case KILLED_BY_LAVA:
        case KILLED_BY_WATER:
            if (!is_feat_dangerous(feat))
                speech_type = "weird death";
            break;

        default:
            if (is_feat_dangerous(feat))
                speech_type = "weird death";
        break;
    }

    canned_msg(MSG_YOU_DIE);
    god_speaks(GOD_XOM, _get_xom_speech(speech_type).c_str());
    god_speaks(GOD_XOM, _get_xom_speech("resurrection").c_str());

    int pre_mut_hp = you.hp;
    if (you.hp <= 0)
        you.hp = 9999; // avoid spurious recursive deaths if heavily rotten

    // If any mutation has changed, death was because of it.
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (orig_mutation[i] > you.mutation[i])
            mutate((mutation_type)i, "Xom's lifesaving", true, true, true);
        else if (orig_mutation[i] < you.mutation[i])
            delete_mutation((mutation_type)i, "Xom's lifesaving", true, true, true);
    }

    if (pre_mut_hp <= 0)
        set_hp(min(orig_hp, you.hp_max));

    if (orig_form != you.form)
    {
        dprf("Trying emergency untransformation.");
        you.transform_uncancellable = false;
        transform(10, orig_form, true);
    }

    if (is_feat_dangerous(feat) && !crawl_state.game_is_sprint())
        you_teleport_now();
}

/**
 * Try to choose an action for Xom to take that is at least notionally 'good'
 * for the player.
 * TODO: Completely rewrite. Please.
 *
 * @param sever         The intended magnitude of the action.
 * @param tension       How much danger we think the player's currently in.
 * @return              A good action for Xom to take, e.g. XOM_GOOD_ALLIES.
 */
static xom_event_type _xom_choose_good_action(int sever, int tension)
{
    // This series of random calls produces a poisson-looking
    // distribution: initial hump, plus a long-ish tail.
    // a wizard has pronounced a curse on the original author of this code

    // Don't make the player go berserk, etc. if there's no danger.
    if (tension > random2(3) && x_chance_in_y(2, sever))
        return XOM_GOOD_POTION;

    if (x_chance_in_y(3, sever) && tension > 0
        && _choose_random_spell(sever) != SPELL_NO_SPELL)
    {
        return XOM_GOOD_SPELL;
    }

    if (x_chance_in_y(4, sever)
        && (_exploration_estimate(false) < 80
        || x_chance_in_y(_exploration_estimate(false), 120)))
    {
        // Detecting creatures is ...somewhat useful regardless of anything else.
        return XOM_GOOD_DIVINATION;
    }

    if (tension <= 0 && x_chance_in_y(5, sever)
        && !you.duration[DUR_CLOUD_TRAIL])
    {
        return XOM_GOOD_CLOUD_TRAIL;
    }

    if (tension > 0 && x_chance_in_y(5, sever)
        && mon_nearby([](monster& mon){ return !mon.wont_attack(); }))
    {
        return XOM_GOOD_CONFUSION;
    }

    if (tension > 0 && x_chance_in_y(6, sever)
        && mon_nearby(_choose_enchantable_monster))
    {
        return XOM_GOOD_ENCHANT_MONSTER;
    }

    if ((tension > random2(5)
        || (_exploration_estimate(false) < 25) && one_chance_in(7))
        && x_chance_in_y(7, sever)
        && !you.allies_forbidden())
    {
        return XOM_GOOD_SINGLE_ALLY;
    }

    if (tension < random2(5) && x_chance_in_y(8, sever)
        && ((!_xom_scenery_candidates().empty() && one_chance_in(3))
        || one_chance_in(8)))
    {
        return XOM_GOOD_SCENERY;
    }

    if (x_chance_in_y(9, sever) && mon_nearby(_hostile_snake))
        return XOM_GOOD_SNAKES;

    if (tension > random2(10) && x_chance_in_y(10, sever)
        && !you.allies_forbidden())
    {
        return XOM_GOOD_ALLIES;
    }
    if (tension > random2(8) && x_chance_in_y(11, sever)
        && _find_monster_with_animateable_weapon()
        && !you.allies_forbidden())
    {
        return XOM_GOOD_ANIMATE_MON_WPN;
    }

    if (x_chance_in_y(12, sever) && _xom_mons_poly_target() != nullptr)
        return XOM_GOOD_POLYMORPH;

    if (tension > random2(3) && x_chance_in_y(13, sever))
    {
        // Check if there's a reasonable amount of open terrain
        // before placing down all the living spells.
        int open_count = 0;
        for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
             ri; ++ri)
        {
            if (!cell_is_solid(*ri))
                open_count++;
        }

        if (open_count > 6)
            return XOM_GOOD_FORCE_LANCE_FLEET;
    }

    if (tension > 0 && x_chance_in_y(14, sever))
    {
        const bool fake = one_chance_in(3);
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_is_projectile(**mi)
                || mons_is_tentacle_or_tentacle_segment(mi->type))
            {
                continue;
            }

            if (fake)
                return XOM_GOOD_FAKE_DESTRUCTION;

            // Skip adjacent monsters, and skip
            // non-hostile monsters if not feeling nasty.
            if (!adjacent(you.pos(), mi->pos())
                 && (!mi->wont_attack() || _xom_feels_nasty()))
            {
                return XOM_GOOD_DESTRUCTION;
            }
        }
    }

    if (tension > random2(5) && x_chance_in_y(15, sever))
        return XOM_GOOD_CLEAVING;

    if (tension > random2(3) && x_chance_in_y(16, sever))
    {
        int plant_capacity = 0;
        int smiters = 0;

        for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
             ri; ++ri)
        {
            if (!monster_at(*ri) && monster_habitable_grid(MONS_PLANT, env.grid(*ri)))
                plant_capacity++;
        }

        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            if (_mons_has_smite_attack(*mi) && grid_distance(mi->pos(), you.pos()) > 2)
                smiters++;

        if (plant_capacity >= 3
            && smiters < div_rand_round(you.experience_level, 6) - 1)
        {
            return XOM_GOOD_FLORA_RING;
        }
    }

    if (tension > random2(3) && x_chance_in_y(17, sever))
    {
        // Assess each if there's enough room to meaningfully raise a door ring,
        // if there's adjacent hostiles to move so it does anything tactically,
        // and if there's at least visible room to move them to.
        int adjacent_hostiles = 0;
        int replaceable = 0;
        int spare_space_out = 0;
        for (radius_iterator ri(you.pos(), 8, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        {
            if (grid_distance(*ri, you.pos()) <= 2
              && monster_at(*ri) && !monster_at(*ri)->wont_attack())
            {
                adjacent_hostiles++;
            }
            else if (grid_distance(*ri, you.pos()) >= 6 && in_bounds(*ri)
                     && feat_has_solid_floor(env.grid(*ri)))
            {
                spare_space_out++;
            }
        }

        for (radius_iterator ri(you.pos(), 5, C_SQUARE, LOS_NONE); ri; ++ri)
        {
            if (grid_distance(*ri, you.pos()) >= 3
              && in_bounds(*ri)
              && _xom_door_replaceable(env.grid(*ri)))
            {
                replaceable++;
            }
        }

        if (replaceable > 24 && adjacent_hostiles - 1 < spare_space_out)
            return XOM_GOOD_DOOR_RING;
    }

    if (tension > 0 && x_chance_in_y(18, sever)
        && mon_nearby(_choose_enchantable_monster))
    {
        return XOM_GOOD_MASS_CHARM;
    }

    if (tension > 0 && x_chance_in_y(19, sever) && !cloud_at(you.pos()))
        return XOM_GOOD_FOG;

    if (random2(tension) < 20 && x_chance_in_y(20, sever))
    {
        return x_chance_in_y(sever, 201) ? XOM_GOOD_ACQUIREMENT
                                         : XOM_GOOD_RANDOM_ITEM;
    }

    if (!player_in_branch(BRANCH_ABYSS) && x_chance_in_y(21, sever)
        && _teleportation_check())
    {
        // This is not very interesting if the level is already fully
        // explored (presumably cleared). Even then, it may
        // occasionally happen.
        const int explored = _exploration_estimate(true);
        if (explored < 80 || !x_chance_in_y(explored, 110))
            return XOM_GOOD_TELEPORT;
    }

    if (random2(tension) < 5 && x_chance_in_y(22, sever)
        && x_chance_in_y(16, you.how_mutated())
        && you.can_safely_mutate())
    {
        return XOM_GOOD_MUTATION;
    }

    // The bazaar's most interesting in its first few trips, so it should be
    // less likely each time unless it's a chance to escape big trouble.
    // Always expect some minimum gold. Don't interrupt autotravel too much.
    if ((tension > 27 || (one_chance_in(you.props[XOM_BAZAAR_TRIP_COUNT].get_int() * 4)
        && (_exploration_estimate(true) < 80
        || x_chance_in_y(_exploration_estimate(true), 120))))
        && you.gold > (777 + sever * (4 + (you.props[XOM_BAZAAR_TRIP_COUNT].get_int() * 2)))
        && x_chance_in_y(23, sever)
        && !player_in_branch(BRANCH_BAZAAR)
        && !player_in_branch(BRANCH_ABYSS))
    {
        return XOM_GOOD_BAZAAR_TRIP;
    }

    if (tension > 0 && x_chance_in_y(24, sever)
        && !you.allies_forbidden())
    {
        vector<monster*> potential = _xom_find_weak_monsters(false);
        int space = 0;

        // Check if there's space to summon something instead.
        if (potential.empty() && one_chance_in(3))
        {
            for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
                ri; ++ri)
            {
                if (!cell_is_solid(*ri))
                    space++;
            }
            if (space >= 0)
                return XOM_GOOD_HYPER_ENCHANT_MONSTER;
        }
        else
            return XOM_GOOD_HYPER_ENCHANT_MONSTER;
    }

    if (tension > 0 && x_chance_in_y(25, sever)
        && player_in_a_dangerous_place())
    {
        // Make sure there's at least one enemy within the lightning radius.
        for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID, true); ri;
             ++ri)
        {
            const monster *mon = monster_at(*ri);
            if (mon && !mon->wont_attack())
                return XOM_GOOD_LIGHTNING;
        }
    }

    if (tension > 0 && x_chance_in_y(26, sever)
        && mon_nearby(_choose_enchantable_monster))
    {
        return XOM_GOOD_WAVE_OF_DESPAIR;
    }

    return XOM_DID_NOTHING; // sigh
}

/**
 * Try to choose an action for Xom to take that is at least notionally 'bad'
 * for the player.
 *
 * @param sever         The intended magnitude of the action.
 * @param tension       How much danger we think the player's currently in.
 * @return              A bad action for Xom to take, e.g. XOM_BAD_NOISE.
 */
static xom_event_type _xom_choose_bad_action(int sever, int tension)
{
    const bool nasty = _miscast_is_nasty(sever);

    if (!nasty && x_chance_in_y(3, sever) && !you.penance[GOD_XOM])
        return XOM_BAD_MISCAST_PSEUDO;

    // Sometimes do noise out of combat.
    if ((tension > 0 || coinflip()) && x_chance_in_y(6, sever)
        && !you.penance[GOD_XOM])
     {
        return XOM_BAD_NOISE;
     }

    if (tension > 0 && x_chance_in_y(7, sever)
        && mon_nearby(_choose_enchantable_monster))
    {
        return XOM_BAD_ENCHANT_MONSTER;
    }

    if (tension > 0 && x_chance_in_y(8, sever)
        && mon_nearby(_mon_valid_blink_victim))
    {
        return XOM_BAD_BLINK_MONSTERS;
    }

    // It's pointless to confuse player if there's no danger nearby.
    if (tension > 0 && x_chance_in_y(9, sever))
        return XOM_BAD_CONFUSION;

    if (tension > 0 && x_chance_in_y(10, sever)
        && _rearrangeable_pieces().size())
    {
        return XOM_BAD_SWAP_MONSTERS;
    }

    if (x_chance_in_y(14, sever) && mon_nearby(_choose_chaos_upgrade))
        return XOM_BAD_CHAOS_UPGRADE;
    if (x_chance_in_y(15, sever) && !player_in_branch(BRANCH_ABYSS)
        && _teleportation_check())
    {
        const int explored = _exploration_estimate(true);
        if (!(nasty && (explored >= 40 || tension > 10)
            || explored >= 60 + random2(40)))
        {
            return XOM_BAD_TELEPORT;
        }
    }

    if (tension > 0 && x_chance_in_y(16, sever)
        && _xom_mons_poly_target() != nullptr)
    {
        return XOM_BAD_POLYMORPH;
    }

    if (tension > random2(5) && x_chance_in_y(16, sever)
        && (!_xom_door_candidates().empty() && one_chance_in(3)))
    {
        return XOM_BAD_FIDDLE_WITH_DOORS;
    }

     // Pushing stairs/exits is always hilarious in the Abyss!
    if ((tension > 0 || player_in_branch(BRANCH_ABYSS))
        && x_chance_in_y(17, sever) && !_nearby_stairs().empty()
        && !you.duration[DUR_REPEL_STAIRS_MOVE]
        && !you.duration[DUR_REPEL_STAIRS_CLIMB])
    {
        if (one_chance_in(5)
            || feat_stair_direction(env.grid(you.pos())) != CMD_NO_CMD
                && env.grid(you.pos()) != DNGN_ENTER_SHOP)
        {
            return XOM_BAD_CLIMB_STAIRS;
        }
        return XOM_BAD_MOVING_STAIRS;
    }
    if (random2(tension) < 11 && x_chance_in_y(18, sever)
        && you.can_safely_mutate())
    {
        return XOM_BAD_MUTATION;
    }

    if (x_chance_in_y(19, sever))
        return XOM_BAD_SUMMON_HOSTILES;

    if (tension > 0 && x_chance_in_y(20, sever)
        && !you.duration[DUR_HASTE] && !you.duration[DUR_SLOW]
        && !you.duration[DUR_PARALYSIS])
    {
        return XOM_BAD_TIME_CONTROL;
    }

    if (tension > 0 && x_chance_in_y(20, sever)
        && you.magic_points > 3)
    {
        return XOM_BAD_BRAIN_DRAIN;
    }

    if (tension > 0 && x_chance_in_y(21, sever))
    {
        // Check if there's a reasonable amount of features
        // the weaker Xom shatter is likely to change.
        int nearby_diggable = 0;
        for (radius_iterator ri(you.pos(), you.current_vision, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        {
            if (feat_is_diggable(env.grid(*ri)) || feat_is_door(env.grid(*ri)))
                nearby_diggable++;
        }
        if (nearby_diggable >= 4)
            return XOM_BAD_FAKE_SHATTER;
    }

    if (tension > 0 && tension < 25 && x_chance_in_y(22, sever) &&
        _has_min_recall_level())
    {
        bool recall_ready = false;
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (_valid_speaker_of_recall(*mi))
                recall_ready = true;
        }

        if (recall_ready)
            return XOM_BAD_GRANT_WORD_OF_RECALL;
    }

    if (tension > 0 && x_chance_in_y(22, sever)
        && !cloud_at(you.pos()))
    {
        return XOM_BAD_CHAOS_CLOUD;
    }

    if (tension > 0 && tension < 25
        && mon_nearby([](monster& mon){ return !mon.wont_attack(); })
        && x_chance_in_y(23, sever))
    {
        int clone_capacity = 0;
        for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
             ri; ++ri)
        {
            if (!monster_at(*ri) && monster_habitable_grid(MONS_PLAYER_ILLUSION, env.grid(*ri)))
                clone_capacity++;
        }

        if (clone_capacity >= 3)
            return XOM_BAD_SEND_IN_THE_CLONES;
    }

    if (tension > random2(5) && x_chance_in_y(24, sever))
    {
        // Calculate if there's enough room to raise a meaningful door ring
        // and also if there's enough hostiles present either
        // already adjacent or near enough to blink inwards.
        int adjacent_hostiles = 0;
        int moveable_hostiles = 0;
        int adjacent_space = 0;
        int replaceable = 0;
        int adjascency_cap = 2 + div_rand_round(you.experience_level, 6);

        for (radius_iterator ri(you.pos(), you.current_vision, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        {
            if (monster_at(*ri) && !monster_at(*ri)->wont_attack())
                if (grid_distance(*ri, you.pos()) <= 2)
                    adjacent_hostiles++;
                else
                    moveable_hostiles++;
            else if (feat_has_solid_floor(env.grid(*ri)))
                adjacent_space++;
        }

        for (radius_iterator ri(you.pos(), 5, C_SQUARE, LOS_NONE); ri; ++ri)
        {
            if (grid_distance(*ri, you.pos()) >= 3
              && in_bounds(*ri)
              && _xom_door_replaceable(env.grid(*ri)))
            {
                replaceable++;
            }
        }

        if (replaceable > 30 && adjacent_hostiles <= adjascency_cap
            && (moveable_hostiles <= adjacent_space && moveable_hostiles >= 2
            || (adjacent_hostiles >= 2)))
        {
            return XOM_BAD_DOOR_RING;
        }
    }


    if (one_chance_in(sever) && !player_in_branch(BRANCH_ABYSS)
        && _allow_xom_banishment())
    {
        return xom_maybe_reverts_banishment(true, true);
    }

    if (x_chance_in_y(27, sever))
    {
        if (coinflip())
            return XOM_BAD_STATLOSS;
        if (coinflip())
        {
            if (player_prot_life() < 3)
                return XOM_BAD_DRAINING;
            // else choose something else
        }
        else if (!you.res_torment() && tension > 0)
            return XOM_BAD_TORMENT;
        // else give up?? >_>
    }

    return XOM_DID_NOTHING; // ugh
}

/**
 * Try to choose an action for Xom to take.
 *
 * @param niceness      Whether the action should be 'good' for the player.
 * @param sever         The intended magnitude of the action.
 * @param tension       How much danger we think the player's currently in.
 * @return              An bad action for Xom to take, e.g. XOM_DID_NOTHING.
 */
xom_event_type xom_choose_action(bool niceness, int sever, int tension)
{
    sever = max(1, sever);

    if (_player_is_dead() && !you.pending_revival)
    {
        // This should only happen if the player used wizard mode to
        // escape death from deep water or lava.
        ASSERT(you.wizard);
        ASSERT(!you.did_escape_death());
        if (is_feat_dangerous(env.grid(you.pos())))
            mprf(MSGCH_DIAGNOSTICS, "Player is standing in deadly terrain, skipping Xom act.");
        else
            mprf(MSGCH_DIAGNOSTICS, "Player is already dead, skipping Xom act.");
        return XOM_PLAYER_DEAD;
    }

    if (niceness)
    {
        // Make good acts at zero tension less likely, especially if Xom
        // is in a bad mood.
        if (tension == 0
            && you_worship(GOD_XOM) && !x_chance_in_y(you.piety, MAX_PIETY))
        {
#ifdef NOTE_DEBUG_XOM
            take_note(Note(NOTE_MESSAGE, 0, 0, "suppress good act because of "
                           "zero tension"), true);
#endif
            return XOM_DID_NOTHING;
        }

        // {sarcastically}: Good stuff. {seriously}: remove this loop
        while (true)
        {
            const xom_event_type action = _xom_choose_good_action(sever,
                                                                  tension);
            if (action != XOM_DID_NOTHING)
                return action;
        }
    }

    // Make bad acts at non-zero tension less likely, especially if Xom
    // is in a good mood.
    if (!_xom_feels_nasty() && tension > random2(10)
        && you_worship(GOD_XOM) && x_chance_in_y(you.piety, MAX_PIETY))
    {
#ifdef NOTE_DEBUG_XOM
        const string note = string("suppress bad act because of ") +
                                   tension + " tension";
        take_note(Note(NOTE_MESSAGE, 0, 0, note), true);
#endif
        return XOM_DID_NOTHING;
    }

    // try to do something bad
    for (int i = 0; i < 100; i++)
    {
        const xom_event_type action = _xom_choose_bad_action(sever, tension);
        if (action != XOM_DID_NOTHING)
            return action;
    }
    // player got lucky
    return XOM_DID_NOTHING;
}

/**
 * Execute the specified Xom Action.
 *
 * @param action        The action type in question; e.g. XOM_BAD_NOISE.
 * @param sever         The severity of the action.
 */
void xom_take_action(xom_event_type action, int sever)
{
    const int  orig_hp       = you.hp;
    const transformation orig_form = you.form;
    const FixedVector<uint8_t, NUM_MUTATIONS> orig_mutation = you.mutation;
    const bool was_bored = _xom_is_bored();

    const bool bad_effect = _action_is_bad(action);

    if (was_bored && bad_effect && Options.note_xom_effects)
        take_note(Note(NOTE_MESSAGE, 0, 0, "XOM is BORED!"), true);

    // actually take the action!
    {
        god_acting gdact(GOD_XOM);
        _do_xom_event(action, sever);
    }

    // If we got here because Xom was bored, reset gift timeout according
    // to the badness of the effect.
    if (bad_effect && _xom_is_bored())
    {
        const int badness = _xom_event_badness(action);
        const int interest = random2avg(badness * 60, 2);
        you.gift_timeout   = min(interest, 255);
        //updating piety status line
        you.redraw_title = true;
#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
        mprf(MSGCH_DIAGNOSTICS, "badness: %d, new interest: %d",
             badness, you.gift_timeout);
#endif
    }

    _handle_accidental_death(orig_hp, orig_mutation, orig_form);

    if (you_worship(GOD_XOM) && one_chance_in(5))
    {
        const string old_xom_favour = describe_xom_favour();
        you.piety = random2(MAX_PIETY + 1);
        you.redraw_title = true; // redraw piety/boredom display
        const string new_xom_favour = describe_xom_favour();
        if (was_bored || old_xom_favour != new_xom_favour)
        {
            const string msg = "You are now " + new_xom_favour;
            god_speaks(you.religion, msg.c_str());
        }
#ifdef NOTE_DEBUG_XOM
        const string note = string("reroll piety: ") + you.piety;
        take_note(Note(NOTE_MESSAGE, 0, 0, note), true);
#endif
    }
    else if (was_bored)
    {
        // If we didn't reroll at least mention the new favour
        // now that it's not "BORING thing" anymore.
        const string new_xom_favour = describe_xom_favour();
        const string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
    }
}

/**
 * Let Xom take an action, probably.
 *
 * @param sever         The intended magnitude of the action.
 * @param nice          Whether the action should be 'good' for the player.
 *                      If maybe_bool::maybe, determined by xom's whim.
 *                      May be overridden.
 * @param tension       How much danger we think the player's currently in.
 * @return              Whichever action Xom took, or XOM_DID_NOTHING.
 */
xom_event_type xom_acts(int sever, maybe_bool nice, int tension, bool debug)
{
    bool niceness = nice.to_bool(xom_is_nice(tension));

#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
    if (!debug)
    {
        // This probably seems a bit odd, but we really don't want to display
        // these when doing a heavy-duty wiz-mode debug test: just ends up
        // as message spam and the player doesn't get any further information
        // anyway. (jpeg)

        // these numbers (sever, tension) may be modified later...
        mprf(MSGCH_DIAGNOSTICS, "xom_acts(%u, %d, %d); piety: %u, interest: %u",
             niceness, sever, tension, you.piety, you.gift_timeout);

        static char xom_buf[100];
        snprintf(xom_buf, sizeof(xom_buf), "xom_acts(%s, %d, %d), mood: %d",
                 (niceness ? "true" : "false"), sever, tension, you.piety);
        take_note(Note(NOTE_MESSAGE, 0, 0, xom_buf), true);
    }
#endif

    if (tension == -1)
        tension = get_tension(GOD_XOM);

#if defined(DEBUG_RELIGION) || defined(DEBUG_XOM) || defined(DEBUG_TENSION)
    // No message during heavy-duty wizmode testing:
    // Instead all results are written into xom_debug.stat.
    if (!debug)
        mprf(MSGCH_DIAGNOSTICS, "Xom tension: %d", tension);
#endif

    const xom_event_type action = xom_choose_action(niceness, sever, tension);
    if (!debug)
        xom_take_action(action, sever);

    return action;
}

void xom_check_lost_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
        xom_is_stimulated(100, "Xom snickers.", true);
}

void xom_check_destroyed_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
        xom_is_stimulated(100, "Xom snickers.", true);
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
        return false;
    default:
        // All others are fun (says Xom).
        return true;
    }
}

void xom_death_message(const kill_method_type killed_by)
{
    if (!you_worship(GOD_XOM) && (!you.worshipped[GOD_XOM] || coinflip()))
        return;

    const int death_tension = get_tension(GOD_XOM);

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

static int _death_is_worth_saving(const kill_method_type killed_by)
{
    switch (killed_by)
    {
    // These don't count.
    case KILLED_BY_LEAVING:
    case KILLED_BY_WINNING:
    case KILLED_BY_QUITTING:

    // These are too much hassle.
    case KILLED_BY_LAVA:
    case KILLED_BY_WATER:
    case KILLED_BY_DRAINING:
    case KILLED_BY_STARVATION:
    case KILLED_BY_ZOT:
    case KILLED_BY_ROTTING:

    // Don't protect the player from these.
    case KILLED_BY_SELF_AIMED:
    case KILLED_BY_TARGETING:
        return false;

    // Everything else is fair game.
    default:
        return true;
    }
}

static string _get_death_type_keyword(const kill_method_type killed_by)
{
    switch (killed_by)
    {
    case KILLED_BY_MONSTER:
    case KILLED_BY_BEAM:
    case KILLED_BY_BEOGH_SMITING:
    case KILLED_BY_TSO_SMITING:
    case KILLED_BY_DIVINE_WRATH:
        return "actor";
    default:
        return "general";
    }
}

/**
 * Have Xom maybe act to save your life. There is both a flat chance
 * and an additional chance based on tension that he will refuse to
 * save you.
 * @param death_type  The type of death that occurred.
 * @return            True if Xom saves your life, false otherwise.
 */
bool xom_saves_your_life(const kill_method_type death_type)
{
    if (!you_worship(GOD_XOM) || _xom_feels_nasty())
        return false;

    // If this happens, don't bother.
    if (you.hp_max < 1 || you.experience_level < 1)
        return false;

    // Generally a rare effect.
    if (!one_chance_in(20))
        return false;

    if (!_death_is_worth_saving(death_type))
        return false;

    // In addition, the chance depends on the current tension and Xom's mood.
    const int death_tension = get_tension(GOD_XOM);
    if (death_tension < random2(5) || !xom_is_nice(death_tension))
        return false;

    // Fake death message.
    canned_msg(MSG_YOU_DIE);
    more();

    const string key = _get_death_type_keyword(death_type);
    // XOM_SPEECH("life saving actor") or XOM_SPEECH("life saving general")
    string speech = _get_xom_speech("life saving " + key);
    god_speaks(GOD_XOM, speech.c_str());

    // Give back some hp.
    if (you.hp < 1)
        set_hp(1 + random2(you.hp_max/4));

    god_speaks(GOD_XOM, "Xom revives you!");

    // Ideally, this should contain the death cause but that is too much
    // trouble for now.
    take_note(Note(NOTE_XOM_REVIVAL));

    // Make sure Xom doesn't get bored within the next couple of turns.
    if (you.gift_timeout < 10)
        you.gift_timeout = 10;

    return true;
}

// Xom might have something to say when you enter a new level.
void xom_new_level_noise_or_stealth()
{
    if (!you_worship(GOD_XOM) && !player_under_penance(GOD_XOM))
        return;

    // But only occasionally.
    if (one_chance_in(30))
    {
        if (!player_under_penance(GOD_XOM) && coinflip())
        {
            god_speaks(GOD_XOM, _get_xom_speech("stealth player").c_str());
            mpr(you.duration[DUR_STEALTH] ? "You feel more stealthy."
                                          : "You feel stealthy.");
            you.increase_duration(DUR_STEALTH, 10 + random2(80));
            take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                           "stealth player"), true);
        }
        else
            _xom_noise(-1);
    }
    return;
}

/**
 * The Xom teleportation train takes you on instant
 * teleportation to a few random areas, stopping randomly but
 * most likely in an area that is not dangerous to you.
 */
static void _xom_good_teleport(int /*sever*/)
{
    god_speaks(GOD_XOM, _get_xom_speech("teleportation journey").c_str());
    int count = 0;
    do
    {
        count++;
        you_teleport_now();
        maybe_update_stashes();
        more();
        if (one_chance_in(10) || count >= 7 + random2(5))
            break;
    }
    while (x_chance_in_y(3, 4) || player_in_a_dangerous_place());
    maybe_update_stashes();

    // Take a note.
    const string note = make_stringf("%d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
             player_in_a_dangerous_place() ? " (dangerous)" :
#endif
             "");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

/**
 * The Xom teleportation train takes you on instant
 * teleportation to a few random areas, stopping if either
 * an area is dangerous to you or randomly.
 */
static void _xom_bad_teleport(int /*sever*/)
{
    god_speaks(GOD_XOM,
               _get_xom_speech("teleportation journey").c_str());

    int count = 0;
    do
    {
        you_teleport_now();
        maybe_update_stashes();
        more();
        if (count++ >= 7 + random2(5))
            break;
    }
    while (x_chance_in_y(3, 4) && !player_in_a_dangerous_place());
    maybe_update_stashes();

    // Take a note.
    const string note = make_stringf("%d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
             badness == 3 ? " (dangerous)" : "");
#else
    "");
#endif
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

/// Place a one-tile chaos cloud on the player, with minor spreading.
static void _xom_chaos_cloud(int /*sever*/)
{
    const int lifetime = 3 + random2(12) * 3;
    const int spread_rate = random_range(5,15);
    check_place_cloud(CLOUD_CHAOS, you.pos(), lifetime,
                      nullptr, spread_rate);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "chaos cloud"),
              true);
    god_speaks(GOD_XOM, _get_xom_speech("cloud").c_str());
}

struct xom_effect_count
{
    string effect;
    int    count;

    xom_effect_count(string e, int c) : effect(e), count(c) {};
};

/// A type of action Xom can take.
struct xom_event
{
    /// Wizmode name for the event.
    const char* name;
    /// The event action.
    function<void(int sever)> action;
    /**
     * Rough estimate of how hard a Xom effect hits the player,
     * scaled between 10 (harmless) and 50 (disastrous). Reduces boredom.
     */
    int badness_10x;
};

static const map<xom_event_type, xom_event> xom_events = {
    { XOM_DID_NOTHING, { "nothing" }},
    { XOM_GOOD_POTION, { "potion", _xom_do_potion }},
    { XOM_GOOD_DIVINATION, { "magic mapping + detect items / creatures",
                              _xom_divination }},
    { XOM_GOOD_SPELL, { "tension spell", _xom_random_spell }},
    { XOM_GOOD_CONFUSION, { "confuse monsters", _xom_confuse_monsters }},
    { XOM_GOOD_SINGLE_ALLY, { "single ally", _xom_send_one_ally }},
    { XOM_GOOD_ANIMATE_MON_WPN, { "animate monster weapon",
                                  _xom_animate_monster_weapon }},
    { XOM_GOOD_RANDOM_ITEM, { "random item gift", _xom_random_item }},
    { XOM_GOOD_ACQUIREMENT, { "acquirement", _xom_acquirement }},
    { XOM_GOOD_BAZAAR_TRIP, { "bazaar trip", _xom_bazaar_trip }},
    { XOM_GOOD_ALLIES, { "summon allies", _xom_send_allies }},
    { XOM_GOOD_POLYMORPH, { "good polymorph", _xom_good_polymorph }},
    { XOM_GOOD_TELEPORT, { "good teleportation", _xom_good_teleport }},
    { XOM_GOOD_MUTATION, { "good mutations", _xom_give_good_mutations }},
    { XOM_GOOD_LIGHTNING, { "lightning", _xom_throw_divine_lightning }},
    { XOM_GOOD_SCENERY, { "change scenery", _xom_change_scenery }},
    { XOM_GOOD_FLORA_RING, {"flora ring", _xom_harmless_flora }},
    { XOM_GOOD_DOOR_RING, {"good door ring enclosure", _xom_good_door_ring }},
    { XOM_GOOD_SNAKES, { "snakes to sticks", _xom_snakes_to_sticks }},
    { XOM_GOOD_DESTRUCTION, { "mass fireball", _xom_real_destruction }},
    { XOM_GOOD_FAKE_DESTRUCTION, { "fake fireball", _xom_fake_destruction }},
    { XOM_GOOD_FORCE_LANCE_FLEET, {"living force lance fleet",
                                   _xom_force_lances }},
    { XOM_GOOD_ENCHANT_MONSTER, { "good enchant monster",
                                  _xom_good_enchant_monster }},
    { XOM_GOOD_HYPER_ENCHANT_MONSTER, { "hyper enchant monster",
                                        _xom_hyper_enchant_monster }},
    { XOM_GOOD_MASS_CHARM, {"mass charm", _xom_mass_charm }},
    { XOM_GOOD_WAVE_OF_DESPAIR, {"wave of despair", _xom_wave_of_despair }},
    { XOM_GOOD_FOG, { "fog", _xom_fog }},
    { XOM_GOOD_CLOUD_TRAIL, { "cloud trail", _xom_cloud_trail }},
    { XOM_GOOD_CLEAVING, { "cleaving", _xom_cleaving }},

    { XOM_BAD_MISCAST_PSEUDO, { "pseudo-miscast", _xom_pseudo_miscast, 10}},
    { XOM_BAD_NOISE, { "noise", _xom_noise, 10 }},
    { XOM_BAD_ENCHANT_MONSTER, { "bad enchant monster",
                                 _xom_bad_enchant_monster, 10}},
    { XOM_BAD_TIME_CONTROL, {"time control", _xom_time_control, 15}},
    { XOM_BAD_BLINK_MONSTERS, { "blink monsters", _xom_blink_monsters, 10}},
    { XOM_BAD_CONFUSION, { "confuse player", _xom_player_confusion_effect, 13}},
    { XOM_BAD_SWAP_MONSTERS, { "swap monsters", _xom_rearrange_pieces, 20 }},
    { XOM_BAD_CHAOS_UPGRADE, { "chaos upgrade", _xom_chaos_upgrade, 20}},
    { XOM_BAD_TELEPORT, { "bad teleportation", _xom_bad_teleport, -1}},
    { XOM_BAD_POLYMORPH, { "bad polymorph", _xom_bad_polymorph, 30}},
    { XOM_BAD_MOVING_STAIRS, { "moving stairs", _xom_moving_stairs, 20}},
    { XOM_BAD_CLIMB_STAIRS, { "unclimbable stairs", _xom_unclimbable_stairs,
                              30}},
    { XOM_BAD_FIDDLE_WITH_DOORS, { "open and close doors", _xom_open_and_close_doors,
                                    15}},
    { XOM_BAD_DOOR_RING, {"bad door ring enclosure", _xom_bad_door_ring, 25}},
    { XOM_BAD_FAKE_SHATTER, {"fake shatter", _xom_fake_shatter, 25}},
    { XOM_BAD_MUTATION, { "bad mutations", _xom_give_bad_mutations, 30}},
    { XOM_BAD_SUMMON_HOSTILES, { "summon hostiles", _xom_summon_hostiles, 35}},
    { XOM_BAD_SEND_IN_THE_CLONES, {"friendly and hostile illusions",
                                   _xom_send_in_clones, 40}},
    { XOM_BAD_GRANT_WORD_OF_RECALL, {"speaker of recall",
                                    _xom_grants_word_of_recall, 40}},
    { XOM_BAD_BRAIN_DRAIN, {"mp brain drain", _xom_brain_drain, 30}},
    { XOM_BAD_STATLOSS, { "statloss", _xom_statloss, 23}},
    { XOM_BAD_DRAINING, { "draining", _xom_draining, 23}},
    { XOM_BAD_TORMENT, { "torment", _xom_torment, 23}},
    { XOM_BAD_CHAOS_CLOUD, { "chaos cloud", _xom_chaos_cloud, 20}},
    { XOM_BAD_BANISHMENT, { "banishment", _xom_banishment, 50}},
    { XOM_BAD_PSEUDO_BANISHMENT, {"pseudo-banishment", _xom_pseudo_banishment,
                                  10}},
};

static void _do_xom_event(xom_event_type event_type, int sever)
{
    const xom_event *event = map_find(xom_events, event_type);
    if (event && event->action)
        event->action(sever);
}

static int _xom_event_badness(xom_event_type event_type)
{
    if (event_type == XOM_BAD_TELEPORT)
        return player_in_a_dangerous_place() ? 3 : 1;

    const xom_event *event = map_find(xom_events, event_type);
    if (event)
        return div_rand_round(event->badness_10x, 10);
    return 0;
}

string xom_effect_to_name(xom_event_type effect)
{
    const xom_event *event = map_find(xom_events, effect);
    return event ? event->name : "bugginess";
}

/// Basic sanity checks on xom_events.
void validate_xom_events()
{
    string fails;
    set<string> action_names;

    for (int i = 0; i < XOM_LAST_REAL_ACT; ++i)
    {
        const xom_event_type event_type = static_cast<xom_event_type>(i);
        const xom_event *event = map_find(xom_events, event_type);
        if (!event)
        {
            fails += make_stringf("Xom event %d has no associated data!\n", i);
            continue;
        }

        if (action_names.count(event->name))
            fails += make_stringf("Duplicate name '%s'!\n", event->name);
        action_names.insert(event->name);

        if (_action_is_bad(event_type))
        {
            if ((event->badness_10x < 10 || event->badness_10x > 50)
                && event->badness_10x != -1) // implies it's special-cased
            {
                fails += make_stringf("'%s' badness %d outside 10-50 range.\n",
                                      event->name, event->badness_10x);
            }
        }
        else if (event->badness_10x)
        {
            fails += make_stringf("'%s' is not bad, but has badness!\n",
                                  event->name);
        }

        if (event_type != XOM_DID_NOTHING && !event->action)
            fails += make_stringf("No action for '%s'!\n", event->name);
    }

    dump_test_fails(fails, "xom-data");
}

#ifdef WIZARD
static bool _sort_xom_effects(const xom_effect_count &a,
                              const xom_effect_count &b)
{
    if (a.count == b.count)
        return a.effect < b.effect;

    return a.count > b.count;
}

static string _list_exploration_estimate()
{
    int explored = 0;
    int mapped   = 0;
    for (int k = 0; k < 10; ++k)
    {
        mapped   += _exploration_estimate(false);
        explored += _exploration_estimate(true);
    }
    mapped /= 10;
    explored /= 10;

    return make_stringf("mapping estimate: %d%%\nexploration estimate: %d%%\n",
                        mapped, explored);
}

// Loops over the entire piety spectrum and calls xom_acts() multiple
// times for each value, then prints the results into a file.
// TODO: Allow specification of niceness, tension, and boredness.
void debug_xom_effects()
{
    // Repeat N times.
    const int N = prompt_for_int("How many iterations over the "
                                 "entire piety range? ", true);

    if (N == 0)
    {
        canned_msg(MSG_OK);
        return;
    }

    FILE *ostat = fopen_u("xom_debug.stat", "w");
    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write 'xom_debug.stat'. Aborting.");
        return;
    }

    const int real_piety    = you.piety;
    const god_type real_god = you.religion;
    you.religion            = GOD_XOM;
    const int tension       = get_tension(GOD_XOM);

    fprintf(ostat, "---- STARTING XOM DEBUG TESTING ----\n");
    fprintf(ostat, "%s\n", dump_overview_screen().c_str());
    fprintf(ostat, "%s\n", screenshot().c_str());
    fprintf(ostat, "%s\n", _list_exploration_estimate().c_str());
    fprintf(ostat, "%s\n", mpr_monster_list().c_str());
    fprintf(ostat, " --> Tension: %d\n", tension);

    if (player_under_penance(GOD_XOM))
        fprintf(ostat, "You are under Xom's penance!\n");
    else if (_xom_is_bored())
        fprintf(ostat, "Xom is BORED.\n");
    fprintf(ostat, "\nRunning %d times through entire mood cycle.\n", N);
    fprintf(ostat, "---- OUTPUT EFFECT PERCENTAGES ----\n");

    vector<xom_event_type>          mood_effects;
    vector<vector<xom_event_type>>  all_effects;
    vector<string>                  moods;
    vector<int>                     mood_good_acts;

    string old_mood = "";
    string     mood = "";

    // Add an empty list to later add all effects to.
    all_effects.push_back(mood_effects);
    moods.emplace_back("total");
    mood_good_acts.push_back(0); // count total good acts

    int mood_good = 0;
    for (int p = 0; p <= MAX_PIETY; ++p)
    {
        you.piety     = p;
        int sever     = abs(p - HALF_MAX_PIETY);
        mood          = describe_xom_mood();
        if (old_mood != mood)
        {
            if (!old_mood.empty())
            {
                all_effects.push_back(mood_effects);
                mood_effects.clear();
                mood_good_acts.push_back(mood_good);
                mood_good_acts[0] += mood_good;
                mood_good = 0;
            }
            moods.push_back(mood);
            old_mood = mood;
        }

        // Repeat N times.
        for (int i = 0; i < N; ++i)
        {
            const xom_event_type result = xom_acts(sever, maybe_bool::maybe, tension,
                                                   true);

            mood_effects.push_back(result);
            all_effects[0].push_back(result);

            if (result <= XOM_LAST_GOOD_ACT)
                mood_good++;
        }
    }
    all_effects.push_back(mood_effects);
    mood_effects.clear();
    mood_good_acts.push_back(mood_good);
    mood_good_acts[0] += mood_good;

    const int num_moods = moods.size();
    vector<xom_effect_count> xom_ec_pairs;
    for (int i = 0; i < num_moods; ++i)
    {
        mood_effects    = all_effects[i];
        const int total = mood_effects.size();

        if (i == 0)
            fprintf(ostat, "\nTotal effects (all piety ranges)\n");
        else
            fprintf(ostat, "\nMood: You are %s\n", moods[i].c_str());

        fprintf(ostat, "GOOD%7.2f%%\n",
                (100.0 * (float) mood_good_acts[i] / (float) total));
        fprintf(ostat, "BAD %7.2f%%\n",
                (100.0 * (float) (total - mood_good_acts[i]) / (float) total));

        sort(mood_effects.begin(), mood_effects.end());

        xom_ec_pairs.clear();
        xom_event_type old_effect = XOM_DID_NOTHING;
        int count      = 0;
        for (int k = 0; k < total; ++k)
        {
            if (mood_effects[k] != old_effect)
            {
                if (count > 0)
                {
                    xom_ec_pairs.emplace_back(xom_effect_to_name(old_effect),
                                              count);
                }
                old_effect = mood_effects[k];
                count = 1;
            }
            else
                count++;
        }

        if (count > 0)
            xom_ec_pairs.emplace_back(xom_effect_to_name(old_effect), count);

        sort(xom_ec_pairs.begin(), xom_ec_pairs.end(), _sort_xom_effects);
        for (const xom_effect_count &xec : xom_ec_pairs)
        {
            fprintf(ostat, "%7.2f%%    %s\n",
                    (100.0 * xec.count / total),
                    xec.effect.c_str());
        }
    }
    fprintf(ostat, "---- FINISHED XOM DEBUG TESTING ----\n");
    fclose(ostat);
    mpr("Results written into 'xom_debug.stat'.");

    you.piety    = real_piety;
    you.religion = real_god;
}
#endif // WIZARD

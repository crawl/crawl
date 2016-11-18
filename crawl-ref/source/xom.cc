/**
 * @file
 * @brief All things Xomly
**/

#include "AppHdr.h"

#include "xom.h"

#include <algorithm>

#include "abyss.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#ifdef WIZARD
#include "dbg-util.h"
#endif
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "errors.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "losglobal.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-transloc.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "unwind.h"
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
// Form spells that require UC to be useful should get
// DUR_XOM_UC_BONUS applied in _xom_makes_you_cast_random_spell.
static const vector<spell_type> _xom_random_spells =
{
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_CONFUSING_TOUCH,
    SPELL_BEASTLY_APPENDAGE,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_SPIDER_FORM,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_ICE_FORM,
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_CAUSE_FEAR,
    SPELL_INTOXICATE,
    SPELL_RING_OF_FLAMES,
    SPELL_SHADOW_CREATURES,
    SPELL_EXCRUCIATING_WOUNDS,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_STATUE_FORM,
    SPELL_DISPERSAL,
    SPELL_ENGLACIATION,
    SPELL_DEATH_CHANNEL,
    SPELL_SUMMON_HYDRA,
    SPELL_HYDRA_FORM,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_DISCORD,
    SPELL_DISJUNCTION,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_NECROMUTATION,
    SPELL_DRAGON_FORM,
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
            xom_acts(abs(you.piety - HALF_MAX_PIETY), MB_MAYBE, tension);
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
            xom_acts(abs(you.piety - HALF_MAX_PIETY), MB_MAYBE, tension);
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
        if (cell_is_solid(pos) && !feat_is_closed_door(grd(pos)))
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(grd(*ai))
                                        || feat_is_closed_door(grd(*ai))))
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

static bool _teleportation_check(const spell_type spell = SPELL_TELEPORT_SELF)
{
    if (crawl_state.game_is_sprint())
        return false;

    switch (spell)
    {
    case SPELL_BLINK:
    case SPELL_TELEPORT_SELF:
        return !you.no_tele(false, false, spell == SPELL_BLINK);
    default:
        return true;
    }
}

static bool _transformation_check(const spell_type spell)
{
    transformation_type tran = TRAN_NONE;
    switch (spell)
    {
    case SPELL_BEASTLY_APPENDAGE:
        tran = TRAN_APPENDAGE;
        break;
    case SPELL_SPIDER_FORM:
        tran = TRAN_SPIDER;
        break;
    case SPELL_STATUE_FORM:
        tran = TRAN_STATUE;
        break;
    case SPELL_ICE_FORM:
        tran = TRAN_ICE_BEAST;
        break;
    case SPELL_HYDRA_FORM:
        tran = TRAN_HYDRA;
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
        return true;

    // Check whether existing enchantments/transformations, cursed
    // equipment or potential stat loss interfere with this
    // transformation.
    return transform(0, tran, true, true);
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
        if (!spell_is_useless(spell, true, true, false, true)
            && _transformation_check(spell))
        {
            ok_spells.push_back(spell);
        }
    }

    if (!ok_spells.size())
        return SPELL_NO_SPELL;
    return ok_spells[random2(ok_spells.size())];
}

/// Cast a random spell 'through' the player.
static void _xom_makes_you_cast_random_spell(int sever)
{
    const spell_type spell = _choose_random_spell(sever);
    if (spell == SPELL_NO_SPELL)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("spell effect").c_str());

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_RELIGION) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "_xom_makes_you_cast_random_spell(); spell: %d",
         spell);
#endif

    your_spells(spell, sever, false, false, true);
    if (spell == SPELL_SPIDER_FORM
           || spell == SPELL_ICE_FORM
           || spell == SPELL_HYDRA_FORM
           || spell == SPELL_DRAGON_FORM)
    {
        god_speaks(GOD_XOM, _get_xom_speech("unarmed bonus").c_str());
        you.increase_duration(DUR_XOM_UC_BONUS, 10 + random2(sever) * 10);
    }
    const string note = make_stringf("cast spell '%s'", spell_title(spell));
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

/// Cast a random spell appropriate for tense/dangerous situations.
static void _xom_good_spell(int sever)
{
    _xom_makes_you_cast_random_spell(sever);
}

/// Map out the level.
static void _xom_magic_mapping(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("divination").c_str());

    // power isn't relevant at present, but may again be, someday?
    const int power = stepdown_value(sever, 10, 10, 40, 45);
    magic_mapping(5 + power, 50 + random2avg(power * 2, 2), false);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                   "divination: magic mapping"), true);
}

/// Detect items across the level.
static void _xom_detect_items(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("divination").c_str());

    if (detect_items(sever) == 0)
        canned_msg(MSG_DETECT_NOTHING);
    else
        mpr("You detect items!");

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                   "divination: detect items"), true);
}

/// Detect creatures across the level.
static void _xom_detect_creatures(int sever)
{
    god_speaks(GOD_XOM, _get_xom_speech("divination").c_str());

    const int prev_detected = count_detected_mons();
    const int num_creatures = detect_creatures(sever);

    if (num_creatures == 0)
        canned_msg(MSG_DETECT_NOTHING);
    else if (num_creatures == prev_detected)
    {
        // This is not strictly true. You could have cast Detect
        // Creatures with a big enough fuzz that the detected glyph is
        // still on the map when the original one has been killed. Then
        // another one is spawned, so the number is the same as before.
        // There's no way we can check this, however.
        mpr("You detect no further creatures.");
    }
    else
        mpr("You detect creatures!");

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                   "divination: detect creatures"), true);
}

static void _try_brand_switch(const int item_index)
{
    if (item_index == NON_ITEM)
        return;

    item_def &item(mitm[item_index]);

    if (item.base_type != OBJ_WEAPONS)
        return;

    if (is_unrandom_artefact(item))
        return;

    // Only do it some of the time.
    if (one_chance_in(3))
        return;

    if (get_weapon_brand(item) == SPWPN_NORMAL)
        return;

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

    _try_brand_switch(thing_created);

    static char gift_buf[100];
    snprintf(gift_buf, sizeof(gift_buf), "god gift: %s",
             mitm[thing_created].name(DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, gift_buf), true);

    canned_msg(MSG_SOMETHING_APPEARS);
    move_item_to_grid(&thing_created, you.pos());

    if (thing_created == NON_ITEM) // if it fell into lava
        simple_god_message(" snickers.", GOD_XOM);

    stop_running();
}

/// Xom's 'acquirement'. A gift for the player, of a sort...
static void _xom_acquirement(int /*sever*/)
{
    god_speaks(GOD_XOM, _get_xom_speech("general gift").c_str());

    const object_class_type types[] =
    {
        OBJ_WEAPONS, OBJ_ARMOUR, OBJ_JEWELLERY,  OBJ_BOOKS,
        OBJ_STAVES,  OBJ_WANDS,  OBJ_MISCELLANY, OBJ_FOOD,  OBJ_GOLD,
        OBJ_MISSILES
    };
    const object_class_type force_class = RANDOM_ELEMENT(types);

    int item_index = NON_ITEM;
    if (!acquirement(force_class, GOD_XOM, false, &item_index)
        || item_index == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }

    _try_brand_switch(item_index);

    const string note = make_stringf("god gift: %s",
                                     mitm[item_index].name(DESC_PLAIN).c_str());
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
    return mon.alive() && mon.can_safely_mutate()
           && !mon.submerged();
}

static bool _choose_enchantable_monster(const monster& mon)
{
    return mon.alive() && !mon.wont_attack()
           && !mons_immune_magic(mon);
}

static bool _is_chaos_upgradeable(const item_def &item,
                                  const monster* mon)
{
    // Since Xom is a god, he is capable of changing randarts, but not
    // other artefacts.
    if (is_unrandom_artefact(item))
        return false;

    // Staves and rods can't be changed either, since they don't have brands
    // in the way other weapons do.
    if (item.base_type == OBJ_STAVES || item.base_type == OBJ_RODS)
        return false;

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

    // Leave branded items alone, since this is supposed to be an
    // upgrade.
    if (item.base_type == OBJ_MISSILES)
    {
        // Don't make boulders or throwing nets of chaos.
        if (item.sub_type == MI_LARGE_ROCK
            || item.sub_type == MI_THROWING_NET)
        {
            return false;
        }

        if (get_ammo_brand(item) == SPMSL_NORMAL)
            return true;
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
            return false;
        }

        if (get_weapon_brand(item) == SPWPN_NORMAL)
            return true;
    }

    return false;
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

    // Beogh presumably doesn't want Xom messing with his orcs, even if
    // it would give them a better weapon.
    if (mons_genus(mon.type) == MONS_ORC
        && (mon.is_priest() || coinflip()))
    {
        return false;
    }

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
        const item_def &item(mitm[midx]);

        // The monster already has a chaos weapon. Give the upgrade to
        // a different monster.
        if (is_chaotic_item(item))
            return false;

        if (_is_chaos_upgradeable(item, &mon))
        {
            if (item.base_type != OBJ_MISSILES)
                return true;

            // If, for some weird reason, a monster is carrying a bow
            // and javelins, then branding the javelins is okay, since
            // they won't be fired by the bow.
            if (!special_launcher || !has_launcher(item))
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

        description_level_type desc = mon->friendly() ? DESC_YOUR :
                                                        DESC_THE;
        string msg = apostrophise(mon->name(desc));

        msg += " ";

        msg += item.name(DESC_PLAIN, false, false, false);

        msg += " is briefly surrounded by a scintillating aura of "
               "random colours.";

        mpr(msg);
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

        // Make the pluses more like a randomly generated ego item.
        if (item.base_type == OBJ_WEAPONS)
            item.plus  += random2(5);
    }
}

static monster_type _xom_random_demon(int sever)
{
    const int roll = random2(1000 - (MAX_PIETY - sever) * 5);
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "_xom_random_demon(); sever = %d, roll: %d",
         sever, roll);
#endif
    monster_type dct = (roll >= 340) ? RANDOM_DEMON_COMMON
                                     : RANDOM_DEMON_LESSER;

    monster_type demon = MONS_PROGRAM_BUG;

    if (dct == RANDOM_DEMON_COMMON && one_chance_in(10))
        demon = MONS_CHAOS_SPAWN;
    else
        demon = summon_any_demon(dct);

    return demon;
}

static bool _player_is_dead()
{
    return you.hp <= 0
        || is_feat_dangerous(grd(you.pos()))
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
                                     10, POT_AGILITY,
                                     10, POT_BRILLIANCE,
                                     10, POT_INVISIBILITY,
                                     5,  POT_BERSERK_RAGE,
                                     1,  POT_EXPERIENCE);
    }
    while (!get_potion_effect(pot)->can_quaff()); // ugh

    god_speaks(GOD_XOM, _get_xom_speech("potion effect").c_str());

    if (pot == POT_INVISIBILITY)
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 1;

    _note_potion_effect(pot);

    get_potion_effect(pot)->effect(true, 150);

    level_change(); // need this for !xp - see mantis #3245
}

static void _confuse_monster(monster* mons, int sever)
{
    if (mons->check_clarity(false))
        return;
    if (mons->holiness() & (MH_NONLIVING | MH_PLANT))
        return;

    const bool was_confused = mons->confused();
    if (mons->add_ench(mon_enchant(ENCH_CONFUSION, 0,
          &menv[ANON_FRIENDLY_MONSTER], random2(sever) * 10)))
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
    // The number of allies is dependent on severity, though heavily
    // randomised.
    int numdemons = sever;
    for (int i = 0; i < 3; i++)
        numdemons = random2(numdemons + 1);
    numdemons = min(numdemons + 2, 16);

    // Limit number of demons by experience level.
    const int maxdemons = (you.experience_level);
    if (numdemons > maxdemons)
        numdemons = maxdemons;

    int num_actually_summoned = 0;

    for (int i = 0; i < numdemons; ++i)
    {
        monster_type mon_type = _xom_random_demon(sever);

        mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH);
        mg.set_summoned(&you, 3, MON_SUMM_AID, GOD_XOM);

        // Even though the friendlies are charged to you for accounting,
        // they should still show as Xom's fault if one of them kills you.
        mg.non_actor_summoner = "Xom";

        if (create_monster(mg))
            num_actually_summoned++;
    }

    if (num_actually_summoned)
    {
        god_speaks(GOD_XOM, _get_xom_speech("multiple summons").c_str());

        const string note = make_stringf("summons %d friendly demon%s",
                                         num_actually_summoned,
                                         num_actually_summoned > 1 ? "s" : "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
    }
}

/// Send a single pal to the player's aid, hopefully.
static void _xom_send_one_ally(int sever)
{
    const monster_type mon_type = _xom_random_demon(sever);

    mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH);
    mg.set_summoned(&you, 6, MON_SUMM_AID, GOD_XOM);

    mg.non_actor_summoner = "Xom";

    if (monster *summons = create_monster(mg))
    {
        god_speaks(GOD_XOM, _get_xom_speech("single summon").c_str());

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
        && mons.holiness() & MH_NATURAL)
    {
        mons.add_ench(one_chance_in(3) ? ENCH_GLOWING_SHAPESHIFTER
                                       : ENCH_SHAPESHIFTER);
    }

    const bool powerup = !(mons.wont_attack() ^ helpful);
    monster_polymorph(&mons, RANDOM_MONSTER,
                      powerup ? PPT_MORE : PPT_LESS);

    const bool see_new = you.can_see(mons);

    if (see_old || see_new)
    {
        const string new_name = see_new ? mons.full_name(DESC_PLAIN)
                                        : "something unseen";

        string note = make_stringf("polymorph %s -> %s",
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
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (_choose_mutatable_monster(**mi) && !mons_is_firewood(**mi))
            return *mi;
    return nullptr;
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

    // Make submerged monsters unsubmerge.
    mon1.del_ench(ENCH_SUBMERGED);
    mon2.del_ench(ENCH_SUBMERGED);

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
        WPN_CLUB,    WPN_SPEAR,      WPN_TRIDENT,      WPN_HALBERD,
        WPN_SCYTHE,  WPN_GLAIVE,     WPN_QUARTERSTAFF,
        WPN_BLOWGUN, WPN_SHORTBOW,   WPN_LONGBOW,      WPN_GIANT_CLUB,
        WPN_GIANT_SPIKED_CLUB
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
static void _xom_snakes_to_sticks(int sever)
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

        const object_class_type base_type = x_chance_in_y(3,5) ? OBJ_MISSILES
                                                               : OBJ_WEAPONS;

        const int sub_type =
            (base_type == OBJ_MISSILES ?
             (x_chance_in_y(3,5) ? MI_ARROW : MI_JAVELIN)
             : _xom_random_stickable(mi->get_experience_level()));

        int item_slot = items(false, base_type, sub_type,
                              mi->get_experience_level() / 3 - 1,
                              0, -1);

        if (item_slot == NON_ITEM)
            continue;

        item_def &item(mitm[item_slot]);

        // Always limit the quantity to 1.
        item.quantity = 1;

        // Output some text since otherwise snakes will disappear silently.
        mprf("%s reforms as %s.", mi->name(DESC_THE).c_str(),
             item.name(DESC_A).c_str());

        // Dismiss monster silently.
        move_item_to_grid(&item_slot, mi->pos());
        monster_die(*mi, KILL_DISMISSED, NON_MONSTER, true, false);
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

        const item_def weapon = mitm[mweap];

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
         mitm[wpn].name(DESC_PLAIN).c_str());

    destroy_item(dancing->inv[MSLOT_WEAPON]);

    dancing->inv[MSLOT_WEAPON] = wpn;
    mitm[wpn].set_holding_monster(*dancing);
    dancing->colour = mitm[wpn].get_colour();
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
        if (!mutate(good ? RANDOM_GOOD_MUTATION : RANDOM_XOM_MUTATION,
                    good ? "Xom's grace" : "Xom's mischief",
                    failMsg, false, true, false, MUTCLASS_NORMAL))
        {
            failMsg = false;
        }
    }
}

static void _xom_give_good_mutations(int) { _xom_give_mutations(true); }
static void _xom_give_bad_mutations(int) { _xom_give_mutations(false); }

/**
 * Have Xom throw divine lightning. May include the player as a victim.
 */
static void _xom_throw_divine_lightning(int /*sever*/)
{
    const bool protection = you.hp <= random2(201);
    if (protection)
        you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 1;

    god_speaks(GOD_XOM, "The area is suffused with divine lightning!");

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
        set_hp(1);
        you.reset_escaped_death();
    }

    const string note = make_stringf("divine lightning%s",
                                     protection ? " (protected)" : "");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

/// What scenery nearby would Xom like to mess with, if any?
static vector<coord_def> _xom_scenery_candidates()
{
    vector<coord_def> candidates;
    vector<coord_def> closed_doors;
    vector<coord_def> open_doors;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        if (!you.see_cell(*ri))
            continue;

        dungeon_feature_type feat = grd(*ri);
        if (feat_is_fountain(feat))
            candidates.push_back(*ri);
        else if (feat_is_closed_door(feat))
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
        else if (feat == DNGN_OPEN_DOOR && !actor_at(*ri)
                 && igrd(*ri) == NON_ITEM)
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
                    if (actor_at(dc) || igrd(dc) != NON_ITEM)
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
    // FIXME: Changed fountains behind doors are not properly remembered.
    //        (At least in tiles.)
    candidates.insert(end(candidates), begin(open_doors), end(open_doors));
    candidates.insert(end(candidates), begin(closed_doors), end(closed_doors));

    return candidates;
}

/// Place one or more altars to Xom nearish the player.
static void _xom_place_altars()
{
    coord_def place;
    bool success = false;
    const int max_altars = max(1, random2(random2(14)));
    for (int tries = max_altars; tries > 0; --tries)
    {
        if ((random_near_space(&you, you.pos(), place, false)
             || random_near_space(&you, you.pos(), place, true))
            && grd(place) == DNGN_FLOOR)
        {
            grd(place) = DNGN_ALTAR_XOM;
            success = true;
        }
    }

    if (success)
    {
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1,
                       "scenery: create altars"), true);
        god_speaks(GOD_XOM, _get_xom_speech("scenery").c_str());
    }
}

/// Mess with nearby terrain features, more-or-less harmlessly.
static void _xom_change_scenery(int /*sever*/)
{
    vector<coord_def> candidates = _xom_scenery_candidates();

    if (candidates.empty())
    {
        _xom_place_altars();
        return;
    }

    int fountains_blood = 0;
    int doors_open      = 0;
    int doors_close     = 0;
    for (coord_def pos : candidates)
    {
        switch (grd(pos))
        {
        case DNGN_CLOSED_DOOR:
        case DNGN_RUNED_DOOR:
            grd(pos) = DNGN_OPEN_DOOR;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                doors_open++;
            break;
        case DNGN_OPEN_DOOR:
            grd(pos) = DNGN_CLOSED_DOOR;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                doors_close++;
            break;
        case DNGN_DRY_FOUNTAIN:
        case DNGN_FOUNTAIN_BLUE:
            if (x_chance_in_y(fountains_blood, 3))
                continue;

            grd(pos) = DNGN_FOUNTAIN_BLOOD;
            set_terrain_changed(pos);
            if (you.see_cell(pos))
                fountains_blood++;
            break;
        default:
            break;
        }
    }
    if (!doors_open && !doors_close && !fountains_blood)
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
    if (!effects.empty())
    {
        mprf("%s!",
             comma_separated_line(effects.begin(), effects.end(),
                                  ", and ").c_str());
        effects.clear();
    }

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

/// Xom hurls fireballs at your foes! Or, possibly, 'fireballs'.
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

        bolt beam;

        beam.flavour      = BEAM_FIRE;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(2, 4 + sever / 10);
        beam.target       = mi->pos();
        beam.name         = "fireball";
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

static void _xom_enchant_monster(bool helpful)
{
    monster* mon = choose_random_nearby_monster(0, _choose_enchantable_monster);
    if (!mon)
        return;

    god_speaks(GOD_XOM,
               helpful ? _get_xom_speech("good enchant monster").c_str()
                       : _get_xom_speech("bad enchant monster").c_str());

    beam_type ench;

    if (helpful) // To the player, not the monster.
    {
        static const beam_type enchantments[] =
        {
            BEAM_PETRIFY,
            BEAM_SLOW,
            BEAM_PARALYSIS,
            BEAM_ENSLAVE,
        };
        ench = RANDOM_ELEMENT(enchantments);
    }
    else
    {
        static const beam_type enchantments[] =
        {
            BEAM_HASTE,
            BEAM_MIGHT,
            BEAM_AGILITY,
            BEAM_INVISIBILITY,
            BEAM_RESISTANCE,
        };
        ench = RANDOM_ELEMENT(enchantments);
    }

    enchant_actor_with_flavour(mon, 0, ench);

    // Take a note.
    const string note = make_stringf("enchant monster %s",
                                     helpful ? "(good)" : "(bad)");
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static void _xom_good_enchant_monster(int /*sever*/) {
    _xom_enchant_monster(true);
}
static void _xom_bad_enchant_monster(int /*sever*/) {
    _xom_enchant_monster(false);
}

/// Toss some fog around the player. Helping...?
static void _xom_fog(int /*sever*/)
{
    big_cloud(CLOUD_RANDOM_SMOKE, &you, you.pos(), 50, 8 + random2(8));
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "fog"), true);
    god_speaks(GOD_XOM, _get_xom_speech("cloud").c_str());
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
        if (item.defined() && !item_is_equipped(item)
            && !item.is_critical())
        {
            inv_items.push_back(&item);
        }
    }

    // Assure that the messages vector has at least one element.
    messages.emplace_back("Nothing appears to happen... Ominous!");

    ///////////////////////////////////
    // Dungeon feature dependent stuff.

    FixedBitVector<NUM_FEATURES> in_view;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
        in_view.set(grd(*ri));

    if (in_view[DNGN_LAVA])
        messages.emplace_back("The lava spits out sparks!");

    if (in_view[DNGN_SHALLOW_WATER] || in_view[DNGN_DEEP_WATER])
    {
        messages.emplace_back("The water briefly bubbles.");
        messages.emplace_back("The water briefly swirls.");
        messages.emplace_back("The water briefly glows.");
    }

    if (in_view[DNGN_DEEP_WATER])
    {
        messages.emplace_back("From the corner of your eye you spot something "
                           "lurking in the deep water.");
    }

    if (in_view[DNGN_ORCISH_IDOL])
        priority.emplace_back("The idol of Beogh turns to glare at you.");

    if (in_view[DNGN_GRANITE_STATUE])
        priority.emplace_back("The granite statue turns to stare at you.");

    if (in_view[DNGN_CLEAR_ROCK_WALL] || in_view[DNGN_CLEAR_STONE_WALL]
        || in_view[DNGN_CLEAR_PERMAROCK_WALL])
    {
        messages.emplace_back("Dim shapes swim through the translucent wall.");
    }

    if (in_view[DNGN_CRYSTAL_WALL])
        messages.emplace_back("Dim shapes swim through the crystal wall.");

    if (in_view[DNGN_METAL_WALL])
    {
        messages.emplace_back("Tendrils of electricity crawl over the metal "
                              "wall!");
    }

    if (in_view[DNGN_FOUNTAIN_BLUE] || in_view[DNGN_FOUNTAIN_SPARKLING])
    {
        priority.emplace_back("The water in the fountain briefly bubbles.");
        priority.emplace_back("The water in the fountain briefly swirls.");
        priority.emplace_back("The water in the fountain briefly glows.");
    }

    if (in_view[DNGN_DRY_FOUNTAIN])
    {
        priority.emplace_back("Water briefly sprays from the dry fountain.");
        priority.emplace_back("Dust puffs up from the dry fountain.");
    }

    if (in_view[DNGN_STONE_ARCH])
    {
        priority.emplace_back("The stone arch briefly shows a sunny meadow on "
                              "the other side.");
    }

    const dungeon_feature_type feat = grd(you.pos());

    if (!feat_is_solid(feat) && feat_stair_direction(feat) == CMD_NO_CMD
        && !feat_is_trap(feat) && feat != DNGN_STONE_ARCH
        && feat != DNGN_OPEN_DOOR && feat != DNGN_ABANDONED_SHOP)
    {
        const string feat_name = feature_description_at(you.pos(), false,
                                                        DESC_THE, false);

        if (you.airborne())
        {
            // Don't put airborne messages into the priority vector for
            // anyone who can fly a lot.
            vector<string>* vec;
            if (you.racial_permanent_flight())
                vec = &messages;
            else
                vec = &priority;

            vec->push_back(feat_name
                           + " seems to fall away from under you!");
            vec->push_back(feat_name
                           + " seems to rush up at you!");

            if (feat_is_water(feat))
            {
                priority.emplace_back("Something invisible splashes into the "
                                      "water beneath you!");
            }
        }
        else if (feat_is_water(feat))
        {
            priority.emplace_back("The water briefly recedes away from you.");
            priority.emplace_back("Something invisible splashes into the water "
                                  "beside you!");
        }
    }

    if (feat_has_solid_floor(feat) && !inv_items.empty())
    {
        const item_def &item = **random_iterator(inv_items);

        string name;
        if (item.quantity == 1)
            name = item.name(DESC_YOUR, false, false, false);
        else
        {
            name  = "One of ";
            name += item.name(DESC_YOUR, false, false, false);
        }
        messages.push_back(name + " falls out of your pack, then "
                           "immediately jumps back in!");
    }

    //////////////////////////////////////////////
    // Body, player species, transformations, etc.

    if (you.species == SP_MUMMY && you_can_wear(EQ_BODY_ARMOUR, true))
    {
        messages.emplace_back("You briefly get tangled in your bandages.");
        if (!you.airborne() && !you.swimming())
            messages.emplace_back("You trip over your bandages.");
    }

    {
        string str = "A monocle briefly appears over your ";
        str += coinflip() ? "right" : "left";
        if (you.form == TRAN_SPIDER)
        {
            if (coinflip())
                str += " primary";
            else
            {
                str += random_choose(" front", " middle", " rear");
                str += " secondary";
            }
        }
        str += " eye.";
        messages.push_back(str);
    }

    if (species_has_hair(you.species))
    {
        messages.emplace_back("Your eyebrows briefly feel incredibly bushy.");
        messages.emplace_back("Your eyebrows wriggle.");
    }

    if (you.species != SP_NAGA && !you.fishtail && !you.airborne())
        messages.emplace_back("You do an impromptu tapdance.");

    ///////////////////////////
    // Equipment related stuff.

    if (you_can_wear(EQ_WEAPON, true)
        && !you.slot_item(EQ_WEAPON))
    {
        string str = "A fancy cane briefly appears in your ";
        str += you.hand_name(false);
        str += ".";

        messages.push_back(str);
    }

    if (you.slot_item(EQ_CLOAK))
        messages.emplace_back("Your cloak billows in an unfelt wind.");

    if (item_def* item = you.slot_item(EQ_HELMET))
    {
        string str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " leaps into the air, briefly spins, then lands back on "
               "your head!";

        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_BOOTS))
    {
        if (item->sub_type == ARM_BOOTS && !you.cannot_act())
        {
            string name = item->name(DESC_BASENAME, false, false, false);
            name = replace_all(name, "pair of ", "");

            string str = "You compulsively click the heels of your ";
            str += name;
            str += " together three times.";
            messages.push_back(str);
        }
    }

    if (item_def* item = you.slot_item(EQ_SHIELD))
    {
        string str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " spins!";

        messages.push_back(str);

        str = "Your ";
        str += item->name(DESC_BASENAME, false, false, false);
        str += " briefly flashes a lurid colour!";
        messages.push_back(str);
    }

    if (item_def* item = you.slot_item(EQ_BODY_ARMOUR))
    {
        string str;
        string name = item->name(DESC_BASENAME, false, false, false);

        if (name.find("dragon") != string::npos)
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
                 && item->sub_type <= ARM_PLATE_ARMOUR)
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
    if (!inv_items.empty())
    {
        item_def &item = **random_iterator(inv_items);

        string name = item.name(DESC_YOUR, false, false, false);
        string verb = coinflip() ? "glow" : "vibrate";

        if (item.quantity == 1)
            verb += "s";

        messages.push_back(name + " briefly " + verb + ".");
    }

    if (!priority.empty() && coinflip())
        mpr(priority[random2(priority.size())]);
    else
        mpr(messages[random2(messages.size())]);
}

static void _get_hand_type(string &hand, bool &can_plural)
{
    hand       = "";
    can_plural = true;

    vector<string> hand_vec;
    vector<bool>   plural_vec;
    bool           plural;

    hand_vec.push_back(you.hand_name(false, &plural));
    plural_vec.push_back(plural);

    if (you.species != SP_NAGA || form_changed_physiology())
    {
        if (item_def* item = you.slot_item(EQ_BOOTS))
        {
            hand_vec.emplace_back(item->name(DESC_BASENAME, false, false, false));
            plural = false; // "pair of boots" is singular
        }
        else
            hand_vec.push_back(you.foot_name(false, &plural));
        plural_vec.push_back(plural);
    }

    if (you.form == TRAN_SPIDER)
    {
        hand_vec.emplace_back("mandible");
        plural_vec.push_back(true);
    }
    else if (you.species != SP_MUMMY && you.species != SP_OCTOPODE
             && !player_mutation_level(MUT_BEAK)
          || form_changed_physiology())
    {
        hand_vec.emplace_back("nose");
        plural_vec.push_back(false);
    }

    if (you.form == TRAN_BAT
        || you.species != SP_MUMMY && you.species != SP_OCTOPODE
           && !form_changed_physiology())
    {
        hand_vec.emplace_back("ear");
        plural_vec.push_back(true);
    }

    if (!form_changed_physiology()
        && you.species != SP_FELID && you.species != SP_OCTOPODE)
    {
        hand_vec.emplace_back("elbow");
        plural_vec.push_back(true);
    }

    ASSERT(hand_vec.size() == plural_vec.size());
    ASSERT(!hand_vec.empty());

    const unsigned int choice = random2(hand_vec.size());

    hand       = hand_vec[choice];
    can_plural = plural_vec[choice];
}

static void _xom_miscast(const int max_level, const bool nasty)
{
    ASSERT_RANGE(max_level, 0, 4);

    const char* speeches[4] =
    {
        XOM_SPEECH("zero miscast effect"),
        XOM_SPEECH("minor miscast effect"),
        XOM_SPEECH("medium miscast effect"),
        XOM_SPEECH("major miscast effect"),
    };

    const char* causes[4] =
    {
        "the mischief of Xom",
        "the capriciousness of Xom",
        "the capriciousness of Xom",
        "the severe capriciousness of Xom"
    };

    const char* speech_str = speeches[max_level];
    const char* cause_str  = causes[max_level];

    const int level = (nasty ? 1 + random2(max_level)
                             : random2(max_level + 1));

    // Take a note.
    const char* levels[4] = { "harmless", "mild", "medium", "severe" };
    const auto school = spschools_type::exponent(random2(SPTYP_LAST_EXPONENT + 1));
    string desc = make_stringf("%s %s miscast", levels[level],
                               spelltype_short_name(school));
#ifdef NOTE_DEBUG_XOM
    if (nasty)
        desc += " (Xom was nasty)";
#endif
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, desc), true);

    string hand_str;
    bool   can_plural;

    _get_hand_type(hand_str, can_plural);

    // If Xom's not being nasty, then prevent spell miscasts from
    // killing the player.
    const int lethality_margin  = nasty ? 0 : random_range(1, 4);

    god_speaks(GOD_XOM, _get_xom_speech(speech_str).c_str());

    MiscastEffect(&you, nullptr, GOD_MISCAST + GOD_XOM,
                  (spschool_flag_type)school, level, cause_str, NH_DEFAULT,
                  lethality_margin, hand_str, can_plural);
}

static bool _miscast_is_nasty(int sever)
{
    return sever >= 5 && _xom_feels_nasty();
}

static void _xom_harmless_miscast(int sever)
{
    _xom_miscast(0, _miscast_is_nasty(sever));
}
static void _xom_minor_miscast(int sever)
{
    _xom_miscast(1, _miscast_is_nasty(sever));
}
static void _xom_major_miscast(int sever)
{
    _xom_miscast(2, _miscast_is_nasty(sever));
}
static void _xom_critical_miscast(int sever)
{
    _xom_miscast(3, _miscast_is_nasty(sever));
}

static void _xom_chaos_upgrade(int /*sever*/)
{
    monster* mon = choose_random_nearby_monster(0, _choose_chaos_upgrade);

    if (!mon)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("chaos upgrade").c_str());

    mon_inv_type slots[] = {MSLOT_WEAPON, MSLOT_ALT_WEAPON, MSLOT_MISSILE};

    bool rc = false;
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

    return grd(pos) == DNGN_FLOOR;
}

bool move_stair(coord_def stair_pos, bool away, bool allow_under)
{
    if (!allow_under)
        ASSERT(stair_pos != you.pos());

    dungeon_feature_type feat = grd(stair_pos);
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

    while (!you.see_cell(ray.pos()) || grd(ray.pos()) != DNGN_FLOOR)
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

    string stair_str = feature_description_at(stair_pos, false, DESC_THE, false);

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

        dungeon_feature_type feat = grd(*ri);
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
        if (feat_is_staircase(grd(loc)))
            real_stairs = true;

    // Don't mention staircases if there aren't any nearby.
    string stair_msg = _get_xom_speech("repel stairs");
    if (stair_msg.find("@staircase@") != string::npos)
    {
        string feat_name;
        if (!real_stairs)
        {
            if (feat_is_escape_hatch(grd(stairs_avail[0])))
                feat_name = "escape hatch";
            else
                feat_name = "gate";
        }
        else
            feat_name = "staircase";
        stair_msg = replace_all(stair_msg, "@staircase@", feat_name);
    }

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
    you.props[XOM_CLOUD_TRAIL_TYPE_KEY] =
        // 80% chance of a useful trail
        random_choose_weighted(20, CLOUD_CHAOS,
                               10, CLOUD_MAGIC_TRAIL,
                               5,  CLOUD_MIASMA,
                               5,  CLOUD_PETRIFY,
                               5,  CLOUD_MUTAGENIC,
                               5,  CLOUD_NEGATIVE_ENERGY);

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "cloud trail"), true);

    const string speech = _get_xom_speech("cloud trail");
    god_speaks(GOD_XOM, speech.c_str());
}

static void _xom_statloss(int /*sever*/)
{
    const string speech = _get_xom_speech("draining or torment");
    const bool nasty = _xom_feels_nasty();

    const stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
    int loss = 1;

    // Don't kill the player unless Xom is being nasty.
    if (nasty)
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
    const string speech = _get_xom_speech("draining or torment");
    god_speaks(GOD_XOM, speech.c_str());

    drain_player(100, true);

    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "draining"), true);
}

static void _xom_torment(int /*sever*/)
{
    const string speech = _get_xom_speech("draining or torment");
    god_speaks(GOD_XOM, speech.c_str());

    torment_player(0, TORMENT_XOM);

    const string note = make_stringf("torment (%d/%d hp)", you.hp, you.hp_max);
    take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);
}

static monster* _xom_summon_hostile(monster_type hostile)
{
    return create_monster(mgen_data::hostile_at(hostile, true, you.pos())
                          .set_summoned(nullptr, 4, MON_SUMM_WRATH, GOD_XOM)
                          .set_non_actor_summoner("Xom"));
}

static void _xom_summon_hostiles(int sever)
{
    int num_summoned = 0;
    const bool shadow_creatures = one_chance_in(3);

    if (shadow_creatures)
    {
        // Small number of shadow creatures.
        int count = 1 + random2(4);
        for (int i = 0; i < count; ++i)
            if (_xom_summon_hostile(RANDOM_MOBILE_MONSTER))
                num_summoned++;
    }
    else
    {
        // The number of demons is dependent on severity, though heavily
        // randomised.
        int numdemons = sever;
        for (int i = 0; i < 3; ++i)
            numdemons = random2(numdemons + 1);
        numdemons = min(numdemons + 1, 14);

        // Limit number of demons by experience level.
        if (!you.penance[GOD_XOM])
        {
            const int maxdemons = (you.experience_level / 2);
            if (numdemons > maxdemons)
                numdemons = maxdemons;
        }

        for (int i = 0; i < numdemons; ++i)
            if (_xom_summon_hostile(_xom_random_demon(sever)))
                num_summoned++;
    }

    if (num_summoned > 0)
    {
        const string note = make_stringf("summons %d hostile %s%s",
                                         num_summoned,
                                         shadow_creatures ? "shadow creature"
                                                          : "demon",
                                         num_summoned > 1 ? "s" : "");
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, note), true);

        const string speech = _get_xom_speech("hostile monster");
        god_speaks(GOD_XOM, speech.c_str());
    }
}

static bool _has_min_banishment_level()
{
    return you.experience_level >= 9;
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

    // If Xom is bored, banishment becomes viable earlier.
    if (_xom_is_bored())
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

    // Handles note taking, scales depth by XL
    banished("Xom", you.experience_level);
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

    you.increase_duration(DUR_CLEAVE, 10 + random2(sever) * 10);

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
    const transformation_type orig_form)
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

    string speech_type = XOM_SPEECH("accidental homicide");

    const dungeon_feature_type feat = grd(you.pos());

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

    if (x_chance_in_y(3, sever))
    {
        const xom_event_type divination
            = random_choose(XOM_GOOD_MAGIC_MAPPING,
                            XOM_GOOD_DETECT_CREATURES,
                            XOM_GOOD_DETECT_ITEMS);

        if (divination == XOM_GOOD_DETECT_CREATURES)
        {
            return divination; // useful regardless of exploration state

        // Only do mmap/detect items if there's a decent chunk of unexplored
        }
        // level left
        const int explored = _exploration_estimate(false);
        if (explored <= 80 || x_chance_in_y(explored, 100))
            return divination;
    }

    if (x_chance_in_y(4, sever) && tension > 0
        && _choose_random_spell(sever) != SPELL_NO_SPELL)
    {
        return XOM_GOOD_SPELL;
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

    if (tension > random2(5) && x_chance_in_y(7, sever)
        && !player_mutation_level(MUT_NO_LOVE))
    {
        return XOM_GOOD_SINGLE_ALLY;
    }
    if (tension < random2(5) && x_chance_in_y(8, sever)
        && !_xom_scenery_candidates().empty() || one_chance_in(8))
    {
        return XOM_GOOD_SCENERY;
    }

    if (x_chance_in_y(9, sever) && mon_nearby(_hostile_snake))
        return XOM_GOOD_SNAKES;

    if (tension > random2(10) && x_chance_in_y(10, sever)
        && !player_mutation_level(MUT_NO_LOVE))
    {
        return XOM_GOOD_ALLIES;
    }
    if (tension > random2(8) && x_chance_in_y(11, sever)
        && _find_monster_with_animateable_weapon()
        && !player_mutation_level(MUT_NO_LOVE))
    {
        return XOM_GOOD_ANIMATE_MON_WPN;
    }

    if (x_chance_in_y(12, sever) && _xom_mons_poly_target() != nullptr)
        return XOM_GOOD_POLYMORPH;

    if (tension > 0 && x_chance_in_y(13, sever))
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

            // Skip adjacent monsters, and skip non-hostile monsters if not feeling nasty.
            if (!adjacent(you.pos(), mi->pos())
                 && (!mi->wont_attack() || _xom_feels_nasty()))
            {
                return XOM_GOOD_DESTRUCTION;
            }
        }
    }

    if (tension > random2(5) && x_chance_in_y(14, sever))
        return XOM_GOOD_CLEAVING;

    if (tension > 0 && x_chance_in_y(15, sever) && !cloud_at(you.pos()))
        return XOM_GOOD_FOG;

    if (random2(tension) < 15 && x_chance_in_y(16, sever))
    {
        return x_chance_in_y(sever, 201) ? XOM_GOOD_ACQUIREMENT
                                         : XOM_GOOD_RANDOM_ITEM;
    }

    if (!player_in_branch(BRANCH_ABYSS) && x_chance_in_y(17, sever)
        && _teleportation_check())
    {
        // This is not very interesting if the level is already fully
        // explored (presumably cleared). Even then, it may
        // occasionally happen.
        const int explored = _exploration_estimate(true);
        if (explored < 80 || !x_chance_in_y(explored, 120))
            return XOM_GOOD_TELEPORT;
    }

    if (random2(tension) < 5 && x_chance_in_y(19, sever)
        && x_chance_in_y(16, how_mutated())
        && you.can_safely_mutate())
    {
        return XOM_GOOD_MUTATION;
    }

    if (tension > 0 && x_chance_in_y(20, sever)
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

    return XOM_DID_NOTHING;
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

    if (!nasty && x_chance_in_y(3, sever))
    {
        return one_chance_in(3) ? XOM_BAD_MISCAST_PSEUDO
                                : XOM_BAD_MISCAST_HARMLESS;
    }

    if (!nasty && x_chance_in_y(4, sever))
        return XOM_BAD_MISCAST_MINOR;

    // Sometimes do noise out of combat.
    if ((tension > 0 || coinflip()) && x_chance_in_y(6, sever))
        return XOM_BAD_NOISE;
    if (tension > 0 && x_chance_in_y(7, sever))
        return XOM_BAD_ENCHANT_MONSTER;

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

    if (x_chance_in_y(12, sever))
        return XOM_BAD_MISCAST_MAJOR;
    if (x_chance_in_y(14, sever) && mon_nearby(_choose_chaos_upgrade))
        return XOM_BAD_CHAOS_UPGRADE;
    if (x_chance_in_y(15, sever) && !player_in_branch(BRANCH_ABYSS)
        && _teleportation_check())
    {
        const int explored = _exploration_estimate(true);
        if (nasty && (explored >= 40 || tension > 10)
            || explored >= 60 + random2(40))
        {
            // TODO: invert this conditional
        }
        else
            return XOM_BAD_TELEPORT;
    }
    if (x_chance_in_y(16, sever))
        return XOM_BAD_POLYMORPH;
 // Pushing stairs/exits is always hilarious in the Abyss!
    if ((tension > 0 || player_in_branch(BRANCH_ABYSS))
        && x_chance_in_y(17, sever) && !_nearby_stairs().empty()
        && !you.duration[DUR_REPEL_STAIRS_MOVE]
        && !you.duration[DUR_REPEL_STAIRS_CLIMB])
    {
        if (one_chance_in(5)
            || feat_stair_direction(grd(you.pos())) != CMD_NO_CMD
                && grd(you.pos()) != DNGN_ENTER_SHOP)
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
    if (x_chance_in_y(20, sever))
        return XOM_BAD_MISCAST_CRITICAL;

    if (x_chance_in_y(21, sever))
    {
        if (coinflip())
            return XOM_BAD_STATLOSS;
        if (coinflip())
        {
            if (player_prot_life() < 3)
                return XOM_BAD_DRAINING;
            // else choose something else
        } else if (!player_res_torment(false))
            return XOM_BAD_TORMENT;
        // else choose something else
    }
    if (tension > 0 && x_chance_in_y(22, sever)
        && !cloud_at(you.pos()))
    {
        return XOM_BAD_CHAOS_CLOUD;
    }
    if (one_chance_in(sever) && !player_in_branch(BRANCH_ABYSS)
        && _allow_xom_banishment())
    {
        return xom_maybe_reverts_banishment(true, true);
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

    if (_player_is_dead())
    {
        // This should only happen if the player used wizard mode to
        // escape death from deep water or lava.
        ASSERT(you.wizard);
        ASSERT(!you.did_escape_death());
        if (is_feat_dangerous(grd(you.pos())))
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

    // Bad mojo. (this loop, that is)
    while (true)
    {
        const xom_event_type action = _xom_choose_bad_action(sever, tension);
        if (action != XOM_DID_NOTHING)
            return action;
    }

    die("This should never happen.");
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
    const transformation_type orig_form = you.form;
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
 *                      If MB_MAYBE, determined by xom's whim.
 *                      May be overridden.
 * @param tension       How much danger we think the player's currently in.
 * @return              Whichever action Xom took, or XOM_DID_NOTHING.
 */
xom_event_type xom_acts(int sever, maybe_bool nice, int tension, bool debug)
{
    bool niceness = tobool(nice, xom_is_nice(tension));

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

static int _death_is_worth_saving(const kill_method_type killed_by,
                                  const char *aux)
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
 * @param aux         Additional string describing this death.
 * @return            True if Xom saves your life, false otherwise.
 */
bool xom_saves_your_life(const kill_method_type death_type, const char *aux)
{
    if (!you_worship(GOD_XOM) || _xom_feels_nasty())
        return false;

    // If this happens, don't bother.
    if (you.hp_max < 1 || you.experience_level < 1)
        return false;

    // Generally a rare effect.
    if (!one_chance_in(20))
        return false;

    if (!_death_is_worth_saving(death_type, aux))
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
            mpr(you.duration[DUR_STEALTH] ? "You feel more catlike."
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
        search_around();
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
static void _xom_bad_teleport(int sever)
{
    god_speaks(GOD_XOM,
               _get_xom_speech("teleportation journey").c_str());

    int count = 0;
    do
    {
        you_teleport_now();
        search_around();
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
    check_place_cloud(CLOUD_CHAOS, you.pos(), 3 + random2(12)*3,
                      nullptr, random_range(5,15));
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
    { XOM_GOOD_MAGIC_MAPPING, { "magic mapping", _xom_magic_mapping }},
    { XOM_GOOD_DETECT_CREATURES, { "detect creatures", _xom_detect_creatures }},
    { XOM_GOOD_DETECT_ITEMS, { "detect items", _xom_detect_items }},
    { XOM_GOOD_SPELL, { "good spell", _xom_good_spell }},
    { XOM_GOOD_CONFUSION, { "confuse monsters", _xom_confuse_monsters }},
    { XOM_GOOD_SINGLE_ALLY, { "single ally", _xom_send_one_ally }},
    { XOM_GOOD_ANIMATE_MON_WPN, { "animate monster weapon",
                                  _xom_animate_monster_weapon }},
    { XOM_GOOD_RANDOM_ITEM, { "random item gift", _xom_random_item }},
    { XOM_GOOD_ACQUIREMENT, { "acquirement", _xom_acquirement }},
    { XOM_GOOD_ALLIES, { "summon allies", _xom_send_allies }},
    { XOM_GOOD_POLYMORPH, { "good polymorph", _xom_good_polymorph }},
    { XOM_GOOD_TELEPORT, { "good teleportation", _xom_good_teleport }},
    { XOM_GOOD_MUTATION, { "good mutations", _xom_give_good_mutations }},
    { XOM_GOOD_LIGHTNING, { "lightning", _xom_throw_divine_lightning }},
    { XOM_GOOD_SCENERY, { "change scenery", _xom_change_scenery }},
    { XOM_GOOD_SNAKES, { "snakes to sticks", _xom_snakes_to_sticks }},
    { XOM_GOOD_DESTRUCTION, { "mass fireball", _xom_real_destruction }},
    { XOM_GOOD_FAKE_DESTRUCTION, { "fake fireball", _xom_fake_destruction }},
    { XOM_GOOD_ENCHANT_MONSTER, { "good enchant monster",
                                  _xom_good_enchant_monster }},
    { XOM_GOOD_FOG, { "fog", _xom_fog }},
    { XOM_GOOD_CLOUD_TRAIL, { "cloud trail", _xom_cloud_trail }},
    { XOM_GOOD_CLEAVING, { "cleaving", _xom_cleaving }},

    { XOM_BAD_MISCAST_PSEUDO, { "pseudo-miscast", _xom_pseudo_miscast, 10}},
    { XOM_BAD_MISCAST_HARMLESS, { "harmless miscast",
                                    _xom_harmless_miscast, 10}},
    { XOM_BAD_MISCAST_MINOR, { "minor miscast", _xom_minor_miscast, 10}},
    { XOM_BAD_MISCAST_MAJOR, { "major miscast", _xom_major_miscast, 20}},
    { XOM_BAD_MISCAST_CRITICAL, { "critical miscast",
                                    _xom_critical_miscast, 45}},
    { XOM_BAD_NOISE, { "noise", _xom_noise, 10 }},
    { XOM_BAD_ENCHANT_MONSTER, { "bad enchant monster",
                                 _xom_bad_enchant_monster, 10}},
    { XOM_BAD_BLINK_MONSTERS, { "blink monsters", _xom_blink_monsters, 10}},
    { XOM_BAD_CONFUSION, { "confuse player", _xom_player_confusion_effect, 13}},
    { XOM_BAD_SWAP_MONSTERS, { "swap monsters", _xom_rearrange_pieces, 20 }},
    { XOM_BAD_CHAOS_UPGRADE, { "chaos upgrade", _xom_chaos_upgrade, 20}},
    { XOM_BAD_TELEPORT, { "bad teleportation", _xom_bad_teleport, -1}},
    { XOM_BAD_POLYMORPH, { "bad polymorph", _xom_bad_polymorph, 30}},
    { XOM_BAD_MOVING_STAIRS, { "moving stairs", _xom_moving_stairs, 20}},
    { XOM_BAD_CLIMB_STAIRS, { "unclimbable stairs", _xom_unclimbable_stairs,
                              30}},
    { XOM_BAD_MUTATION, { "bad mutations", _xom_give_bad_mutations, 30}},
    { XOM_BAD_SUMMON_HOSTILES, { "summon hostiles", _xom_summon_hostiles, 35}},
    { XOM_BAD_STATLOSS, { "statloss", _xom_statloss, 23}},
    { XOM_BAD_DRAINING, { "draining", _xom_draining, 23}},
    { XOM_BAD_TORMENT, { "torment", _xom_torment, 23}},
    { XOM_BAD_CHAOS_CLOUD, { "chaos cloud", _xom_chaos_cloud, 20}},
    { XOM_BAD_BANISHMENT, { "banishment", _xom_banishment, 50}},
    { XOM_BAD_PSEUDO_BANISHMENT, {"psuedo-banishment", _xom_pseudo_banishment,
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
        } else if (event->badness_10x)
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

    FILE *ostat = fopen("xom_debug.stat", "w");
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
    fprintf(ostat, "%s\n", dump_overview_screen(false).c_str());
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
            if (old_mood != "")
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
            const xom_event_type result = xom_acts(sever, MB_MAYBE, tension,
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

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
#include "branch.h"
#include "cloud.h"
#include "colour.h"
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
#include "fineff.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-companions.h"
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
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "player-notices.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random-pick.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-zap.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#include "timed-effects.h"
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

static void _do_xom_event(xom_event_type event_type, int tension);
static bool _explore_event_is_valid(xom_event_type event, int tension);

static int _count_reachable_monsters();

static constexpr int XOM_EXPLORE_THRESHOLD = 910;

#define XOM_QUEUED_EXPLORE_KEY "xom_queued_explore"

enum xom_explore_type
{
    // This explore effect is triggered immediately at 0 tension.
    XOM_EXPLORE_IMMEDIATE,

    // This explore effect is triggered at 0 tension, only if the player is at
    // least 50% of their max HP
    XOM_EXPLORE_HEALTHY,

    // This explore effect is delayed until the next time tension raises above 0.
    XOM_EXPLORE_NEXT_BATTLE,
};

// Picks 100 random grids from the level and checks whether they've been
// marked as seen (explored) or known (mapped). If seen_only is true,
// grids only "seen" via magic mapping don't count. Returns the
// estimated percentage value of exploration.
static void _calculate_exploration_estimate(bool seen_only = false)
{
    int seen = 0, total = 0, tries = 0;

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
    while (total < 100 && tries < 500);

    // If we didn't get any qualifying grids, there are probably so few
    // of them you've already seen them all.
    if (total == 0)
        seen = 0;

    if (total < 100)
        seen *= 100 / total;

    you.explore_estimate = seen;
}

// Spells to be cast at tension > 0, i.e. usually in battle situations.
// Spells later in the list require higher severity to have a chance of being
// selected. Spells are sorted by level, then very roughly by power when
// neither planned for nor capability to repeatedly cast it.
static const vector<spell_type> _xom_random_spells =
{
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_GLOOM,
    SPELL_FUGUE_OF_THE_FALLEN,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_BATTLESPHERE,
    SPELL_AWAKEN_ARMOUR,
    SPELL_MARTYRS_KNELL,
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_FORGE_BLAZEHEART_GOLEM,
    SPELL_ANIMATE_DEAD,
    SPELL_INTOXICATE,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_WALKING_ALEMBIC,
    SPELL_SUMMON_CACTUS,
    SPELL_DISPERSAL,
    SPELL_DEATH_CHANNEL,
    SPELL_SUMMON_HYDRA,
    SPELL_SPHINX_SISTERS,
    SPELL_DIAMOND_SAWBLADES,
    SPELL_MALIGN_GATEWAY,
    SPELL_DISCORD,
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

const static int _tension_pips[] = { 0, 1, 5, 10, 20, 35, 50};
static int _xom_tension_breakpoints(int tension)
{
    return tension ==  0 ? 0 :
           tension  <  5 ? 1 :
           tension  < 10 ? 2 :
           tension  < 20 ? 3 :
           tension  < 35 ? 4 :
           tension  < 50 ? 5
                         : 6;
}

int xom_tension_pips()
{
    return _xom_tension_breakpoints(get_tension(GOD_XOM));
}

static int _chance_for_pip(int pip)
{
    return max(100, pip * 2950 / 6 - 250);
}

static int _action_chance_for_tension(int tension)
{
    const int pips = _xom_tension_breakpoints(tension);

    // If we've hit max displayed tension, or are exactly on a pip, use that
    // pip's exact action chance.
    if (tension == _tension_pips[pips] || pips >= 6)
        return _chance_for_pip(pips);
    // Otherwise, linearly interpolate with the chance from the next pip.
    else
    {
        const int diff = _tension_pips[pips + 1] - _tension_pips[pips];
        const int to_next = (tension - _tension_pips[pips]) * 100 / diff;
        const int lerp = ((_chance_for_pip(pips + 1) * to_next)
                          + (_chance_for_pip(pips) * (100 - to_next))) / 100;

        return lerp;
    }
}

const string describe_xom_favour()
{
    if (!you_worship(GOD_XOM))
        return "a very buggy toy of Xom.";
    return "a plaything of Xom.";
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

bool xom_is_nice(int tension)
{
    if (player_under_penance(GOD_XOM))
        return false;

    if (you_worship(GOD_XOM))
    {
        // At high tension Xom is more likely to be nice and at zero tension
        // it's a coin flip.
        const int tension_bonus
            = (tension == -1 ? 0 : random2(tension));

        const int effective_piety = HALF_MAX_PIETY + tension_bonus * 5 / 4;

#ifdef DEBUG_XOM
        mprf(MSGCH_DIAGNOSTICS,
             "Xom: tension: %d -> tension bonus = %d, eff. disposition: %d",
             tension, tension_bonus, effective_piety);
#endif

        return x_chance_in_y(effective_piety, MAX_PIETY);
    }
    else // CARD_XOM (XXX: There is no Xom card anymore. Is this needed?)
        return coinflip();
}

static void _xom_is_stimulated(int maxinterestingness,
                               const char *message_array[],
                               bool force_message)
{
    if (!you_worship(GOD_XOM) || maxinterestingness <= 0)
        return;

    // Xom is not directly stimulated by Their own acts.
    if (crawl_state.which_god_acting() == GOD_XOM)
        return;

    int interestingness = random2(piety_scale(maxinterestingness));

    interestingness = min(200, interestingness);

#if defined(DEBUG_RELIGION) || defined(DEBUG_GIFTS) || defined(DEBUG_XOM)
    mprf(MSGCH_DIAGNOSTICS,
         "Xom: maxinterestingness = %d, interestingness = %d",
         maxinterestingness, interestingness);
#endif

    if (!force_message
        && (interestingness < 10 || !x_chance_in_y(interestingness, 200)))
    {
        return;
    }

    god_speaks(GOD_XOM,
               ((interestingness > 160) ? message_array[5] :
                (interestingness >  80) ? message_array[4] :
                (interestingness >  60) ? message_array[3] :
                (interestingness >  40) ? message_array[2] :
                (interestingness >  20) ? message_array[1]
                                        : message_array[0]));
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
    // Redraw tension meter.
    you.redraw_title = true;
    if (x_chance_in_y(you.time_taken, 30))
    {
        const int tension = get_tension(GOD_XOM);

        if (tension == 0)
            xom_check_blessing();

        // If we have a queued explore effect, check if now is a good time and
        // possibly perform one of them.
        bool did_event = false;
        if (you.props.exists(XOM_QUEUED_EXPLORE_KEY))
        {
            CrawlVector& vec = you.props[XOM_QUEUED_EXPLORE_KEY].get_vector();
            for (int i = 0; i < vec.size(); ++i)
            {
                const xom_event_type event = static_cast<xom_event_type>(vec[i].get_int());
                if (_explore_event_is_valid(event, tension))
                {
                    xom_take_action(event, tension);
                    vec.erase(i);

                    did_event = true;
                    break;      // Only one event per tick.
                }
            }
        }

        // We can perform new explore events while waiting on something in the
        // queue, so long as we didn't do anything else this tick.
        if (!did_event && tension == 0 && you.attribute[ATTR_XOM_EXPLORE_COUNT] >= XOM_EXPLORE_THRESHOLD)
            xom_acts_exploration();

        int chance = _action_chance_for_tension(tension);
        if (you.faith())
            chance = chance * 3 / 2;
        if (x_chance_in_y(chance, 10000))
            xom_acts(maybe_bool::maybe, tension);
    }
}

static bool mon_nearby(function<bool(monster&)> filter)
{
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (filter(**mi))
            return true;
    return false;
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
static void _xom_random_spell(int tension)
{
    const spell_type spell = _choose_random_spell(you.experience_level / 2
                                                  + tension / 2);
    int power = you.experience_level * 4
                + get_tension() * 2 + you.runes.count() * 3;

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
    take_note(Note(NOTE_XOM_EFFECT, -1, tension, note), true);
}

// Map out the level, detect items across the level, and detect creatures.
static void _xom_divination(int)
{
    god_speaks(GOD_XOM, _get_xom_speech("divination").c_str());

    magic_mapping(100, 100, false);

    const int prev_detected = count_detected_mons();
    const int num_creatures = detect_creatures(50);
    const int num_items = detect_items(50);

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

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "divination: all"), true);
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

/// Xom's 'acquirement'. A gift for the player, of a sort...
static void _xom_acquirement(int)
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
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);

    stop_running();
    more();
}

static void _xom_give_item(object_class_type type, int subtype, int quantity = 0)
{
    god_speaks(GOD_XOM, _get_xom_speech("general gift").c_str());

    int thing_created = items(true, type, subtype,
                              random_range(0, 200), 0, GOD_XOM);

    if (thing_created == NON_ITEM)
    {
        god_speaks(GOD_XOM, "\"No, never mind.\"");
        return;
    }

    item_def &item(env.item[thing_created]);
    if (item.is_type(OBJ_ARMOUR, ARM_ORB))
        god_speaks(GOD_XOM, _get_xom_speech("orb gift").c_str());

    _try_brand_switch(thing_created);

    if (quantity > 0)
        item.quantity = quantity;

    static char gift_buf[100];
    snprintf(gift_buf, sizeof(gift_buf), "god gift: %s",
             item.name(DESC_PLAIN).c_str());

    // If the player is over terrain that would destroy the item, place an
    // altar here for it to land on.
    if ((env.grid(you.pos()) == DNGN_DEEP_WATER && !player_likes_water(true)
            || env.grid(you.pos()) == DNGN_LAVA))
    {
        env.grid(you.pos()) = DNGN_ALTAR_XOM;
        set_terrain_changed(you.pos());
    }

    canned_msg(MSG_SOMETHING_APPEARS);
    move_item_to_grid(&thing_created, you.pos());

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, gift_buf), true);

    stop_running();
    more();
}

/// Create a random item and give it to the player.
static void _xom_random_item(int)
{
    _xom_give_item(OBJ_RANDOM, OBJ_RANDOM);
}

static void _xom_joke_consumable(int)
{
    object_class_type type = OBJ_POTIONS;
    int subtype = POT_BERSERK_SAGE;
    const int quantity = 1 + coinflip() + one_chance_in(3);
    switch (random2(4))
    {
        case 0:
            break;

        case 1:
            subtype = POT_EXPEDIENCE;
            break;

        case 2:
            type = OBJ_SCROLLS;
            subtype = SCR_PLINKING;
            break;

        case 3:
            type = OBJ_SCROLLS;
            subtype = SCR_FROG;
            break;
    }

    identify_item_type(type, subtype);
    _xom_give_item(type, subtype, quantity);
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
    // chance to use the item if they can defeat the monster.
    if (item.summoned())
        return false;

    // Blessed weapons are protected, being gifts from good gods.
    if (is_blessed(item))
        return false;

    // God gifts are protected -- but not Their own!
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
    if (!mon.alive() || mon.attitude() != ATT_HOSTILE
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

    if (mon && you.can_see(*mon) && item.base_type == OBJ_WEAPONS)
    {
        const description_level_type desc = mon->friendly() ? DESC_YOUR
                                                            : DESC_THE;
        mprf("%s %s erupts in a glittering mayhem of colour.",
            apostrophise(mon->name(desc)).c_str(),
            item.name(DESC_PLAIN, false, false, false).c_str());
    }

    const int brand = (item.base_type == OBJ_WEAPONS) ? (int) SPWPN_CHAOS
                                                      : (int) SPMSL_CHAOS;

    if (is_random_artefact(item))
        artefact_set_property(item, ARTP_BRAND, brand);
    else
    {
        item.brand = brand;

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
static void _xom_bazaar_trip(int)
{
    if (brdepth[BRANCH_ABYSS] == -1)
    {
        // Only possible in wizmode, as the message implies, but it crashes.
        mprf(MSGCH_ERROR, "Not even Xom can make wizards leave Sprint. "
                          "Aborting Xom's Bazaar banishment.");
    }
    else
    {
        god_speaks(GOD_XOM, _get_xom_speech("bazaar trip").c_str());
        run_animation(ANIMATION_BANISH, UA_BRANCH_ENTRY, false);
        dlua.callfn("dgn_set_persistent_var", "sb", "xom_bazaar", true);
        down_stairs(DNGN_ENTER_BAZAAR);
        you.props[XOM_BAZAAR_TRIP_COUNT].get_int()++;
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "banished to a bazaar"),
                  true);
    }
}

static const vector<random_pick_entry<monster_type>> _xom_summons =
{
  { -4, 26,   5, FLAT, MONS_BUTTERFLY },
  { -4, -1,   5, FLAT, MONS_QUOKKA },
  { -3,  6,  60, SEMI, MONS_CRIMSON_IMP },
  { -1,  6,  40, SEMI, MONS_IRON_IMP },
  {  2,  7,  40, SEMI, MONS_SHADOW_IMP },
  {  3,  7,  40, SEMI, MONS_WHITE_IMP },
  {  4,  8,  50, SEMI, MONS_UFETUBUS },
  {  4,  8,  50, SEMI, MONS_DRUDE },
  {  4,  8,   5, SEMI, MONS_MUMMY },
  {  4, 10,  75, SEMI, MONS_PHANTOM },
  {  4, 11,  25, SEMI, MONS_WEEPING_SKULL },
  {  5,  9,  10, SEMI, MONS_BOMBARDIER_BEETLE },
  {  6, 12,  90, SEMI, MONS_SWAMP_DRAKE },
  {  7, 15,  60, SEMI, MONS_BOGGART },
  {  8, 14,  50, SEMI, MONS_ORANGE_DEMON },
  {  8, 15,  90, SEMI, MONS_CHAOS_SPAWN },
  {  9, 13,  40, SEMI, MONS_ICE_DEVIL },
  {  9, 13,  40, SEMI, MONS_RED_DEVIL },
  {  9, 14,   5, FLAT, MONS_TOENAIL_GOLEM },
  {  9, 13,  30, SEMI, MONS_VAMPIRE },
  {  9, 14,  95, SEMI, MONS_SHAPESHIFTER },
  {  9, 15,  50, SEMI, MONS_NEQOXEC },
  { 10, 14,  40, SEMI, MONS_HELL_RAT },
  { 10, 14,  30, SEMI, MONS_KOBOLD_DEMONOLOGIST },
  { 10, 15,  90, SEMI, MONS_ABOMINATION_SMALL },
  { 10, 15,  50, SEMI, MONS_UGLY_THING },
  { 10, 15,  60, SEMI, MONS_HELLWING },
  { 10, 16,  85, SEMI, MONS_RUST_DEVIL },
  { 11, 17,  50, SEMI, MONS_SMOKE_DEMON },
  { 11, 17,  30, SEMI, MONS_DREAM_SHEEP },
  { 11, 18,  50, SEMI, MONS_YNOXINUL },
  { 12, 16,  30, SEMI, MONS_OBSIDIAN_BAT },
  { 12, 17,  15, SEMI, MONS_DEMONSPAWN },
  { 12, 17,  30, SEMI, MONS_TARANTELLA },
  { 12, 18,  50, SEMI, MONS_SOUL_EATER },
  { 12, 20,  80, SEMI, MONS_WORLDBINDER },
  { 13, 18,  75, SEMI, MONS_SUN_DEMON },
  { 13, 18,  75, SEMI, MONS_SIXFIRHY },
  { 13, 21, 105, SEMI, MONS_LAUGHING_SKULL },
  { 14, 20,  15, SEMI, MONS_GREAT_ORB_OF_EYES },
  { 14, 20,  90, SEMI, MONS_ABOMINATION_LARGE },
  { 14, 21, 105, SEMI, MONS_GLOWING_SHAPESHIFTER },
  { 15, 23,   1, FLAT, MONS_OBSIDIAN_STATUE },
  { 16, 22,  10, SEMI, MONS_BUNYIP },
  { 16, 22,  50, SEMI, MONS_HELL_HOG },
  { 16, 23,  50, SEMI, MONS_RADROACH },
  { 16, 25,  75, SEMI, MONS_VERY_UGLY_THING },
  { 17, 24,  25, SEMI, MONS_SPHINX_MARAUDER },
  { 17, 24,  35, SEMI, MONS_GLOWING_ORANGE_BRAIN },
  { 17, 33,  35, SEMI, MONS_SHADOW_DEMON },
  { 17, 33,  65, SEMI, MONS_SIN_BEAST },
  { 17, 33,  15, SEMI, MONS_CACODEMON },
  { 17, 33,  15, SEMI, MONS_ZYKZYL },
  { 18, 25,  50, SEMI, MONS_GUARDIAN_SPHINX },
  { 18, 33,  35, SEMI, MONS_REAPER },
  { 18, 23,  30, SEMI, MONS_DANCING_WEAPON },
  { 19, 26,   1, FLAT, MONS_ORANGE_STATUE },
  { 20, 30,  30, SEMI, MONS_APOCALYPSE_CRAB },
  { 21, 33,  30, SEMI, MONS_TENTACLED_MONSTROSITY },
  { 22, 33,   1, FLAT, MONS_STARFLOWER },
  { 23, 33,  30, SEMI, MONS_HELLEPHANT },
  { 24, 30,   5, SEMI, MONS_MOTH_OF_WRATH },
  { 25, 36,   5, SEMI, MONS_NEKOMATA },
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
             mtype == MONS_HELL_HOG || mtype == MONS_VERY_UGLY_THING ||
             mtype == MONS_SIN_BEAST)
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

    if (you.runes.count() > ZOT_ENTRY_RUNES + 1)
        count += div_rand_round(you.runes.count(), ZOT_ENTRY_RUNES + 2);

    if (!isFriendly && you.penance[GOD_XOM])
        count *= 1.5;

    return count;
}

// Endgame and extended monsters Xom has some fondness for (in being jokes
// or highly chaotic), but which are mostly meant to stay in their branches,
// get a small extra chance to be picked in the appropriate branch or Zigs.
// Makes it a little more dramatic. Guardian picks can access this pool in
// general at XL 27, but with fairly low odds without 4+ runes.
static monster_type _xom_chaos_branch_pal(monster_type mon_type, bool guard)
{
    int rune_roll = 1;
    if (you.runes.count() > ZOT_ENTRY_RUNES)
        rune_roll = div_rand_round(you.runes.count(), ZOT_ENTRY_RUNES);

    if ((player_in_branch(BRANCH_ZOT) || player_in_branch(BRANCH_ZIGGURAT)
         && one_chance_in(13)) || guard && x_chance_in_y(rune_roll, 27))
    {
        mon_type = random_choose_weighted(5, MONS_DEATH_COB,
                                          4, MONS_KILLER_KLOWN,
                                          3, MONS_PROTEAN_PROGENITOR,
                                          2, MONS_CURSE_TOE);
    }
    else if (((player_in_branch(BRANCH_PANDEMONIUM)
              || player_in_branch(BRANCH_ZIGGURAT)) && one_chance_in(13))
              || guard && x_chance_in_y(rune_roll, 54))
    {
        mon_type = x_chance_in_y(3, 5) ? MONS_DEMONSPAWN_BLOOD_SAINT :
                                         MONS_DEMONSPAWN_CORRUPTER;
    }
    else if (player_in_branch(BRANCH_ZIGGURAT) && one_chance_in(4) ||
            guard && x_chance_in_y(rune_roll, 81))
    {
        mon_type = x_chance_in_y(5, 7) ? MONS_PANDEMONIUM_LORD :
                                         MONS_PLAYER_GHOST;
    }

    return mon_type;
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
    else if (you.penance[GOD_XOM])
        variance += random_range(4, 6);

    variance = min(33, variance);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "_xom_random_pal(); xl variance roll: %d", roll);
#endif

    monster_type mon_type = xom_picker.pick(_xom_summons, variance, MONS_CRIMSON_IMP);

    if (player_in_branch(BRANCH_ZOT)
        || player_in_branch(BRANCH_PANDEMONIUM)
        || player_in_branch(BRANCH_ZIGGURAT))
    {
        mon_type = _xom_chaos_branch_pal(mon_type, false);
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

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, potion_msg), true);
}


/// Feed the player a notionally-good potion effect.
static void _xom_do_potion(int)
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
                                     5,  POT_BERSERK_RAGE);
    }
    while (!get_potion_effect(pot)->can_quaff()); // ugh

    god_speaks(GOD_XOM, _get_xom_speech("potion effect").c_str());

    _note_potion_effect(pot);

    // Many potions get bonus power.
    // Curing, heal wounds, magic and berserk rage ignore power.
    get_potion_effect(pot)->effect(true, 150);
}

static void _confuse_monster(monster* mons, int dur = 50)
{
    if (mons->clarity())
        return;
    if (mons->is_peripheral())
        return;

    const bool was_confused = mons->confused();
    if (mons->add_ench(mon_enchant(ENCH_CONFUSION,
          &env.mons[ANON_FRIENDLY_MONSTER], random2(dur) * 10)))
    {
        if (was_confused)
            simple_monster_message(*mons, " looks rather more confused.");
        else
            simple_monster_message(*mons, " looks rather confused.");
    }
}

static void _xom_confuse_monsters(int)
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

        _confuse_monster(*mi);
    }

    if (spoke)
    {
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "confuse monster(s)"),
                  true);
    }
}

/// Post a passel of pals to the player.
static void _xom_send_allies(int)
{
    int strengthRoll = random2(600);
    int count = _xom_pal_counting(strengthRoll, true);
    int num_actually_summoned = 0;

    for (int i = 0; i < count; ++i)
    {
        monster_type mon_type = _xom_random_pal(strengthRoll, true);

        mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH, GOD_XOM);
        mg.set_summoned(&you, MON_SUMM_AID, summ_dur(3));

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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    }
}

/// Send a single pal to the player's aid, hopefully.
static void _xom_send_one_ally(int)
{
    int strengthRoll = random2(600);
    const monster_type mon_type = _xom_random_pal(strengthRoll, true);

    mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH, GOD_XOM);
    mg.set_summoned(&you, MON_SUMM_AID, summ_dur(6));

    mg.non_actor_summoner = "Xom";

    if (monster *summons = create_monster(mg))
    {
        // Add a little extra length and regen. Make friends with your new pal.
        int extra = random_range(100, 200);
        summons->add_ench(mon_enchant(ENCH_SUMMON_TIMER, nullptr, extra));
        summons->add_ench(mon_enchant(ENCH_REGENERATION, nullptr, 2000));
        god_speaks(GOD_XOM, _get_xom_speech("single summon").c_str());

        if (summons->type == MONS_REAPER)
            _do_chaos_upgrade(*summons->weapon(), summons);

        const string note = make_stringf("summons friendly %s",
                                         summons->name(DESC_PLAIN).c_str());
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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

    const bool powerup = !(mons.wont_attack() ^ helpful);

    if (powerup)
    {
        if (you.experience_level > 4 && !helpful)
            mons.polymorph(PPT_MORE);
        else
            mons.polymorph(PPT_SAME);

        if (you.experience_level < 10 && !helpful)
            mons.malmutate(nullptr);
    }
    else
        mons.polymorph(PPT_LESS);

    // If we do this before the polymorph, it can sometimes make the polymorph
    // itself fail (by trying to turn the monster into a different tier of
    // shapeshifter.
    if (one_chance_in(8)
        && !mons_is_ghost_demon(mons.type)
        && !mons.is_shapeshifter()
        && mons.holiness() & MH_NATURAL
        && (you.experience_level > 4 || you.penance[GOD_XOM]))
    {
        mons.add_ench(one_chance_in(3) ? ENCH_GLOWING_SHAPESHIFTER
                                       : ENCH_SHAPESHIFTER);
    }

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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    }
}

/// Find a monster to poly.
static monster* _xom_mons_poly_target()
{
    vector<monster*> polymorphable;

    // XXX: Polymorphing early bats over liquids turn into D:1 acid dragons
    //      and killer bees, so skip them while polymorphing at low xls.
    //      Likewise, there's very few plant polymorph options.
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->can_polymorph() && !mi->is_firewood()
           && ((env.grid(mi->pos()) != DNGN_DEEP_WATER)
               && (env.grid(mi->pos()) != DNGN_LAVA) || you.experience_level > 4)
           && (!(mi->holiness() & MH_PLANT) || you.experience_level > 8))
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
static void _xom_good_polymorph(int)
{
    _xom_polymorph_nearby_monster(true);
}

/// Try to polymporph a nearby monster into something stronger.
static void _xom_bad_polymorph(int)
{
    _xom_polymorph_nearby_monster(false);
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

// Swap places with a random monster and sometimes also shuffle other monsters.
// This can be pretty bad if there are a lot of hostile monsters around.
static void _xom_rearrange_pieces(int)
{
    vector<monster*> mons = _rearrangeable_pieces();
    if (mons.empty())
        return;

    god_speaks(GOD_XOM, _get_xom_speech("rearrange the pieces").c_str());

    const int num_mons = mons.size();

    // Swap places with a random monster.
    monster* mon = mons[random2(num_mons)];
    transpose_with_monster(mon);

    // Sometimes confuse said monster.
    if (coinflip())
        _confuse_monster(mon);

    if (num_mons > 1 && x_chance_in_y(4, 7))
    {
        bool did_message = false;
        const int max_repeats = min(num_mons / 2, 8);
        const int repeats     = min(random2(4) + 1, max_repeats);
        for (int i = 0; i < repeats; ++i)
        {
            const int mon1 = random2(num_mons);
            int mon2 = mon1;
            while (mon1 == mon2)
                mon2 = random2(num_mons);

            if (mons[mon1]->swap_with(mons[mon2], MV_TRANSLOCATION))
            {
                if (!did_message)
                {
                    mpr("Some monsters swap places.");
                    did_message = true;
                }
                if (one_chance_in(4))
                    _confuse_monster(mons[mon1]);
                if (one_chance_in(4))
                    _confuse_monster(mons[mon2]);
            }
        }
    }
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "swap monsters"), true);
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
    return mon.base_attitude == ATT_HOSTILE
            && mons_genus(mon.type) == MONS_SNAKE;
}

// An effect similar to old sticks to snakes (which worked on "sticks" other
// than arrows)
//  * Transformations are permanent.
//  * Weapons are always non-cursed.
//  * HD influences the enchantment and type of the weapon.
//  * Weapon is not guaranteed to be useful.
//  * Weapon will never be branded.
static void _xom_snakes_to_sticks(int)
{
    bool action = false;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!_hostile_snake(**mi))
            continue;

        if (!action)
        {
            take_note(Note(NOTE_XOM_EFFECT, -1, -1,
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
        monster_die(**mi, KILL_RESET, NON_MONSTER, true);
    }
}

// Xom counts up webs to light on fire.
static vector<coord_def> _xom_counts_webs()
{
    vector<coord_def> webs;

    for (vision_iterator ri(you); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_TRAP_WEB)
            webs.push_back(*ri);
    }

    return webs;
}

// Xom burns down all webs in sight, replacing them with fire clouds.
// All other sorts of clouds are removed, and anybody in a web is freed.
static void _xom_lights_up_webs(int)
{
    int webs_count = 0;
    int blaze_time = 3 + random2(4) * 3;

    vector<coord_def> candidates = _xom_counts_webs();

    for (coord_def pos : candidates)
    {
        flash_tile(pos, RED, 0);
        animation_delay(20, true);

        if (cloud_at(pos))
            delete_cloud(pos);

        place_cloud(CLOUD_FIRE, pos, blaze_time, nullptr, 0);

        webs_count++;
        dungeon_terrain_changed(pos, DNGN_FLOOR);
        update_terrain_knowledge(pos);

        if (actor* act = actor_at(pos))
            if (act->caught_by() == CAUGHT_WEB)
                act->stop_being_caught();
    }

    view_clear_overlays();

    if (webs_count > 0)
    {
        god_speaks(GOD_XOM, _get_xom_speech("lights up webs").c_str());
        mprf("%s %s into flame!", number_in_words(webs_count).c_str(),
              webs_count == 1 ? "web bursts" : "webs burst");
        string note = make_stringf("lit up %d webs", webs_count);
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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
            && !weapon.summoned()
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

static void _xom_animate_monster_weapon(int)
{
    // Pick a random monster...
    monster* mon = _find_monster_with_animateable_weapon();
    if (!mon)
        return;

    god_speaks(GOD_XOM, _get_xom_speech("animate monster weapon").c_str());

    // ...and get its weapon.
    const int wpn = mon->inv[MSLOT_WEAPON];
    ASSERT(wpn != NON_ITEM);

    const int dur = random_range(30, 100);

    mgen_data mg(MONS_DANCING_WEAPON, BEH_FRIENDLY, mon->pos(), mon->mindex(),
                 MG_NONE, GOD_XOM);
    mg.set_summoned(&you, SPELL_TUKIMAS_DANCE, dur);

    mg.non_actor_summoner = "Xom";

    monster *dancing = create_monster(mg);

    if (!dancing)
        return;

    // Make the monster unwield its weapon.
    mon->unequip(MSLOT_WEAPON);

    mprf("%s %s dances into the air!",
         apostrophise(mon->name(DESC_THE)).c_str(),
         env.item[wpn].name(DESC_PLAIN).c_str());

    destroy_item(dancing->inv[MSLOT_WEAPON]);

    dancing->inv[MSLOT_WEAPON] = wpn;
    env.item[wpn].set_holding_monster(*dancing);
    dancing->colour = env.item[wpn].get_colour();
}

// Have Xom make a big ring of temporary harmless plantlife around the player.
static void _xom_harmless_flora(int)
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

        if (!actor_at(*ri) && monster_habitable_grid(MONS_PLANT, *ri))
        {
            mgen_data mg(mon_type, BEH_HOSTILE, *ri, MHITYOU,
                         MG_FORCE_BEH | MG_FORCE_PLACE, GOD_XOM);
            mg.set_summoned(&you, MON_SUMM_AID, summ_dur(5));

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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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
                                          2, empty))
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
                if (actor_at(*ri) || !monster_habitable_grid(moving_mons, *ri)
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
                    if (actor_at(*ri) || !monster_habitable_grid(moving_mons, *ri)
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
// and move enemies in or out according to whether the effect is good or bad.
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
        int inner_cap = you.penance[GOD_XOM] ? 8 : 2 + random_range(1, soft_cap);

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

        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _xom_good_door_ring(int)
{
    _xom_door_ring(true);
}

static void _xom_bad_door_ring(int)
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
    { DNGN_ZOT_STATUE,        5 },
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
static void _xom_fake_shatter(int tension)
{
    // Don't dig through hard walls out of combat, to prevent the possibility
    // of players waiting around for it to happen, in order to dig into a vault.
    bool more_than_dig = one_chance_in(5) && tension > 0;

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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "fake shatter"), true);

}

// Scan any bad mutations the player has and remove a bad one at random, if one
// is found.
// Returns whether a mutation was removed.
static bool _maybe_remove_badmut()
{
    vector<mutation_type> badmuts;
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        const mutation_type mut = (mutation_type)i;
        if (is_bad_mutation(mut) && you.get_base_mutation_level(mut, false, false, true))
            badmuts.push_back(mut);
    }

    if (badmuts.empty())
        return false;

    return delete_mutation(badmuts[random2(badmuts.size())], "Xom's grace", false, false, true);
}

static void _xom_give_mutations(bool good)
{
    if (!you.can_safely_mutate())
        return;

    god_speaks(GOD_XOM, good ? _get_xom_speech("good mutations").c_str()
                             : _get_xom_speech("random mutations").c_str());

    int num_tries = random2(4) + 1;

    const string note = make_stringf("give %smutation%s",
#ifdef NOTE_DEBUG_XOM
             good ? "good " : "random ",
#else
             "",
#endif
             num_tries > 1 ? "s" : "");

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    mpr("Your body is suffused with distortional energy.");

    if (good && one_chance_in(4))
        if (_maybe_remove_badmut())
            num_tries -= 1;

    bool failMsg = true;

    for (int i = num_tries; i > 0; --i)
    {
        // One bad mutation guaranteed when under Xom wrath.
        if (i == num_tries && !good && you.penance[GOD_XOM])
        {
            if (!mutate(RANDOM_BAD_MUTATION, "Xom's mischief",
                        failMsg, false, true, false, MUTCLASS_NORMAL))
            {
                failMsg = false;
            }
        }
        // RANDOM_XOM_MUTATION is a flat coinflip for good or bad,
        // as opposed to RANDOM_MUTATION being 60-40 good or bad.
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
    beam.tile_explode = TILE_BOLT_ELECTRIC_BLAST;
    beam.thrower      = KILL_NON_ACTOR;
    beam.source_id    = MID_NOBODY;
    beam.aux_source   = "Xom's lightning strike";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    beam.explode(true, true);
}

static void _xom_spray_lightning(coord_def position)
{
    bolt beam;

    beam.source       = you.pos();
    beam.target       = position;
    beam.target.x     += random_range(-1, 1);
    beam.target.y     += random_range(-1, 1);
    while (beam.target == you.pos())
    {
        beam.target.x     += random_range(-1, 1);
        beam.target.y     += random_range(-1, 1);
    }
    beam.tile_beam    = TILE_BOLT_STRONG_ELEC;
    beam.thrower      = KILL_NON_ACTOR;
    beam.source_id    = MID_NOBODY;
    beam.aux_source   = "Xom's lightning strike";

    int power = 20 + you.experience_level * 5;
    if (you.runes.count() > ZOT_ENTRY_RUNES + 1)
        power += you.runes.count() * 3;

    // uncontrolled, so no player tracer.
    zappy(ZAP_LIGHTNING_BOLT, power, true, beam);
    beam.fire();
}

// Have Xom throw down divine lightning in a 5x5 explosion, then fire random
// lightning bolts in random directions potentially fudged towards, like
// old black draconian breath.
static void _xom_throw_divine_lightning(int tension)
{
    god_speaks(GOD_XOM, _get_xom_speech("divine lightning").c_str());

    _xom_drop_lightning();

    int spray_count = random_range(4, max(5, (min(27, tension / 5))));
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
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1,
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
                     MG_FORCE_BEH, GOD_XOM);
        mg.set_summoned(&you, MON_SUMM_AID, summ_dur(3));
        if (create_monster(mg))
            success = true;
    }

    if (success)
    {
        take_note(Note(NOTE_XOM_EFFECT, -1, -1,
                       "scenery: summon butterflies"), true);
        god_speaks(GOD_XOM, _get_xom_speech("scenery").c_str());
    }
}

static void _change_scenery_square(coord_def pos, dungeon_feature_type type)
{
    dungeon_terrain_changed(pos, type, false, true, false, true);
}

// Mess with nearby terrain features, mostly harmlessly.
static void _xom_change_scenery(int)
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

            _change_scenery_square(pos, DNGN_FOUNTAIN_BLOOD);
            if (you.see_cell(pos))
                fountains_blood++;
            break;
        case DNGN_CACHE_OF_FRUIT:
        case DNGN_CACHE_OF_MEAT:
        case DNGN_CACHE_OF_BAKED_GOODS:
            if (x_chance_in_y(food_swapped, 3))
                continue;

            if (env.grid(pos) == DNGN_CACHE_OF_FRUIT)
                _change_scenery_square(pos, DNGN_CACHE_OF_MEAT);
            else if (env.grid(pos) == DNGN_CACHE_OF_MEAT)
                _change_scenery_square(pos, DNGN_CACHE_OF_BAKED_GOODS);
            else
                _change_scenery_square(pos, DNGN_CACHE_OF_FRUIT);
            if (you.see_cell(pos))
                food_swapped++;
            break;
        case DNGN_TREE:
        case DNGN_MANGROVE:
            if (x_chance_in_y(trees_polymorphed, 3))
                continue;

            _change_scenery_square(pos, wtree);
            if (you.see_cell(pos))
                trees_polymorphed++;
            break;
        case DNGN_DEMONIC_TREE:
        case DNGN_PETRIFIED_TREE:
            if (x_chance_in_y(trees_polymorphed, 3))
                continue;

            _change_scenery_square(pos, btree);
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, ("scenery: "
            + comma_separated_line(terse.begin(), terse.end(), ", ", ", ")).c_str()),
            true);
        mprf("%s!",
             comma_separated_line(effects.begin(), effects.end(),
                                  ", and ").c_str());
    }
}

static void _xom_open_and_close_doors(int)
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
            if (you.see_cell(pos))
                doors_open++;
            break;
        case DNGN_OPEN_DOOR:
        case DNGN_OPEN_CLEAR_DOOR:
            dgn_close_door(pos);
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, ("scenery: "
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
static void _xom_destruction(bool real)
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
                || mi->wont_attack() && !you.penance[GOD_XOM]))
        {
            continue;
        }

        if (!real)
        {
            if (!rc)
                god_speaks(GOD_XOM, _get_xom_speech("fake destruction").c_str());
            rc = true;
            corona_monster(*mi, &you);
            continue;
        }

        int dice = 2 + div_rand_round(you.experience_level, 13);
        if (you.runes.count() > ZOT_ENTRY_RUNES + 1)
            dice += div_rand_round(you.runes.count(), ZOT_ENTRY_RUNES);

        bolt beam;

        beam.flavour      = BEAM_FIRE;
        beam.origin_spell = SPELL_STICKY_FLAME;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(dice, 9);
        beam.target       = mi->pos();
        beam.name         = "sticky fireball";
        beam.colour       = RED;
        beam.thrower      = KILL_NON_ACTOR;
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1,
                       real ? "destruction" : "fake destruction"), true);
    }
}

static void _xom_real_destruction(int) { _xom_destruction(true); }
static void _xom_fake_destruction(int) { _xom_destruction(false); }

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
static void _xom_force_lances(int)
{
    int xl = you.penance[GOD_XOM] ? you.experience_level / 3
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
                     MHITYOU, MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE, GOD_XOM);

        mg.set_summoned(&you, MON_SUMM_AID, summ_dur(2));
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _xom_enchant_monster(bool helpful)
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
        time = random_range(30, 40);
    else if (ench == ENCH_VITRIFIED || ench == ENCH_SLOW)
        time = random_range(200, 300);
    else if (ench == ENCH_HASTE || ench == ENCH_MIGHT)
        time = random_range(400, 700);
    else if (ench == ENCH_REGENERATION || ench == ENCH_RESISTANCE)
        time = random_range(800, 1100);

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

        application->add_ench(mon_enchant(ench, &you, time));
        affected++;
    }

    // Take a note.
    const string note = make_stringf("enchant monster%s (%s, %s)",
                                     affected >= 1 ? "s" : "",
                                     helpful ? "good" : "bad",
                                     ench_name.c_str());
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

static void _xom_good_enchant_monster(int)
{
    _xom_enchant_monster(true);
}
static void _xom_bad_enchant_monster(int)
{
    _xom_enchant_monster(false);
}

// Look for monsters sufficiently weak enough for Xom to buff.
static vector<monster*> _xom_find_weak_monsters(bool range)
{
    vector<monster*> targetable;
    int runes = (you.runes.count() > ZOT_ENTRY_RUNES) ?
                    div_rand_round(you.runes.count(), ZOT_ENTRY_RUNES) : 0;
    int maximum_hd = 3 + you.experience_level * 7 / 20 + runes;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        // No counting battlespheres or orbs of destruction. Try not to buff
        // the same target multiple times by checking the most prominent ones.
        // Fuzz the HD range to make it harder to deliberately plan around.
        if (mi->attitude() == ATT_FRIENDLY
            && !mi->is_peripheral()
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
static void _xom_hyper_enchant_monster(int)
{
    vector<enchant_type> buff_list { ENCH_MIGHT, ENCH_SWIFT, ENCH_HASTE,
                                     ENCH_INVIS, ENCH_EMPOWERED_SPELLS,
                                     ENCH_DEFLECT_MISSILES, ENCH_RESISTANCE,
                                     ENCH_REGENERATION, ENCH_TOXIC_RADIANCE,
                                     ENCH_CHAOS_LACE, ENCH_WARDING,
                                     ENCH_DOUBLED_VIGOUR, ENCH_MIRROR_DAMAGE,
                                     ENCH_STRONG_WILLED, ENCH_PARADOX_TOUCHED };
    vector<monster*> targetable = _xom_find_weak_monsters(true);
    int time = random_range(200, 350);
    int xl = you.experience_level;
    int buff_count = 0;

    if (targetable.empty())
    {
        monster_type mon_type;
        bool mundane = is_good_god(you.religion) || one_chance_in(4);

        // Mostly more mundane choices than usual Xom summons.
        if (xl < 7)
            mon_type = mundane ? MONS_IGUANA : MONS_CERULEAN_IMP;
        else if (xl < 14 + random_range(-1, 1))
            mon_type = mundane ? MONS_BLACK_BEAR : MONS_HELL_RAT;
        else if (xl < 21 + random_range(-1, 1))
            mon_type = mundane ? MONS_WYVERN : MONS_HELL_HOUND;
        else
            mon_type = mundane ? MONS_ELEPHANT : MONS_TOENAIL_GOLEM;

        mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(),
                    MHITYOU, MG_FORCE_BEH | MG_FORCE_PLACE, GOD_XOM);
        mg.set_summoned(&you, MON_SUMM_AID, summ_dur(3));
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

        if (you.penance[GOD_XOM])
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

            if (apply == ENCH_DEFLECT_MISSILES || apply == ENCH_REGENERATION
                || apply == ENCH_TOXIC_RADIANCE || apply == ENCH_MIRROR_DAMAGE
                || apply == ENCH_SWIFT)
            {
                lines += make_stringf("starts %s, ", ench_name.c_str());
            }
            else if (apply == ENCH_DOUBLED_VIGOUR || apply == ENCH_CHAOS_LACE
                || apply == ENCH_WARDING || apply == ENCH_PARADOX_TOUCHED)
            {
                lines += make_stringf("becomes %s, ", ench_name.c_str());
            }
            else if (apply == ENCH_EMPOWERED_SPELLS)
                lines += make_stringf("has its spells empowered, ");
            else
                lines += make_stringf("looks %s, ", ench_name.c_str());

            targetable[0]->add_ench(mon_enchant(apply, nullptr, time));
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
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
    }
}

static void _xom_mass_charm(int)
{
    vector<monster*> targetable;
    int affected = 0;
    int iters = 0;
    int hd_target = 0;
    int target_count = 0;
    int time = random_range(500, 600);

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

    hd_target /= max(1, target_count);
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
            application->add_ench(mon_enchant(ENCH_CHARM, &you, time));
            affected++;
        }

        iters++;
    }

    const string note = make_stringf("charmed %d monster%s",
                                     affected,  affected != 1 ? "s" : "");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Xom makes some scary skeletons pop out.
// This halves enemy willpower, drains them, and fears them.
static void _xom_wave_of_despair(int)
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

            item_def* corpse = place_corpse_or_gold(dummy, true);
            if (corpse)
            {
                turn_corpse_into_skeleton(*corpse);
                skeleton_count++;
            }
        }
    }

    if (skeleton_count)
        mpr("Skeletons, inanimate yet cursed, drop down from the ceiling.");

    draw_ring_animation(you.pos(), you.current_vision, DARKGRAY, MAGENTA, true, 35);
    mprf(MSGCH_DANGER, "A draining tide of despair and horror washes over you and your surroundings!");

    const int pow = random_range(60, 100);

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            mon->strip_willpower(&you, pow, true);

            if (mon->holiness() & (MH_NATURAL | MH_PLANT))
                mon->add_ench(mon_enchant(ENCH_DRAINED, &you, pow, 2));

            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.strip_willpower(&you, pow, true);
    mass_enchantment(ENCH_FEAR, pow * 5);

    const string note = make_stringf("spooky wave of despair");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Xom shifts both sight and space, inflicting both blind and blinkitis on
// every nearby enemy, so they're forced away from the player each turn and
// are much less accurate at hitting the player from range. Also produces a
// bunch of clouds from both blinking and one immediately beneath the player.
static void _xom_blinding_blinkitis(int)
{
    vector<monster*> target;
    const int dur = random_range(70, 130);
    god_speaks(GOD_XOM, _get_xom_speech("shifts sight and space").c_str());

    draw_ring_animation(you.pos(), you.current_vision, BLUE, MAGENTA,
                        true, 25, TILE_BOLT_CORRUPTION);

    // Gather the list separately from the action for the sake of messaging.
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (!mi->wont_attack() && !mi->is_peripheral() && !mi->stasis())
            target.push_back(*mi);

    mprf(MSGCH_MONSTER_ENCHANT, "%s %s blinded and untethered in space!",
         multimonster_name_string(target).c_str(), target.size() > 1 ? "are" : "is");

    for (monster *mon : target)
    {
        // Animate both where they were and where they land.
        // As with darts of disjunction, give a free blink immediately.
        flash_tile(mon->pos(), LIGHTMAGENTA, 45,
                   coinflip() ? TILE_BOLT_MYSTIC_BLAST : TILE_BOLT_WARP_SPACE);
        mon->add_ench(mon_enchant(ENCH_BLIND, &you, dur));
        mon->add_ench(mon_enchant(ENCH_BLINKITIS, &you, dur));
        blink_away(mon, &you, false, false, 3);
        mon->hurt(NULL, roll_dice(2, 2));

        if (you.see_cell(mon->pos()))
        {
            flash_tile(mon->pos(), LIGHTMAGENTA, 45,
                       coinflip() ? TILE_BOLT_MYSTIC_BLAST : TILE_BOLT_WARP_SPACE);
        }
    }

    place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 12, nullptr, 0);
    draw_ring_animation(you.pos(), you.current_vision, BLUE, MAGENTA,
                        false, 50, TILE_BOLT_CORRUPTION);

    const string note = make_stringf("warped sight and space for %d monster%s",
                                     (int) target.size(),
                                     target.size() > 1 ? "s" : "");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Another short reusable logic check list for valid Vex targets.
static bool _valid_vex_target(monster* mon)
{
    return mon->alive() && !mon->wont_attack() && !mon->is_peripheral()
           && !mon->clarity() && !mons_invuln_will(*mon) && !mon->helpless();
}

// Xom hands out both chaos lace and vex buffs to all enemies, with at least
// one next to another enemy. Unlikely to be a direct full kill for its short
// duration (monster versus monster damage isn't that high), but can make you
// back off or maybe help you leave.
static void _xom_chaos_vex(int)
{
    vector<monster*> target;
    const int dur = random_range(40, 80);
    god_speaks(GOD_XOM, _get_xom_speech("chaos vex").c_str());

    // Flash on non-occupied tiles first.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
        if (!monster_at(*ri) && one_chance_in(3))
            flash_tile(*ri, BLUE, 1, TILE_BOLT_CHAOS_BUFF);

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!_valid_vex_target(*mi))
            continue;

        if (mi->asleep())
            behaviour_event(*mi, ME_DISTURB, 0, mi->pos());

        flash_tile(mi->pos(), BLUE, 40, TILE_BOLT_CHAOS_BUFF);
        mi->add_ench(mon_enchant(ENCH_CHAOS_LACE, &you, dur));
        mi->add_ench(mon_enchant(ENCH_VEXED, &you, dur));
        target.push_back(*mi);
    }

    mprf(MSGCH_MONSTER_ENCHANT, "%s %s interlaced with infuriating chaos!",
         multimonster_name_string(target).c_str(),
         target.size() > 1 ? "are" : "is");

    const string note = make_stringf("chaos laced + vexed %d monster%s",
                                     (int) target.size(),
                                     target.size() > 1 ? "s" : "");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Xom hastes, slows, or paralyzes the player and everything else in sight
// for the same length of time (give or take time slices and base speed).
// Mostly screws with whatever else might join the fight afterwards,
// ally creation, and also with damage-over-time effects.
static void _xom_time_control(int)
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
            message = "Your stasis prevents you from being hasted,"
                      " but everything else in sight speeds up!";
            note = "hasted everything in sight";
        }
        else
        {
            message = "You and everything else in sight speed up!";
            note = "hasted player and everything else in sight";
        }
        time = random_range(100, 200);
    }
    else if (coinflip())
    {
        dur = DUR_SLOW;
        ench = ENCH_SLOW;
        xomline = "slow motion";
        if (you.stasis())
        {
            message = "Your stasis prevents you from being slowed,"
                      " but everything else in sight slows down!";
            note = "slowed everything in sight";
            bad = false;
        }
        else
        {
            message = "You and everything else in sight slow down!";
            note = "slowed player and everything else";
        }
        time = random_range(100, 200);
    }
    else
    {
        dur = DUR_PARALYSIS;
        ench = ENCH_PARALYSIS;
        xomline = "pause";
        if (you.stasis())
        {
            message = "Your stasis prevents you from being paralysed,"
                      " but everything else in sight stops moving!";
            note = "paralysed everything in sight";
            bad = false;
        }
        else
        {
            message = "You and everything else in sight suddenly stop moving!";
            note = "paralysed player and everything else";

            // Less of a decent joke if it directly kills, so.
            const auto pos = you.pos();
            if (cloud_at(pos) && !is_harmless_cloud(cloud_at(pos)->type))
                delete_cloud(pos);
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

        mi->add_ench(mon_enchant(ench, &you, time));
    }

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

/// Toss some fog around the player. Helping...?
static void _xom_fog(int /*sever*/)
{
    big_cloud(CLOUD_RANDOM_SMOKE, &you, you.pos(), 50, 8 + random2(8));
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "fog"), true);
    god_speaks(GOD_XOM, _get_xom_speech("cloud").c_str());
}

static item_def* _xom_get_random_worn_slot_item(equipment_slot item_slot)
{
    vector<item_def*> worn_slot_item = you.equipment.get_slot_items(item_slot);

    if (worn_slot_item.empty())
        return nullptr;

    return worn_slot_item[random2(worn_slot_item.size())];
}

static void _xom_pseudo_miscast(int)
{
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "silly message"), true);
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
        else if (in_view[iv] == DNGN_FOUNTAIN_EYES)
            str = _get_xom_speech("feature eyes");
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

            // For name-related bits in graffiti.
            str = do_mon_name_replacements(str);

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
        && you_can_wear(SLOT_BODY_ARMOUR, true) != false)
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

    if (you_can_wear(SLOT_WEAPON, true) != false && !you.weapon())
    {
        const bool one_handed = you.equipment.get_first_slot_item(SLOT_OFFHAND)
                                || you.get_mutation_level(MUT_MISSING_HAND);
        string str = _get_xom_speech(one_handed ? "unarmed one hand"
                                                : "unarmed two hands");

        str = replace_all(str, "@hand@", you.hand_name(false));
        str = replace_all(str, "@hands@", you.hand_name(true));

        messages.push_back(str);
    }

    if (item_def* item = you.equipment.get_first_slot_item(SLOT_OFFHAND))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("offhand slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_slot_item(SLOT_CLOAK))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("cloak slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        // XXX: The formless mutation doesn't technically mean you don't have a
        // form; it means you don't have a head.
        str = replace_all(str, "@head@",
                          you.has_mutation(MUT_FORMLESS) ? "form" : "head");

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_slot_item(SLOT_HELMET))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("helmet slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        // XXX: The formless mutation doesn't technically mean you don't have a
        // form; it means you don't have a head.
        str = replace_all(str, "@head@",
                          you.has_mutation(MUT_FORMLESS) ? "form" : "head");

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_slot_item(SLOT_GLOVES))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("gloves slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_slot_item(SLOT_LOWER_BODY))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("boots slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = you.equipment.get_first_slot_item(SLOT_AMULET))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("amulet slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        // XXX: The formless mutation doesn't technically mean you don't have a
        // form; it means you don't have a head.
        str = replace_all(str, "@head@",
                          you.has_mutation(MUT_FORMLESS) ? "form" : "head");

        messages.push_back(str);
    }

    if (item_def* item = you.equipment.get_first_slot_item(SLOT_GIZMO))
    {
        string name = "your " + item->name(DESC_BASENAME, false, false, false);
        string str = _get_xom_speech("gizmo slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        messages.push_back(str);
    }

    if (item_def* item = _xom_get_random_worn_slot_item(SLOT_RING))
    {
        // Don't just say "your ring" here. We want to know which one.
        string name = "your " + item->name(DESC_QUALNAME, false, false, false);
        string str = _get_xom_speech("ring slot");

        str = replace_all(str, "@your_item@", name);
        str = replace_all(str, "@Your_item@", uppercase_first(name));

        string ring_holder_singular;
        string ring_holder_plural;

        ring_holder_singular = you.hand_name(false);
        ring_holder_plural = you.hand_name(true);

        str = replace_all(str, "@hand@", ring_holder_singular);
        str = replace_all(str, "@hands@", ring_holder_plural);

        messages.push_back(str);
    }

    if (item_def* item = you.body_armour())
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

static void _xom_chaos_upgrade(int)
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "chaos upgrade"), true);
}

static void _xom_player_confusion_effect(int)
{
    const bool conf = you.confused();
    const int dur = random_range(5, 7);

    if (!confuse_player(dur, true))
        return;

    if (you.can_drink())
    {
        god_speaks(GOD_XOM, _get_xom_speech("confusion").c_str());
        mprf(MSGCH_WARN, "You are %sconfused.", conf ? "more " : "");
    }
    else
    {
        // Since this is very mean if one can't cure it, we might as well
        // fit in another joke simultaneously.
        const bool was_mighty = you.duration[DUR_MIGHT];
        you.increase_duration(DUR_MIGHT, dur);
        god_speaks(GOD_XOM, _get_xom_speech("drinkless confusion").c_str());
        mprf(MSGCH_WARN, "You feel %s and %sconfused.",
            was_mighty ? "mightier" : "very mighty", conf ? "more " : "");
    }

    // Xom may confuse surrounding creatures as well.
    // Make this more likey if you're unable to cure your own confusion.
    bool mons_too = false;
    if ((!you.can_drink() && !one_chance_in(10) || !one_chance_in(3)))
    {
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if ((!you.can_drink() && one_chance_in(10) || one_chance_in(3)))
                continue;
            _confuse_monster(*mi, dur);
            mons_too = true;
        }
    }

    // Take a note.
    string conf_msg = "confusion";
    if (mons_too)
        conf_msg += " (+ monsters)";
    if (!you.can_drink())
        conf_msg += " (+ might)";
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, conf_msg), true);
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "repel stairs"), true);
}

static void _xom_moving_stairs(int) { _xom_repel_stairs(false); }
static void _xom_unclimbable_stairs(int) { _xom_repel_stairs(true); }

static void _xom_cloud_trail(int)
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

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "cloud trail"), true);

    const string speech = _get_xom_speech("cloud trail");
    god_speaks(GOD_XOM, speech.c_str());

    if (suppressed)
        simple_god_message(" purifies the foul vapours!");
}

static void _xom_draining(int)
{
    int power = 100;
    const string speech = _get_xom_speech("suffering");
    god_speaks(GOD_XOM, speech.c_str());

    if (you.experience_level < 4
        || (you.hp_max_adj_temp < 0 && !you.penance[GOD_XOM]))
    {
        power /= 2;
    }

    drain_player(power + random_range(-5, 5), true);

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "draining"), true);
}

static void _xom_doom(int)
{
    int power = random_range(20, 25);
    const string speech = _get_xom_speech("suffering");
    god_speaks(GOD_XOM, speech.c_str());

    if (you.experience_level < 9
        || (you.attribute[ATTR_DOOM] > 50 && !you.penance[GOD_XOM]))
    {
        power = power * 2 / 3;
    }

    if (!(you.attribute[ATTR_DOOM] + power >= 100))
        mpr("Your doom draws closer.");

    you.doom(power);

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "inflicting Doom"), true);
}

static void _xom_torment(int)
{
    if (you.penance[GOD_XOM])
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
                                      you.penance[GOD_XOM] ? " all (player " : " (",
                                      you.hp, you.hp_max);
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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

    monster* mon = create_monster(mgen_data::hostile_at(hostile, true, spot, GOD_XOM)
                          .set_summoned(nullptr, MON_SUMM_WRATH, summ_dur(4))
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

static void _xom_summon_hostiles(int)
{
    int strengthRoll = random2(750);
    int num_summoned = 0;
    const bool shadow_creatures = one_chance_in(4);

    if (shadow_creatures)
    {
        // Small number of shadow creatures, but still a little touch of chaos.
        int multiplier = 1;
        int range_cap = div_rand_round(you.experience_level, 8);

        if (you.penance[GOD_XOM])
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
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);

        const string speech = _get_xom_speech("hostile monster");
        god_speaks(GOD_XOM, speech.c_str());
    }
}

// Make two hostile clones and one friendly clone. Net danger, but maybe
// the player can make something of it anyway.
static void _xom_send_in_clones(int)
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
            mon->base_attitude = ATT_HOSTILE;
            power = -1;
        }
        else
        {
            mon->base_attitude = ATT_FRIENDLY;
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
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Roll per-monster whether they're neutral or hostile.
static bool _xom_maybe_neutral_summon(bool threat,
                                      monster_type mon_type)
{
    beh_type setting = (x_chance_in_y(5, 30) || threat) ? BEH_HOSTILE
                                                        : BEH_NEUTRAL;

    mgen_data mg(mon_type, setting, you.pos(), MHITYOU, MG_FORCE_BEH, GOD_XOM);
    mg.set_summoned(&you, MON_SUMM_AID, summ_dur(4));
    mg.non_actor_summoner = "Xom";

    return create_monster(mg);
}

// Drain you for 3/4ths of your mp, then create several brain worms, mana
// vipers, and / or quicksilver elementals from your mp based on your xl. If
// Xom isn't wrathful, there is a chance of some being neutral.
static void _xom_brain_drain(int)
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
            if (_xom_maybe_neutral_summon(upgrade, MONS_BRAIN_WORM))
                created = true;

        for (int i = 0; i < viper_count; ++i)
            if (_xom_maybe_neutral_summon(upgrade, MONS_MANA_VIPER))
                created = true;

        for (int i = 0; i < quicksilver_count; ++i)
            if (_xom_maybe_neutral_summon(upgrade, MONS_QUICKSILVER_ELEMENTAL))
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
            take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
        }
        else
        {
            mprf(MSGCH_WARN, "You feel nearly all of your power leaking away!");
            const string note = make_stringf("drained mp");
            take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
        }
    }
}

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
            && !mon->is_summoned() && !mons_is_confused(*mon)
            && !mon->is_peripheral()
            && !mon->petrifying() && !mon->cannot_act() && !mon->asleep()
            && !mon->is_silenced() && !mon->has_ench(ENCH_WORD_OF_RECALL);
}

// Xom forces a random monster in sight to start reciting a Word of Recall.
// Since this is much rarer and less anticipated compared to normal sightings
// of ironbound convokers, it takes extra long to cast.
static void _xom_grants_word_of_recall(int)
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
    if (shouting == S_SILENT)
        xom_speech = "grant voiceless word of recall";
    phrasing = get_speaking_type(shouting);

    god_speaks(GOD_XOM, _get_xom_speech(xom_speech).c_str());
    mprf(MSGCH_WARN, "%s is forced to slowly start %s a word of recall!",
                     targetable[0]->name(DESC_A, true, false).c_str(),
                     phrasing.c_str());
    mon_enchant chant_timer = mon_enchant(ENCH_WORD_OF_RECALL, targetable[0], duration);
    targetable[0]->add_ench(chant_timer);

    note = make_stringf("made %s speak a word of recall",
                        targetable[0]->name(DESC_A, true, false).c_str());

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Disallow early banishment and make it rarer at lower XL in general.
static bool _allow_xom_banishment()
{
    // Always allowed if under penance.
    if (player_under_penance(GOD_XOM))
        return true;

    if (you.experience_level < 9)
        return false;

    const int lv = min(you.experience_level - 9, 11);
    if (!x_chance_in_y(lv, lv + 5))
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
    take_note(Note(NOTE_XOM_EFFECT, -1, -1,
                   "revert banishment"), true);
}

// Only used for Xom rarely reverting others' banishment, now.
void xom_maybe_reverts_banishment()
{
    if (!you.penance[GOD_XOM] && x_chance_in_y(HALF_MAX_PIETY, 1000))
        _revert_banishment(false);
}

static void _xom_do_banishment(bool real)
{
    god_speaks(GOD_XOM, _get_xom_speech("banishment").c_str());

    // Handles note taking
    banished("Xom");
    if (!real)
        _revert_banishment();
}

static void _xom_banishment(int) { _xom_do_banishment(true); }
static void _xom_pseudo_banishment(int) { _xom_do_banishment(false); }

static void _xom_noise(int)
{
    // Ranges from shout to shatter volume.
    const int noisiness = 12 + random2(19);

    god_speaks(GOD_XOM, _get_xom_speech("noise").c_str());
    // Xom isn't subject to silence.
    fake_noisy(noisiness, you.pos());

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "noise"), true);
}

static bool _mon_valid_blink_victim(const monster& mon)
{
    return !mon.wont_attack()
            && !mon.no_tele()
            && !mons_is_projectile(mon);
}

static void _xom_blink_monsters(int)
{
    int blinks = 0;
    // Sometimes blink towards the player, sometimes randomly. It might
    // end up being helpful instead of dangerous, but Xom doesn't mind.
    const bool blink_to_player = you.penance[GOD_XOM] || coinflip();
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
            monster_blink(*mi, true);

        blinks++;
    }

    if (blinks)
    {
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "blink monster(s)"),
                  true);
    }
}

static const string _player_leg_name()
{
    if (form_changes_anatomy())
        return you.foot_name(true);

    if (you.species == SP_NAGA
        || you.species == SP_MERFOLK && you.fishtail)
    {
        return "tail";
    }
    else if (you.species == SP_DJINNI)
        return "smoke trail";
    else if (you.species == SP_OCTOPODE)
        return "tentacles";
    else
        return "legs";
}

static void _xom_reaching(int)
{
    god_speaks(GOD_XOM, _get_xom_speech("reaching").c_str());

    you.increase_duration(DUR_REACH, random_range(120, 200));

    const string arms = you.arm_name(true);
    const string legs = _player_leg_name();

    if (you.species == SP_POLTERGEIST || arms == legs)
        mprf(MSGCH_DURATION, "Your %s grow incredibly long!", arms.c_str());
    else
    {
        mprf(MSGCH_DURATION, "Your %s and %s grow incredibly long!",
                               arms.c_str(), legs.c_str());
    }

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "reaching"), true);
}

// Gives instant training in 3 random skills, strongly favoring those in which
// the player has very little training (and trying never to pick those in which
// the player is highly trained).
static void _xom_random_knowledge(int)
{
    // First, calculate skill level of all non-useless skills.
    vector<pair<int, skill_type>> skill_list;
    for (int i = SK_FIRST_SKILL; i < SK_LAST_SKILL; ++i)
    {
        skill_type skill = static_cast<skill_type>(i);
        if (!is_useless_skill(skill))
        {
            const int val = you.skill(skill, 10, true, false);
            skill_list.push_back({val, skill});
        }
    }

    // Then determine the average of the player's 3 highest skills.
    sort(skill_list.begin(), skill_list.end(),
         [](const pair<int, skill_type>& a, const pair<int, skill_type>& b)
         { return a.first < b.first;});

    int sum = 0;
    for (int i = max(0, (int)skill_list.size() - 3); i < (int)skill_list.size(); ++i)
        sum += skill_list[i].first;
    sum /= 3;

    // Try to skip all skills whose level is greater than 2/3 of the average of
    // the player's 3 highest skills.
    const int threshold = sum * 2 / 3;
    vector<pair<int, skill_type>> low_skill_list = skill_list;

    vector<skill_type> removed_skill;
    for (int i = skill_list.size() - 1; i >= 0; --i)
    {
        if (low_skill_list[i].first > threshold)
        {
            removed_skill.push_back(low_skill_list[i].second);
            low_skill_list.erase(low_skill_list.begin() + i);
        }
    }

    // Now remove any skills that would cross-train into a removed skill
    // (and thus effectively boost them anyway.)
    for (skill_type removed_sk : removed_skill)
    {
        vector<skill_type> cross = get_crosstrain_skills(removed_sk);
        for (skill_type sk : cross)
        {
            for (int i = 0; i < (int)low_skill_list.size(); ++i)
            {
                if (sk == low_skill_list[i].second)
                {
                    low_skill_list.erase(low_skill_list.begin() + i);
                    break;
                }
            }
        }
    }

    god_speaks(GOD_XOM, _get_xom_speech("knowledge").c_str());
    more();

    vector<skill_type> given_skills;

    // If these criteria have eliminated too many things to even have 3 skills
    // left, instead pick skills at random from anything that isn't fully trained.
    int num_wanted = 3;
    if (low_skill_list.size() < 3)
    {
        shuffle_array(skill_list);
        for (size_t i = 0; i < skill_list.size() && (int)given_skills.size() < num_wanted; ++i)
        {
            if (skill_list[i].first < 270)
            {
                given_skills.push_back(skill_list[i].second);

                // Invocations is useless, but funny; allow giving it, but also
                // give a 4th skill, too.
                if (skill_list[i].second == SK_INVOCATIONS)
                    num_wanted++;
            }
        }
    }
    // Otherwise, pick randomly from remaining skills.
    else
    {
        shuffle_array(low_skill_list);
        for (int i = 0; i < min((int)low_skill_list.size(), num_wanted); ++i)
        {
            given_skills.push_back(low_skill_list[i].second);
            if (low_skill_list[i].second == SK_INVOCATIONS)
                num_wanted++;
        }
    }

    // Now actually give the skill points.
    vector<string> skill_names;
    for (skill_type skill : given_skills)
    {
        skill_names.push_back(skill_name(skill));
        int sk_p = pow(you.skill_cost_level, 1.25) * random_range(75, 90);

        // A mild hack to make these gifts less wild on gnolls/djinni
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
            sk_p /= 3;
        else if (you.has_mutation(MUT_INNATE_CASTER) && is_magic_skill(skill))
            sk_p /= 6;

        change_skill_points(skill, sk_p, true);
    }

    string msg = make_stringf("knowledge: %s", comma_separated_line(skill_names.begin(), skill_names.end()).c_str());
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, msg), true);
}

static bool _is_mirror_candidate(const monster* mon)
{
    return mon->hostile() && !mon->is_firewood() && !mon->is_summoned()
           && !mon->has_ench(ENCH_HALL_OF_MIRRORS)
           && !mons_is_unique(mon->type)
           && !mons_is_ghost_demon(mon->type)
           && you.can_see(*mon);
}

static void _xom_hall_of_mirrors(int)
{
    vector<monster*> candidates;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (_is_mirror_candidate(*mi))
            candidates.push_back(*mi);

    if (candidates.empty())
        return;

    monster* orig = candidates[random2(candidates.size())];
    orig->add_ench(ENCH_HALL_OF_MIRRORS);

    coord_def spot = find_newmons_square(orig->type, you.pos(), 3, 5, 2);
    if (!spot.origin())
        orig->move_to(spot, MV_INTERNAL);

    int num_copies = random_range(5, 10);

    vector<monster*> group;
    group.push_back(orig);
    for (int i = 0; i < num_copies; ++i)
    {
        spot = find_newmons_square(orig->type, you.pos(), 3, 5, 2);
        if (monster* clone = cast_phantom_mirror(orig, orig, 30, MON_SUMM_CHAOS, i, spot))
        {
            mon_enchant timer(ENCH_SUMMON_TIMER, nullptr, INFINITE_DURATION);
            clone->update_ench(timer);
            clone->add_ench(ENCH_MOSTLY_HARMLESS);
            clone->add_ench(ENCH_HALL_OF_MIRRORS);
            group.push_back(clone);
        }
    }

    god_speaks(GOD_XOM, _get_xom_speech("mirrors").c_str());
    mprf(MSGCH_WARN, "%s shimmers and splits into %s!",
         orig->name(DESC_THE).c_str(),
         number_in_words(group.size()).c_str());
}

static void _xom_make_traps(int)
{
    int num_traps = random_range(3, 9);
    vector<coord_def> spots;

    // Only deploy traps on plain floor, not other types of floor.
    for (radius_iterator ri(you.pos(), 6, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        if (env.grid(*ri) == DNGN_FLOOR)
            spots.push_back(*ri);

    num_traps = min(num_traps, (int)spots.size() / 4);

    if (num_traps < 1)
        return;

    shuffle_array(spots);
    int placed = 0;
    for (int i = 0; i < (int)spots.size() && placed < num_traps; ++i)
    {
        // Skip spots that might locally-disconnect the level
        if (plant_forbidden_at(spots[i]))
            continue;

        // Draw traps from the local set, skipping ones that aren't really traps
        // for the player, and adding in chaos-flavor traps and ones that don't
        // spawn normally. (Otherwise you get just alarm/dispersal in many places.)
        dungeon_feature_type trap = DNGN_FLOOR;
        if (one_chance_in(4))
            trap = DNGN_TRAP_HARLEQUIN;
        else if (one_chance_in(4))
            trap = DNGN_TRAP_NET;
        else
            trap = random_trap_for_place();
        while (trap == DNGN_TRAP_TELEPORT || trap == DNGN_TRAP_SHAFT)
            trap = random_trap_for_place();

        dungeon_terrain_changed(spots[i], trap);
        placed++;
    }

    if (placed)
    {
        god_speaks(GOD_XOM, _get_xom_speech("make traps").c_str());
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "made traps"), true);
    }
}

static void _handle_accidental_death(const int orig_hp,
    const FixedVector<uint8_t, NUM_MUTATIONS> &orig_mutation,
    const transformation orig_form)
{
    // Did ouch() return early because the player died from the Xom
    // effect, even though the player isn't under penance?
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
 * A formatting of Xom actions, their chance to happen, and if it should be done.
 *
 * xom_event_type name              An action for Xom to take.
 * @param int tension_weight        The chance with visible monsters.
 * @param int zero_tension_weight   The chance without visible monsters (but not
 *                                  when choosing explore actions).
 * @param int explore_weight        The chance when chosen as an explore action.
 * @param bool valid                A check to see if this action should be done.
 * @param tension                   How much danger we think the player's in.
 * @param explore_type              If an explore effect, whether this effect
 *                                  should be delayed until the player heals
 *                                  or re-enters a battle.
 */
struct xom_event_data
{
    xom_event_type name;
    int tension_weight;
    int zero_tension_weight;
    int explore_weight;
    function<bool(int tension)> valid;
    xom_explore_type explore_type;
};

static const vector<xom_event_data> _list_xom_good_actions = {
    {
        XOM_GOOD_POTION, 500, 0, 0
    },
    {
        XOM_GOOD_SPELL, 450, 0, 0
    },
    {
        XOM_GOOD_CONFUSION, 435, 0, 0, [](int /*tn*/)
        {return mon_nearby([](monster& mon){ return !mon.wont_attack(); });}
    },
    {
        XOM_GOOD_ENCHANT_MONSTER, 415, 0, 0, [](int)
        {return mon_nearby(_choose_enchantable_monster);}
    },
    {
        XOM_GOOD_SINGLE_ALLY, 300, 0, 50, [](int)
        {return !you.allies_forbidden();},
        XOM_EXPLORE_NEXT_BATTLE
    },
    {
        XOM_GOOD_POLYMORPH, 275, 0, 0, [](int)
        {return _xom_mons_poly_target() != nullptr;}
    },
    {
        XOM_GOOD_ALLIES, 280, 0, 0, [](int tn)
        {return tn > 4 && !you.allies_forbidden();}
    },
    {
        XOM_GOOD_FORCE_LANCE_FLEET, 230, 0, 0, [](int tn)
        {
            if (tn <= 4)
                return false;

            // Check for a reasonable amount of open space to place
            // these many living spells down in.
            int open_count = 0;
            for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
                 ri; ++ri)
            {
                if (!cell_is_solid(*ri))
                    open_count++;
            }

            return open_count > 6;
        }
    },
    {
        XOM_GOOD_FLORA_RING, 125, 0, 0, [](int)
        {
            // Check both that there's at least some space to spawn plants
            // and very few smiting-attack monsters around.
            int plant_capacity = 0;
            int smiters = 0;

            for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
                ri; ++ri)
            {
                if (!monster_at(*ri) && monster_habitable_grid(MONS_PLANT, *ri))
                    plant_capacity++;
            }

            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
                if (mi->threat_range(false) > 2 && grid_distance(mi->pos(), you.pos()) > 2)
                    smiters++;

            return plant_capacity >= 3
                   && smiters < div_rand_round(you.experience_level, 6) - 1;
        }
    },
    {
        XOM_GOOD_DESTRUCTION, 125, 0, 0, [](int)
        {
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            {
                // Skip adjacent monsters, and skip
                // non-hostile monsters if not feeling nasty.
                if (!mons_is_projectile(**mi)
                    && !mons_is_tentacle_or_tentacle_segment(mi->type)
                    && !adjacent(you.pos(), mi->pos())
                    && (!mi->wont_attack() || you.penance[GOD_XOM]))
                {
                    return true;
                }
            }
            return false;
        }
    },
    {
        XOM_GOOD_DOOR_RING, 110, 0, 0, [](int)
        {
            // Assess each if there's enough room to meaningfully raise a door
            // ring, if there's adjacent hostiles to move so it does anything
            // tactically, and if there's at least visible room to move them to.
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
                if (grid_distance(*ri, you.pos()) >= 3 && in_bounds(*ri)
                    && _xom_door_replaceable(env.grid(*ri)))
                {
                    replaceable++;
                }
            }

            return replaceable > 24 && adjacent_hostiles - 1 < spare_space_out;
        }
    },
    {
        XOM_GOOD_MASS_CHARM, 85, 0, 0, [](int tn)
        {return tn > 4 && mon_nearby(_choose_enchantable_monster);}
    },
    {
        XOM_GOOD_HYPER_ENCHANT_MONSTER, 70, 0, 0, [](int)
        {
            // First, look for weak present monsters.
            vector<monster*> potential = _xom_find_weak_monsters(false);

            // If there isn't, check if there's space to summon something weak.
            if (potential.empty() && x_chance_in_y(2, 5))
            {
                for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
                    ri; ++ri)
                {
                    if (!cell_is_solid(*ri))
                        return true;
                }
            }

            return !potential.empty();
        }
    },
    {
        XOM_GOOD_FOG, 60, 0, 0, [](int)
        {return !cloud_at(you.pos());}
    },
    {
        XOM_GOOD_FAKE_DESTRUCTION, 50, 0, 0, [](int)
        {return mon_nearby(_choose_enchantable_monster);}
    },
    {
        // Not the most interesting if the level is already nearly fully
        // explored (presumably cleared), but it can still occasionally
        // happen for runed door vaults and branch ends.
        XOM_GOOD_TELEPORT, 40, 590, 0, [](int /*tn*/)
        {return (!player_in_branch(BRANCH_ABYSS) && _teleportation_check())
                && (you.explore_estimate < 80
                || !x_chance_in_y(you.explore_estimate, 110));}
    },
    {
        XOM_GOOD_LIGHTNING, 21, 0, 0, [](int)
        {
            if (!player_in_a_dangerous_place())
                return false;

            // Make sure there's at least one enemy within
            // the initial lightning radius.
            for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID, true); ri;
                ++ri)
            {
                const monster *mon = monster_at(*ri);
                if (mon && !mon->wont_attack())
                    return true;
            }

            return false;
        }
    },
    {
        XOM_GOOD_WAVE_OF_DESPAIR, 15, 0, 0, [](int)
        {return mon_nearby(_choose_enchantable_monster);}
    },
    {
        XOM_GOOD_BLINDING_BLINKITIS, 20, 0, 0, [](int)
        {
            // Calculate that there's at least one adjacent monster,
            // and that total hostile monsters > smiter count * 5.
            int adjacent_hostiles = 0, total_hostiles = 0, smiters = 0;

            for (adjacent_iterator ai(you.pos()); ai; ++ai)
            {
                if (monster_at(*ai) && !monster_at(*ai)->wont_attack()
                   && !monster_at(*ai)->is_peripheral())
                {
                    adjacent_hostiles++;
                }
            }
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            {
                if (!mi->wont_attack())
                {
                    total_hostiles++;
                    if (mi->threat_range(false) > 2)
                        smiters++;
                }
            }
            return (you.hp < you.hp_max * 75 / 100) &&
                   adjacent_hostiles > 0 && smiters * 5 < total_hostiles;
        }
    },
    {
        XOM_GOOD_CHAOS_VEX, 25, 0, 0, [](int)
        {
            // Look through all hostile monsters to see if any with attacks
            // that aren't will immune are adjacent to any other monsters.
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
                if (_valid_vex_target(*mi))
                    for (adjacent_iterator ai(mi->pos()); ai; ++ai)
                        if (monster_at(*ai) && !monster_at(*ai)->wont_attack())
                            return true;

            return false;
        }
    },
    // Effects with very specific conditions, given a seemingly high weight
    // despite their flashiness due to how rarely they'll actually come up.
    // Might have more of these come in eventually...
    {
        XOM_GOOD_SNAKES, 405, 0,  0, [](int /*tn*/)
        {return mon_nearby(_hostile_snake);}
    },
    {
        XOM_GOOD_LIGHT_UP_WEBS, 205, 0, 0, [](int)
        {
            // Since this is meant to be a good action, don't set the player's
            // tile on fire if they're close to death.
            if (env.grid(you.pos()) == DNGN_TRAP_WEB
                && you.hp <= (27 - you.res_fire() * 4))
            {
                return false;
            }

            return !_xom_counts_webs().empty();
        }
    },
    {
        XOM_GOOD_ANIMATE_MON_WPN, 325, 0, 0, [](int tn)
        {return tn > 4 && _find_monster_with_animateable_weapon()
                       && !you.allies_forbidden();}
    },
    {
        XOM_GOOD_SCENERY, 120, 135, 0, [](int tn)
        {return (tn <= 4) && (!(_xom_scenery_candidates().empty())
                         || one_chance_in(3));}
    },
    {
        XOM_GOOD_CLOUD_TRAIL, 0, 375, 0, [](int)
        {return you.explore_estimate < 100 && !you.duration[DUR_CLOUD_TRAIL];}
    },

    // Exploration effects:
    {
        XOM_GOOD_REACHING, 0, 0, 70, [](int){ return !you.duration[DUR_REACH];},
        XOM_EXPLORE_NEXT_BATTLE
    },
    {
        XOM_GOOD_DIVINATION, 0, 0, 80, [](int)
        {return you.explore_estimate < 80
                || x_chance_in_y(you.explore_estimate, 120);}
    },
    {
        // Weight is high, since it can't happen early and repeat visits are far rarer
        XOM_GOOD_BAZAAR_TRIP, 0, 0, 120, [](int)
        {
           // Nesting bazaars makes exploration and shopping lists complain.
           // No leaving Sprint floors in Sprint.
           if (player_in_branch_recursive(BRANCH_BAZAAR)
              || brdepth[BRANCH_ABYSS] == -1)
           {
              return false;
           }

           // Check if you have enough gold, increased by each trip.
           if (you.gold < (1000 + 50 * (2 +
                          (you.props[XOM_BAZAAR_TRIP_COUNT].get_int() * 2))))
           {
              return false;
           }

           // Each bazaar trip reduces the chance of the next.
           return one_chance_in(you.props[XOM_BAZAAR_TRIP_COUNT].get_int() * 5);
        }
    },
    {
        XOM_GOOD_MUTATION, 0, 0, 80, [](int)
        {return !x_chance_in_y(you.how_mutated(true, false, false, false, true, true), 16)
                && you.can_safely_mutate();}
    },
    {
        XOM_GOOD_RANDOM_ITEM, 0, 0, 105
    },
    {
        XOM_GOOD_ACQUIREMENT, 0, 0, 45
    },
    {
        XOM_GOOD_RANDOM_KNOWLEDGE, 0, 0, 15, [](int)
        {
            return true;
        }
    },
    {
        XOM_GOOD_JOKE_CONSUMABLE, 0, 0, 70
    },
};

static const vector<xom_event_data> _list_xom_bad_actions = {
    {
        XOM_BAD_NOISE, 975, 415, 0, [](int tn)
        {
            return tn > 0 || !you.penance[GOD_XOM];
        }
    },
    {
        XOM_BAD_MISCAST_PSEUDO, 825, 715, 0, [](int /*tn*/)
        {
            return !you.penance[GOD_XOM];
        }
    },
    {
        XOM_BAD_ENCHANT_MONSTER, 790, 0, 0, [](int)
        {return mon_nearby(_choose_enchantable_monster);}
    },
    {
        XOM_BAD_BLINK_MONSTERS, 680, 0, 0, [](int)
        {return mon_nearby(_mon_valid_blink_victim);}
    },
    {
        XOM_BAD_CONFUSION, 610, 0, 0, [](int)
        {return !you.clarity();}
    },
    {
        XOM_BAD_CHAOS_UPGRADE, 590, 0, 0, [](int /*tn*/)
        {return mon_nearby(_choose_chaos_upgrade);}
    },
    {
        XOM_BAD_SWAP_MONSTERS, 540, 0, 0, [](int)
        {return _rearrangeable_pieces().size();}
    },
    {
        XOM_BAD_TELEPORT, 375, 0, 25, [](int tn)
        {
            const int explored = you.explore_estimate;

            return ((!player_in_branch(BRANCH_ABYSS) && _teleportation_check()))
                 && !((you.penance[GOD_XOM] && (explored >= 40 || tn > 10))
                     || (explored >= 60 + random2(40)));
        }
    },
    {
        XOM_BAD_POLYMORPH, 380, 0, 0, [](int)
        {return _xom_mons_poly_target() != nullptr;}
    },
    {
        XOM_BAD_SUMMON_HOSTILES, 170, 450, 40, [](int tn)
        {return tn <= 48 && you.hp * 3 >= you.hp_max;}, XOM_EXPLORE_HEALTHY
    },
    {
        XOM_BAD_TIME_CONTROL, 133, 0, 0, [](int)
        {return !you.duration[DUR_HASTE]
         && !you.duration[DUR_SLOW]
         && !you.duration[DUR_PARALYSIS];}
    },
    {
        XOM_BAD_BRAIN_DRAIN, 100, 0, 0, [](int)
        {return you.magic_points > 3;}
    },
    {
        XOM_BAD_FAKE_SHATTER, 65, 80, 0, [](int)
        {
            // Check if there's a reasonable amount of features
            // the fake Xom shatter is likely to collapse.
            int nearby_diggable = 0;
            for (radius_iterator ri(you.pos(), you.current_vision,
                                    C_SQUARE, LOS_NO_TRANS); ri; ++ri)
            {
                if (feat_is_diggable(env.grid(*ri))
                    || feat_is_door(env.grid(*ri)))
                {
                    nearby_diggable++;
                    if (nearby_diggable >= 4)
                        return true;
                }
            }

            return false;
        }
    },
    {
        XOM_BAD_CHAOS_CLOUD, 58, 0, 0, [](int)
        {return !cloud_at(you.pos());}
    },
    {
        XOM_BAD_GRANT_WORD_OF_RECALL, 80, 0, 0, [](int tn)
        {
            if (tn > 24 || !_has_min_recall_level())
                return false;

            bool recall_ready = false;
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            {
                if (_valid_speaker_of_recall(*mi))
                    recall_ready = true;
            }

            return recall_ready;
        }
    },
    {
        XOM_BAD_SEND_IN_THE_CLONES, 45, 0, 0, [](int tn)
        {
            if (tn > 24)
                return false;

            int clone_capacity = 0;

            for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true);
                ri; ++ri)
            {
                if (!monster_at(*ri) && monster_habitable_grid(MONS_PLAYER_ILLUSION, *ri))
                    clone_capacity++;
            }

            return clone_capacity >= 3;
        }
    },
    {
        XOM_BAD_DOOR_RING, 38, 0, 0, [](int tn)
        {
            if (tn <= 4)
                return false;

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

           return (replaceable > 30 && adjacent_hostiles <= adjascency_cap)
                   && ((moveable_hostiles <= adjacent_space
                   && moveable_hostiles >= 2) || (adjacent_hostiles >= 2));
        }
    },
    {
        XOM_BAD_TORMENT, 6, 0, 0, [](int)
        {return !you.res_torment();}
    },
    {
        XOM_BAD_MAKE_TRAPS, 100, 0, 0
    },

    // Strategic effects.
    {
        XOM_BAD_MUTATION, 0, 0, 120, [](int)
        {return you.can_safely_mutate();}
    },
    {
        XOM_BAD_DRAINING, 0, 0, 150, [](int /*tn*/)
        {return player_prot_life() < 3;}
    },
    {
        XOM_BAD_DOOM, 0, 250, 0, [](int)
        {return you.attribute[ATTR_DOOM] < 80 || you.penance[GOD_XOM]
                || one_chance_in(3);}
    },
    {
        XOM_BAD_PSEUDO_BANISHMENT, 0, 0, 10, [](int /*tn*/)
        {return !you.penance[GOD_XOM]
                && !player_in_branch_recursive(BRANCH_ABYSS);}
    },
    {
        XOM_BAD_BANISHMENT, 0, 0, 1, [](int /*tn*/)
        {return _allow_xom_banishment()
                && !x_chance_in_y(HALF_MAX_PIETY * 2, 1000)
                && !player_in_branch_recursive(BRANCH_ABYSS);}
    },
    {
        XOM_BAD_CHAOS_REINFORCEMENTS, 0, 0, 80
    },
    {
        XOM_BAD_DUEL, 25, 0, 60, [](int)
        {
            return !player_in_branch_recursive(BRANCH_XOM_ARENA)
                   && you.experience_level >= 7;
        }, XOM_EXPLORE_HEALTHY
    },
    {
        XOM_BAD_FLOOR_LOCK, 0, 40, 50, [](int)
        {
            return is_connected_branch(you.where_are_you)
                   && !you.attribute[ATTR_XOM_FLOOR_LOCK]
                   && _count_reachable_monsters() > 5;
        }
    },
    {
        XOM_BAD_MINIBANE, 0, 0, 40
    },
    {
        XOM_BAD_HALL_OF_MIRRORS, 0, 0, 55, [](int)
        {
            for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
                if (_is_mirror_candidate(*mi))
                    return true;
            return false;
        }, XOM_EXPLORE_NEXT_BATTLE
    },
    // Highly circumstantial effects with less total
    // chances than their weight would seem to show.
    {
        XOM_BAD_MOVING_STAIRS, 215, 0, 0, [](int /*tn*/)
        {return !(_nearby_stairs().empty());} // Abyss is always min 2 tension!
    },
    {
        XOM_BAD_FIDDLE_WITH_DOORS, 95, 5, 0, [](int tn)
        {return ((tn > 4 || (you.explore_estimate < 5)
                 && !you.penance[GOD_XOM])) && !_xom_door_candidates().empty();}
    },
    {
        XOM_BAD_CLIMB_STAIRS, 55, 0, 0, [](int)
        { return !(_nearby_stairs().empty()) &&
                 (feat_stair_direction(env.grid(you.pos())) != CMD_NO_CMD
                 && env.grid(you.pos()) != DNGN_ENTER_SHOP);}
    },
};


static const xom_event_data DUMMY_DATA = {XOM_DID_NOTHING, 0, 0, 0};

static const xom_event_data& _get_event_data(xom_event_type event)
{
    for (const xom_event_data& data : _list_xom_bad_actions)
        if (data.name == event)
            return data;

    for (const xom_event_data& data : _list_xom_good_actions)
        if (data.name == event)
            return data;

    return DUMMY_DATA;
}

static bool _explore_type_is_valid(xom_explore_type type, int tension)
{
    if (type == XOM_EXPLORE_IMMEDIATE)
        return true;
    else if (type == XOM_EXPLORE_NEXT_BATTLE && tension <= 0)
        return false;
    else if (type == XOM_EXPLORE_HEALTHY
        && you.hp * random_range(15, 20) < you.hp_max * 10)
    {
        return false;
    }

    return true;
}

static bool _explore_event_is_valid(xom_event_type event, int tension)
{
    const xom_event_data& data = _get_event_data(event);

    if (!_explore_type_is_valid(data.explore_type, tension))
        return false;

    return !data.valid || data.valid(tension);
}

/**
 * Try to choose an action for Xom to take.
 *
 * @param niceness      Whether the action should be 'good' for the player.
 * @param tension       How much danger we think the player's currently in.
 * @param is_explore    Whether this is an exploration effect (ie: a strategic effect)
 * @return              An bad action for Xom to take, e.g. XOM_DID_NOTHING.
 */
xom_event_type xom_choose_action(bool niceness, int tension, bool is_explore)
{
    vector<pair<const xom_event_data&, int>> weighted_list;
    const xom_event_data* action;

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

    // Choosing any type of good act is less likely at zero tension. Choosing
    // any type of bad action at non-zero tension is likewise less likely.
    if (!is_explore && niceness && tension == 0 && you_worship(GOD_XOM)
        && !x_chance_in_y(1, 2))
    {
#ifdef NOTE_DEBUG_XOM
        take_note(Note(NOTE_MESSAGE, 0, 0, "suppress good act because of "
                        "zero tension"), true);
#endif
        return XOM_DID_NOTHING;
    }
    else if (!is_explore && !niceness && !you.penance[GOD_XOM] && tension > random2(10)
             && you_worship(GOD_XOM) && x_chance_in_y(1, 2))
    {
#ifdef NOTE_DEBUG_XOM
        const string note = string("suppress bad act because of ") +
                                   tension + " tension";
        take_note(Note(NOTE_MESSAGE, 0, 0, note), true);
#endif
        return XOM_DID_NOTHING;
    }

    // Compile the total list based on Xom's niceness and any degree of tension.
    const vector<xom_event_data>& actions =
        niceness ? _list_xom_good_actions : _list_xom_bad_actions;

    if (is_explore)
    {
        for (const xom_event_data &effect : actions)
            if (effect.explore_weight > 0)
                weighted_list.push_back({effect, effect.explore_weight});
    }
    else if (tension > 0)
    {
        for (const xom_event_data &effect : actions)
            if (effect.tension_weight > 0)
                weighted_list.push_back({effect, effect.tension_weight});
    }
    else
    {
        for (const xom_event_data &effect : actions)
            if (effect.zero_tension_weight > 0)
                weighted_list.push_back({effect, effect.zero_tension_weight});
    }

    // With the list compiled, roll through action options to find one.
    while (true)
    {
        action = random_choose_weighted(weighted_list);

        // If there are somehow no valid actions at all (due to changes
        // made to Xom action criteria), break out to do nothing.
        if (!action)
            break;

        // If the action was invalid, set its weight to 0 and try again.
        // (But don't check validity of 'Next combat' explore events, since
        // those will often not be valid until combat happens.)
        if (!(is_explore && action->explore_type == XOM_EXPLORE_NEXT_BATTLE)
            && action->valid && !action->valid(tension))
        {
            find_if(weighted_list.begin(), weighted_list.end(),
                    [action](const pair<const xom_event_data&, int>& entry)
                    {return entry.first.name == action->name;})->second = 0;
        }
        else
        {
            // If this is an explore effect that should be deferred to later,
            // queue it and do nothing right now.
            if (is_explore
                && !you.penance[GOD_XOM]
                && !_explore_type_is_valid(action->explore_type, tension))
            {
                CrawlVector& vec = you.props[XOM_QUEUED_EXPLORE_KEY].get_vector();
                vec.push_back(action->name);
                return XOM_DID_NOTHING;
            }

            return action->name;
        }
    }

    return XOM_DID_NOTHING;
}

/**
 * Execute the specified Xom Action.
 *
 * @param action        The action type in question; e.g. XOM_BAD_NOISE.
 * @param tension       The current tension.
 */
void xom_take_action(xom_event_type action, int tension)
{
    const int  orig_hp       = you.hp;
    const transformation orig_form = you.form;
    const FixedVector<uint8_t, NUM_MUTATIONS> orig_mutation = you.mutation;

    // actually take the action!
    {
        god_acting gdact(GOD_XOM);
        _do_xom_event(action, tension);
    }

    _handle_accidental_death(orig_hp, orig_mutation, orig_form);
}

/**
 * Let Xom take an action, probably.
 *
 * @param nice          Whether the action should be 'good' for the player.
 *                      If maybe_bool::maybe, determined by xom's whim.
 *                      May be overridden.
 * @param tension       How much danger we think the player's currently in.
 * @return              Whichever action Xom took, or XOM_DID_NOTHING.
 */
xom_event_type xom_acts(maybe_bool nice, int tension, bool debug)
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
        mprf(MSGCH_DIAGNOSTICS, "xom_acts(%u, %d)",
             niceness, tension);

        static char xom_buf[100];
        snprintf(xom_buf, sizeof(xom_buf), "xom_acts(%s, %d)",
                 (niceness ? "true" : "false"), tension);
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

    _calculate_exploration_estimate(true);
    const xom_event_type action = xom_choose_action(niceness, tension);
    if (!debug)
        xom_take_action(action, tension);

    return action;
}

xom_event_type xom_acts_exploration()
{
    // Chance of good/bad action is flat, but the first few actions after
    // worship will always be good.
    bool nice;
    if (you.gift_timeout > 0)
    {
        nice = true;
        --you.gift_timeout;
    }
    else
        nice = x_chance_in_y(3, 5);

    _calculate_exploration_estimate(true);
    const xom_event_type action = xom_choose_action(nice, 0, true);
    xom_take_action(action, 0);

    you.attribute[ATTR_XOM_EXPLORE_COUNT] -= XOM_EXPLORE_THRESHOLD;

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
 * and an additional chance based on tension that They will refuse to
 * save you.
 * @param death_type  The type of death that occurred.
 * @return            True if Xom saves your life, false otherwise.
 */
bool xom_saves_your_life(const kill_method_type death_type)
{
    if (!you_worship(GOD_XOM) || you.penance[GOD_XOM])
        return false;

    // If this happens, don't bother.
    if (you.hp_max < 1 || you.experience_level < 1)
        return false;

    // Generally a rare effect.
    if (!one_chance_in(20))
        return false;

    if (!_death_is_worth_saving(death_type))
        return false;

    // In addition, the chance depends on the current tension.
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

    return true;
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
        more();
        if (one_chance_in(10) || count >= 7 + random2(5))
            break;
    }
    while (x_chance_in_y(3, 4) || player_in_a_dangerous_place());

    // Take a note.
    const string note = make_stringf("%d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
             player_in_a_dangerous_place() ? " (dangerous)" :
#endif
             "");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
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
        more();
        if (count++ >= 7 + random2(5))
            break;
    }
    while (x_chance_in_y(3, 4) && !player_in_a_dangerous_place());

    // Take a note.
    const string note = make_stringf("%d-stop teleportation journey%s", count,
#ifdef NOTE_DEBUG_XOM
             badness == 3 ? " (dangerous)" : "");
#else
    "");
#endif
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

// Finds a random position near some monster which it out of sight of the player
static coord_def _find_pos_near_monster(monster_type type, vector<monster*>& mon_list)
{
    shuffle_array(mon_list);

    coord_def spot;
    for (monster* mon : mon_list)
    {
        if (find_habitable_spot_near(mon->pos(), type, 4, spot, -1, mon))
        {
            if (!you.see_cell(spot))
                return spot;
        }
    }

    return coord_def();
}

/**
 * Adds a handful of additional monster spawns to the current floor, ideally
 * near existing monsters, and never in line of sight of the player, with the
 * chaoslace buff.
 */
static void _xom_chaos_reinforcements(int)
{
    const int num = random_range(3, 8);

    mgen_data mg(RANDOM_MONSTER, BEH_HOSTILE, coord_def(), MHITNOT,
                 MG_PERMIT_BANDS, GOD_XOM);
    mg.proximity = PROX_AWAY_FROM_PLAYER;

    if (env.absdepth0 <= 20)
        mg.place = level_id(BRANCH_DUNGEON, env.absdepth0 + 1);
    else
        mg.place = level_id(BRANCH_DEPTHS, env.absdepth0 - 20);

    vector<monster*> near_targs;
    for (monster_iterator mi; mi; ++mi)
        if (mi->hostile() && !mi->is_peripheral() && !you.see_cell(mi->pos()))
            near_targs.push_back(*mi);

    bool created = false;
    for (int i = 0; i < num; ++i)
    {
        monster_type mtype = resolve_monster_type(RANDOM_MONSTER, mg.base_type,
                                                  &mg.place, nullptr, false);
        mg.cls = mtype;

        coord_def spot = _find_pos_near_monster(mtype, near_targs);

        if (!spot.origin())
        {
            mg.pos = spot;
            mg.flags |= MG_FORCE_PLACE;
        }
        else
        {
            mg.pos = coord_def();
            mg.flags &= ~MG_FORCE_PLACE;
        }

        if (monster* mon = place_monster(mg))
        {
            mon->add_ench(mon_enchant(ENCH_CHAOS_LACE, nullptr, INFINITE_DURATION));
            created = true;
        }
    }

    if (created)
    {
        simple_god_message(" dispatches reinforcements to the rest of the floor.", false, GOD_XOM);
        take_note(Note(NOTE_XOM_EFFECT, -1, -1, "chaos reinforcements"), true);
    }
}

static void _xom_duel(int)
{
    // Save the player's location before we move them.
    const level_id orig_place = level_id::current();

    stop_delay(true);
    down_stairs(DNGN_ENTER_XOM_ARENA);

    mgen_data mg(RANDOM_MONSTER, BEH_HOSTILE, you.pos(), MHITNOT,
                 MG_NONE, GOD_XOM);

    // Three major styles, chosen at random:

    // 1) Some random floor-appropriate monsters
    if (!one_chance_in(5))
    {
        mg.place = orig_place;
        mg.set_range(4, 8, 3);

        // For places with no natural population table, use the dungeon's.
        if (orig_place.branch == BRANCH_TEMPLE
            || orig_place.branch == BRANCH_BAZAAR
            || orig_place.branch == BRANCH_ARENA
            || orig_place.branch == BRANCH_TROVE)
        {
            mg.place.branch = BRANCH_DUNGEON;
            mg.place.depth = you.experience_level;
        }
        else
        {
            if (mg.place.branch == BRANCH_DUNGEON)
                mg.place.depth += 1;
            else
                mg.place.depth += 5;
        }

        int to_make = 2
                      + (you.experience_level > 10 ? coinflip() : 0)
                      + (you.experience_level > 16 ? 1 : 0)
                      + (you.experience_level >= 25 ? coinflip() : 0);

        for (int i = 0; i < to_make; ++i)
        {
            mg.cls = resolve_monster_type(RANDOM_MONSTER, mg.base_type,
                                          &mg.place, nullptr, false);
            if (monster* mon = create_monster(mg))
                behaviour_event(mon, ME_ANNOY, &you, you.pos());
        }
    }
    // 2) A very sad Pan lord
    else if (!one_chance_in(4) && you.experience_level >= 14)
    {
        mg.cls = MONS_PANDEMONIUM_LORD;

        if (monster* mons = create_monster(mg))
        {
            // Starting at XL 25, you can get real Pan lords!
            if (!x_chance_in_y(you.experience_level - 24, 3))
            {
                ghost_demon ghd;
                ghd.init_xom_pandemonium_lord(you.experience_level);
                mons->spells.clear();
                mons->set_ghost(ghd);
                mons->ghost_demon_init();
                mons->props[XOM_PAN_LORD_DESC] = "most pitiful";
                behaviour_event(mons, ME_ANNOY, &you, you.pos());
            }
        }
    }
    // 3) Something completely harmless.
    else
    {
        const int to_make = random_range(8, 12);
        mg.cls = MONS_BUTTERFLY;
        for (int i = 0; i < to_make; ++i)
        {
            if (monster* mons = create_monster(mg))
                mons->add_ench(mon_enchant(ENCH_SLOWLY_DYING, nullptr, random_range(30, 60)));
        }
    }

    god_speaks(GOD_XOM, _get_xom_speech("duel").c_str());

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "duel"), true);
}

/// Place a one-tile chaos cloud on the player, with minor spreading.
static void _xom_chaos_cloud(int /*sever*/)
{
    const int lifetime = 3 + random2(12) * 3;
    const int spread_rate = random_range(5,15);
    place_cloud(CLOUD_CHAOS, you.pos(), lifetime, nullptr, spread_rate);
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "chaos cloud"),
              true);
    god_speaks(GOD_XOM, _get_xom_speech("cloud").c_str());
}

// A rough count of how many full hostile monsters are available within the
// player's area (which could count for the floor lock effect).
static int _count_reachable_monsters()
{
    // Pathfinding is a bit intensive, so first do a quick pass to make sure
    // that there's at least *something* on the floor (excluding monsters in
    // no_tele_into areas as they are likely to be unreachable or behind runed
    // doors).
    bool found = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_summoned() && !mi->is_peripheral()
            && mi->base_attitude == ATT_HOSTILE
            && !(env.pgrid(mi->pos()) & FPROP_NO_TELE_INTO))
        {
            found = true;
            break;
        }
    }

    if (!found)
        return 0;

    // Now use pathfinding to ensure that the player can reach these monsters.

    // This is slightly hacky; we control terrain traversal ability by picking
    // a monster whose properties matches the players permanent swimming/flying.

    monster_pathfind pf;
    monster fake_mons;
    fake_mons.type = you.permanent_flight() ? MONS_PLAYER_ILLUSION
                     : you.can_swim(true) ? MONS_MERFOLK
                     : MONS_ORC;
    fake_mons.position = you.pos();
    define_monster(fake_mons);

    pf.fill_traversability(&fake_mons, 60);

    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_summoned() && !mi->is_peripheral()
            && mi->base_attitude == ATT_HOSTILE
            && pf.is_reachable(mi->pos()))
        {
            ++count;
        }
    }

    return count;
}

// Seal all exits from a floor until the player kills a certain small number of
// monsters.
static void _xom_floor_lock(int)
{
    int count = _count_reachable_monsters();

    // Just in case.
    if (count == 0)
        return;

    const int to_kill = min(count, random_range(5, 12));
    you.attribute[ATTR_XOM_FLOOR_LOCK] = to_kill;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feat_is_travelable_stair(env.grid(*ri)))
        {
            dungeon_feature_type stype;
            if (feat_stair_direction(env.grid(*ri)) == CMD_GO_UPSTAIRS)
                stype = DNGN_SEALED_STAIRS_UP;
            else
                stype = DNGN_SEALED_STAIRS_DOWN;

            temp_change_terrain(*ri, stype, INFINITE_DURATION,
                                TERRAIN_CHANGE_XOM_SEAL);
        }
    }

    god_speaks(GOD_XOM, _get_xom_speech("floor lock").c_str());
    mpr("Xom locks all of the stairs on the floor! (Kill monsters to unlock them again.)");
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "floor lock"), true);
}

static void _xom_minibane(int)
{
    god_speaks(GOD_XOM, _get_xom_speech("suffering").c_str());
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "minibane"), true);
    add_bane(NUM_BANES, "Xom's whimsy", 0, 20);
    more();
}

// On an appropriate kill, maybe progress the floor lock.
void xom_reduce_floor_lock()
{
    if (!you.attribute[ATTR_XOM_FLOOR_LOCK])
        return;

    if (--you.attribute[ATTR_XOM_FLOOR_LOCK] == 0)
    {
        god_speaks(GOD_XOM, _get_xom_speech("congratulations").c_str());
        end_terrain_changes(TERRAIN_CHANGE_XOM_SEAL, 0, true);
    }
}

static void _xom_cleaving()
{
    god_speaks(GOD_XOM, _get_xom_speech("cleaving").c_str());

    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_CLEAVE;

    brand_type brand = random_choose(SPWPN_CONCUSSION,
                                           SPWPN_SUNDERING,
                                           SPWPN_ENTANGLING,
                                           SPWPN_VALOUR,
                                           SPWPN_REBUKE);

    //brand = SPWPN_REBUKE;

    you.props[XOM_CLEAVE_BRAND_KEY] = brand;

    const item_def* const weapon = you.weapon();
    bool can_plural = false;
    const string wpn_name = weapon ? weapon->name(DESC_YOUR).c_str()
                                   : make_stringf("your bare %s", you.hand_name(true, &can_plural).c_str()).c_str();
    const char* suffix = can_plural ? "them" : "it";
    string msg;

    switch (brand)
    {
        default:
        case SPWPN_CONCUSSION:
            msg = make_stringf("%s %s dense and you feel ready to concuss everything with %s.",
                               wpn_name.c_str(), conjugate_verb("grow", can_plural).c_str(), suffix);
            break;

        case SPWPN_SUNDERING:
            msg = make_stringf("%s %s sharply and you feel ready to sunder everything with %s.",
                               wpn_name.c_str(), conjugate_verb("glint", can_plural).c_str(), suffix);
            break;

        case SPWPN_ENTANGLING:
            msg = make_stringf("Vines sprout from %s and you feel ready to entangle everything with %s.",
                               wpn_name.c_str(), suffix);
            break;

        case SPWPN_REBUKE:
            msg = make_stringf("%s %s with scorn and you feel ready to rebuke everything with %s.",
                               wpn_name.c_str(), conjugate_verb("quiver", can_plural).c_str(), suffix);
            break;

        case SPWPN_VALOUR:
            msg = make_stringf("%s %s with valour and you feel ready to face down everything with %s.",
                               wpn_name.c_str(), conjugate_verb("gleam", can_plural).c_str(), suffix);
    }

    mpr(msg);

    string note = make_stringf("blessing of cleaving (%s)", brand_type_name(brand, false));
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

static const vector<spell_type> _eisegesis_spells =
{
    SPELL_HAILSTORM,
    SPELL_INNER_FLAME,
    SPELL_PILEDRIVER,
    SPELL_MEPHITIC_CLOUD,
    SPELL_TUKIMAS_DANCE,
    SPELL_BLASTMOTE,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_TELEPORT_OTHER,
    SPELL_AIRSTRIKE,
    SPELL_BOULDER,
    SPELL_FULMINANT_PRISM,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_STICKY_FLAME,
    SPELL_ELECTRIC_CHARGE,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_SURPRISING_CROCODILE,
    SPELL_PUTREFACTION,
    SPELL_INTOXICATE,
    SPELL_ARCJOLT,
    SPELL_FIREBALL,
    SPELL_LRD,
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_FREEZING_CLOUD,
    SPELL_SUMMON_FOREST,
    SPELL_BOMBARD,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_DISPERSAL,
    SPELL_NOXIOUS_BOG,
    SPELL_MONARCH_BOMB,
    SPELL_GELLS_GAVOTTE,
    SPELL_PERMAFROST_ERUPTION,
    SPELL_PLASMA_BEAM,
    SPELL_SIMULACRUM,
    SPELL_STARBURST,
    SPELL_FORTRESS_BLAST,
    SPELL_ENFEEBLE,
    SPELL_HELLFIRE_MORTAR,
    SPELL_MAGNAVOLT,
    SPELL_MALIGN_GATEWAY,
    SPELL_MANIFOLD_ASSAULT,
    SPELL_IOOD,
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_DISCORD,
    SPELL_FULSOME_FUSILLADE,
    SPELL_IGNITION,
    SPELL_CHAIN_LIGHTNING,
    SPELL_FIRE_STORM,
    SPELL_POLAR_VORTEX,
    SPELL_SHATTER,
};

static void _xom_eisegesis()
{
    const int spell_level = max(3, min(9, random_range(
        you.experience_level / 5 + 2, you.experience_level / 3 + 3)));

    vector<spell_type> choices;
    for (int i = 0; i < (int)_eisegesis_spells.size(); ++i)
        if (spell_difficulty(_eisegesis_spells[i]) == spell_level)
            choices.push_back(_eisegesis_spells[i]);

    spell_type spell = choices[random2(choices.size())];

    god_speaks(GOD_XOM, _get_xom_speech("eisegesis").c_str());
    mprf(MSGCH_DURATION, "Xom grants you chaotic knowledge of %s.", spell_title(spell));

    you.props[EISEGESIS_SPELL] = spell;

    const string msg = make_stringf("blessing of eisegesis (%s)", spell_title(spell));
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, msg), true);

    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_CHAOS_EISEGESIS;
}

static void _xom_guardian()
{
    xom_remove_guardians();

    // Since _xom_random_pal has such high variablity, here we directly skip
    // such to choose a specific short range boosted by extra runes.
    monster_picker xom_picker;
    int range = you.experience_level + random_range(-1, 1);
    if (you.runes.count() > ZOT_ENTRY_RUNES + 1)
        range = min(33, range + div_rand_round(you.runes.count(), ZOT_ENTRY_RUNES));

    monster_type mon_type = xom_picker.pick(_xom_summons, range, MONS_CRIMSON_IMP);
    if (you.experience_level == 27)
        mon_type = _xom_chaos_branch_pal(mon_type, true);

    // No butterflies (useless), statues (blocks off exploration in dead ends),
    // or shapeshifters (incentivizes waiting for good rolls across stairs).
    while (mons_class_is_stationary(mon_type) || !mons_class_is_threatening(mon_type)
           || mon_type == MONS_SHAPESHIFTER || mon_type == MONS_GLOWING_SHAPESHIFTER)
    {
        mon_type = _xom_random_pal(0, true);
    }

    mgen_data mg(mon_type, BEH_FRIENDLY, you.pos(), MHITYOU, MG_FORCE_BEH, GOD_XOM);
    mg.non_actor_summoner = "Xom";
    mg.set_summoned(nullptr, MON_SUMM_XOM_GUARDIAN, 0, false, false);

    int count = _xom_pal_minibands(mon_type);

    int made_count = 0;
    // Proper demons get Pan lord-style names. Other monsters get incongruously bland names.
    string name = mons_class_holiness(mon_type) == MH_DEMONIC
                  && mons_class_intel(mon_type) == I_HUMAN
                    ? make_name()
                    : do_mon_name_replacements(getRandMonNameString("bland name"));
    string other_name = coinflip() ? "Other " : "Not-";
    string other_other_name = other_name + other_name;
    for (int i = 0; i < count; ++i)
    {
        if (monster *summons = create_monster(mg))
        {
            // Make them a bit more durable than a usual monster of their type.
            summons->max_hit_points = summons->max_hit_points * 3 / 2;
            summons->hit_points = summons->max_hit_points;

            summons->flags |= (MF_GUARDIAN | MF_NO_REWARD | MF_HARD_RESET);

            summons->mname = make_stringf("%s%s",
                                          i == 0 ? "" :
                                          i == 1 ? other_name.c_str() : other_other_name.c_str(),
                                          name.c_str());
            summons->props[DBNAME_KEY] = mons_class_name(mon_type);

            if (summons->type == MONS_REAPER)
                _do_chaos_upgrade(*summons->weapon(), summons);

            xom_add_guardian(*summons);
            ++made_count;
        }
    }

    if (made_count > 0)
    {
        you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_GUARDIAN;

        if (made_count > 1)
        {
            god_speaks(GOD_XOM, _get_xom_speech("guardian summon plural").c_str());
            mprf("Xom grants you %s devoted guardians.", number_in_words(made_count).c_str());
        }
        else
        {
            god_speaks(GOD_XOM, _get_xom_speech("guardian summon").c_str());
            mpr("Xom grants you a devoted guardian.");
        }
    }

    const string note = make_stringf("blessing of guardians: %d %s",
                                     made_count,
                                     mons_type_name(mon_type, DESC_PLAIN).c_str());
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, note), true);
}

static void _xom_spellstride()
{
    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_SPELLSTRIDE;

    spell_type spell = random_choose
    (
        SPELL_QUICKSILVER_BOLT,
        SPELL_CORROSIVE_BOLT,
        SPELL_BOLT_OF_LIGHT,
        SPELL_BOLT_OF_FLESH,
        SPELL_BOLT_OF_DEVASTATION,
        SPELL_BOLT_OF_ANTIMAGIC
    );

    you.props[XOM_SPELLSTRIDE_SPELL] = spell;

    string zname = zap_name(spell_to_zap(spell));
    zname = replace_all(zname, "bolt of ", "");

    string xomspeak = _get_xom_speech("spellstride");
    xomspeak = replace_all(xomspeak, "@feet@", you.foot_name(true));
    xomspeak = replace_all(xomspeak, "@beam_type@", zname);

    god_speaks(GOD_XOM, xomspeak.c_str());

    mprf("You manifest living %s spells as you rampage.", lowercase_string(spell_title(spell)).c_str());

    const string msg = make_stringf("blessing of spellstride (%s)", spell_title(spell));
    take_note(Note(NOTE_XOM_EFFECT, -1, -1, msg), true);
}

static void _xom_mannequins()
{
    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_MANNEQUINS;

    god_speaks(GOD_XOM, _get_xom_speech("mannequin").c_str());
    mprf("Xom raises dopplegangers of enemies you slay yourself.");

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "blessing of mannequins"), true);
}

static void _xom_battleshape()
{
    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_BATTLESHAPE;
    you.duration[DUR_XOM_BATTLESHAPE_COOLDOWN] = 0;

    god_speaks(GOD_XOM, _get_xom_speech("battleshape").c_str());
    mprf("A polyhedron of chaotic magic now fights alongside you.");

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "blessing of battleshape"), true);
}

static void _xom_protean()
{
    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_PROTEAN;

    god_speaks(GOD_XOM, _get_xom_speech("protean").c_str());
    mpr("Your body ripples with infinite possibility.");

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "blessing of protean form"), true);
}

static void _xom_sneaky()
{
    you.attribute[ATTR_XOM_BLESSING] = XOM_BLESSING_SNEAKY;

    god_speaks(GOD_XOM, _get_xom_speech("sneaky").c_str());
    mpr("You feel like a master of disguise.");

    take_note(Note(NOTE_XOM_EFFECT, -1, -1, "sneaky"), true);
}

static void _xom_disguise(monster* observer)
{
    tileidx_t tile = TILEP_MONS_PILE_OF_DEBRIS;
    string msg;
    switch (random2(4))
    {
        case 0:
            msg = "You disguise yourself as a nondescript pile of rubble";
            tile = random_choose(TILEP_MONS_DISGUISE_RUBBLE_1,
                                 TILEP_MONS_DISGUISE_RUBBLE_2);
            break;

        case 1:
            msg = "You disguise yourself as a common bush";
            tile = TILEP_MONS_DISGUISE_BUSH;
            break;

        case 2:
            msg = "You disguise yourself as a convenient statue";
            tile = random_choose(TILEP_MONS_DISGUISE_STATUE_HERO,
                                 TILEP_MONS_DISGUISE_STATUE_HORSE,
                                 TILEP_MONS_DISGUISE_STATUE_IMP,
                                 TILEP_MONS_DISGUISE_STATUE_MERMAID,
                                 TILEP_MONS_DISGUISE_STATUE_SIGMUND,
                                 TILEP_MONS_DISGUISE_STATUE_TRIANGLE);
            break;

        case 3:
            msg = "You disguise yourself as an ordinary barrel";
            if (you.body_size(PSIZE_BODY) >= SIZE_LARGE)
                tile = TILEP_MONS_DISGUISE_BARREL_LARGE;
            else if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM)
                tile = TILEP_MONS_DISGUISE_BARREL_SMALL;
            else
                tile = TILEP_MONS_DISGUISE_BARREL;
            break;
    }

    mprf("%s to avoid %s notice.",
         msg.c_str(),
         observer->name(DESC_ITS).c_str());
    you.duration[DUR_DISGUISED] = random_range(80, 130);
    you.props[XOM_DISGUISE_APPEARANCE_KEY].get_int() = tile;

    if (mons_intel(*observer) == I_HUMAN && !observer->is_silenced()
        && mon_shape_is_humanoid(get_mon_shape(*observer))
        && one_chance_in(20))
    {
        mons_speaks_msg(observer, getSpeakString("disguise success"), MSGCH_TALK);
    }

    return;
}

static bool _valid_disguise_target(const monster* mon)
{
    return mon->hostile()
           && !mon->is_peripheral()
           && !mon->asleep()
           && mons_is_threatening(*mon)
           && you.can_see(*mon);
}

void xom_maybe_disguise_self(monster* observer)
{
    if (you.attribute[ATTR_XOM_BLESSING] != XOM_BLESSING_SNEAKY
        || you.duration[DUR_DISGUISED]
        || you.props.exists(XOM_DISGUISE_RECOVERY_KEY)
        || !_valid_disguise_target(observer))
    {
        return;
    }

    schedule_disguise_fineff();
}

void xom_disguise_self()
{
    // Gather all valid observers in sight and remember them
    vector<monster*> observers;
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
        if (_valid_disguise_target(*mi))
            observers.push_back(*mi);

    // If everyone has died between noticing the player and getting this far,
    // don't do anything.
    if (observers.empty())
        return;

    // Find the most threatening monster to mention avoiding.
    sort(observers.begin(), observers.end(), [](monster* a, monster* b)
        { return exp_value(*a) < exp_value(*b); });

    _xom_disguise(observers[0]);

    CrawlVector& vec = you.props[XOM_DISGUISE_OBSERVER_KEY].get_vector();
    for (monster* mon : observers)
        vec.push_back((int)mon->mid);
}

void xom_end_disguise(bool force)
{
    if (!force && !you.duration[DUR_DISGUISED])
        return;

    you.duration[DUR_DISGUISED] = 0;

    // If the disguise breaks with no witnesses left alive, and no observers,
    // you can avoid the cooldown.
    bool observer = false;
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (_valid_disguise_target(*mi) && !mi->asleep())
        {
            observer = true;
            break;
        }
    }
    // Now check the list of things that saw us transform and confirm whether
    // they're still alive. (Technically, this is a minor information leak,
    // since one of them *could* have died out of LoS of the player and the
    // player may not be certain about that, but I consider that unimportant.)
    if (!observer)
    {
        CrawlVector& vec = you.props[XOM_DISGUISE_OBSERVER_KEY].get_vector();
        for (int i = 0; i < vec.size(); ++i)
        {
            if (monster* mon = monster_by_mid(vec[i].get_int()))
            {
                if (mon->alive())
                {
                    observer = true;
                    break;
                }
            }
        }
    }

    you.props.erase(XOM_DISGUISE_OBSERVER_KEY);

    if (!observer)
    {
        mprf(MSGCH_DURATION, "With no witnesses left, you stow your disguise for future use.");
        return;
    }

    mprf(MSGCH_DURATION, "Your disguise falls apart.");

    you.props[XOM_DISGUISE_RECOVERY_KEY] = random_range(120, 170);

    // Make it obvious what just happened, but don't wake up sleeping things automatically.
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
        if (mons_is_wandering(**mi))
            behaviour_event(*mi, ME_ALERT, &you, you.pos(), true);
}

// See if the player's blessing timer has been filled up. And if it has and the
// tension is zero, give them a new blessing.
void xom_check_blessing()
{
    if (you.attribute[ATTR_XOM_BLESSING_TIMER] <= 0
        && you.hp * 3 >= you.hp_max * 2)
    {
        xom_give_blessing();
        you.attribute[ATTR_XOM_BLESSING_TIMER] = random_range(190, 285);
        if (you.skill_cost_level > 25)
            you.attribute[ATTR_XOM_BLESSING_TIMER] += random_range(40, 60);
    }
}

static const vector<pair<xom_event_type, int>> _blessing_weights =
{
    { XOM_BLESSING_CLEAVE, 10},
    { XOM_BLESSING_CHAOS_EISEGESIS, 30},
    { XOM_BLESSING_GUARDIAN, 20},
    { XOM_BLESSING_SPELLSTRIDE, 10},
    { XOM_BLESSING_MANNEQUINS, 10},
    { XOM_BLESSING_BATTLESHAPE, 10},
    { XOM_BLESSING_PROTEAN, 10},
    { XOM_BLESSING_SNEAKY, 10},
};

void xom_give_blessing()
{
    // Don't give the player the same blessing they already have.
    xom_event_type blessing = XOM_DID_NOTHING;
    do
    {
        blessing = *random_choose_weighted(_blessing_weights);
    } while (blessing == you.attribute[ATTR_XOM_BLESSING]);

    draw_ring_animation(you.pos(), 5, ETC_RANDOM, ETC_RANDOM, false, 20, TILE_BOLT_CHAOS);

    string removal_msg = xom_remove_blessing();
    xom_take_action(blessing, 0);

    if (!removal_msg.empty())
        mpr(removal_msg.c_str());
}

// Removes the current Xom blessing and returns a string explaining what happened.
// (Since it usually needs to be printed after adding the new blessing.)
string xom_remove_blessing()
{
    string msg;
    switch (you.attribute[ATTR_XOM_BLESSING])
    {
        case XOM_BLESSING_CLEAVE:
            msg = "Your cleaving frenzy subsides.";
            you.props.erase(XOM_CLEAVE_BRAND_KEY);
            break;

        case XOM_BLESSING_CHAOS_EISEGESIS:
            msg = make_stringf("Your divinely inspired misunderstanding of %s fades.",
                    spell_title(static_cast<spell_type>(you.props[EISEGESIS_SPELL].get_int())));
            you.props.erase(EISEGESIS_SPELL);
            break;

        case XOM_BLESSING_GUARDIAN:
            msg = make_stringf("Xom sends your guardian%s elsewhere.",
                               xom_guardians.size() > 1 ? "s" : "");
            xom_remove_guardians();
            break;

        case XOM_BLESSING_MANNEQUINS:
            msg = "Xom cuts the strings.";
            for (monster_iterator mi; mi; ++mi)
                if (mi->was_created_by(MON_SUMM_XOM_MANNEQUIN))
                    monster_die(**mi, KILL_TIMEOUT, NON_MONSTER);
            you.props.erase(XOM_MANNEQUIN_TROUPE);
            you.duration[DUR_XOM_MANNEQUIN_TIMER] = 0;
            break;

        case XOM_BLESSING_BATTLESHAPE:
        {
            monster* mon = find_chaos_battleshape();
            msg = make_stringf("You no longer feel a connection to %s.",
                               mon ? mon->name(DESC_THE).c_str() : "your battlecube");
            if (mon)
                monster_die(*mon, KILL_RESET, NON_MONSTER);
        }
        break;

        case XOM_BLESSING_PROTEAN:
            msg = "Your body no longer feels quite so unstable.";
            you.duration[DUR_XOM_PROTEAN_COOLDOWN] = 0;
            break;

        case XOM_BLESSING_SNEAKY:
            msg = "You feel abruptly conspicuous.";
            xom_end_disguise();
            break;

        default:
            return "";
    }

    you.attribute[ATTR_XOM_BLESSING] = 0;
    return msg;
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
    { XOM_GOOD_LIGHT_UP_WEBS, { "light up webs", _xom_lights_up_webs }},
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
    { XOM_GOOD_BLINDING_BLINKITIS, {"blinding blinkitis", _xom_blinding_blinkitis }},
    { XOM_GOOD_CHAOS_VEX, {"chaos vex", _xom_chaos_vex }},
    { XOM_GOOD_FOG, { "fog", _xom_fog }},
    { XOM_GOOD_CLOUD_TRAIL, { "cloud trail", _xom_cloud_trail }},
    { XOM_GOOD_REACHING, { "reaching", _xom_reaching }},
    { XOM_GOOD_RANDOM_KNOWLEDGE, { "knowledge", _xom_random_knowledge }},
    { XOM_GOOD_JOKE_CONSUMABLE, {"consumable", _xom_joke_consumable }},

    { XOM_BAD_MISCAST_PSEUDO, { "pseudo-miscast", _xom_pseudo_miscast }},
    { XOM_BAD_NOISE, { "noise", _xom_noise }},
    { XOM_BAD_ENCHANT_MONSTER, { "bad enchant monster",
                                 _xom_bad_enchant_monster }},
    { XOM_BAD_TIME_CONTROL, {"time control", _xom_time_control }},
    { XOM_BAD_BLINK_MONSTERS, { "blink monsters", _xom_blink_monsters }},
    { XOM_BAD_CONFUSION, { "confuse player", _xom_player_confusion_effect }},
    { XOM_BAD_SWAP_MONSTERS, { "swap monsters", _xom_rearrange_pieces }},
    { XOM_BAD_CHAOS_UPGRADE, { "chaos upgrade", _xom_chaos_upgrade }},
    { XOM_BAD_TELEPORT, { "bad teleportation", _xom_bad_teleport }},
    { XOM_BAD_POLYMORPH, { "bad polymorph", _xom_bad_polymorph }},
    { XOM_BAD_MOVING_STAIRS, { "moving stairs", _xom_moving_stairs }},
    { XOM_BAD_CLIMB_STAIRS, { "unclimbable stairs", _xom_unclimbable_stairs }},
    { XOM_BAD_FIDDLE_WITH_DOORS, { "open and close doors",
                                   _xom_open_and_close_doors }},
    { XOM_BAD_DOOR_RING, {"bad door ring enclosure", _xom_bad_door_ring }},
    { XOM_BAD_FAKE_SHATTER, {"fake shatter", _xom_fake_shatter }},
    { XOM_BAD_MUTATION, { "random mutations", _xom_give_bad_mutations }},
    { XOM_BAD_SUMMON_HOSTILES, { "summon hostiles", _xom_summon_hostiles }},
    { XOM_BAD_SEND_IN_THE_CLONES, {"friendly and hostile illusions",
                                   _xom_send_in_clones }},
    { XOM_BAD_GRANT_WORD_OF_RECALL, {"speaker of recall",
                                    _xom_grants_word_of_recall }},
    { XOM_BAD_BRAIN_DRAIN, {"mp brain drain", _xom_brain_drain }},
    { XOM_BAD_DRAINING, { "draining", _xom_draining }},
    { XOM_BAD_DOOM, { "doom", _xom_doom }},
    { XOM_BAD_TORMENT, { "torment", _xom_torment }},
    { XOM_BAD_CHAOS_CLOUD, { "chaos cloud", _xom_chaos_cloud }},
    { XOM_BAD_BANISHMENT, { "banishment", _xom_banishment }},
    { XOM_BAD_PSEUDO_BANISHMENT, {"pseudo-banishment",
                                  _xom_pseudo_banishment }},
    { XOM_BAD_CHAOS_REINFORCEMENTS, {"chaos reinforcements", _xom_chaos_reinforcements }},
    { XOM_BAD_DUEL, {"duel", _xom_duel }},
    { XOM_BAD_FLOOR_LOCK, {"floor_lock", _xom_floor_lock }},
    { XOM_BAD_MINIBANE, {"minibane", _xom_minibane }},
    { XOM_BAD_HALL_OF_MIRRORS, {"hall of mirrors", _xom_hall_of_mirrors }},
    { XOM_BAD_MAKE_TRAPS, {"traps", _xom_make_traps }},

    { XOM_BLESSING_CLEAVE, {"cleaving", [](int){ _xom_cleaving();} }},
    { XOM_BLESSING_CHAOS_EISEGESIS, { "eisegesis", [](int){ _xom_eisegesis();} }},
    { XOM_BLESSING_GUARDIAN, { "guardian", [](int){ _xom_guardian();} }},
    { XOM_BLESSING_SPELLSTRIDE, { "spellstride", [](int){ _xom_spellstride();} }},
    { XOM_BLESSING_MANNEQUINS, { "mannequins", [](int){ _xom_mannequins();} }},
    { XOM_BLESSING_BATTLESHAPE, { "battleshape", [](int){ _xom_battleshape();} }},
    { XOM_BLESSING_PROTEAN, { "protean", [](int){ _xom_protean();} }},
    { XOM_BLESSING_SNEAKY, { "sneaky", [](int){ _xom_sneaky();} }},
    { XOM_BLESSING_RANDOM, {"blessing", [](int){ xom_give_blessing();} }},
};

static void _do_xom_event(xom_event_type event_type, int tension)
{
    const xom_event *event = map_find(xom_events, event_type);
    if (event && event->action)
        event->action(tension);
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
    _calculate_exploration_estimate(true);
    int explored = you.explore_estimate;
    _calculate_exploration_estimate(false);
    int mapped = you.explore_estimate;
    return make_stringf("mapping estimate: %d%%\nexploration estimate: %d%%\n",
                        mapped, explored);
}

// Print a sorted breakdown of how often each effect came up in a sample.
static void _write_xom_effect_breakdown(FILE *ostat,
                                        const vector<xom_event_type> &effects)
{
    const int total = effects.size();
    if (total == 0)
        return;

    vector<xom_event_type> sorted = effects;
    sort(sorted.begin(), sorted.end());

    vector<xom_effect_count> xom_ec_pairs;
    xom_event_type old_effect = XOM_DID_NOTHING;
    int count = 0;
    for (int k = 0; k < total; ++k)
    {
        if (sorted[k] != old_effect)
        {
            if (count > 0)
                xom_ec_pairs.emplace_back(xom_effect_to_name(old_effect), count);
            old_effect = sorted[k];
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
        fprintf(ostat, "%7.2f%%    %s\n", (100.0 * xec.count / total),
                xec.effect.c_str());
    }
}

// Print a titled section for one sample of effects: the good/bad split followed
// by the sorted breakdown of which effects came up.
static void _write_xom_effect_section(FILE *ostat, const string &title,
                                      const vector<xom_event_type> &effects)
{
    const int total = effects.size();
    if (total == 0)
        return;

    int good = 0;
    for (xom_event_type effect : effects)
        if (effect <= XOM_LAST_GOOD_ACT)
            good++;

    fprintf(ostat, "\n%s\n", title.c_str());
    fprintf(ostat, "GOOD%7.2f%%\n", (100.0 * good / total));
    fprintf(ostat, "BAD %7.2f%%\n", (100.0 * (total - good) / total));
    _write_xom_effect_breakdown(ostat, effects);
}

// Sweeps a large tension range, calling xom_acts() many times at each
// tension, and prints the distribution of effects into a file.
// TODO: Allow specification of niceness.
void debug_xom_effects()
{
    // Repeat N times per tension.
    const int N = prompt_for_int("How many iterations per tension? ", true);

    if (N <= 0)
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

    // Not a true maximum tension, but high enough for this purpose.
    const int TENSION_MAX = 50;
    const god_type real_god = you.religion;
    you.religion            = GOD_XOM;

    fprintf(ostat, "---- STARTING XOM DEBUG TESTING ----\n");
    fprintf(ostat, "%s\n", dump_overview_screen().c_str());
    fprintf(ostat, "%s\n", screenshot().c_str());
    fprintf(ostat, "%s\n", _list_exploration_estimate().c_str());
    fprintf(ostat, "%s\n", mpr_monster_list().c_str());

    if (player_under_penance(GOD_XOM))
        fprintf(ostat, "You are under Xom's penance!\n");
    fprintf(ostat, "\nRunning %d effects at each tension from 0 to %d.\n",
            N, TENSION_MAX);
    fprintf(ostat, "---- OUTPUT EFFECT PERCENTAGES ----\n");

    // Bucket tension into chunks of 10, the last bucket absorbing the top of
    // the range, and report each bucket separately as well as in total.
    const int bucket_size = 10;
    const int num_buckets = TENSION_MAX / bucket_size;

    vector<xom_event_type> all_effects;
    vector<vector<xom_event_type>> bucket_effects(num_buckets);
    for (int tension = 0; tension <= TENSION_MAX; ++tension)
    {
        const int bucket = min(tension / bucket_size, num_buckets - 1);
        for (int i = 0; i < N; ++i)
        {
            const xom_event_type result = xom_acts(maybe_bool::maybe,
                                                   tension, true);
            all_effects.push_back(result);
            bucket_effects[bucket].push_back(result);
        }
    }

    _write_xom_effect_section(ostat, "Total effects (any tension)",
                              all_effects);
    for (int b = 0; b < num_buckets; ++b)
    {
        const int lo = b * bucket_size;
        const int hi = b == num_buckets - 1 ? TENSION_MAX
                                            : (b + 1) * bucket_size - 1;
        _write_xom_effect_section(ostat, make_stringf("Tension %d-%d", lo, hi),
                                  bucket_effects[b]);
    }

    fprintf(ostat, "---- FINISHED XOM DEBUG TESTING ----\n");
    fclose(ostat);
    mpr("Results written into 'xom_debug.stat'.");

    you.religion = real_god;
}
#endif // WIZARD

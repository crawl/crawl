/**
 * @file
 * @brief Pseudo spells triggered by gods and various items.
**/

#include "AppHdr.h"

#include "spl-goditem.h"

#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "directn.h"
#include "env.h"
#include "godconduct.h"
#include "godpassive.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "status.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"

static void _print_holy_pacification_speech(const string key,
                                            monster* mon,
                                            msg_channel_type channel)
{
    string full_key = "holy_being_";
    full_key += key;

    string msg = getSpeakString(full_key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        strip_channel_prefix(msg, channel);
        mprf(channel, "%s", msg.c_str());
    }
}

static bool _mons_hostile(const monster* mon)
{
    // Needs to be done this way because of friendly/neutral enchantments.
    return !mon->wont_attack() && !mon->neutral();
}

// Check whether it is possible at all to pacify this monster.
// Returns -1, if monster can never be pacified.
// Returns -2, if monster can currently not be pacified (asleep).
// Returns 0, if it's possible to pacify this monster.
int is_pacifiable(const monster* mon)
{
    if (!you_worship(GOD_ELYVILON))
        return -1;

    // I was thinking of jellies when I wrote this, but maybe we shouldn't
    // exclude zombies and such... (jpeg)
    if (mons_intel(mon) <= I_INSECT // no self-awareness
        || mons_is_tentacle_or_tentacle_segment(mon->type)) // body part
    {
        return -1;
    }

    const mon_holy_type holiness = mon->holiness();

    if (!mon->is_holy()
        && holiness != MH_UNDEAD
        && holiness != MH_DEMONIC
        && holiness != MH_NATURAL)
    {
        return -1;
    }

    if (mon->is_stationary()) // not able to leave the level
        return -1;

    if (mon->asleep()) // not aware of what is happening
        return -2;

    return 0;
}

// Check whether this monster might be pacified.
// Returns 0, if monster can be pacified but the attempt failed.
// Returns 1, if monster is pacified.
// Returns -1, if monster can never be pacified.
// Returns -2, if monster can currently not be pacified (asleep).
// Returns -3, if monster can be pacified but the attempt narrowly failed.
// Returns -4, if monster can currently not be pacified (too much hp).
static int _can_pacify_monster(const monster* mon, const int healed,
                               const int max_healed)
{
    int pacifiable = is_pacifiable(mon);
    if (pacifiable < 0)
        return pacifiable;

    if (healed < 1)
        return 0;

    const int factor = (mons_intel(mon) <= I_ANIMAL)       ? 3 : // animals
                       (is_player_same_genus(mon->type))   ? 2   // same genus
                                                           : 1;  // other

    int divisor = 3;

    const mon_holy_type holiness = mon->holiness();
    if (mon->is_holy())
        divisor--;
    else if (holiness == MH_UNDEAD)
        divisor++;
    else if (holiness == MH_DEMONIC)
        divisor += 2;

    if (mon->max_hit_points > factor * ((you.skill(SK_INVOCATIONS, max_healed)
                                         + max_healed) / divisor))
    {
        return -4;
    }

    int random_factor = random2((you.skill(SK_INVOCATIONS, healed) + healed)
                                / divisor);

    dprf("pacifying %s? max hp: %d, factor: %d, Inv: %d, healed: %d, rnd: %d",
         mon->name(DESC_PLAIN).c_str(), mon->max_hit_points, factor,
         you.skill(SK_INVOCATIONS), healed, random_factor);

    if (mon->max_hit_points < factor * random_factor)
        return 1;
    if (mon->max_hit_points < factor * random_factor * 1.15)
        return -3;

    return 0;
}

static vector<string> _desc_mindless(const monster_info& mi)
{
    if (mi.intel() <= I_INSECT)
        return { "mindless" };
    else
        return {};
}

static spret_type _healing_spell(int healed, int max_healed,
                                 bool divine_ability, const coord_def& where,
                                 bool not_self, targ_mode_type mode)
{
    ASSERT(healed >= 1);

    bolt beam;
    dist spd;

    if (where.origin())
    {
        spd.isValid = spell_direction(spd, beam, DIR_TARGET,
                                      mode != TARG_NUM_MODES ? mode :
                                      you_worship(GOD_ELYVILON) ?
                                            TARG_ANY : TARG_FRIEND,
                                      LOS_RADIUS, false, true, true, "Heal",
                                      nullptr, false, nullptr, _desc_mindless);
    }
    else
    {
        spd.target  = where;
        spd.isValid = in_bounds(spd.target);
    }

    if (!spd.isValid)
        return SPRET_ABORT;

    if (spd.target == you.pos())
    {
        if (not_self)
        {
            mpr("You can only heal others!");
            return SPRET_ABORT;
        }

        mpr("You are healed.");
        inc_hp(healed);
        return SPRET_SUCCESS;
    }

    monster* mons = monster_at(spd.target);
    if (!mons)
    {
        canned_msg(MSG_NOTHING_THERE);
        // This isn't a cancel, to avoid leaking invisible monster
        // locations.
        return SPRET_FAIL;
    }

    bool did_something = false;

    if (divine_ability
        && you_worship(GOD_ELYVILON)
        && _mons_hostile(mons))
    {
        const int can_pacify  = _can_pacify_monster(mons, healed, max_healed);

        // Don't divinely heal a monster you can't pacify.
        if (can_pacify <= 0)
        {
            if (can_pacify == 0)
            {
                mprf("The light of Elyvilon fails to reach %s.",
                     mons->name(DESC_THE).c_str());
                return SPRET_FAIL;
            }
            else if (can_pacify == -3)
            {
                mprf("The light of Elyvilon almost touches upon %s.",
                     mons->name(DESC_THE).c_str());
                return SPRET_FAIL;
            }
            else if (can_pacify == -4)
            {
                mprf("%s is completely unfazed by your meager offer of peace.",
                     mons->name(DESC_THE).c_str());
                return SPRET_FAIL;
            }
            else
            {
                if (can_pacify == -2)
                {
                    mprf("You cannot pacify this monster while %s is sleeping!",
                         mons->pronoun(PRONOUN_SUBJECTIVE).c_str());
                }
                else
                    mpr("You cannot pacify this monster!");
                return SPRET_ABORT;
            }
        }

        if (can_pacify == 1)
        {
            did_something = true;

            if (mons->is_holy())
            {
                string key = "pacification";

                // Quadrupeds can't salute, etc.
                mon_body_shape shape = get_mon_shape(mons);
                if (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA)
                    key += "_humanoid";

                _print_holy_pacification_speech(key, mons,
                                                MSGCH_FRIEND_ENCHANT);

                if (!one_chance_in(3)
                    && mons->can_speak()
                    && mons->type != MONS_MENNAS) // Mennas is mute and only has visual speech
                {
                    _print_holy_pacification_speech("speech", mons, MSGCH_TALK);
                }
            }
            else
                simple_monster_message(mons, " turns neutral.");

            record_monster_defeat(mons, KILL_PACIFIED);
            mons_pacify(mons, ATT_NEUTRAL);
        }
    }

    if (mons->heal(healed))
    {
        did_something = true;
        mprf("You heal %s.", mons->name(DESC_THE).c_str());

        if (mons->hit_points == mons->max_hit_points)
            simple_monster_message(mons, " is completely healed.");
        else
            print_wounds(mons);
    }

    if (!did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_FAIL;
    }

    return SPRET_SUCCESS;
}

spret_type cast_healing(int pow, int max_pow, bool divine_ability,
                        const coord_def& where, bool not_self,
                        targ_mode_type mode)
{
    pow = min(50, pow);
    max_pow = min(50, max_pow);
    return _healing_spell(pow + roll_dice(2, pow) - 2, (3 * max_pow) - 2,
                          divine_ability, where, not_self, mode);
}

/**
 * Remove magical effects from the player.
 *
 * Forms, buffs, debuffs, contamination, probably a few other things.
 * Flight gets an extra 11 aut before going away to minimize instadeaths.
 */
void debuff_player()
{
    bool need_msg = false, danger = false;

    if (you.attribute[ATTR_DELAYED_FIREBALL])
    {
        you.attribute[ATTR_DELAYED_FIREBALL] = 0;
        mprf(MSGCH_DURATION, "Your charged fireball dissipates.");
    }

    if (you.attribute[ATTR_REPEL_MISSILES])
    {
        you.attribute[ATTR_REPEL_MISSILES] = 0;
        need_msg = true;
    }

    if (you.attribute[ATTR_DEFLECT_MISSILES])
    {
        you.attribute[ATTR_DEFLECT_MISSILES] = 0;
        need_msg = true;
    }

    if (you.attribute[ATTR_SWIFTNESS] > 0)
    {
        you.attribute[ATTR_SWIFTNESS] = 0;
        need_msg = true;
    }

    for (unsigned int i = 0; i < NUM_DURATIONS; ++i)
    {
        int& dur = you.duration[i];
        if (duration_dispellable((duration_type) i) && dur > 0)
        {
            if ((i == DUR_FLIGHT || i == DUR_TRANSFORMATION) && dur > 11)
            {
                dur = 11;
                need_msg = true;
                danger = need_expiration_warning(you.pos());
            }
            else if (i == DUR_TELEPORT)
            {
                dur = 0;
                mprf(MSGCH_DURATION, "You feel strangely stable.");
            }
            else if (i == DUR_PETRIFYING)
            {
                dur = 0;
                mprf(MSGCH_DURATION, "You feel limber!");
                you.redraw_evasion = true;
            }
            else if (dur > 1)
            {
                dur = 1;
                need_msg = true;
            }
        }
    }

    if (need_msg)
    {
        mprf(danger ? MSGCH_DANGER : MSGCH_WARN,
             "%sYour magical effects are unravelling.",
             danger ? "Careful! " : "");
    }

    contaminate_player(-1 * (1000 + random2(4000)));
}

void debuff_monster(monster* mon)
{
    // List of magical enchantments which will be dispelled.
    static const enchant_type lost_enchantments[] =
    {
        ENCH_SLOW,
        ENCH_HASTE,
        ENCH_SWIFT,
        ENCH_MIGHT,
        ENCH_AGILE,
        ENCH_FEAR,
        ENCH_CONFUSION,
        ENCH_INVIS,
        ENCH_CORONA,
        ENCH_CHARM,
        ENCH_PARALYSIS,
        ENCH_PETRIFYING,
        ENCH_PETRIFIED,
        ENCH_REGENERATION,
        ENCH_STICKY_FLAME,
        ENCH_TP,
        ENCH_INNER_FLAME,
        ENCH_OZOCUBUS_ARMOUR,
        ENCH_INJURY_BOND,
        ENCH_DIMENSION_ANCHOR,
        ENCH_CONTROL_WINDS,
        ENCH_TOXIC_RADIANCE,
        ENCH_AGILE,
        ENCH_EPHEMERAL_INFUSION,
        ENCH_BLACK_MARK,
        ENCH_SHROUD,
        ENCH_SAP_MAGIC,
        ENCH_REPEL_MISSILES,
        ENCH_DEFLECT_MISSILES,
        ENCH_CONDENSATION_SHIELD,
        ENCH_RESISTANCE,
        ENCH_HEXED,
    };

    bool dispelled = false;

    // Dispel all magical enchantments...
    for (enchant_type ench : lost_enchantments)
    {
        // ...except for natural invisibility...
        if (ench == ENCH_INVIS && mons_class_flag(mon->type, M_INVIS))
            continue;
        // ...permaconfusion...
        if (ench == ENCH_CONFUSION && mons_class_flag(mon->type, M_CONFUSED))
            continue;
        // ...and regeneration from Trog.
        if (ench == ENCH_REGENERATION && mon->has_ench(ENCH_RAISED_MR))
            continue;

        if (mon->del_ench(ench, true, true))
            dispelled = true;
    }
    if (dispelled)
        simple_monster_message(mon, "'s magical effects unravel!");
}

int detect_traps(int pow)
{
    pow = min(50, pow);

    // Trap detection moved to traps.cc. -am
    const int range = 8 + random2(8) + pow;
    return reveal_traps(range);
}

// pow -1 for passive
int detect_items(int pow)
{
    int items_found = 0;
    int map_radius;
    if (pow >= 0)
        map_radius = 8 + random2(8) + pow;
    else
    {
        if (you_worship(GOD_ASHENZARI))
        {
            map_radius = min(you.piety / 20, LOS_RADIUS);
            if (map_radius <= 0)
                return 0;
        }
        else // MUT_JELLY_GROWTH
            map_radius = 6;
    }

    for (radius_iterator ri(you.pos(), map_radius, C_ROUND); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus item free).
        if (pow != -1 && env.map_knowledge(*ri).changed())
            continue;

        if (igrd(*ri) != NON_ITEM
            && !env.map_knowledge(*ri).item())
        {
            items_found++;
            env.map_knowledge(*ri).set_detected_item();
        }
    }

    return items_found;
}

static void _fuzz_detect_creatures(int pow, int *fuzz_radius, int *fuzz_chance)
{
    dprf("dc_fuzz: Power is %d", pow);

    pow = max(1, pow);

    *fuzz_radius = pow >= 50 ? 1 : 2;

    // Fuzz chance starts off at 100% and declines to a low of 10% for
    // obscenely powerful castings (pow caps around the 60 mark).
    *fuzz_chance = 100 - 90 * (pow - 1) / 59;
    if (*fuzz_chance < 10)
        *fuzz_chance = 10;
}

static bool _mark_detected_creature(coord_def where, monster* mon,
                                    int fuzz_chance, int fuzz_radius)
{
    bool found_good = false;

    if (fuzz_radius && x_chance_in_y(fuzz_chance, 100))
    {
        const int fuzz_diam = 2 * fuzz_radius + 1;

        coord_def place;
        // Try five times to find a valid placement, else we attempt to place
        // the monster where it really is (and may fail).
        for (int itry = 0; itry < 5; ++itry)
        {
            place.set(where.x + random2(fuzz_diam) - fuzz_radius,
                      where.y + random2(fuzz_diam) - fuzz_radius);

            if (!map_bounds(place))
                continue;

            // If the player would be able to see a monster at this location
            // don't place it there.
            if (you.see_cell(place))
                continue;

            // Try not to overwrite another detected monster.
            if (env.map_knowledge(place).detected_monster())
                continue;

            // Don't print monsters on terrain they cannot pass through,
            // not even if said terrain has since changed.
            if (!env.map_knowledge(place).changed()
                && mon->can_pass_through_feat(grd(place)))
            {
                found_good = true;
                break;
            }
        }

        if (found_good)
            where = place;
    }

    env.map_knowledge(where).set_detected_monster(mons_detected_base(mon->type));

    return found_good;
}

int detect_creatures(int pow, bool telepathic)
{
    int fuzz_radius = 0, fuzz_chance = 0;
    if (!telepathic)
        _fuzz_detect_creatures(pow, &fuzz_radius, &fuzz_chance);

    int creatures_found = 0;
    const int map_radius = 10 + random2(8) + pow;

    // Clear the map so detect creatures is more useful and the detection
    // fuzz is harder to analyse by averaging.
    clear_map(false);

    for (radius_iterator ri(you.pos(), map_radius, C_ROUND); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            // If you can see the monster, don't "detect" it elsewhere.
            if (!mons_near(mon) || !mon->visible_to(&you))
            {
                creatures_found++;
                _mark_detected_creature(*ri, mon, fuzz_chance, fuzz_radius);
            }
        }
    }

    return creatures_found;
}

static bool _selectively_remove_curse(const string &pre_msg)
{
    bool used = false;

    while (1)
    {
        if (!any_items_of_type(OSEL_CURSED_WORN) && used)
        {
            mpr("You have uncursed all your worn items.");
            return used;
        }

        int item_slot = prompt_invent_item("Uncurse which item?", MT_INVLIST,
                                           OSEL_CURSED_WORN, true, true, false);
        if (prompt_failed(item_slot))
            return used;

        item_def& item(you.inv[item_slot]);

        if (!item.cursed()
            || !item_is_equipped(item)
            || &item == you.weapon() && !is_weapon(item))
        {
            mpr("Choose a cursed equipped item, or Esc to abort.");
            more();
            continue;
        }

        if (!used && !pre_msg.empty())
            mpr(pre_msg);

        do_uncurse_item(item, true, false, false);
        used = true;
    }
}

bool remove_curse(bool alreadyknown, const string &pre_msg)
{
    if (you_worship(GOD_ASHENZARI) && alreadyknown)
    {
        if (_selectively_remove_curse(pre_msg))
        {
            ash_check_bondage();
            return true;
        }
        else
            return false;
    }

    bool success = false;

    // Only cursed *weapons* in hand count as cursed. - bwr
    // Not you.weapon() because we want to handle melded weapons too.
    item_def * const weapon = you.slot_item(EQ_WEAPON, true);
    if (weapon && is_weapon(*weapon) && weapon->cursed())
    {
        // Also sets wield_change.
        do_uncurse_item(*weapon);
        success = true;
    }

    // Everything else uses the same paradigm - are we certain?
    // What of artefact rings and amulets? {dlb}:
    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; i++)
    {
        // Melded equipment can also get uncursed this way.
        if (you.equip[i] != -1 && you.inv[you.equip[i]].cursed())
        {
            do_uncurse_item(you.inv[you.equip[i]]);
            success = true;
        }
    }

    if (success)
    {
        if (!pre_msg.empty())
            mpr(pre_msg);
        mpr("You feel as if something is helping you.");
        learned_something_new(HINT_REMOVED_CURSE);
    }
    else if (alreadyknown)
        mprf(MSGCH_PROMPT, "None of your equipped items are cursed.");
    else
    {
        if (!pre_msg.empty())
            mpr(pre_msg);
        mpr("You feel blessed for a moment.");
    }

    return success;
}

static bool _selectively_curse_item(bool armour, const string &pre_msg)
{
    while (1)
    {
        int item_slot = prompt_invent_item("Curse which item?", MT_INVLIST,
                                           armour ? OSEL_UNCURSED_WORN_ARMOUR
                                                  : OSEL_UNCURSED_WORN_JEWELLERY,
                                           true, true, false);
        if (prompt_failed(item_slot))
            return false;

        item_def& item(you.inv[item_slot]);

        if (item.cursed()
            || !item_is_equipped(item)
            || armour && item.base_type != OBJ_ARMOUR
            || !armour && item.base_type != OBJ_JEWELLERY)
        {
            mprf("Choose an uncursed equipped piece of %s, or Esc to abort.",
                 armour ? "armour" : "jewellery");
            more();
            continue;
        }

        if (!pre_msg.empty())
            mpr(pre_msg);
        do_curse_item(item, false);
        learned_something_new(HINT_YOU_CURSED);
        return true;
    }
}

bool curse_item(bool armour, const string &pre_msg)
{
    // Make sure there's something to curse first.
    bool found = false;
    int min_type, max_type;
    if (armour)
        min_type = EQ_MIN_ARMOUR, max_type = EQ_MAX_ARMOUR;
    else
        min_type = EQ_LEFT_RING, max_type = EQ_RING_AMULET;
    for (int i = min_type; i <= max_type; i++)
    {
        if (you.equip[i] != -1 && !you.inv[you.equip[i]].cursed())
            found = true;
    }
    if (!found)
    {
        mprf(MSGCH_PROMPT, "You aren't wearing any piece of uncursed %s.",
             armour ? "armour" : "jewellery");
        return false;
    }

    return _selectively_curse_item(armour, pre_msg);
}

static bool _do_imprison(int pow, const coord_def& where, bool zin)
{
    // power guidelines:
    // powc is roughly 50 at Evoc 10 with no godly assistance, ranging
    // up to 300 or so with godly assistance or end-level, and 1200
    // as more or less the theoretical maximum.
    int number_built = 0;

    static const set<dungeon_feature_type> safe_tiles =
    {
        DNGN_SHALLOW_WATER, DNGN_DEEP_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR
    };

    bool proceed;
    monster *mon;
    string targname;

    if (zin)
    {
        // We need to get this now because we won't be able to see
        // the monster once the walls go up!
        mon = monster_at(where);
        targname = mon->name(DESC_THE);
        bool success = true;
        bool none_vis = true;

        vector<coord_def> veto_spots(8);
        for (adjacent_iterator ai(where); ai; ++ai)
            veto_spots.push_back(*ai);

        // Check that any adjacent creatures can be pushed out of the way.
        for (adjacent_iterator ai(where); ai; ++ai)
        {
            // The tile is occupied.
            if (actor *act = actor_at(*ai))
            {
                // Can't push ourselves.
                coord_def newpos;
                if (act->is_player()
                    || !get_push_space(*ai, newpos, act, true, &veto_spots))
                {
                    success = false;
                    if (you.can_see(act))
                        none_vis = false;
                    break;
                }
                else
                    veto_spots.push_back(newpos);
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            if (cell_is_solid(*ai) && !feat_is_opaque(grd(*ai)))
            {
                success = false;
                none_vis = false;
                break;
            }
        }

        if (!success)
        {
            mprf(none_vis ? "You briefly glimpse something next to %s."
                        : "You need more space to imprison %s.",
                targname.c_str());
            return false;
        }
    }

    for (adjacent_iterator ai(where); ai; ++ai)
    {
        // This is where power comes in.
        if (!zin && one_chance_in(pow / 3))
            continue;

        // The tile is occupied.
        if (zin)
        {
            if (actor* act = actor_at(*ai))
            {
                coord_def newpos;
                get_push_space(*ai, newpos, act, true);
                act->move_to_pos(newpos);
            }
        }

        // Make sure we have a legitimate tile.
        proceed = false;
        if (!zin && !monster_at(*ai))
        {
            if (feat_is_trap(grd(*ai), true) || feat_is_stone_stair(grd(*ai))
                || safe_tiles.count(grd(*ai)))
            {
                proceed = true;
            }
        }
        else if (zin && !cell_is_solid(*ai))
            proceed = true;

        if (proceed)
        {
            // All items are moved aside.
            if (igrd(*ai) != NON_ITEM)
            {
                coord_def newpos;
                get_push_space(*ai, newpos, nullptr, true);
                move_items(*ai, newpos);
            }

            // All clouds are destroyed.
            if (env.cgrid(*ai) != EMPTY_CLOUD)
                delete_cloud(env.cgrid(*ai));

            // All traps are destroyed.
            if (trap_def *ptrap = find_trap(*ai))
            {
                ptrap->destroy();
                grd(*ai) = DNGN_FLOOR;
            }

            // Actually place the wall.
            if (zin)
            {
                map_wiz_props_marker *marker = new map_wiz_props_marker(*ai);
                marker->set_property("feature_description", "a gleaming silver wall");
                env.markers.add(marker);

                temp_change_terrain(*ai, DNGN_METAL_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_IMPRISON);

                // Make the walls silver.
                env.grid_colours(*ai) = WHITE;
                env.tile_flv(*ai).feat_idx =
                        store_tilename_get_index("dngn_silver_wall");
                env.tile_flv(*ai).feat = TILE_DNGN_SILVER_WALL;
                if (env.map_knowledge(*ai).seen())
                {
                    env.map_knowledge(*ai).set_feature(DNGN_METAL_WALL);
                    env.map_knowledge(*ai).clear_item();
#ifdef USE_TILE
                    env.tile_bk_bg(*ai) = TILE_DNGN_SILVER_WALL;
                    env.tile_bk_fg(*ai) = 0;
#endif
                }
            }
            // Tomb card
            else
            {
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB);
            }

            number_built++;
        }
    }

    if (number_built > 0)
    {
        if (zin)
        {
            mprf("Zin imprisons %s with walls of pure silver!",
                 targname.c_str());
        }
        else
            mpr("Walls emerge from the floor!");

        you.update_beholders();
        you.update_fearmongers();
        env.markers.clear_need_activate();
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return number_built > 0;
}

bool entomb(int pow)
{
    // Zotdef - turned off
    if (crawl_state.game_is_zotdef())
    {
        mpr("The dungeon rumbles ominously, and rocks fall from the ceiling!");
        return false;
    }
    if (_do_imprison(pow, you.pos(), false))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(you.pos(),
                                            tomb_duration,
                                            you.mindex(),
                                            you.mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_imprison(int pow, monster* mons, int source)
{
    if (_do_imprison(pow, mons->pos(), true))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(mons->pos(),
                                            tomb_duration,
                                            source,
                                            mons->mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_smiting(int pow, monster* mons)
{
    if (mons == nullptr || mons->submerged())
    {
        canned_msg(MSG_NOTHING_THERE);
        // Counts as a real cast, due to invisible/submerged monsters.
        return true;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(mons, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, mons);

        mprf("You smite %s!", mons->name(DESC_THE).c_str());

        behaviour_event(mons, ME_ANNOY, &you);
    }

    enable_attack_conducts(conducts);

    if (success)
    {
        // Maxes out at around 40 damage at 27 Invocations, which is
        // plenty in my book (the old max damage was around 70, which
        // seems excessive).
        mons->hurt(&you, 7 + (random2(pow) * 33 / 191));
        if (mons->alive())
            print_wounds(mons);
    }

    return success;
}

static void _holy_word_player(int pow, holy_word_source_type source, actor *attacker)
{
    if (!you.undead_or_demonic())
        return;

    int hploss;

    // Holy word won't kill its user.
    if (attacker && attacker->is_player())
        hploss = max(0, you.hp / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 3);

    if (!hploss)
        return;

    mpr("You are blasted by holy energy!");

    const char *aux = "holy word";

    kill_method_type type = KILLED_BY_SOMETHING;
    if (crawl_state.is_god_acting())
        type = KILLED_BY_DIVINE_WRATH;

    switch (source)
    {
    case HOLY_WORD_SCROLL:
        aux = "scroll of holy word";
        break;

    case HOLY_WORD_ZIN:
        aux = "Zin's holy word";
        break;

    case HOLY_WORD_TSO:
        aux = "the Shining One's holy word";
        break;
    }

    ouch(hploss, type, MID_NOBODY, aux);

    return;
}

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker)
{
    pow = min(300, pow);

    // Is the player in this cell?
    if (where == you.pos())
        _holy_word_player(pow, source, attacker);

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (!mons || !mons->alive() || !mons->undead_or_demonic())
        return;

    int hploss;

    // Holy word won't kill its user.
    if (attacker == mons)
        hploss = max(0, mons->hit_points / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 10);

    if (hploss)
    {
        if (source == HOLY_WORD_ZIN)
            simple_monster_message(mons, " is blasted by Zin's holy word!");
        else
            simple_monster_message(mons, " convulses!");
    }
    mons->hurt(attacker, hploss, BEAM_MISSILE);

    if (!hploss || !mons->alive())
        return;
    // Holy word won't annoy or stun its user.
    if (attacker != mons)
    {
        // Currently, holy word annoys the monsters it affects
        // because it can kill them, and because hostile
        // monsters don't use it.
        // Tolerate unknown scroll, to not annoy Yred worshippers too much.
        if (attacker != nullptr
            && (attacker != &you
                || source != HOLY_WORD_SCROLL
                || item_type_known(OBJ_SCROLLS, SCR_HOLY_WORD)))
        {
            behaviour_event(mons, ME_ANNOY, attacker);
        }

        if (mons->speed_increment >= 25)
            mons->speed_increment -= 20;
    }
}

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent, actor *attacker)
{
    if (!silent && attacker)
    {
        mprf("%s %s a Word of immense power!",
             attacker->name(DESC_THE).c_str(),
             attacker->conj_verb("speak").c_str());
    }

    for (radius_iterator ri(where, LOS_SOLID); ri; ++ri)
        holy_word_monsters(*ri, pow, source, attacker);
}

void torment_player(actor *attacker, torment_source_type taux)
{
    ASSERT(!crawl_state.game_is_arena());

    int hploss = 0;

    if (!player_res_torment())
    {
        // Negative energy resistance can alleviate torment.
        hploss = max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
        // Statue form is only partial petrification.
        if (you.form == TRAN_STATUE || you.species == SP_GARGOYLE)
            hploss /= 2;
    }

    // Kiku protects you from torment to a degree.
    const bool kiku_shielding_player = player_kiku_res_torment();

    if (kiku_shielding_player)
    {
        if (hploss > 0)
        {
            if (random2(600) < you.piety) // 13.33% to 33.33% chance
            {
                hploss = 0;
                simple_god_message(" shields you from torment!");
            }
            else if (random2(250) < you.piety) // 24% to 80% chance
            {
                hploss -= random2(hploss - 1);
                simple_god_message(" partially shields you from torment!");
            }
        }
    }

    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return;
    }

    mpr("Your body is wracked with pain!");


    kill_method_type type = KILLED_BY_BEAM;
    if (crawl_state.is_god_acting())
        type = KILLED_BY_DIVINE_WRATH;
    else if (taux == TORMENT_MISCAST)
        type = KILLED_BY_WILD_MAGIC;

    const char *aux = "";

    switch (taux)
    {
    case TORMENT_CARDS:
    case TORMENT_SPELL:
        aux = "Symbol of Torment";
        break;

    case TORMENT_SCEPTRE:
        aux = "Sceptre of Torment";
        break;

    case TORMENT_SCROLL:
        aux = "a scroll of torment";
        break;

    case TORMENT_XOM:
        type = KILLED_BY_XOM;
        aux = "Xom's torment";
        break;

    case TORMENT_KIKUBAAQUDGHA:
        aux = "Kikubaaqudgha's torment";
        break;

    case TORMENT_LURKING_HORROR:
        type = KILLED_BY_SPORE;
        aux = "an exploding lurking horror";

    case TORMENT_MISCAST:
        aux = "by torment";
        break;
    }

    ouch(hploss, type, attacker? attacker->mid : MID_NOBODY, aux);

    return;
}

void torment_cell(coord_def where, actor *attacker, torment_source_type taux)
{
    // Is the player in this cell?
    if (where == you.pos())
        torment_player(attacker, taux);
    // Don't return, since you could be standing on a monster.

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (!mons || !mons->alive() || mons->res_torment())
        return;

    int hploss = max(0, mons->hit_points * (50 - mons->res_negative_energy() * 5) / 100 - 1);

    if (hploss)
    {
        simple_monster_message(mons, " convulses!");

        // Currently, torment doesn't annoy the monsters it affects
        // because it can't kill them, and because hostile monsters use
        // it.  It does alert them, though.
        // XXX: attacker isn't passed through "int torment()".
        behaviour_event(mons, ME_ALERT, attacker);
    }

    mons->hurt(attacker, hploss, BEAM_TORMENT_DAMAGE);
}

void torment(actor *attacker, torment_source_type taux, const coord_def& where)
{
    for (radius_iterator ri(where, LOS_NO_TRANS); ri; ++ri)
        torment_cell(*ri, attacker, taux);
}

void setup_cleansing_flame_beam(bolt &beam, int pow, int caster,
                                coord_def where, actor *attacker)
{
    beam.flavour      = BEAM_HOLY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(2, pow);
    beam.target       = where;
    beam.name         = "golden flame";
    beam.colour       = YELLOW;
    beam.aux_source   = (caster == CLEANSING_FLAME_TSO)
                        ? "the Shining One's cleansing flame"
                        : "cleansing flame";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    if (caster == CLEANSING_FLAME_GENERIC || caster == CLEANSING_FLAME_TSO)
    {
        beam.thrower   = KILL_MISC;
        beam.source_id = MID_NOBODY;
    }
    else if (attacker && attacker->is_player())
    {
        beam.thrower   = KILL_YOU;
        beam.source_id = MID_PLAYER;
    }
    else
    {
        // If there was no attacker, caster should have been
        // CLEANSING_FLAME_{GENERIC,TSO} which we handled above.
        ASSERT(attacker);

        beam.thrower   = KILL_MON;
        beam.source_id = attacker->mid;
    }
}

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker)
{
    bolt beam;
    setup_cleansing_flame_beam(beam, pow, caster, where, attacker);
    beam.explode();
}

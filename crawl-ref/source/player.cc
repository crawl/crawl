/**
 * @file
 * @brief Player related functions.
**/

#include "AppHdr.h"

#include "player.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include <sstream>
#include <algorithm>

#include "areas.h"
#include "art-enum.h"
#include "branch.h"
#ifdef DGL_WHEREIS
 #include "chardump.h"
#endif
#include "cloud.h"
#include "clua.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "errors.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "godpassive.h"
#include "godwrath.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "map_knowledge.h"
#include "melee_attack.h"
#include "message.h"
#include "misc.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mon-iter.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player-stats.h"
#include "potion.h"
#include "quiver.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "skills2.h"
#include "species.h"
#include "spl-damage.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "status.h"
#include "stuff.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"
#include "xom.h"

static void _moveto_maybe_repel_stairs()
{
    const dungeon_feature_type new_grid = env.grid(you.pos());
    const command_type stair_dir = feat_stair_direction(new_grid);

    if (stair_dir == CMD_NO_CMD
        || new_grid == DNGN_ENTER_SHOP
        ||  !you.duration[DUR_REPEL_STAIRS_MOVE])
    {
        return;
    }

    int pct = you.duration[DUR_REPEL_STAIRS_CLIMB] ? 29 : 50;

    // When the effect is still strong, the chance to actually catch
    // a stair is smaller. (Assuming the duration starts out at 1000.)
    const int dur = max(0, you.duration[DUR_REPEL_STAIRS_MOVE] - 700);
    pct += dur/10;

    if (x_chance_in_y(pct, 100))
    {
        if (slide_feature_over(you.pos(), coord_def(-1, -1), false))
        {
            string stair_str = feature_description_at(you.pos(), false,
                                                      DESC_THE, false);
            string prep = feat_preposition(new_grid, true, &you);

            mprf("%s slides away as you move %s it!", stair_str.c_str(),
                 prep.c_str());

            if (player_in_a_dangerous_place() && one_chance_in(5))
                xom_is_stimulated(25);
        }
    }
}

static bool _check_moveto_cloud(const coord_def& p, const string &move_verb)
{
    const int cloud = env.cgrid(p);
    if (cloud != EMPTY_CLOUD && !you.confused())
    {
        const cloud_type ctype = env.cloud[ cloud ].type;
        // Don't prompt if already in a cloud of the same type.
        if (is_damaging_cloud(ctype, true)
            && (env.cgrid(you.pos()) == EMPTY_CLOUD
                || ctype != env.cloud[ env.cgrid(you.pos()) ].type)
            && !crawl_state.disables[DIS_CONFIRMATIONS])
        {
            // Don't prompt for steam unless we're at uncomfortably low hp.
            if (ctype == CLOUD_STEAM)
            {
                int threshold = 20;
                if (player_res_steam() < 0)
                    threshold = threshold * 3 / 2;
                threshold = threshold * you.time_taken / BASELINE_DELAY;
                // Do prompt if we'd lose icemail, though.
                if (you.hp > threshold && !you.mutation[MUT_ICEMAIL])
                    return true;
            }

            string prompt = make_stringf("Really %s into that cloud of %s?",
                                         move_verb.c_str(),
                                         cloud_name_at_index(cloud).c_str());
            learned_something_new(HINT_CLOUD_WARNING);

            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }
    }
    return true;
}

static bool _check_moveto_trap(const coord_def& p, const string &move_verb)
{
    // If there's no trap, let's go.
    trap_def* trap = find_trap(p);
    if (!trap || env.grid(p) == DNGN_UNDISCOVERED_TRAP)
        return true;

    if (trap->type == TRAP_ZOT && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt = make_stringf(
            "Do you really want to %s into the Zot trap",
            move_verb.c_str());

        if (!yes_or_no("%s", prompt.c_str()))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    else if (!trap->is_safe() && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt = make_stringf(
            "Really %s %s that %s?",
            move_verb.c_str(),
            (trap->type == TRAP_ALARM || trap->type == TRAP_PLATE) ? "onto"
                                                                   : "into",
            feature_description_at(p, false, DESC_BASENAME, false).c_str());

        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

static bool _check_moveto_dangerous(const coord_def& p, const string& msg,
                                    bool cling = true)
{
    if (you.can_swim() && feat_is_water(env.grid(p))
        || you.airborne() || cling && you.can_cling_to(p)
        || !is_feat_dangerous(env.grid(p)))
    {
        return true;
    }

    if (msg != "")
        mpr(msg.c_str());
    else if (you.species == SP_MERFOLK && feat_is_water(env.grid(p)))
        mpr("You cannot swim in your current form.");
    else if (you.species == SP_LAVA_ORC && feat_is_lava(env.grid(p))
             && is_feat_dangerous(env.grid(p)))
    {
        mpr("You cannot enter lava in your current form.");
    }
    else
        canned_msg(MSG_UNTHINKING_ACT);

    return false;
}

static bool _check_moveto_terrain(const coord_def& p, const string &move_verb,
                                  const string &msg)
{
    if (you.is_wall_clinging()
        && (move_verb == "blink" || move_verb == "passwall"))
    {
        return _check_moveto_dangerous(p, msg, false);
    }

    if (!need_expiration_warning() && need_expiration_warning(p)
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        if (!_check_moveto_dangerous(p, msg))
            return false;

        string prompt;

        if (msg != "")
            prompt = msg + " ";

        prompt += "Are you sure you want to " + move_verb;

        if (you.ground_level())
            prompt += " into ";
        else
            prompt += " over ";

        prompt += env.grid(p) == DNGN_DEEP_WATER ? "deep water" : "lava";

        prompt += need_expiration_warning(DUR_FLIGHT, p)
                      ? " while you are losing your buoyancy?"
                      : " while your transformation is expiring?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    return _check_moveto_dangerous(p, msg);
}

static bool _check_moveto_exclusion(const coord_def& p, const string &move_verb)
{
    if (is_excluded(p)
        && !is_stair_exclusion(p)
        && !is_excluded(you.pos())
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt = make_stringf("Really %s into a travel-excluded area?",
                                     move_verb.c_str());

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

bool check_moveto(const coord_def& p, const string &move_verb,
                  const string &msg)
{
    return (_check_moveto_terrain(p, move_verb, msg)
            && _check_moveto_cloud(p, move_verb)
            && _check_moveto_trap(p, move_verb)
            && _check_moveto_exclusion(p, move_verb));
}

static void _splash()
{
    if (you.can_swim())
        noisy(4, you.pos(), "Floosh!");
    else if (!beogh_water_walk())
        noisy(8, you.pos(), "Splash!");
}

void moveto_location_effects(dungeon_feature_type old_feat,
                             bool stepped, bool allow_shift,
                             const coord_def& old_pos)
{
    const dungeon_feature_type new_grid = env.grid(you.pos());

    // Terrain effects.
    if (is_feat_dangerous(new_grid) && (!you.is_wall_clinging()
                                        || !cell_is_clingable(you.pos())))
    {
        // Lava and dangerous deep water (ie not merfolk).
        const coord_def& entry = (stepped) ? old_pos : you.pos();

        // If true, we were shifted and so we're done.
        if (fall_into_a_pool(entry, allow_shift, new_grid))
            return;
    }

    if (you.ground_level())
    {
        if (player_likes_lava(false))
        {
            if (feat_is_lava(new_grid) && !feat_is_lava(old_feat))
            {
                if (!stepped)
                    noisy(4, you.pos(), "Gloop!");

                mprf("You %s lava.",
                     (stepped) ? "slowly immerse yourself in the" : "fall into the");

                // Extra time if you stepped in.
                if (stepped)
                    you.time_taken *= 2;

                // This gets called here because otherwise you wouldn't heat
                // until your second turn in lava.
                if (temperature() < TEMP_FIRE)
                    mpr("The lava instantly superheats you.");
                you.temperature = TEMP_MAX;
            }

            else if (!feat_is_lava(new_grid) && feat_is_lava(old_feat))
            {
                mpr("You slowly pull yourself out of the lava.");
                you.time_taken *= 2;
            }
        }

        if (you.species == SP_MERFOLK)
        {
            if (feat_is_water(new_grid) // We're entering water
                // We're not transformed, or with a form compatible with tail
                && (you.form == TRAN_NONE
                    || you.form == TRAN_APPENDAGE
                    || you.form == TRAN_BLADE_HANDS))
            {
                merfolk_start_swimming(stepped);
            }
            else if (!feat_is_water(new_grid) && !is_feat_dangerous(new_grid))
                merfolk_stop_swimming();
        }

        if (feat_is_water(new_grid) && !stepped)
            _splash();

        if (feat_is_water(new_grid) && !you.can_swim() && !beogh_water_walk())
        {
            if (stepped)
            {
                you.time_taken *= 13 + random2(8);
                you.time_taken /= 10;
            }
            const bool will_cling = you.can_cling_to_walls()
                                    && cell_is_clingable(you.pos());

            if (!feat_is_water(old_feat))
            {
                if (stepped && will_cling)
                {
                    mpr("You slowly cross the shallow water and cling to the "
                        "wall.");
                }
                else
                {
                    mprf("You %s the %s water.",
                         stepped ? "enter" : "fall into",
                         new_grid == DNGN_SHALLOW_WATER ? "shallow" : "deep");
                }
            }

            if (new_grid == DNGN_DEEP_WATER && old_feat != DNGN_DEEP_WATER)
                mpr("You sink to the bottom.");

            if (!feat_is_water(old_feat) && !will_cling)
            {
                mpr("Moving in this stuff is going to be slow.");
                if (you.invisible())
                    mpr("...and don't expect to remain undetected.");
            }
        }
    }
    else if (you.species == SP_DJINNI && !feat_has_dry_floor(new_grid)
             && feat_has_dry_floor(old_feat))
    {
        mprf("You heave yourself high above the %s.", feat_type_name(new_grid));
    }

    const bool was_clinging = you.is_wall_clinging();
    const bool is_clinging = stepped && you.check_clinging(stepped);

    if (feat_is_water(new_grid) && was_clinging && !is_clinging)
        _splash();

    // Traps go off.
    if (trap_def* ptrap = find_trap(you.pos()))
        ptrap->trigger(you, !stepped); // blinking makes it hard to evade

    if (stepped)
        _moveto_maybe_repel_stairs();
}

// Use this function whenever the player enters (or lands and thus re-enters)
// a grid.
//
// stepped     - normal walking moves
// allow_shift - allowed to scramble in any direction out of lava/water
void move_player_to_grid(const coord_def& p, bool stepped, bool allow_shift)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT_IN_BOUNDS(p);

    if (!stepped)
    {
        you.clear_clinging();
        tornado_move(p);
    }

    // assuming that entering the same square means coming from above (flight)
    const coord_def old_pos = you.pos();
    const bool from_above = (old_pos == p);
    const dungeon_feature_type old_grid =
        (from_above) ? DNGN_FLOOR : grd(old_pos);

    // Really must be clear.
    ASSERT(you.can_pass_through_feat(grd(p)));

    // Better not be an unsubmerged monster either.
    ASSERT(!monster_at(p) || monster_at(p)->submerged()
           || fedhas_passthrough(monster_at(p)));

    // Move the player to new location.
    you.moveto(p, true);
    viewwindow();

    moveto_location_effects(old_grid, stepped, allow_shift, old_pos);
}

bool is_feat_dangerous(dungeon_feature_type grid, bool permanently,
                       bool ignore_items)
{
    if (you.permanent_flight() || you.species == SP_DJINNI
        || you.airborne() && !permanently)
    {
        return false;
    }
    else if (grid == DNGN_DEEP_WATER && !player_likes_water(permanently)
             || grid == DNGN_LAVA && !player_likes_lava(permanently))
    {
        return true;
    }
    else
        return false;
}

bool is_map_persistent(void)
{
    return !testbits(env.level_flags, LFLAG_NO_MAP);
}

bool player_in_hell(void)
{
    return is_hell_subbranch(you.where_are_you);
}

bool player_in_connected_branch(void)
{
    return is_connected_branch(you.where_are_you);
}

bool player_likes_water(bool permanently)
{
    return (!permanently && beogh_water_walk()
            || (species_likes_water(you.species) || !permanently)
                && form_likes_water());
}

bool player_likes_lava(bool permanently)
{
    return (species_likes_lava(you.species)
            || (!permanently && form_likes_lava()));
}

bool player_can_open_doors()
{
    return (you.form != TRAN_BAT && you.form != TRAN_JELLY);
}

// TODO: get rid of this.
bool player_genus(genus_type which_genus, species_type species)
{
    if (species == SP_UNKNOWN)
        species = you.species;

    return (species_genus(species) == which_genus);
}

// If transform is true, compare with current transformation instead
// of (or in addition to) underlying species.
// (See mon-data.h for species/genus use.)
bool is_player_same_genus(const monster_type mon, bool transform)
{
    if (transform)
    {
        switch (you.form)
        {
        // Unique monsters.
        case TRAN_BAT:
            return (mon == MONS_BAT);
        case TRAN_ICE_BEAST:
            return (mon == MONS_ICE_BEAST);
        case TRAN_TREE:
            return (mon == MONS_ANIMATED_TREE);
        case TRAN_PORCUPINE:
            return (mon == MONS_PORCUPINE);
        case TRAN_WISP:
            return (mon == MONS_INSUBSTANTIAL_WISP);
        // Compare with monster *species*.
        case TRAN_LICH:
            return (mons_species(mon) == MONS_LICH);
        // Compare with monster *genus*.
        case TRAN_FUNGUS:
            return (mons_genus(mon) == MONS_FUNGUS);
        case TRAN_SPIDER:
            return (mons_genus(mon) == MONS_SPIDER);
        case TRAN_DRAGON:
            return (mons_genus(mon) == MONS_DRAGON); // Includes all drakes.
        case TRAN_PIG:
            return (mons_genus(mon) == MONS_HOG);
        case TRAN_JELLY:
            return (mons_genus(mon) == MONS_JELLY);
        case TRAN_STATUE:
        case TRAN_BLADE_HANDS:
        case TRAN_NONE:
        case TRAN_APPENDAGE:
            break; // Check real (non-transformed) form.
        }
    }

    // Genus would include necrophage and rotting hulk.
    if (you.species == SP_GHOUL)
        return (mons_species(mon) == MONS_GHOUL);

    if (you.species == SP_MERFOLK && mons_genus(mon) == MONS_MERMAID)
        return true;

    // Note that these are currently considered to be the same genus:
    // * humans, demigods, and demonspawn
    // * ogres and two-headed ogres
    // * trolls, iron trolls, and deep trolls
    // * kobolds and big kobolds
    // * dwarves and deep dwarves
    // * all elf races
    // * all orc races
    return (mons_genus(mon) == mons_genus(player_mons(false)));
}

void update_player_symbol()
{
    you.symbol = Options.show_player_species ? player_mons() : transform_mons();
}

monster_type player_mons(bool transform)
{
    monster_type mons;

    if (transform)
    {
        mons = transform_mons();
        if (mons != MONS_PLAYER)
            return mons;
    }

    mons = player_species_to_mons_species(you.species);

    if (mons == MONS_ORC)
    {
        if (you_worship(GOD_BEOGH))
            mons = (you.piety >= piety_breakpoint(4)) ? MONS_ORC_HIGH_PRIEST
                                                      : MONS_ORC_PRIEST;
    }
    else if (mons == MONS_OGRE)
    {
        const skill_type sk = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
        if (sk >= SK_SPELLCASTING && sk < SK_INVOCATIONS)
            mons = MONS_OGRE_MAGE;
    }

    return mons;
}

void update_vision_range()
{
    you.normal_vision = LOS_RADIUS;
    int nom   = 1;
    int denom = 1;

    // Nightstalker gives -1/-2/-3.
    if (player_mutation_level(MUT_NIGHTSTALKER))
    {
        nom *= LOS_RADIUS - player_mutation_level(MUT_NIGHTSTALKER);
        denom *= LOS_RADIUS;
    }

    // Lantern of shadows.
    if (you.attribute[ATTR_SHADOWS])
        nom *= 3, denom *= 4;

    // the Darkness spell.
    if (you.duration[DUR_DARKNESS])
        nom *= 3, denom *= 4;

    // robe of Night.
    if (player_equip_unrand(UNRAND_NIGHT))
        nom *= 3, denom *= 4;

    you.current_vision = (you.normal_vision * nom + denom / 2) / denom;
    ASSERT(you.current_vision > 0);
    set_los_radius(you.current_vision);
}

// Checks whether the player's current species can
// use (usually wear) a given piece of equipment.
// Note that EQ_BODY_ARMOUR and EQ_HELMET only check
// the ill-fitting variant (i.e., not caps and robes).
// If special_armour is set to true, special cases
// such as bardings, light armour and caps are
// considered. Otherwise, these simply return false.
// ---------------------------------------------------
bool you_can_wear(int eq, bool special_armour)
{
    if (you.species == SP_FELID)
        return (eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING || eq == EQ_AMULET);

    // Octopodes can wear soft helmets, eight rings, and an amulet.
    if (you.species == SP_OCTOPODE)
    {
        if (special_armour && eq == EQ_HELMET)
            return true;
        else
            return (eq >= EQ_RING_ONE && eq <= EQ_RING_EIGHT
                    || eq == EQ_AMULET || eq == EQ_SHIELD || eq == EQ_WEAPON);
    }

    switch (eq)
    {
    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_AMULET:
    case EQ_CLOAK:
        return true;

    case EQ_GLOVES:
        if (player_mutation_level(MUT_CLAWS, false) == 3)
            return false;
        // These species cannot wear gloves.
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE)
        {
            return false;
        }
        return true;

    case EQ_BOOTS:
        // Bardings.
        if (you.species == SP_NAGA || you.species == SP_CENTAUR)
            return special_armour;
        if (player_mutation_level(MUT_HOOVES, false) == 3
            || player_mutation_level(MUT_TALONS, false) == 3)
        {
            return false;
        }
        // These species cannot wear boots.
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE
            || you.species == SP_DJINNI)
        {
            return false;
        }
        return true;

    case EQ_BODY_ARMOUR:
        if (player_genus(GENPC_DRACONIAN))
            return false;

    case EQ_SHIELD:
        // Most races can wear robes or a buckler/shield.
        if (special_armour)
            return true;
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE)
        {
            return false;
        }
        return true;

    case EQ_HELMET:
        // No caps or hats with Horns 3 or Antennae 3.
        if (player_mutation_level(MUT_HORNS, false) == 3
            || player_mutation_level(MUT_ANTENNAE, false) == 3)
        {
            return false;
        }
        // Anyone else can wear caps.
        if (special_armour)
            return true;
        if (player_mutation_level(MUT_HORNS, false)
            || player_mutation_level(MUT_BEAK, false)
            || player_mutation_level(MUT_ANTENNAE, false))
        {
            return false;
        }
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE
            || player_genus(GENPC_DRACONIAN))
        {
            return false;
        }
        return true;

    default:
        return true;
    }
}

bool player_has_feet(bool temp)
{
    if (you.species == SP_NAGA
        || you.species == SP_FELID
        || you.species == SP_OCTOPODE
        || you.species == SP_DJINNI
        || you.fishtail && temp)
    {
        return false;
    }

    if (player_mutation_level(MUT_HOOVES, temp) == 3
        || player_mutation_level(MUT_TALONS, temp) == 3)
    {
        return false;
    }

    return true;
}

bool player_wearing_slot(int eq)
{
    ASSERT(you.equip[eq] != -1 || !you.melded[eq]);
    return (you.equip[eq] != -1 && !you.melded[eq]);
}

bool you_tran_can_wear(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return you_tran_can_wear(EQ_WEAPON);

    case OBJ_JEWELLERY:
        return you_tran_can_wear(jewellery_is_amulet(item) ? EQ_AMULET
                                                           : EQ_RINGS);
    case OBJ_ARMOUR:
        if (item.sub_type == ARM_NAGA_BARDING)
            return (you.species == SP_NAGA && you_tran_can_wear(EQ_BOOTS));
        else if (item.sub_type == ARM_CENTAUR_BARDING)
            return (you.species == SP_CENTAUR && you_tran_can_wear(EQ_BOOTS));

        if (fit_armour_size(item, you.body_size()) != 0)
            return false;

        return you_tran_can_wear(get_armour_slot(item), true);

    default:
        return true;
    }
}

bool you_tran_can_wear(int eq, bool check_mutation)
{
    if (eq == EQ_NONE)
        return true;

    if (you.form == TRAN_JELLY
        || you.form == TRAN_PORCUPINE
        || you.form == TRAN_WISP)
    {
        return false;
    }

    if (eq == EQ_STAFF)
        eq = EQ_WEAPON;
    else if (eq >= EQ_RINGS && eq <= EQ_RINGS_PLUS2)
        eq = EQ_RINGS;

    // Everybody but jellies and porcupines can wear at least some type of armour.
    if (eq == EQ_ALL_ARMOUR)
        return true;

    // Not a transformation, but also temporary -> check first.
    if (check_mutation)
    {
        if (eq == EQ_GLOVES && you.has_claws(false) == 3)
            return false;

        if (eq == EQ_HELMET && player_mutation_level(MUT_HORNS) == 3)
            return false;

        if (eq == EQ_HELMET && player_mutation_level(MUT_ANTENNAE) == 3)
            return false;

        if (eq == EQ_BOOTS
            && (you.fishtail
                || player_mutation_level(MUT_HOOVES) == 3
                || you.has_talons(false) == 3))
        {
            return false;
        }
    }

    // No further restrictions.
    if (you.form == TRAN_NONE
        || you.form == TRAN_LICH
        || you.form == TRAN_APPENDAGE)
    {
        return true;
    }

    // Bats and pigs cannot wear anything except amulets.
    if ((you.form == TRAN_BAT || you.form == TRAN_PIG) && eq != EQ_AMULET)
        return false;

    // Everyone else can wear jewellery...
    if (eq == EQ_AMULET || eq == EQ_RINGS
        || eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING
        || eq == EQ_RING_ONE || eq == EQ_RING_TWO)
    {
        return true;
    }

    // ...but not necessarily in all slots.
    if (eq >= EQ_RING_THREE && eq <= EQ_RING_EIGHT)
    {
        return (you.species == SP_OCTOPODE
                && (form_keeps_mutations() || you.form == TRAN_SPIDER));
    }

    // These cannot use anything but jewellery.
    if (you.form == TRAN_SPIDER || you.form == TRAN_DRAGON)
        return false;

    if (you.form == TRAN_BLADE_HANDS)
    {
        if (eq == EQ_WEAPON || eq == EQ_GLOVES || eq == EQ_SHIELD)
            return false;
        return true;
    }

    if (you.form == TRAN_ICE_BEAST)
    {
        if (eq != EQ_CLOAK)
            return false;
        return true;
    }

    if (you.form == TRAN_STATUE)
    {
        if (eq == EQ_WEAPON || eq == EQ_SHIELD
            || eq == EQ_CLOAK || eq == EQ_HELMET)
        {
            return true;
        }
        return false;
    }

    if (you.form == TRAN_FUNGUS)
        return (eq == EQ_HELMET);

    if (you.form == TRAN_TREE)
        return (eq == EQ_WEAPON || eq == EQ_SHIELD || eq == EQ_HELMET);

    return true;
}

bool player_weapon_wielded()
{
    if (you.melded[EQ_WEAPON])
        return false;

    const int wpn = you.equip[EQ_WEAPON];

    if (wpn == -1)
        return false;

    if (!is_weapon(you.inv[wpn]))
        return false;

    return true;
}

// Returns false if the player is wielding a weapon inappropriate for Berserk.
bool berserk_check_wielded_weapon()
{
    const item_def * const wpn = you.weapon();
    if (wpn && (wpn->defined() && (!is_melee_weapon(*wpn)
                                   || needs_handle_warning(*wpn, OPER_ATTACK))
                || you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]))
    {
        string prompt = "Do you really want to go berserk while wielding "
                        + wpn->name(DESC_YOUR) + "?";

        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
    }

    return true;
}

// Looks in equipment "slot" to see if there is an equipped "sub_type".
// Returns number of matches (in the case of rings, both are checked)
int player::wearing(equipment_type slot, int sub_type, bool calc_unid) const
{
    int ret = 0;

    const item_def* item;

    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if (weapon()
            && weapon()->base_type == OBJ_WEAPONS
            && weapon()->sub_type == sub_type)
        {
            ret++;
        }
        break;

    case EQ_STAFF:
        // Like above, but must be magical staff.
        if (weapon()
            && weapon()->base_type == OBJ_STAVES
            && weapon()->sub_type == sub_type
            && (calc_unid || item_type_known(*weapon())))
        {
            ret++;
        }
        break;

    case EQ_RINGS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; slots++)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret++;
            }
        }
        break;

    case EQ_RINGS_PLUS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; slots++)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret += item->plus;
            }
        }
        break;

    case EQ_RINGS_PLUS2:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; ++slots)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret += item->plus2;
            }
        }
        break;

    case EQ_ALL_ARMOUR:
        // Doesn't make much sense here... be specific. -- bwr
        die("EQ_ALL_ARMOUR is not a proper slot");
        break;

    default:
        if (! (slot > EQ_NONE && slot < NUM_EQUIP))
            die("invalid slot");
        if ((item = slot_item(slot))
            && item->sub_type == sub_type
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        break;
    }

    return ret;
}

// Looks in equipment "slot" to see if equipped item has "special" ego-type
// Returns number of matches (jewellery returns zero -- no ego type).
// [ds] There's no equivalent of calc_unid or req_id because as of now, weapons
// and armour type-id on wield/wear.
int player::wearing_ego(equipment_type slot, int special, bool calc_unid) const
{
    int ret = 0;

    const item_def* item;
    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if ((item = slot_item(EQ_WEAPON))
            && item->base_type == OBJ_WEAPONS
            && get_weapon_brand(*item) == special)
        {
            ret++;
        }
        break;

    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_AMULET:
    case EQ_STAFF:
    case EQ_RINGS:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
        // no ego types for these slots
        break;

    case EQ_ALL_ARMOUR:
        // Check all armour slots:
        for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
        {
            if ((item = slot_item(static_cast<equipment_type>(i)))
                && get_armour_ego_type(*item) == special
                && (calc_unid || item_type_known(*item)))
            {
                ret++;
            }
        }
        break;

    default:
        if (slot < EQ_MIN_ARMOUR || slot > EQ_MAX_ARMOUR)
            die("invalid slot: %d", slot);
        // Check a specific armour slot for an ego type:
        if ((item = slot_item(static_cast<equipment_type>(slot)))
            && get_armour_ego_type(*item) == special
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        break;
    }

    return ret;
}

bool player_equip_unrand_effect(int unrand_index)
{
    if (you.suppressed())
        return false;
    else
        return player_equip_unrand(unrand_index);
}

// Return's true if the indicated unrandart is equipped
// [ds] There's no equivalent of calc_unid or req_id because as of now, weapons
// and armour type-id on wield/wear.
bool player_equip_unrand(int unrand_index)
{
    const unrandart_entry* entry = get_unrand_entry(unrand_index);
    equipment_type   slot  = get_item_slot(entry->base_type,
                                           entry->sub_type);

    item_def* item;

    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if ((item = you.slot_item(slot))
            && item->base_type == OBJ_WEAPONS
            && is_unrandom_artefact(*item)
            && item->special == unrand_index)
        {
            return true;
        }
        break;

    case EQ_RINGS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; ++slots)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = you.slot_item(static_cast<equipment_type>(slots)))
                && is_unrandom_artefact(*item)
                && item->special == unrand_index)
            {
                return true;
            }
        }
        break;

    case EQ_NONE:
    case EQ_STAFF:
    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
    case EQ_ALL_ARMOUR:
        // no unrandarts for these slots.
        break;

    default:
        if (slot <= EQ_NONE || slot >= NUM_EQUIP)
            die("invalid slot: %d", slot);
        // Check a specific slot.
        if ((item = you.slot_item(slot))
            && is_unrandom_artefact(*item)
            && item->special == unrand_index)
        {
            return true;
        }
        break;
    }

    return false;
}

// Given an adjacent monster, returns true if the player can hit it (the
// monster should not be submerged, or be submerged in shallow water if
// the player has a polearm).
bool player_can_hit_monster(const monster* mon)
{
    if (!mon->submerged())
        return true;

    if (grd(mon->pos()) != DNGN_SHALLOW_WATER)
        return false;

    const item_def *weapon = you.weapon();
    return (weapon && weapon_skill(*weapon) == SK_POLEARMS);
}

bool player_can_hear(const coord_def& p, int hear_distance)
{
    return (!silenced(p)
            && !silenced(you.pos())
            && you.pos().distance_from(p) <= hear_distance);
}

int player_teleport(bool calc_unid)
{
    ASSERT(!crawl_state.game_is_arena());

    // Don't allow any form of teleportation in Sprint.
    if (crawl_state.game_is_sprint())
        return 0;

    int tp = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // rings (keep in sync with _equip_jewellery_effect)
        tp += 8 * you.wearing(EQ_RINGS, RING_TELEPORTATION, calc_unid);

        // randart weapons only
        if (you.weapon()
            && you.weapon()->base_type == OBJ_WEAPONS
            && is_artefact(*you.weapon()))
        {
            tp += you.scan_artefacts(ARTP_CAUSE_TELEPORTATION, calc_unid);
        }
    }

    // mutations
    tp += player_mutation_level(MUT_TELEPORT) * 3;

    return tp;
}

// Computes bonuses to regeneration from most sources. Does not handle
// slow healing, vampireness, or Trog's Hand.
static int _player_bonus_regen()
{
    int rr = 0;

    // Trog's Hand is handled separately so that it will bypass slow healing,
    // and it overrides the spell.
    if (you.duration[DUR_REGENERATION]
        && !you.attribute[ATTR_DIVINE_REGENERATION])
    {
        rr += 100;
    }

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Rings.
        rr += 40 * you.wearing(EQ_RINGS, RING_REGENERATION);

        // Artefacts
        rr += you.scan_artefacts(ARTP_REGENERATION);

        // Troll leather (except for trolls).
        if ((you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR)
             || you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_HIDE))
            && you.species != SP_TROLL)
        {
            rr += 30;
        }
    }

    // Fast heal mutation.
    rr += player_mutation_level(MUT_REGENERATION) * 20;

    // Powered By Death mutation, boosts regen by 10 per corpse in
    // a mutation_level * 3 (3/6/9) radius, to a maximum of 7
    // corpses.  If and only if the duration of the effect is
    // still active.
    if (you.duration[DUR_POWERED_BY_DEATH])
        rr += handle_pbd_corpses(false) * 100;

    return rr;
}

int player_regen()
{
    int rr = you.hp_max / 3;

    if (rr > 20)
        rr = 20 + ((rr - 20) / 2);

    // Add in miscellaneous bonuses
    rr += _player_bonus_regen();

    // Before applying other effects, make sure that there's something
    // to heal.
    rr = max(1, rr);

    // Healing depending on satiation.
    // The better-fed you are, the faster you heal.
    if (you.species == SP_VAMPIRE)
    {
        if (you.hunger_state == HS_STARVING)
            rr = 0;   // No regeneration for starving vampires.
        else if (you.hunger_state == HS_ENGORGED)
            rr += 20; // More bonus regeneration for engorged vampires.
        else if (you.hunger_state < HS_SATIATED)
            rr /= 2;  // Halved regeneration for hungry vampires.
        else if (you.hunger_state >= HS_FULL)
            rr += 10; // Bonus regeneration for full vampires.
    }

    // Compared to other races, a starting djinni would have regen of 4 (hp)
    // plus 17 (mp).  So let's compensate them early; they can stand getting
    // shafted on the total regen rates later on.
    if (you.species == SP_DJINNI)
        if (you.hp_max < 100)
            rr += (100 - you.hp_max) / 6;

    // Slow heal mutation.  Each level reduces your natural healing by
    // one third.
    if (player_mutation_level(MUT_SLOW_HEALING) > 0)
    {
        rr *= 3 - player_mutation_level(MUT_SLOW_HEALING);
        rr /= 3;
    }

    if (you.stat_zero[STAT_STR])
        rr /= 4;

    if (you.disease)
        rr = 0;

    // Trog's Hand.  This circumvents the slow healing effect.
    if (you.attribute[ATTR_DIVINE_REGENERATION])
        rr += 100;

    return rr;
}

int player_hunger_rate(bool temp)
{
    int hunger = 3;

    if (temp && you.form == TRAN_BAT)
        return 1;

    if (you.species == SP_TROLL)
        hunger += 3;            // in addition to the +3 for fast metabolism

    if (temp && you.duration[DUR_REGENERATION] && you.hp < you.hp_max)
        hunger += 4;

    // If Cheibriados has slowed your life processes, you will hunger less.
    if (you_worship(GOD_CHEIBRIADOS) && you.piety >= piety_breakpoint(0))
        hunger--;

    // Moved here from main.cc... maintaining the >= 40 behaviour.
    if (temp && you.hunger >= 40)
    {
        if (you.duration[DUR_INVIS])
            hunger += 5;

        // Berserk has its own food penalty - excluding berserk haste.
        // Doubling the hunger cost for haste so that the per turn hunger
        // is consistent now that a hasted turn causes 50% the normal hunger
        // -cao
        if (you.duration[DUR_HASTE])
            hunger += haste_mul(5);
    }

    if (you.species == SP_VAMPIRE)
    {
        switch (you.hunger_state)
        {
        case HS_STARVING:
        case HS_NEAR_STARVING:
            hunger -= 3;
            break;
        case HS_VERY_HUNGRY:
            hunger -= 2;
            break;
        case HS_HUNGRY:
            hunger--;
            break;
        case HS_SATIATED:
            break;
        case HS_FULL:
            hunger++;
            break;
        case HS_VERY_FULL:
            hunger += 2;
            break;
        case HS_ENGORGED:
            hunger += 3;
        }
    }
    else
    {
        hunger += player_mutation_level(MUT_FAST_METABOLISM)
                - player_mutation_level(MUT_SLOW_METABOLISM);
    }

    // burden
    if (temp)
        hunger += you.burden_state;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (you.hp < you.hp_max
            && player_mutation_level(MUT_SLOW_HEALING) < 3)
        {
            // rings
            hunger += 3 * you.wearing(EQ_RINGS, RING_REGENERATION);

            // troll leather
            if (you.species != SP_TROLL
                && (you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR)
                    || you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_HIDE)))
            {
                hunger += coinflip() ? 2 : 1;
            }
        }

        hunger += 4 * you.wearing(EQ_RINGS, RING_HUNGER);

        // randarts
        hunger += you.scan_artefacts(ARTP_METABOLISM);

        // sustenance affects things at the end, because it is multiplicative
        for (int s = you.wearing(EQ_RINGS, RING_SUSTENANCE); s > 0; s--)
            hunger = (3*hunger)/5;
    }

    if (hunger < 1)
        hunger = 1;

    return hunger;
}

int player_spell_levels(void)
{
    int sl = you.experience_level - 1 + you.skill(SK_SPELLCASTING, 2, true);

    bool fireball = false;
    bool delayed_fireball = false;

    if (sl > 99)
        sl = 99;

    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (you.spells[i] == SPELL_FIREBALL)
            fireball = true;
        else if (you.spells[i] == SPELL_DELAYED_FIREBALL)
            delayed_fireball = true;

        if (you.spells[i] != SPELL_NO_SPELL)
            sl -= spell_difficulty(you.spells[i]);
    }

    // Fireball is free for characters with delayed fireball
    if (fireball && delayed_fireball)
        sl += spell_difficulty(SPELL_FIREBALL);

    // Note: This can happen because of level drain.  Maybe we should
    // force random spells out when that happens. -- bwr
    if (sl < 0)
        sl = 0;

    return sl;
}

int player_likes_chunks(bool permanently)
{
    return you.gourmand(true, !permanently)
           ? 3 : player_mutation_level(MUT_CARNIVOROUS);
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_fire(bool calc_unid, bool temp, bool items)
{
    if (you.species == SP_DJINNI)
        return 4; // full immunity

    int rf = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items)
        {
            // rings of fire resistance/fire
            rf += you.wearing(EQ_RINGS, RING_PROTECTION_FROM_FIRE, calc_unid);
            rf += you.wearing(EQ_RINGS, RING_FIRE, calc_unid);

            // rings of ice
            rf -= you.wearing(EQ_RINGS, RING_ICE, calc_unid);

            // Staves
            rf += you.wearing(EQ_STAFF, STAFF_FIRE, calc_unid);

            // body armour:
            rf += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
            rf += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
            rf -= you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR);
            rf += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_HIDE);
            rf += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
            rf -= you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_HIDE);

            // ego armours
            rf += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FIRE_RESISTANCE);
            rf += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_RESISTANCE);

            // randart weapons:
            rf += you.scan_artefacts(ARTP_FIRE, calc_unid);

            // dragonskin cloak: 0.5 to draconic resistances
            if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN)
                && coinflip())
            {
                rf++;
            }
        }
    }

    // species:
    if (you.species == SP_MUMMY)
        rf--;

    if (you.species == SP_LAVA_ORC)
    {
        if (temperature_effect(LORC_FIRE_RES_I))
            rf++;
        if (temperature_effect(LORC_FIRE_RES_II))
            rf++;
        if (temperature_effect(LORC_FIRE_RES_III))
            rf++;
    }

    // mutations:
    rf += player_mutation_level(MUT_HEAT_RESISTANCE, temp);
    rf += player_mutation_level(MUT_MOLTEN_SCALES, temp) == 3 ? 1 : 0;

    // spells:
    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rf++;

        if (you.duration[DUR_FIRE_SHIELD])
            rf += 2;

        if (you.duration[DUR_FIRE_VULN])
            rf--;

        // transformations:
        switch (you.form)
        {
        case TRAN_TREE:
            if (you_worship(GOD_FEDHAS) && !player_under_penance())
                rf++;
            break;
        case TRAN_ICE_BEAST:
            rf--;
            break;
        case TRAN_WISP:
            rf += 2;
            break;
        case TRAN_DRAGON:
        {
            monster_type drag = dragon_form_dragon_type();
            if (drag == MONS_DRAGON)
                rf += 2;
            else if (drag == MONS_ICE_DRAGON)
                rf--;
            break;
        }
        default:
            break;
        }
    }

    if (rf < -3)
        rf = -3;
    else if (rf > 3)
        rf = 3;

    return rf;
}

int player_res_steam(bool calc_unid, bool temp, bool items)
{
    int res = 0;
    const int rf = player_res_fire(calc_unid, temp, items);

    if (you.species == SP_PALE_DRACONIAN)
        res += 2;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items && you.wearing(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_ARMOUR))
            res += 2;

        if (items && you.wearing(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_HIDE))
            res += 2;
    }

    res += (rf < 0) ? rf
                    : (rf + 1) / 2;

    if (res > 3)
        res = 3;

    return res;
}

int player_res_cold(bool calc_unid, bool temp, bool items)
{
    int rc = 0;

    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rc++;

        if (you.duration[DUR_FIRE_SHIELD])
            rc -= 2;

        // transformations:
        switch (you.form)
        {
        case TRAN_TREE:
            if (you_worship(GOD_FEDHAS) && !player_under_penance())
                rc++;
            break;
        case TRAN_ICE_BEAST:
            rc += 3;
            break;
        case TRAN_WISP:
            rc += 2;
            break;
        case TRAN_DRAGON:
        {
            monster_type form = dragon_form_dragon_type();
            if (form == MONS_DRAGON)
                rc--;
            else if (form == MONS_ICE_DRAGON)
                rc += 2;
            break;
        }
        case TRAN_LICH:
            rc++;
            break;
        default:
            break;
        }

        if (you.species == SP_VAMPIRE)
        {
            if (you.hunger_state <= HS_NEAR_STARVING)
                rc += 2;
            else if (you.hunger_state < HS_SATIATED)
                rc++;
        }

        if (you.species == SP_LAVA_ORC && temperature_effect(LORC_COLD_VULN))
            rc--;
    }

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items)
        {
            // rings of cold resistance/ice
            rc += you.wearing(EQ_RINGS, RING_PROTECTION_FROM_COLD, calc_unid);
            rc += you.wearing(EQ_RINGS, RING_ICE, calc_unid);

            // rings of fire
            rc -= you.wearing(EQ_RINGS, RING_FIRE, calc_unid);

            // Staves
            rc += you.wearing(EQ_STAFF, STAFF_COLD, calc_unid);

            // body armour:
            rc += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR);
            rc += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
            rc -= you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
            rc += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_HIDE);
            rc += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
            rc -= you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_HIDE);

            // ego armours
            rc += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_COLD_RESISTANCE);
            rc += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_RESISTANCE);

            // randart weapons:
            rc += you.scan_artefacts(ARTP_COLD, calc_unid);

            // dragonskin cloak: 0.5 to draconic resistances
            if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
                rc++;
        }
    }

    // species:
    if (you.species == SP_DJINNI)
        rc--;

    // mutations:
    rc += player_mutation_level(MUT_COLD_RESISTANCE, temp);
    rc += player_mutation_level(MUT_ICY_BLUE_SCALES, temp) == 3 ? 1 : 0;
    rc += player_mutation_level(MUT_SHAGGY_FUR, temp) == 3 ? 1 : 0;

    if (rc < -3)
        rc = -3;
    else if (rc > 3)
        rc = 3;

    return rc;
}

bool player::res_corr(bool calc_unid, bool items) const
{
    if (religion == GOD_JIYVA && piety >= piety_breakpoint(2))
        return true;

    if (form == TRAN_JELLY || form == TRAN_WISP)
        return 1;

    if (items && !suppressed())
    {
        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            return true;
        }
    }

    if ((form_keeps_mutations() || form == TRAN_DRAGON)
        && species == SP_YELLOW_DRACONIAN)
    {
        return true;
    }

    if (form_keeps_mutations()
        && player_mutation_level(MUT_YELLOW_SCALES) >= 3)
    {
        return true;
    }

    return actor::res_corr(calc_unid, items);
}

int player_res_acid(bool calc_unid, bool items)
{
    if (you.form == TRAN_JELLY || you.form == TRAN_WISP)
        return 3;

    return you.res_corr(calc_unid, items) ? 1 : 0;
}

// Returns a factor X such that post-resistance acid damage can be calculated
// as pre_resist_damage * X / 100.
int player_acid_resist_factor()
{
    int rA = player_res_acid();
    if (rA >= 3)
        return 0;
    else if (rA >= 1)
        return 50;
    return 100;
}

int player_res_electricity(bool calc_unid, bool temp, bool items)
{
    int re = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items)
        {
            // staff
            re += you.wearing(EQ_STAFF, STAFF_AIR, calc_unid);

            // body armour:
            re += you.wearing(EQ_BODY_ARMOUR, ARM_STORM_DRAGON_ARMOUR);
            re += you.wearing(EQ_BODY_ARMOUR, ARM_STORM_DRAGON_HIDE);

            // randart weapons:
            re += you.scan_artefacts(ARTP_ELECTRICITY, calc_unid);

            // dragonskin cloak: 0.5 to draconic resistances
            if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
                re++;
        }
    }

    // mutations:
    re += player_mutation_level(MUT_THIN_METALLIC_SCALES, temp) == 3 ? 1 : 0;
    re += player_mutation_level(MUT_SHOCK_RESISTANCE, temp);

    if (temp)
    {
        if (you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION])
            return 3;

        if (you.duration[DUR_RESISTANCE])
            re++;

        // transformations:
        if (you.form == TRAN_STATUE || you.form == TRAN_WISP)
            re += 1;

        if (re > 1)
            re = 1;
    }

    return re;
}

bool player_control_teleport(bool temp)
{
    return ((temp && you.duration[DUR_CONTROL_TELEPORT])
            || crawl_state.game_is_zotdef());
}

int player_res_torment(bool, bool temp)
{
    return (player_mutation_level(MUT_TORMENT_RESISTANCE)
            || you.form == TRAN_LICH
            || you.form == TRAN_FUNGUS
            || you.form == TRAN_TREE
            || you.form == TRAN_WISP
            || you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING
            || you.petrified()
            || (temp && player_mutation_level(MUT_STOCHASTIC_TORMENT_RESISTANCE)
                && coinflip()));
}

// Kiku protects you from torment to a degree.
int player_kiku_res_torment()
{
    return (you_worship(GOD_KIKUBAAQUDGHA)
            && !player_under_penance()
            && you.piety >= piety_breakpoint(3)
            && !you.gift_timeout); // no protection during pain branding weapon
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_poison(bool calc_unid, bool temp, bool items)
{
    if (you.is_undead == US_SEMI_UNDEAD ? you.hunger_state == HS_STARVING
            : you.is_undead && (temp || you.form != TRAN_LICH)
              || you.is_artificial())
    {
        return 3;
    }

    int rp = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items)
        {
            // rings of poison resistance
            rp += you.wearing(EQ_RINGS, RING_POISON_RESISTANCE, calc_unid);

            // Staves
            rp += you.wearing(EQ_STAFF, STAFF_POISON, calc_unid);

            // ego armour:
            rp += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_POISON_RESISTANCE);

            // body armour:
            rp += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
            rp += you.wearing(EQ_BODY_ARMOUR, ARM_SWAMP_DRAGON_ARMOUR);
            rp += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
            rp += you.wearing(EQ_BODY_ARMOUR, ARM_SWAMP_DRAGON_HIDE);

            // randart weapons:
            rp += you.scan_artefacts(ARTP_POISON, calc_unid);

            // dragonskin cloak: 0.5 to draconic resistances
            if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
                rp++;
        }
    }

    // mutations:
    rp += player_mutation_level(MUT_POISON_RESISTANCE, temp);
    rp += player_mutation_level(MUT_SLIMY_GREEN_SCALES, temp) == 3 ? 1 : 0;

    // Only thirsty vampires are naturally poison resistant.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED)
        rp++;

    if (temp)
    {
        // potions/cards:
        if (you.duration[DUR_RESISTANCE])
            rp++;

        // transformations:
        switch (you.form)
        {
        case TRAN_ICE_BEAST:
        case TRAN_STATUE:
        case TRAN_DRAGON:
        case TRAN_FUNGUS:
        case TRAN_TREE:
        case TRAN_JELLY:
        case TRAN_WISP:
            rp++;
            break;
        default:
            break;
        }

        if (you.petrified())
            rp++;
    }

    // Give vulnerability for Spider Form, and only let one level of rP to make
    // up for it (never be poison resistant in Spider Form).
    rp = (rp > 0 ? 1 : 0);

    if (temp)
    {
        if (you.form == TRAN_SPIDER)
            rp--;
    }

    return rp;
}

static int _maybe_reduce_poison(int amount)
{
    int rp = player_res_poison(true, true, true);

    if (rp <= 0)
        return amount;

    int reduction = binomial_generator(amount, 90);
    int new_amount = amount - reduction;

    if (amount != new_amount)
        dprf("Poison reduced (%d -> %d)", amount, new_amount);
    else
        dprf("Poison not reduced (%d)", amount);

    return new_amount;
}

int player_res_sticky_flame(bool calc_unid, bool temp, bool items)
{
    int rsf = 0;

    if (you.species == SP_MOTTLED_DRACONIAN)
        rsf++;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items && you.wearing(EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_ARMOUR))
            rsf++;
        if (items && you.wearing(EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_HIDE))
            rsf++;

        // dragonskin cloak: 0.5 to draconic resistances
        if (items && calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
            rsf++;
    }

    if (you.form == TRAN_WISP)
        rsf++;

    if (rsf > 1)
        rsf = 1;

    return rsf;
}

int player_spec_death()
{
    int sd = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        sd += you.wearing(EQ_STAFF, STAFF_DEATH);
    }

    // species:
    if (you.species == SP_MUMMY)
    {
        if (you.experience_level >= 13)
            sd++;
        if (you.experience_level >= 26)
            sd++;
    }

    // transformations:
    if (you.form == TRAN_LICH)
        sd++;

    return sd;
}

int player_spec_fire()
{
    int sf = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // staves:
        sf += you.wearing(EQ_STAFF, STAFF_FIRE);

        // rings of fire:
        sf += you.wearing(EQ_RINGS, RING_FIRE);
    }

    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_FIRE_BOOST))
        sf++;

    if (you.duration[DUR_FIRE_SHIELD])
        sf++;

    return sf;
}

int player_spec_cold()
{
    int sc = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // staves:
        sc += you.wearing(EQ_STAFF, STAFF_COLD);

        // rings of ice:
        sc += you.wearing(EQ_RINGS, RING_ICE);
    }

    if (you.species == SP_LAVA_ORC
        && (temperature_effect(LORC_LAVA_BOOST)
            || temperature_effect(LORC_FIRE_BOOST)))
    {
        sc--;
    }

    return sc;
}

int player_spec_earth()
{
    int se = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        se += you.wearing(EQ_STAFF, STAFF_EARTH);
    }

    return se;
}

int player_spec_air()
{
    int sa = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        sa += you.wearing(EQ_STAFF, STAFF_AIR);
    }

    return sa;
}

int player_spec_conj()
{
    int sc = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        sc += you.wearing(EQ_STAFF, STAFF_CONJURATION);
    }

    return sc;
}

int player_spec_hex()
{
    int sh = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Unrands
        if (player_equip_unrand(UNRAND_BOTONO))
            sh++;
    }

    return sh;
}

int player_spec_charm()
{
    // Nothing, for the moment.
    return 0;
}

int player_spec_summ()
{
    int ss = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        ss += you.wearing(EQ_STAFF, STAFF_SUMMONING);
    }

    return ss;
}

int player_spec_poison()
{
    int sp = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        sp += you.wearing(EQ_STAFF, STAFF_POISON);

        if (player_equip_unrand(UNRAND_OLGREB))
            sp++;
    }

    return sp;
}

int player_energy()
{
    int pe = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Staves
        pe += you.wearing(EQ_STAFF, STAFF_ENERGY);
    }

    return pe;
}

// If temp is set to false, temporary sources of resistance won't be
// counted.
int player_prot_life(bool calc_unid, bool temp, bool items)
{
    int pl = 0;

    // Hunger is temporary, true, but that's something you can control,
    // especially as life protection only increases the hungrier you
    // get.
    if (you.species == SP_VAMPIRE)
    {
        switch (you.hunger_state)
        {
        case HS_STARVING:
        case HS_NEAR_STARVING:
            pl = 3;
            break;
        case HS_VERY_HUNGRY:
        case HS_HUNGRY:
            pl = 2;
            break;
        case HS_SATIATED:
            pl = 1;
            break;
        default:
            break;
        }
    }

    // Same here.  Your piety status, and, hence, TSO's protection, is
    // something you can more or less control.
    if (you_worship(GOD_SHINING_ONE) && you.piety > pl * 50)
        pl = you.piety / 50;

    if (temp)
    {
        // Now, transformations could stop at any time.
        switch (you.form)
        {
        case TRAN_STATUE:
            pl++;
            break;
        case TRAN_FUNGUS:
        case TRAN_TREE:
        case TRAN_WISP:
        case TRAN_LICH:
            pl += 3;
            break;
        default:
           break;
        }

        // completely stoned, unlike statue which has some life force
        if (you.petrified())
            pl += 3;
    }

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (items)
        {
            if (you.wearing(EQ_AMULET, AMU_WARDING, calc_unid))
                pl++;

            // rings
            pl += you.wearing(EQ_RINGS, RING_LIFE_PROTECTION, calc_unid);

            // armour (checks body armour only)
            pl += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_POSITIVE_ENERGY);

            // pearl dragon counts
            pl += you.wearing(EQ_BODY_ARMOUR, ARM_PEARL_DRAGON_ARMOUR);
            pl += you.wearing(EQ_BODY_ARMOUR, ARM_PEARL_DRAGON_HIDE);

            // randart wpns
            pl += you.scan_artefacts(ARTP_NEGATIVE_ENERGY, calc_unid);

            // dragonskin cloak: 0.5 to draconic resistances
            // this one is dubious (no pearl draconians)
            if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
                pl++;

            pl += you.wearing(EQ_STAFF, STAFF_DEATH, calc_unid);
        }
    }

    // undead/demonic power
    pl += player_mutation_level(MUT_NEGATIVE_ENERGY_RESISTANCE, temp);

    pl = min(3, pl);

    return pl;
}

// New player movement speed system... allows for a bit more than
// "player runs fast" and "player walks slow" in that the speed is
// actually calculated (allowing for centaurs to get a bonus from
// swiftness and other such things).  Levels of the mutation now
// also have meaning (before they all just meant fast).  Most of
// this isn't as fast as it used to be (6 for having anything), but
// even a slight speed advantage is very good... and we certainly don't
// want to go past 6 (see below). -- bwr
int player_movement_speed(bool ignore_burden)
{
    int mv = 10;

    // transformations
    if (you.form == TRAN_SPIDER)
        mv = 8;
    else if (you.form == TRAN_BAT)
        mv = 5; // but allowed minimum is six
    else if (you.form == TRAN_PIG)
        mv = 7;
    else if (you.form == TRAN_PORCUPINE || you.form == TRAN_WISP)
        mv = 8;
    else if (you.form == TRAN_JELLY)
        mv = 11;
    else if (you.fishtail)
        mv = 6;

    // moving on liquefied ground takes longer
    if (you.liquefied_ground())
        mv += 3;

    // armour
    if (you.run())
        mv -= 1;
    if (!you.suppressed())
        mv += 2 * you.wearing_ego(EQ_ALL_ARMOUR, SPARM_PONDEROUSNESS);

    // Cheibriados
    if (you_worship(GOD_CHEIBRIADOS))
        mv += 2 + min(div_rand_round(you.piety, 20), 8);

    // Tengu can move slightly faster when flying.
    if (you.tengu_flight())
        mv--;

    // Swiftness doesn't work in liquid.
    if (you.duration[DUR_SWIFTNESS] > 0 && !you.in_liquid())
    {
        mv -= 2;
    }

    if (you.duration[DUR_GRASPING_ROOTS])
        mv += 5;

    // Lava orc heat-based speed. -2 when cold; 0 when normal; +2 when hot.
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_SLOW_MOVE))
        mv += 2;
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_FAST_MOVE))
        mv -= 2;

    // Mutations: -2, -3, -4, unless innate and shapechanged.
    // Not when swimming, since it is "cover the ground quickly".
    if (player_mutation_level(MUT_FAST) > 0 && !you.swimming())
        mv -= player_mutation_level(MUT_FAST) + 1;

    if (player_mutation_level(MUT_SLOW) > 0 && !you.swimming())
    {
        mv *= 10 + player_mutation_level(MUT_SLOW) * 2;
        mv /= 10;
    }

    // Burden
    if (!ignore_burden)
    {
        if (you.burden_state == BS_ENCUMBERED)
            mv++;
        else if (you.burden_state == BS_OVERLOADED)
            mv += 3;
    }

    // We'll use the old value of six as a minimum, with haste this could
    // end up as a speed of three, which is about as fast as we want
    // the player to be able to go (since 3 is 3.33x as fast and 2 is 5x,
    // which is a bit of a jump, and a bit too fast) -- bwr
    if (mv < 6)
        mv = 6;

    return mv;
}

// This function differs from the above in that it's used to set the
// initial time_taken value for the turn.  Everything else (movement,
// spellcasting, combat) applies a ratio to this value.
int player_speed(void)
{
    int ps = 10;

    // When paralysed, speed is irrelevant.
    if (you.cannot_act())
        return ps;

    for (int i = 0; i < NUM_STATS; ++i)
        if (you.stat_zero[i])
            ps *= 2;

    if (you.duration[DUR_SLOW])
        ps = haste_mul(ps);

    if (you.duration[DUR_BERSERK] && !you_worship(GOD_CHEIBRIADOS))
        ps = berserk_div(ps);
    else if (you.duration[DUR_HASTE])
        ps = haste_div(ps);

    if (you.form == TRAN_STATUE || you.duration[DUR_PETRIFYING])
    {
        ps *= 15;
        ps /= 10;
    }

    if (is_hovering())
        ps = ps * 3 / 2;

    if (you.form == TRAN_TREE)
    {
        ps *= 15 - you.experience_level / 5;
        ps /= 10;
    }

    return ps;
}

// Get level of player mutation, ignoring mutations with an activity level
// less than minact (unless they have MUTACT_HUNGER, in which case check
// the player's hunger state).
static int _mut_level(mutation_type mut, mutation_activity_type minact)
{
    const int mlevel = you.mutation[mut];

    const mutation_activity_type active = mutation_activity_level(mut);

    if (active >= minact)
        return mlevel;
    else if (active == MUTACT_HUNGER)
    {
        switch (you.hunger_state)
        {
        case HS_ENGORGED:
            return mlevel;
        case HS_VERY_FULL:
        case HS_FULL:
            return min(mlevel, 2);
        case HS_SATIATED:
            return min(mlevel, 1);
        default:
            return 0;
        }
    }

    return 0;
}

// Output level of player mutation.  If temp is true (the default), take into
// account the suppression of mutations by non-"Alive" Vampires and by changes
// of form.
int player_mutation_level(mutation_type mut, bool temp)
{
    return _mut_level(mut, temp ? MUTACT_PARTIAL : MUTACT_INACTIVE);
}

static int _player_armour_racial_bonus(const item_def& item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    int racial_bonus = 0;
    const iflags_t armour_race = get_equip_race(item);
    const iflags_t racial_type = get_species_race(you.species);

    // Dwarven armour is universally good -- bwr
    if (armour_race == ISFLAG_DWARVEN)
        racial_bonus += 4;

    if (racial_type && armour_race == racial_type)
    {
        // Elven armour is light, but still gives one level to elves.
        // Orcish and Dwarven armour are worth +2 to the correct
        // species, plus the plus that anyone gets with dwarven armour.
        // -- bwr
        if (racial_type == ISFLAG_ELVEN)
            racial_bonus += 2;
        else
            racial_bonus += 4;

        // an additional bonus for Beogh worshippers
        if (you_worship(GOD_BEOGH) && !player_under_penance())
        {
            if (you.piety >= 185)
                racial_bonus += racial_bonus * 9 / 4;
            else if (you.piety >= 160)
                racial_bonus += racial_bonus * 2;
            else if (you.piety >= 120)
                racial_bonus += racial_bonus * 7 / 4;
            else if (you.piety >= 80)
                racial_bonus += racial_bonus * 5 / 4;
            else if (you.piety >= 40)
                racial_bonus += racial_bonus * 3 / 4;
            else
                racial_bonus += racial_bonus / 4;
        }
    }

    return racial_bonus;
}

bool is_effectively_light_armour(const item_def *item)
{
    return (!item
            || (abs(property(*item, PARM_EVASION)) < 6));
}

bool player_effectively_in_light_armour()
{
    const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
    return is_effectively_light_armour(armour);
}

// This function returns true if the player has a radically different
// shape... minor changes like blade hands don't count, also note
// that lich transformation doesn't change the character's shape
// (so we end up with Naga-liches, Spiggan-liches, Minotaur-liches)
// it just makes the character undead (with the benefits that implies). - bwr
bool player_is_shapechanged(void)
{
    if (you.form == TRAN_NONE
        || you.form == TRAN_BLADE_HANDS
        || you.form == TRAN_LICH
        || you.form == TRAN_APPENDAGE)
    {
        return false;
    }

    return true;
}

// An evasion factor based on the player's body size, smaller == higher
// evasion size factor.
static int _player_evasion_size_factor()
{
    // XXX: you.body_size() implementations are incomplete, fix.
    const size_type size = you.body_size(PSIZE_BODY);
    return 2 * (SIZE_MEDIUM - size);
}

// The total EV penalty to the player for all their worn armour items
// with a base EV penalty (i.e. EV penalty as a base armour property,
// not as a randart property).
static int _player_adjusted_evasion_penalty(const int scale)
{
    int piece_armour_evasion_penalty = 0;

    // Some lesser armours have small penalties now (barding).
    for (int i = EQ_MIN_ARMOUR; i < EQ_MAX_ARMOUR; i++)
    {
        if (i == EQ_SHIELD || !player_wearing_slot(i))
            continue;

        // [ds] Evasion modifiers for armour are negatives, change
        // those to positive for penalty calc.
        const int penalty = (-property(you.inv[you.equip[i]], PARM_EVASION))/3;
        if (penalty > 0)
            piece_armour_evasion_penalty += penalty;
    }

    return (piece_armour_evasion_penalty * scale +
            you.adjusted_body_armour_penalty(scale));
}

// EV bonuses that work even when helpless.
static int _player_para_evasion_bonuses(ev_ignore_type evit)
{
    int evbonus = 0;

    if (you.duration[DUR_PHASE_SHIFT] && !(evit & EV_IGNORE_PHASESHIFT))
        evbonus += 8;

    if (player_mutation_level(MUT_DISTORTION_FIELD) > 0)
        evbonus += player_mutation_level(MUT_DISTORTION_FIELD) + 1;

    return evbonus;
}

// Player EV bonuses for various effects and transformations. This
// does not include tengu/merfolk EV bonuses for flight/swimming.
static int _player_evasion_bonuses(ev_ignore_type evit)
{
    int evbonus = _player_para_evasion_bonuses(evit);

    if (you.duration[DUR_AGILITY])
        evbonus += 5;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        evbonus += you.wearing(EQ_RINGS_PLUS, RING_EVASION);

        if (you.wearing_ego(EQ_WEAPON, SPWPN_EVASION))
            evbonus += 5;

        evbonus += you.scan_artefacts(ARTP_EVASION);
    }

    // mutations
    if (_mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) > 1)
        evbonus--;
    if (_mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) > 1)
        evbonus--;
    evbonus += max(0, player_mutation_level(MUT_GELATINOUS_BODY) - 1);

    // transformation penalties/bonuses not covered by size alone:
    if (you.form == TRAN_STATUE)
        evbonus -= 10;               // stiff and slow

    return evbonus;
}

// Player EV scaling for being flying tengu or swimming merfolk.
static int _player_scale_evasion(int prescaled_ev, const int scale)
{
    if (you.duration[DUR_PETRIFYING] || you.duration[DUR_GRASPING_ROOTS]
        || you.caught())
    {
        prescaled_ev /= 2;
    }

    switch (you.species)
    {
    case SP_MERFOLK:
        // Merfolk get an evasion bonus in water.
        if (you.fishtail)
        {
            const int ev_bonus = min(9 * scale,
                                     max(2 * scale, prescaled_ev / 4));
            return (prescaled_ev + ev_bonus);
        }
        break;

    case SP_TENGU:
        // Flying Tengu get an evasion bonus.
        if (you.flight_mode())
        {
            const int ev_bonus = min(9 * scale,
                                     max(1 * scale, prescaled_ev / 5));
            return (prescaled_ev + ev_bonus);
        }
        break;

    default:
        break;
    }
    return prescaled_ev;
}

// Total EV for player using the revised 0.6 evasion model.
int player_evasion(ev_ignore_type evit)
{
    const int size_factor = _player_evasion_size_factor();
    // Repulsion fields and size are all that matters when paralysed or
    // at 0 dex.
    if ((you.cannot_move() || you.stat_zero[STAT_DEX] || you.form == TRAN_TREE)
        && !(evit & EV_IGNORE_HELPLESS))
    {
        const int paralysed_base_ev = 2 + size_factor / 2;
        const int repulsion_ev = _player_para_evasion_bonuses(evit);
        return max(1, paralysed_base_ev + repulsion_ev);
    }

    const int scale = 100;
    const int size_base_ev = (10 + size_factor) * scale;

    const int adjusted_evasion_penalty =
        _player_adjusted_evasion_penalty(scale);

    // The last two parameters are not important.
    const int ev_dex = stepdown_value(you.dex(), 10, 24, 72, 72);

    const int dodge_bonus =
        (70 + you.skill(SK_DODGING, 10) * ev_dex) * scale
        / (20 - size_factor) / 10;

    // [ds] Dodging penalty for being in high EVP armour, almost
    // identical to v0.5/4.1 penalty, but with the EVP discount being
    // 1 instead of 0.5 so that leather armour is fully discounted.
    // The 1 EVP of leather armour may still incur an
    // adjusted_evasion_penalty, however.
    const int armour_dodge_penalty = max(0,
        (10 * you.adjusted_body_armour_penalty(scale, true)
         - 30 * scale)
        / max(1, (int) you.strength()));

    // Adjust dodge bonus for the effects of being suited up in armour.
    const int armour_adjusted_dodge_bonus =
        max(0, dodge_bonus - armour_dodge_penalty);

    const int adjusted_shield_penalty = you.adjusted_shield_penalty(scale);

    const int prestepdown_evasion =
        size_base_ev
        + armour_adjusted_dodge_bonus
        - adjusted_evasion_penalty
        - adjusted_shield_penalty;

    const int poststepdown_evasion =
        stepdown_value(prestepdown_evasion, 20*scale, 30*scale, 60*scale, -1);

    const int evasion_bonuses = _player_evasion_bonuses(evit) * scale;

    const int prescaled_evasion =
        poststepdown_evasion + evasion_bonuses;

    const int final_evasion =
        _player_scale_evasion(prescaled_evasion, scale);

    return unscale_round_up(final_evasion, scale);
}

static int _player_body_armour_racial_spellcasting_bonus(const int scale)
{
    const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
    if (!body_armour)
        return 0;

    const iflags_t armour_race = get_equip_race(*body_armour);
    const iflags_t player_race = get_species_race(you.species);

    int armour_racial_spellcasting_bonus = 0;
    if (armour_race & ISFLAG_ELVEN)
        armour_racial_spellcasting_bonus += 25;

    if (armour_race & ISFLAG_DWARVEN)
        armour_racial_spellcasting_bonus -= 15;

    if (armour_race & player_race)
        armour_racial_spellcasting_bonus += 15;

    return (armour_racial_spellcasting_bonus * scale);
}

// Returns the spellcasting penalty (increase in spell failure) for the
// player's worn body armour and shield.
int player_armour_shield_spell_penalty()
{
    const int scale = 100;

    const int body_armour_penalty =
        max(25 * you.adjusted_body_armour_penalty(scale)
            - _player_body_armour_racial_spellcasting_bonus(scale), 0);

    const int total_penalty = body_armour_penalty
                 + 25 * you.adjusted_shield_penalty(scale)
                 - 20 * scale;

    return (max(total_penalty, 0) / scale);
}

int player_mag_abil(bool is_weighted)
{
    int ma = 0;

    // Brilliance Potion
    ma += 6 * (you.duration[DUR_BRILLIANCE] ? 1 : 0);

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // Rings
        ma += 3 * you.wearing(EQ_RINGS, RING_WIZARDRY);

        // Staves
        ma += 4 * you.wearing(EQ_STAFF, STAFF_WIZARDRY);
    }

    return ((is_weighted) ? ((ma * you.intel()) / 10) : ma);
}

int player_shield_class(void)
{
    int shield = 0;
    int stat = 0;

    if (you.incapacitated())
        return 0;

    if (player_wearing_slot(EQ_SHIELD))
    {
        const item_def& item = you.inv[you.equip[EQ_SHIELD]];
        int size_factor = (you.body_size(PSIZE_TORSO) - SIZE_MEDIUM)
                        * (item.sub_type - ARM_LARGE_SHIELD);
        int base_shield = property(item, PARM_AC) * 2 + size_factor;

        int racial_bonus = _player_armour_racial_bonus(item);

        // bonus applied only to base, see above for effect:
        shield += base_shield * 50;
        shield += base_shield * you.skill(SK_SHIELDS, 5) / 2;
        shield += base_shield * racial_bonus * 10 / 6;

        shield += item.plus * 100;

        if (item.sub_type == ARM_BUCKLER)
            stat = you.dex() * 38;
        else if (item.sub_type == ARM_LARGE_SHIELD)
            stat = you.dex() * 12 + you.strength() * 26;
        else
            stat = you.dex() * 19 + you.strength() * 19;
        stat = stat * (base_shield + 13) / 26;
    }
    else
    {
        if (you.duration[DUR_MAGIC_SHIELD])
            shield += 900 + you.skill(SK_EVOCATIONS, 75);

        if (!you.duration[DUR_FIRE_SHIELD]
            && you.duration[DUR_CONDENSATION_SHIELD])
        {
            shield += 800 + you.skill(SK_ICE_MAGIC, 60);
        }
    }

    if (you.duration[DUR_DIVINE_SHIELD])
    {
        shield += you.attribute[ATTR_DIVINE_SHIELD] * 150;
        stat = max(stat, int(you.attribute[ATTR_DIVINE_SHIELD] * 300));
    }

    if (shield + stat > 0
        && (player_wearing_slot(EQ_SHIELD) || you.duration[DUR_DIVINE_SHIELD]))
    {
        shield += you.skill(SK_SHIELDS, 38)
                + min(you.skill(SK_SHIELDS, 38), 3 * 38);
    }

    // mutations
    // +2, +4, +6
    shield += (player_mutation_level(MUT_LARGE_BONE_PLATES) > 0
               ? player_mutation_level(MUT_LARGE_BONE_PLATES) * 200
               : 0);

    return (shield + stat + 50) / 100;
}

int player_sust_abil(bool calc_unid)
{
    int sa = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
        sa += you.wearing(EQ_RINGS, RING_SUSTAIN_ABILITIES, calc_unid);

    if (sa > 2)
        sa = 2;

    return sa;
}

int carrying_capacity(burden_state_type bs)
{
    // Yuck.  We need this for gameplay - it nerfs small forms too much
    // otherwise - but there's no good way to rationalize here...  --sorear
    const int used_weight = max(you.body_weight(), you.body_weight(true));

    int cap = 2 * used_weight + you.strength() * 250 + 1000;
    // We are nice to the lighter species in that strength adds absolutely
    // instead of relatively to body weight. --dpeg

    if (you.stat_zero[STAT_STR])
        cap /= 2;

    if (bs == BS_UNENCUMBERED)
        return ((cap * 5) / 6);
    else if (bs == BS_ENCUMBERED)
        return ((cap * 11) / 12);
    else
        return cap;
}

int burden_change(void)
{
    const burden_state_type old_burdenstate = you.burden_state;

    you.burden = 0;

    for (int bu = 0; bu < ENDOFPACK; bu++)
    {
        if (you.inv[bu].quantity < 1)
            continue;

        you.burden += item_mass(you.inv[bu]) * you.inv[bu].quantity;
    }

    you.burden_state = BS_UNENCUMBERED;
    set_redraw_status(REDRAW_BURDEN);
    you.redraw_evasion = true;

    // changed the burdened levels to match the change to max_carried
    if (you.burden <= carrying_capacity(BS_UNENCUMBERED))
    {
        you.burden_state = BS_UNENCUMBERED;

        // this message may have to change, just testing {dlb}
        if (old_burdenstate != you.burden_state)
            mpr("Your possessions no longer seem quite so burdensome.");
    }
    else if (you.burden <= carrying_capacity(BS_ENCUMBERED))
    {
        you.burden_state = BS_ENCUMBERED;

        if (old_burdenstate != you.burden_state)
        {
            mpr("You are being weighed down by all of your possessions.");
            learned_something_new(HINT_HEAVY_LOAD);
        }
    }
    else
    {
        you.burden_state = BS_OVERLOADED;

        if (old_burdenstate != you.burden_state)
        {
            mpr("You are being crushed by all of your possessions.");
            learned_something_new(HINT_HEAVY_LOAD);
        }
    }

    // Stop travel if we get burdened (as from potions of might wearing off).
    if (you.burden_state > old_burdenstate)
        interrupt_activity(AI_BURDEN_CHANGE);

    return you.burden;
}

void forget_map(bool rot)
{
    ASSERT(!crawl_state.game_is_arena());

    // If forgetting was intentional, clear the travel trail.
    if (!rot)
        clear_travel_trail();

    // Labyrinth and the Abyss use special rotting rules.
    const bool rot_resist = player_in_branch(BRANCH_LABYRINTH)
                                && you.species == SP_MINOTAUR
                            || player_in_branch(BRANCH_ABYSS)
                                && you_worship(GOD_LUGONU);
    const double geometric_chance = 0.99;
    const int radius = (rot_resist ? 200 : 100);

    const int scalar = 0xFF;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def &p = *ri;
        if (you.see_cell(p) || !env.map_knowledge(p).known())
            continue;

        if (rot)
        {
            const int dist = distance2(you.pos(), p);
            int chance = pow(geometric_chance,
                             max(1, (dist - radius) / 40)) * scalar;
            if (x_chance_in_y(chance, scalar))
                continue;
        }

        env.map_knowledge(p).clear();
        if (env.map_forgotten.get())
            (*env.map_forgotten.get())(p).clear();
        StashTrack.update_stash(p);
#ifdef USE_TILE
        tile_forget_map(p);
#endif
    }

    ash_detect_portals(is_map_persistent());
#ifdef USE_TILE
    tiles.update_minimap_bounds();
#endif
}

static void _remove_temp_mutations()
{
    int num_remove = min(you.attribute[ATTR_TEMP_MUTATIONS],
        max(you.attribute[ATTR_TEMP_MUTATIONS] * 5 / 12 - random2(3),
        2 + random2(3)));

    if (num_remove >= you.attribute[ATTR_TEMP_MUTATIONS])
        mpr("You feel the corruption within you wane completely.", MSGCH_DURATION);
    else
        mpr("You feel the corruption within you wane somewhat.", MSGCH_DURATION);

    for (int i = 0; i < num_remove; ++i)
        delete_temp_mutation();

    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
    {
        you.attribute[ATTR_TEMP_MUT_XP] +=
            min(you.experience_level, 17) * (350 + roll_dice(5, 350)) / 17;
    }
}

int get_exp_progress()
{
    if (you.experience_level >= 27)
        return 0;

    const int current = exp_needed(you.experience_level);
    const int next    = exp_needed(you.experience_level + 1);
    if (next == current)
        return 0;
    return ((you.experience - current) * 100 / (next - current));
}

void gain_exp(unsigned int exp_gained, unsigned int* actual_gain)
{
    if (crawl_state.game_is_arena())
        return;

    if (crawl_state.game_is_zotdef())
    {
        you.zot_points += exp_gained;
        // All XP, for some reason Sprint speeds up only skill training,
        // but not levelling, Ash skill transfer, etc.
        exp_gained *= 2;
    }

    if (player_under_penance(GOD_ASHENZARI))
        ash_reduce_penance(exp_gained);

    const unsigned int old_exp = you.experience;

    dprf("gain_exp: %d", exp_gained);

    if (you.transfer_skill_points > 0)
    {
        // Can happen if the game got interrupted during target skill choice.
        if (is_invalid_skill(you.transfer_to_skill))
        {
            you.transfer_from_skill = SK_NONE;
            you.transfer_skill_points = 0;
            you.transfer_total_skill_points = 0;
        }
        else
        {
            int amount = exp_gained * 10
                                / calc_skill_cost(you.skill_cost_level);
            if (amount >= 20 || one_chance_in(20 - amount))
            {
                amount = max(20, amount);
                transfer_skill_points(you.transfer_from_skill,
                                      you.transfer_to_skill, amount, false);
            }
        }
    }

    if (you.experience + exp_gained > (unsigned int)MAX_EXP_TOTAL)
        you.experience = MAX_EXP_TOTAL;
    else
        you.experience += exp_gained;

    you.attribute[ATTR_EVOL_XP] += exp_gained;

    if (!you.sage_skills.empty())
    {
        int which_sage = random2(you.sage_skills.size());
        skill_type skill = you.sage_skills[which_sage];

#if TAG_MAJOR_VERSION > 34
        // These are supposed to be purged from the sage lists in
        // _change_skill_level()
        ASSERT(you.skills[skill] < 27);
#endif

        // FIXME: shouldn't use more XP than needed to max the skill
        const int old_avail = you.exp_available;
        // Bonus skill training from Sage.
        you.exp_available =
            div_rand_round(exp_gained * (you.sage_bonus[which_sage] + 50), 100);
        you.sage_xp[which_sage] -= you.exp_available;
        train_skill(skill, you.exp_available);
        you.exp_available = old_avail;
        exp_gained = div_rand_round(exp_gained, 2);

        if (you.sage_xp[which_sage] <= 0
#if TAG_MAJOR_VERSION == 34
            || you.skills[skill] == 27
#endif
            )
        {
            mprf("You feel less studious about %s.", skill_name(skill));
            erase_any(you.sage_skills, which_sage);
            erase_any(you.sage_xp, which_sage);
            erase_any(you.sage_bonus, which_sage);
        }
    }

    if (crawl_state.game_is_sprint())
        exp_gained = sprint_modify_exp(exp_gained);

    you.exp_available += exp_gained;

    train_skills();
    while (check_selected_skills()
           && you.exp_available >= calc_skill_cost(you.skill_cost_level))
    {
        train_skills();
    }

    if (you.exp_available >= calc_skill_cost(you.skill_cost_level))
        you.exp_available = calc_skill_cost(you.skill_cost_level);

    level_change();

    if (actual_gain != NULL)
        *actual_gain = you.experience - old_exp;

    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
    {
        you.attribute[ATTR_TEMP_MUT_XP] -= exp_gained;
        if (you.attribute[ATTR_TEMP_MUT_XP] <= 0)
            _remove_temp_mutations();
    }

    recharge_elemental_evokers(exp_gained);

    if (you.attribute[ATTR_XP_DRAIN])
    {
        int loss = div_rand_round(exp_gained * 3 / 2,
                                  calc_skill_cost(you.skill_cost_level));

        // Make it easier to recover from very heavy levels of draining
        // (they're nasty enough as it is)
        loss = loss * (1 + (you.attribute[ATTR_XP_DRAIN] / 250.0f));

        dprf("Lost %d of %d draining points", loss, you.attribute[ATTR_XP_DRAIN]);

        you.attribute[ATTR_XP_DRAIN] -= loss;
        if (you.attribute[ATTR_XP_DRAIN] <= 0)
        {
            you.attribute[ATTR_XP_DRAIN] = 0;
            mpr("Your life force feels restored.", MSGCH_RECOVERY);
        }
    }
}

static void _draconian_scale_colour_message()
{
    switch (you.species)
    {
    case SP_RED_DRACONIAN:
        mpr("Your scales start taking on a fiery red colour.",
            MSGCH_INTRINSIC_GAIN);
        perma_mutate(MUT_HEAT_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_WHITE_DRACONIAN:
        mpr("Your scales start taking on an icy white colour.",
            MSGCH_INTRINSIC_GAIN);
        perma_mutate(MUT_COLD_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_GREEN_DRACONIAN:
        mpr("Your scales start taking on a lurid green colour.",
            MSGCH_INTRINSIC_GAIN);
        perma_mutate(MUT_POISON_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_YELLOW_DRACONIAN:
        mpr("Your scales start taking on a golden yellow colour.",
            MSGCH_INTRINSIC_GAIN);
        break;

    case SP_GREY_DRACONIAN:
        mpr("Your scales start taking on a dull iron-grey colour.",
            MSGCH_INTRINSIC_GAIN);
        perma_mutate(MUT_UNBREATHING, 1, "draconian maturity");
        break;

    case SP_BLACK_DRACONIAN:
        mpr("Your scales start taking on a glossy black colour.",
            MSGCH_INTRINSIC_GAIN);
        perma_mutate(MUT_SHOCK_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_PURPLE_DRACONIAN:
        mpr("Your scales start taking on a rich purple colour.",
            MSGCH_INTRINSIC_GAIN);
        break;

    case SP_MOTTLED_DRACONIAN:
        mpr("Your scales start taking on a weird mottled pattern.",
            MSGCH_INTRINSIC_GAIN);
        break;

    case SP_PALE_DRACONIAN:
        mpr("Your scales start fading to a pale cyan-grey colour.",
            MSGCH_INTRINSIC_GAIN);
        break;

    case SP_BASE_DRACONIAN:
        mpr("");
        break;

    default:
        break;
    }
}

bool will_gain_life(int lev)
{
    if (lev < you.attribute[ATTR_LIFE_GAINED] - 2)
        return false;

    return (you.lives + you.deaths < (lev - 1) / 3);
}

static void _felid_extra_life()
{
    if (will_gain_life(you.max_level)
        && you.lives < 2)
    {
        you.lives++;
        mpr("Extra life!", MSGCH_INTRINSIC_GAIN);
        you.attribute[ATTR_LIFE_GAINED] = you.max_level;
        // Should play the 1UP sound from SMB...
    }
}

void level_change(int source, const char* aux, bool skip_attribute_increase)
{
    const bool wiz_cmd = crawl_state.prev_cmd == CMD_WIZARD
                      || crawl_state.repeat_cmd == CMD_WIZARD;

    // necessary for the time being, as level_change() is called
    // directly sometimes {dlb}
    you.redraw_experience = true;

    while (you.experience < exp_needed(you.experience_level))
        lose_level(source, aux);

    while (you.experience_level < 27
           && you.experience >= exp_needed(you.experience_level + 1))
    {
        if (!skip_attribute_increase && !wiz_cmd)
        {
            crawl_state.cancel_cmd_all();

            if (is_processing_macro())
                flush_input_buffer(FLUSH_ABORT_MACRO);
        }

        // [ds] Make sure we increment you.experience_level and apply
        // any stat/hp increases only after we've cleared all prompts
        // for this experience level. If we do part of the work before
        // the prompt, and a player on telnet gets disconnected, the
        // SIGHUP will save Crawl in the in-between state and rob the
        // player of their level-up perks.

        const int new_exp = you.experience_level + 1;

        if (new_exp <= you.max_level)
        {
            mprf(MSGCH_INTRINSIC_GAIN,
                 "Welcome back to level %d!", new_exp);

            // No more prompts for this XL past this point.

            you.experience_level = new_exp;
        }
        else  // Character has gained a new level
        {
            // Don't want to see the dead creature at the prompt.
            redraw_screen();
            // There may be more levels left to gain.
            you.redraw_experience = true;

            if (new_exp == 27)
            {
                mpr("You have reached level 27, the final one!",
                    MSGCH_INTRINSIC_GAIN);
            }
            else
            {
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level %d!",
                     new_exp);
            }

            if (!(new_exp % 3) && !skip_attribute_increase)
                if (!attribute_increase())
                    return; // abort level gain, the xp is still there

            crawl_state.stat_gain_prompt = false;
            you.experience_level = new_exp;
            you.max_level = you.experience_level;

#ifdef USE_TILE_LOCAL
            // In case of intrinsic ability changes.
            tiles.layout_statcol();
            redraw_screen();
#endif

            switch (you.species)
            {
            case SP_HUMAN:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_HIGH_ELF:
                if (!(you.experience_level % 3))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEEP_ELF:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_INT, 1, false, "level gain");
                break;

            case SP_SLUDGE_ELF:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEEP_DWARF:
                if (you.experience_level == 14)
                {
                    mpr("You feel somewhat more resistant.",
                        MSGCH_INTRINSIC_GAIN);
                    perma_mutate(MUT_NEGATIVE_ENERGY_RESISTANCE, 1, "level up");
                }

                if ((you.experience_level == 9)
                    || (you.experience_level == 18))
                {
                    perma_mutate(MUT_PASSIVE_MAPPING, 1, "level up");
                }

                if (!(you.experience_level % 4))
                {
                    modify_stat(coinflip() ? STAT_STR
                                           : STAT_INT, 1, false,
                                "level gain");
                }
                break;

            case SP_HALFLING:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_DEX, 1, false, "level gain");
                break;

            case SP_KOBOLD:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_HILL_ORC:
            case SP_LAVA_ORC:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_MUMMY:
                if (you.experience_level == 13 || you.experience_level == 26)
                {
                    mpr("You feel more in touch with the powers of death.",
                        MSGCH_INTRINSIC_GAIN);
                }

                if (you.experience_level == 13)  // level 13 for now -- bwr
                {
                    mpr("You can now infuse your body with magic to restore "
                        "decomposition.", MSGCH_INTRINSIC_GAIN);
                }
                break;

            case SP_VAMPIRE:
                if (you.experience_level == 3)
                {
                    if (you.hunger_state > HS_SATIATED)
                    {
                        mpr("If you weren't so full you could now transform "
                            "into a vampire bat.", MSGCH_INTRINSIC_GAIN);
                    }
                    else
                    {
                        mpr("You can now transform into a vampire bat.",
                            MSGCH_INTRINSIC_GAIN);
                    }
                }
                else if (you.experience_level == 6)
                {
                    mpr("You can now bottle potions of blood from corpses.",
                        MSGCH_INTRINSIC_GAIN);
                }
                break;

            case SP_NAGA:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (!(you.experience_level % 3))
                {
                    mpr("Your skin feels tougher.", MSGCH_INTRINSIC_GAIN);
                    you.redraw_armour_class = true;
                }

                if (you.experience_level == 13)
                {
                    mpr("Your tail grows strong enough to constrict"
                        " your enemies.", MSGCH_INTRINSIC_GAIN);
                }
                break;

            case SP_TROLL:
                if (!(you.experience_level % 3))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_OGRE:
                if (!(you.experience_level % 3))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_BASE_DRACONIAN:
                if (you.experience_level >= 7)
                {
                    you.species = random_draconian_player_species();

                    // We just changed our aptitudes, so some skills may now
                    // be at the wrong level (with negative progress); if we
                    // print anything in this condition, we might trigger a
                    // --More--, a redraw, and a crash (#6376 on Mantis).
                    //
                    // Hence we first fix up our skill levels silently (passing
                    // do_level_up = false) but save the old values; then when
                    // we want the messages later, we restore the old skill
                    // levels and call check_skill_level_change() again, this
                    // time passing do_update = true.

                    uint8_t saved_skills[NUM_SKILLS];
                    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
                    {
                        saved_skills[i] = you.skills[i];
                        check_skill_level_change(static_cast<skill_type>(i),
                                                 false);
                    }
                    // The player symbol depends on species.
                    update_player_symbol();
#ifdef USE_TILE
                    init_player_doll();
#endif
                    _draconian_scale_colour_message();

                    // Produce messages about skill increases/decreases. We
                    // restore one skill level at a time so that at most the
                    // skill being checked is at the wrong level.
                    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
                    {
                        you.skills[i] = saved_skills[i];
                        check_skill_level_change(static_cast<skill_type>(i));
                    }

                    redraw_screen();
                }
            case SP_RED_DRACONIAN:
            case SP_WHITE_DRACONIAN:
            case SP_GREEN_DRACONIAN:
            case SP_YELLOW_DRACONIAN:
            case SP_GREY_DRACONIAN:
            case SP_BLACK_DRACONIAN:
            case SP_PURPLE_DRACONIAN:
            case SP_MOTTLED_DRACONIAN:
            case SP_PALE_DRACONIAN:
                if (!(you.experience_level % 3))
                {
                    mpr("Your scales feel tougher.", MSGCH_INTRINSIC_GAIN);
                    you.redraw_armour_class = true;
                }

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (you.experience_level == 14)
                {
                    switch (you.species)
                    {
                    case SP_GREEN_DRACONIAN:
                        perma_mutate(MUT_STINGER, 1, "draconian growth");
                        break;
                    case SP_YELLOW_DRACONIAN:
                        perma_mutate(MUT_ACIDIC_BITE, 1, "draconian growth");
                        break;
                    case SP_BLACK_DRACONIAN:
                        perma_mutate(MUT_BIG_WINGS, 1, "draconian growth");
                        mpr("You can now fly continuously.", MSGCH_INTRINSIC_GAIN);
                        break;
                    default:
                        break;
                    }
                }
                break;

            case SP_CENTAUR:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEMIGOD:
                if (!(you.experience_level % 2))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_SPRIGGAN:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_MINOTAUR:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEMONSPAWN:
            {
                bool gave_message = false;
                int level = 0;
                mutation_type first_body_facet = NUM_MUTATIONS;

                for (unsigned i = 0; i < you.demonic_traits.size(); ++i)
                {
                    if (is_body_facet(you.demonic_traits[i].mutation))
                    {
                        if (first_body_facet < NUM_MUTATIONS
                            && you.demonic_traits[i].mutation
                                != first_body_facet)
                        {
                            if (you.experience_level == level)
                            {
                                mpr("You feel monstrous as your "
                                     "demonic heritage exerts itself.",
                                     MSGCH_MUTATION);

                                mark_milestone("monstrous", "is a "
                                               "monstrous demonspawn!");
                            }
                            break;
                        }

                        if (first_body_facet == NUM_MUTATIONS)
                        {
                            first_body_facet = you.demonic_traits[i].mutation;
                            level = you.demonic_traits[i].level_gained;
                        }
                    }
                }

                for (unsigned i = 0; i < you.demonic_traits.size(); ++i)
                {
                    if (you.demonic_traits[i].level_gained
                        == you.experience_level)
                    {
                        if (!gave_message)
                        {
                            mpr("Your demonic ancestry asserts itself...",
                                MSGCH_INTRINSIC_GAIN);

                            gave_message = true;
                        }

                        perma_mutate(you.demonic_traits[i].mutation, 1,
                                     "demonic ancestry");
                    }
                }

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;
            }

            case SP_GHOUL:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_TENGU:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (you.experience_level == 5)
                {
                    mpr("You have gained the ability to fly.",
                        MSGCH_INTRINSIC_GAIN);
                }
                else if (you.experience_level == 15)
                    mpr("You can now fly continuously.", MSGCH_INTRINSIC_GAIN);
                break;

            case SP_MERFOLK:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_FELID:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }

                if (you.experience_level == 6 || you.experience_level == 12)
                    perma_mutate(MUT_SHAGGY_FUR, 1, "growing up");

                _felid_extra_life();
                break;

            case SP_OCTOPODE:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_DJINNI:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_GARGOYLE:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_INT), 1, false,
                                "level gain");
                }

                if (you.experience_level == 14)
                {
                    perma_mutate(MUT_BIG_WINGS, 1, "gargoyle growth");
                    mpr("You can now fly continuously.", MSGCH_INTRINSIC_GAIN);
                }

                break;

            default:
                break;
            }
        }

        // zot defence abilities; must also be updated in abl-show.cc when these levels are changed
        if (crawl_state.game_is_zotdef())
        {
            if (you.experience_level == 1)
                mpr("Your Zot abilities now extend through the making of dart traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 2)
                mpr("Your Zot abilities now extend through the making of oklob saplings.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 3)
                mpr("Your Zot abilities now extend through the making of arrow traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 4)
                mpr("Your Zot abilities now extend through the making of plants.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 4)
                mpr("Your Zot abilities now extend through removing curses.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 5)
                mpr("Your Zot abilities now extend through the making of burning bushes.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 6)
                mpr("Your Zot abilities now extend through the making of altars and grenades.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 7)
                mpr("Your Zot abilities now extend through the making of oklob plants.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 8)
                mpr("Your Zot abilities now extend through the making of net traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 9)
                mpr("Your Zot abilities now extend through the making of ice statues.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 10)
                mpr("Your Zot abilities now extend through the making of spear traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 11)
                mpr("Your Zot abilities now extend through the making of alarm traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 12)
                mpr("Your Zot abilities now extend through the making of mushroom circles.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 13)
                mpr("Your Zot abilities now extend through the making of bolt traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 14)
                mpr("Your Zot abilities now extend through the making of orange crystal statues.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 15)
                mpr("Your Zot abilities now extend through the making of needle traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 16)
                mpr("Your Zot abilities now extend through self-teleportation.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 17)
                mpr("Your Zot abilities now extend through making water.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 19)
                mpr("Your Zot abilities now extend through the making of lightning spires.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 20)
                mpr("Your Zot abilities now extend through the making of silver statues.", MSGCH_INTRINSIC_GAIN);
            // gold and bazaars gained together
            if (you.experience_level == 21)
                mpr("Your Zot abilities now extend through the making of bazaars.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 21)
                mpr("Your Zot abilities now extend through acquiring gold.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 22)
                mpr("Your Zot abilities now extend through the making of oklob circles.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 23)
                mpr("Your Zot abilities now extend through invoking Sage effects.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 24)
                mpr("Your Zot abilities now extend through acquirement.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 25)
                mpr("Your Zot abilities now extend through the making of blade traps.", MSGCH_INTRINSIC_GAIN);
            if (you.experience_level == 26)
                mpr("Your Zot abilities now extend through the making of curse skulls.", MSGCH_INTRINSIC_GAIN);
#if 0
            if (you.experience_level == 27)
                mpr("Your Zot abilities now extend through the making of teleport traps.", MSGCH_INTRINSIC_GAIN);
#endif
        }

        const int old_hp = you.hp;
        const int old_maxhp = you.hp_max;
        const int old_mp = you.magic_points;
        const int old_maxmp = you.max_magic_points;

        // recalculate for game
        calc_hp();
        calc_mp();

        you.hp = old_hp * you.hp_max / old_maxhp;
        you.magic_points = old_maxmp > 0 ?
          old_mp * you.max_magic_points / old_maxmp : you.max_magic_points;

        // Get "real" values for note-taking, i.e. ignore Berserk,
        // transformations or equipped items.
        const int note_maxhp = get_real_hp(false, false);
        const int note_maxmp = get_real_mp(false);

        char buf[200];
        if (you.species != SP_DJINNI)
        {
            sprintf(buf, "HP: %d/%d MP: %d/%d",
                    min(you.hp, note_maxhp), note_maxhp,
                    min(you.magic_points, note_maxmp), note_maxmp);
        }
        else
        {
            // Djinn don't HP/MP
            sprintf(buf, "EP: %d/%d",
                    min(you.hp, note_maxhp + note_maxmp),
                    note_maxhp + note_maxmp);
        }
        take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

        xom_is_stimulated(12);

        learned_something_new(HINT_NEW_LEVEL);
    }

    while (you.experience >= exp_needed(you.max_level + 1))
    {
        ASSERT(you.experience_level == 27);
        ASSERT(you.max_level < 127); // marshalled as an 1-byte value
        you.max_level++;
        if (you.species == SP_FELID)
            _felid_extra_life();
    }

    you.redraw_title = true;

#ifdef DGL_WHEREIS
    whereis_record();
#endif

    // Hints mode arbitrarily ends at xp 7.
    if (crawl_state.game_is_hints() && you.experience_level >= 7)
        hints_finished();
}

void adjust_level(int diff, bool just_xp)
{
    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);

    if (you.experience_level + diff < 1)
        you.experience = 0;
    else if (you.experience_level + diff >= 27)
        you.experience = max(you.experience, exp_needed(27));
    else
    {
        while (diff < 0 && you.experience >= exp_needed(27))
        {
            // Having XP for level 53 and going back to 26 due to a single
            // card would mean your felid is not going to get any extra lives
            // in foreseable future.
            you.experience -= exp_needed(27) - exp_needed(26);
            diff++;
        }
        int old_min = exp_needed(you.experience_level);
        int old_max = exp_needed(you.experience_level + 1);
        int new_min = exp_needed(you.experience_level + diff);
        int new_max = exp_needed(you.experience_level + 1 + diff);
        dprf("XP before: %d\n", you.experience);
        dprf("%4.2f of %d..%d to %d..%d",
             (you.experience - old_min) * 1.0 / (old_max - old_min),
             old_min, old_max, new_min, new_max);

        you.experience = ((int64_t)(new_max - new_min))
                       * (you.experience - old_min)
                       / (old_max - old_min)
                       + new_min;
        dprf("XP after: %d\n", you.experience);
    }

    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);

    if (!just_xp)
        level_change();
}

// Here's a question for you: does the ordering of mods make a difference?
// (yes) -- are these things in the right order of application to stealth?
// - 12mar2000 {dlb}
int check_stealth(void)
{
    ASSERT(!crawl_state.game_is_arena());
    // Extreme stealthiness can be enforced by wizmode stealth setting.
    if (crawl_state.disables[DIS_MON_SIGHT])
        return 1000;

    if (you.attribute[ATTR_SHADOWS] || you.berserk() || you.stat_zero[STAT_DEX])
        return 0;

    int stealth = you.dex() * 3;

    int race_mod = 0;
    if (player_genus(GENPC_DRACONIAN))
        race_mod = 12;
    else
    {
        switch (you.species) // why not use body_size here?
        {
        case SP_TROLL:
        case SP_OGRE:
        case SP_CENTAUR:
        case SP_DJINNI:
            race_mod = 9;
            break;
        case SP_MINOTAUR:
            race_mod = 12;
            break;
        case SP_VAMPIRE:
            // Thirsty/bat-form vampires are (much) more stealthy
            if (you.hunger_state == HS_STARVING)
                race_mod = 21;
            else if (you.form == TRAN_BAT
                     || you.hunger_state <= HS_NEAR_STARVING)
            {
                race_mod = 20;
            }
            else if (you.hunger_state < HS_SATIATED)
                race_mod = 19;
            else
                race_mod = 18;
            break;
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:       // not small but very good at stealth
        case SP_FELID:
        case SP_OCTOPODE:
            race_mod = 18;
            break;
        default:
            race_mod = 15;
            break;
        }
    }

    switch (you.form)
    {
    case TRAN_FUNGUS:
        race_mod = 30;
        break;
    case TRAN_TREE:
        race_mod = 27; // masquerading as scenery
        break;
    case TRAN_SPIDER:
    case TRAN_JELLY:
    case TRAN_WISP:
        race_mod = 21;
        break;
    case TRAN_ICE_BEAST:
        race_mod = 15;
        break;
    case TRAN_STATUE:
        race_mod -= 3; // depends on the base race
        break;
    case TRAN_DRAGON:
        race_mod = 6;
        break;
    case TRAN_PORCUPINE:
        race_mod = 12; // small but noisy
        break;
    case TRAN_PIG:
        race_mod = 9; // trotters, oinking...
        break;
    case TRAN_BAT:
        if (you.species != SP_VAMPIRE)
            race_mod = 17;
        break;
    case TRAN_BLADE_HANDS:
        if (you.species == SP_FELID && !you.airborne())
            stealth -= 50; // a constant penalty
        break;
    case TRAN_NONE:
    case TRAN_APPENDAGE:
    case TRAN_LICH:
        break;
    }

    stealth += you.skill(SK_STEALTH, race_mod);

    if (you.burden_state > BS_UNENCUMBERED)
        stealth /= you.burden_state;

    if (you.confused())
        stealth /= 3;

    if (you.duration[DUR_SWIFTNESS] > 0)
        stealth /= 2;

    const item_def *arm = you.slot_item(EQ_BODY_ARMOUR, false);
    const item_def *cloak = you.slot_item(EQ_CLOAK, false);
    const item_def *boots = you.slot_item(EQ_BOOTS, false);

    if (arm)
    {
        // [ds] New stealth penalty formula from rob: SP = 6 * (EP^2)
        // Now 2 * EP^2 / 3 after EP rescaling.
        const int ep = -property(*arm, PARM_EVASION);
        const int penalty = 2 * ep * ep / 3;
#if 0
        dprf("Stealth penalty for armour (ep: %d): %d", ep, penalty);
#endif
        stealth -= penalty;
    }

    if (!you.suppressed())
        stealth += you.scan_artefacts(ARTP_STEALTH);

    // Not exactly magical, so not suppressed.
    if (cloak && get_equip_race(*cloak) == ISFLAG_ELVEN)
        stealth += 20;

    if (you.duration[DUR_STEALTH])
        stealth += 80;

    if (you.duration[DUR_AGILITY])
        stealth += 50;

    if (you.airborne())
        stealth += 10;

    else if (you.in_water())
    {
        // Merfolk can sneak up on monsters underwater -- bwr
        if (you.fishtail || you.species == SP_OCTOPODE)
            stealth += 50;
        else if (!you.can_swim() && !you.extra_balanced())
            stealth /= 2;       // splashy-splashy
    }

    // No stealth bonus from boots if you're airborne or in water
    else if (boots)
    {
        if (!you.suppressed() && get_armour_ego_type(*boots) == SPARM_STEALTH)
            stealth += 50;

        // Not exactly magical, so not suppressed.
        if (get_equip_race(*boots) == ISFLAG_ELVEN)
            stealth += 20;
    }

    else if (player_mutation_level(MUT_HOOVES) > 0)
        stealth -= 5 + 5 * player_mutation_level(MUT_HOOVES);

    else if (you.species == SP_FELID && (!you.form || you.form == TRAN_APPENDAGE))
        stealth += 20;  // paws

    // Radiating silence is the negative complement of shouting all the
    // time... a sudden change from background noise to no noise is going
    // to clue anything in to the fact that something is very wrong...
    // a personal silence spell would naturally be different, but this
    // silence radiates for a distance and prevents monster spellcasting,
    // which pretty much gives away the stealth game.
    // this penalty is dependent on the actual amount of ambient noise
    // in the level -doy
    if (you.duration[DUR_SILENCE])
        stealth -= 50 + current_level_ambient_noise();

    // Mutations.
    stealth += 40 * player_mutation_level(MUT_NIGHTSTALKER);
    stealth += 25 * player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);
    stealth += 40 * player_mutation_level(MUT_CAMOUFLAGE);
    if (player_mutation_level(MUT_TRANSLUCENT_SKIN) > 1)
        stealth += 20 * (player_mutation_level(MUT_TRANSLUCENT_SKIN) - 1);

    // it's easier to be stealthy when there's a lot of background noise
    stealth += 2 * current_level_ambient_noise();

    // If you've been tagged with Corona or are Glowing, the glow
    // makes you extremely unstealthy.
    // The darker it is, the bigger the penalty.
    if (you.backlit())
        stealth = (2 * you.current_vision * stealth) / (5 * LOS_RADIUS);
    // On the other hand, shrouding has the reverse effect:
    if (you.umbra())
        stealth = (2 * LOS_RADIUS * stealth) / you.current_vision;
    // The shifting glow from the Orb, while too unstable to negate invis
    // or affect to-hit, affects stealth even more than regular glow.
    if (orb_haloed(you.pos()))
        stealth /= 3;

    stealth = max(0, stealth);

    return stealth;
}

// Returns the medium duration value which is usually announced by a special
// message ("XY is about to time out") or a change of colour in the
// status display.
// Note that these values cannot be relied on when playing since there are
// random decrements precisely to avoid this.
int get_expiration_threshold(duration_type dur)
{
    switch (dur)
    {
    case DUR_PETRIFYING:
        return (1 * BASELINE_DELAY);

    case DUR_QUAD_DAMAGE:
        return (3 * BASELINE_DELAY); // per client.qc

    case DUR_FIRE_SHIELD:
    case DUR_SILENCE: // no message
        return (5 * BASELINE_DELAY);

    case DUR_DEFLECT_MISSILES:
    case DUR_REPEL_MISSILES:
    case DUR_REGENERATION:
    case DUR_RESISTANCE:
    case DUR_SWIFTNESS:
    case DUR_INVIS:
    case DUR_HASTE:
    case DUR_BERSERK:
    case DUR_ICY_ARMOUR:
    case DUR_CONDENSATION_SHIELD:
    case DUR_PHASE_SHIFT:
    case DUR_CONTROL_TELEPORT:
    case DUR_DEATH_CHANNEL:
    case DUR_SHROUD_OF_GOLUBRIA:
    case DUR_INFUSION:
    case DUR_SONG_OF_SLAYING:
        return (6 * BASELINE_DELAY);

    case DUR_FLIGHT:
    case DUR_TRANSFORMATION: // not on status
    case DUR_DEATHS_DOOR:    // not on status
    case DUR_SLIMIFY:
    case DUR_SONG_OF_SHIELDING:
        return (10 * BASELINE_DELAY);

    // These get no messages when they "flicker".
    case DUR_BARGAIN:
        return (15 * BASELINE_DELAY);

    case DUR_CONFUSING_TOUCH:
        return (20 * BASELINE_DELAY);

    case DUR_ANTIMAGIC:
        return you.hp_max; // not so severe anymore

    default:
        return 0;
    }
}

// Is a given duration about to expire?
bool dur_expiring(duration_type dur)
{
    const int value = you.duration[dur];
    if (value <= 0)
        return false;

    return (value <= get_expiration_threshold(dur));
}

static void _output_expiring_message(duration_type dur, const char* msg)
{
    if (you.duration[dur])
    {
        const bool expires = dur_expiring(dur);
        mprf("%s%s", expires ? "Expiring: " : "", msg);
    }
}

static void _display_vampire_status()
{
    string msg = "At your current hunger state you ";
    vector<string> attrib;

    switch (you.hunger_state)
    {
        case HS_STARVING:
            attrib.push_back("resist poison");
            attrib.push_back("significantly resist cold");
            attrib.push_back("strongly resist negative energy");
            attrib.push_back("resist torment");
            attrib.push_back("do not heal.");
            break;
        case HS_NEAR_STARVING:
            attrib.push_back("resist poison");
            attrib.push_back("significantly resist cold");
            attrib.push_back("strongly resist negative energy");
            attrib.push_back("have an extremely slow metabolism");
            attrib.push_back("heal slowly.");
            break;
        case HS_VERY_HUNGRY:
        case HS_HUNGRY:
            attrib.push_back("resist poison");
            attrib.push_back("resist cold");
            attrib.push_back("significantly resist negative energy");
            if (you.hunger_state == HS_HUNGRY)
                attrib.push_back("have a slow metabolism");
            else
                attrib.push_back("have a very slow metabolism");
            attrib.push_back("heal slowly.");
            break;
        case HS_SATIATED:
            attrib.push_back("resist negative energy.");
            break;
        case HS_FULL:
            attrib.push_back("have a fast metabolism");
            attrib.push_back("heal quickly.");
            break;
        case HS_VERY_FULL:
            attrib.push_back("have a very fast metabolism");
            attrib.push_back("heal quickly.");
            break;
        case HS_ENGORGED:
            attrib.push_back("have an extremely fast metabolism");
            attrib.push_back("heal extremely quickly.");
            break;
    }

    if (!attrib.empty())
    {
        msg += comma_separated_line(attrib.begin(), attrib.end());
        mpr(msg.c_str());
    }
}

static void _display_movement_speed()
{
    const int move_cost = (player_speed() * player_movement_speed()) / 10;

    const bool water  = you.in_liquid();
    const bool swim   = you.swimming();

    const bool fly    = you.flight_mode();
    const bool swift  = (you.duration[DUR_SWIFTNESS] > 0);

    mprf("Your %s speed is %s%s%s.",
          // order is important for these:
          (swim)    ? "swimming" :
          (water)   ? "wading" :
          (fly)     ? "flying"
                    : "movement",

          (water && !swim)  ? "uncertain and " :
          (!water && swift) ? "aided by the wind" : "",

          (!water && swift) ? ((move_cost >= 10) ? ", but still "
                                                 : " and ")
                            : "",

          (move_cost <   8) ? "very quick" :
          (move_cost <  10) ? "quick" :
          (move_cost == 10) ? "average" :
          (move_cost <  13) ? "slow"
                            : "very slow");
}

static void _display_tohit()
{
#ifdef DEBUG_DIAGNOSTICS
    melee_attack attk(&you, NULL);

    const int to_hit = attk.calc_to_hit(false);

    dprf("To-hit: %d", to_hit);
#endif
/*
    // Messages based largely on percentage chance of missing the
    // average EV 10 humanoid, and very agile EV 30 (pretty much
    // max EV for monsters currently).
    //
    // "awkward"    - need lucky hit (less than EV)
    // "difficult"  - worse than 2 in 3
    // "hard"       - worse than fair chance
    mprf("%s given your current equipment.",
         (to_hit <   1) ? "You are completely incapable of fighting" :
         (to_hit <   5) ? "Hitting even clumsy monsters is extremely awkward" :
         (to_hit <  10) ? "Hitting average monsters is awkward" :
         (to_hit <  15) ? "Hitting average monsters is difficult" :
         (to_hit <  20) ? "Hitting average monsters is hard" :
         (to_hit <  30) ? "Very agile monsters are a bit awkward to hit" :
         (to_hit <  45) ? "Very agile monsters are a bit difficult to hit" :
         (to_hit <  60) ? "Very agile monsters are a bit hard to hit" :
         (to_hit < 100) ? "You feel comfortable with your ability to fight"
                        : "You feel confident with your ability to fight");
*/
}

static string _attack_delay_desc(int attack_delay)
{
    return ((attack_delay >= 200) ? "extremely slow" :
            (attack_delay >= 155) ? "very slow" :
            (attack_delay >= 125) ? "quite slow" :
            (attack_delay >= 105) ? "below average" :
            (attack_delay >=  95) ? "average" :
            (attack_delay >=  75) ? "above average" :
            (attack_delay >=  55) ? "quite fast" :
            (attack_delay >=  45) ? "very fast" :
            (attack_delay >=  35) ? "extremely fast" :
                                    "blindingly fast");
}

static void _display_attack_delay()
{
    melee_attack attk(&you, NULL);
    const int delay = attk.calc_attack_delay(false, false);

    // Scale to fit the displayed weapon base delay, i.e.,
    // normal speed is 100 (as in 100%).
    int avg;
    const item_def* weapon = you.weapon();
    if (weapon && is_range_weapon(*weapon))
        avg = launcher_final_speed(*weapon, you.shield(), false);
    else
        avg = 10 * delay;

    // Haste shouldn't be counted, but let's show finesse.
    if (you.duration[DUR_FINESSE])
        avg = max(20, avg / 2);

    string msg = "Your attack speed is " + _attack_delay_desc(avg)
                 + (you.wizard ? make_stringf(" (%d)", avg) : "") + ".";

    mpr(msg);
}

// forward declaration
static string _constriction_description();

void display_char_status()
{
    if (you.is_undead == US_SEMI_UNDEAD && you.hunger_state == HS_ENGORGED)
        mpr("You feel almost alive.");
    else if (you.is_undead)
        mpr("You are undead.");
    else if (you.duration[DUR_DEATHS_DOOR])
    {
        _output_expiring_message(DUR_DEATHS_DOOR,
                                 "You are standing in death's doorway.");
    }
    else
        mpr("You are alive.");

    const int halo_size = you.halo_radius2();
    if (halo_size >= 0)
    {
        if (halo_size > 37)
            mpr("You are illuminated by a large divine halo.");
        else if (halo_size > 10)
            mpr("You are illuminated by a divine halo.");
        else
            mpr("You are illuminated by a small divine halo.");
    }
    else if (you.haloed())
        mpr("An external divine halo illuminates you.");

    if (you.species == SP_VAMPIRE)
        _display_vampire_status();

    static int statuses[] = {
        STATUS_STR_ZERO, STATUS_INT_ZERO, STATUS_DEX_ZERO,
        DUR_PETRIFYING,
        DUR_TRANSFORMATION,
        STATUS_BURDEN,
        STATUS_MANUAL,
        STATUS_SAGE,
        DUR_BARGAIN,
        DUR_BREATH_WEAPON,
        DUR_LIQUID_FLAMES,
        DUR_FIRE_SHIELD,
        DUR_ICY_ARMOUR,
        DUR_ANTIMAGIC,
        STATUS_MISSILES,
        DUR_JELLY_PRAYER,
        STATUS_REGENERATION,
        DUR_SWIFTNESS,
        DUR_RESISTANCE,
        DUR_TELEPORT,
        DUR_CONTROL_TELEPORT,
        DUR_DISJUNCTION,
        DUR_DEATH_CHANNEL,
        DUR_PHASE_SHIFT,
        DUR_SILENCE,
        DUR_STONESKIN,
        DUR_INVIS,
        DUR_CONF,
        STATUS_BEHELD,
        DUR_PARALYSIS,
        DUR_PETRIFIED,
        DUR_SLEEP,
        DUR_EXHAUSTED,
        STATUS_SPEED,
        DUR_MIGHT,
        DUR_BRILLIANCE,
        DUR_AGILITY,
        DUR_DIVINE_VIGOUR,
        DUR_DIVINE_STAMINA,
        DUR_BERSERK,
        STATUS_AIRBORNE,
        STATUS_NET,
        DUR_POISONING,
        STATUS_SICK,
        STATUS_ROT,
        STATUS_CONTAMINATION,
        DUR_CONFUSING_TOUCH,
        DUR_SURE_BLADE,
        DUR_AFRAID,
        DUR_MIRROR_DAMAGE,
        DUR_SCRYING,
        STATUS_CLINGING,
        STATUS_HOVER,
        STATUS_FIREBALL,
        DUR_SHROUD_OF_GOLUBRIA,
        STATUS_BACKLIT,
        STATUS_UMBRA,
        STATUS_CONSTRICTED,
        STATUS_AUGMENTED,
        STATUS_SUPPRESSED,
        STATUS_SILENCE,
        DUR_SENTINEL_MARK,
        STATUS_RECALL,
        STATUS_LIQUEFIED,
        DUR_WATER_HOLD,
        DUR_FLAYED,
        DUR_RETCHING,
        DUR_WEAK,
        DUR_DIMENSION_ANCHOR,
        DUR_SPIRIT_HOWL,
        DUR_INFUSION,
        DUR_SONG_OF_SHIELDING,
        DUR_SONG_OF_SLAYING,
        STATUS_DRAINED,
        DUR_TOXIC_RADIANCE,
        DUR_RECITE,
        DUR_GRASPING_ROOTS,
        DUR_FIRE_VULN,
    };

    status_info inf;
    for (unsigned i = 0; i < ARRAYSZ(statuses); ++i)
    {
        if (fill_status_info(statuses[i], &inf) && !inf.long_text.empty())
            mpr(inf.long_text);
    }
    string cinfo = _constriction_description();
    if (!cinfo.empty())
        mpr(cinfo.c_str());

    _display_movement_speed();
    _display_tohit();
    _display_attack_delay();

    // magic resistance
    mprf("You are %s to hostile enchantments.",
         magic_res_adjective(player_res_magic(false)).c_str());
    dprf("MR: %d", you.res_magic());

    // character evaluates their ability to sneak around:
    mprf("You feel %s.", stealth_desc(check_stealth()).c_str());
    dprf("Stealth: %d", check_stealth());
}

bool player::clarity(bool calc_unid, bool items) const
{
    if (player_mutation_level(MUT_CLARITY))
        return true;

    if (religion == GOD_ASHENZARI && piety >= piety_breakpoint(2)
        && !player_under_penance())
    {
        return true;
    }

    return actor::clarity(calc_unid, items);
}

bool player::gourmand(bool calc_unid, bool items) const
{
    if (player_mutation_level(MUT_GOURMAND) > 0)
        return true;

    return actor::gourmand(calc_unid, items);
}

unsigned int exp_needed(int lev, int exp_apt)
{
    unsigned int level = 0;

    // Basic plan:
    // Section 1: levels  1- 5, second derivative goes 10-10-20-30.
    // Section 2: levels  6-13, second derivative is exponential/doubling.
    // Section 3: levels 14-27, second derivative is constant at 8470.

    // Here's a table:
    //
    // level      xp      delta   delta2
    // =====   =======    =====   ======
    //   1           0        0       0
    //   2          10       10      10
    //   3          30       20      10
    //   4          70       40      20
    //   5         140       70      30
    //   6         270      130      60
    //   7         520      250     120
    //   8        1010      490     240
    //   9        1980      970     480
    //  10        3910     1930     960
    //  11        7760     3850    1920
    //  12       15450     7690    3840
    //  13       26895    11445    3755
    //  14       45585    18690    7245
    //  15       72745    27160    8470
    //  16      108375    35630    8470
    //  17      152475    44100    8470
    //  18      205045    52570    8470
    //  19      266085    61040    8470
    //  20      335595    69510    8470
    //  21      413575    77980    8470
    //  22      500025    86450    8470
    //  23      594945    94920    8470
    //  24      698335    103390   8470
    //  25      810195    111860   8470
    //  26      930525    120330   8470
    //  27     1059325    128800   8470


    switch (lev)
    {
    case 1:
        level = 1;
        break;
    case 2:
        level = 10;
        break;
    case 3:
        level = 30;
        break;
    case 4:
        level = 70;
        break;

    default:
        if (lev < 13)
        {
            lev -= 4;
            level = 10 + 10 * lev + (60 << lev);
        }
        else
        {
            lev -= 12;
            level = 16675 + 5985 * lev + 4235 * lev * lev;
        }
        break;
    }

    if (exp_apt == -99)
        exp_apt = species_exp_modifier(you.species);

    return (unsigned int) ((level - 1) * exp(-log(2.0) * (exp_apt - 1) / 4));
}

// returns bonuses from rings of slaying, etc.
int slaying_bonus(weapon_property_type which_affected, bool ranged)
{
    int ret = 0;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (which_affected == PWPN_HIT)
        {
            ret += you.wearing(EQ_RINGS_PLUS, RING_SLAYING);
            ret += you.scan_artefacts(ARTP_ACCURACY);
            if (you.wearing_ego(EQ_GLOVES, SPARM_ARCHERY))
                ret += ranged ? 5 : -1;
        }
        else if (which_affected == PWPN_DAMAGE)
        {
            ret += you.wearing(EQ_RINGS_PLUS2, RING_SLAYING);
            ret += you.scan_artefacts(ARTP_DAMAGE);
            if (you.wearing_ego(EQ_GLOVES, SPARM_ARCHERY))
                ret += ranged ? 3 : -1;
        }
    }

    ret += min(you.duration[DUR_SLAYING] / (13 * BASELINE_DELAY), 6);
    ret += 4 * augmentation_amount();

    if (you.duration[DUR_SONG_OF_SLAYING])
        ret += you.props["song_of_slaying_bonus"].get_int();

    return ret;
}

// Checks each equip slot for an evokable item (jewellery or randart).
// Returns true if any of these has the same ability as the one handed in.
bool items_give_ability(const int slot, artefact_prop_type abil)
{
    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        if (!player_wearing_slot(i))
            continue;

        const int eq = you.equip[i];

        // skip item to compare with
        if (eq == slot)
            continue;

        // only weapons give their effects when in our hands
        if (i == EQ_WEAPON && you.inv[ eq ].base_type != OBJ_WEAPONS)
            continue;

        if (eq >= EQ_LEFT_RING && eq < NUM_EQUIP && eq != EQ_AMULET)
        {
            if (abil == ARTP_FLY && you.inv[eq].sub_type == RING_FLIGHT)
                return true;
            if (abil == ARTP_INVISIBLE && you.inv[eq].sub_type == RING_INVISIBILITY)
                return true;
        }

        else if (eq == EQ_AMULET)
        {
            if (abil == ARTP_BERSERK && you.inv[eq].sub_type == AMU_RAGE)
                return true;
        }

        // other items are not evokable
        if (!is_artefact(you.inv[ eq ]))
            continue;

        if (artefact_wpn_property(you.inv[ eq ], abil))
            return true;
    }

    // none of the equipped items possesses this ability
    return false;
}

// Checks each equip slot for a randart, and adds up all of those with
// a given property. Slow if any randarts are worn, so avoid where
// possible.
int player::scan_artefacts(artefact_prop_type which_property,
                           bool calc_unid) const
{
    int retval = 0;

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
    {
        if (melded[i] || equip[i] == -1)
            continue;

        const int eq = equip[i];

        // Only weapons give their effects when in our hands.
        if (i == EQ_WEAPON && inv[ eq ].base_type != OBJ_WEAPONS)
            continue;

        if (!is_artefact(inv[ eq ]))
            continue;

        bool known;
        int val = artefact_wpn_property(inv[eq], which_property, known);
        if (calc_unid || known)
            retval += val;
    }

    return retval;
}

void dec_hp(int hp_loss, bool fatal, const char *aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!fatal && you.hp < 1)
        you.hp = 1;

    if (!fatal && hp_loss >= you.hp)
        hp_loss = you.hp - 1;

    if (hp_loss < 1)
        return;

    // If it's not fatal, use ouch() so that notes can be taken. If it IS
    // fatal, somebody else is doing the bookkeeping, and we don't want to mess
    // with that.
    if (!fatal && aux)
        ouch(hp_loss, NON_MONSTER, KILLED_BY_SOMETHING, aux);
    else
        you.hp -= hp_loss;

    you.redraw_hit_points = true;
}

void flush_mp()
{
  if (Options.magic_point_warning
          && you.magic_points < (you.max_magic_points
                                 * Options.magic_point_warning) / 100)
      {
          mpr("* * * LOW MAGIC WARNING * * *", MSGCH_DANGER);
      }

      take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
      you.redraw_magic_points = true;
}

void dec_mp(int mp_loss, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_loss < 1)
        return;

    if (you.species == SP_DJINNI)
        return dec_hp(mp_loss * DJ_MP_RATE, false);

    you.magic_points -= mp_loss;

    you.magic_points = max(0, you.magic_points);
    if (!silent)
        flush_mp();
}

void drain_mp(int loss)
{
    if (you.species != SP_DJINNI)
        return dec_mp(loss);

    if (loss <= 0)
        return;

    you.duration[DUR_ANTIMAGIC] = min(you.duration[DUR_ANTIMAGIC] + loss * 3,
                                      1000); // so it goes away after one '5'
}

bool enough_hp(int minimum, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    // We want to at least keep 1 HP. -- bwr
    if (you.hp < minimum + 1)
    {
        if (!suppress_msg)
        {
            mpr(you.species != SP_DJINNI ?
                "You haven't enough vitality at the moment." :
                "You haven't enough essence at the moment.");
        }

        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    return true;
}

bool enough_mp(int minimum, bool suppress_msg, bool include_items)
{
    if (you.species == SP_DJINNI)
        return enough_hp(minimum * DJ_MP_RATE, suppress_msg);

    ASSERT(!crawl_state.game_is_arena());

    if (you.magic_points < minimum)
    {
        if (!suppress_msg)
        {
            if (get_real_mp(include_items) < minimum)
                mpr("You haven't enough magic capacity.");
            else
                mpr("You haven't enough magic at the moment.");
        }
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    return true;
}

bool enough_zp(int minimum, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.zot_points < minimum)
    {
        if (!suppress_msg)
            mpr("You haven't enough Zot Points.");

        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }
    return true;
}

void inc_mp(int mp_gain, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_gain < 1)
        return;

    if (you.species == SP_DJINNI)
        return inc_hp(mp_gain * DJ_MP_RATE);

    bool wasnt_max = (you.magic_points < you.max_magic_points);

    you.magic_points += mp_gain;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    if (!silent)
    {
        if (wasnt_max && you.magic_points == you.max_magic_points)
            interrupt_activity(AI_FULL_MP);
        you.redraw_magic_points = true;
    }
}

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
// To avoid message spam, don't take notes when HP increases.
void inc_hp(int hp_gain)
{
    ASSERT(!crawl_state.game_is_arena());

    if (hp_gain < 1)
        return;

    bool wasnt_max = (you.hp < you.hp_max);

    you.hp += hp_gain;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    if (wasnt_max && you.hp == you.hp_max)
        interrupt_activity(AI_FULL_HP);

    you.redraw_hit_points = true;
}

void rot_hp(int hp_loss)
{
    you.hp_max_temp -= hp_loss;
    calc_hp();

    // Kill the player if they reached 0 maxhp.
    ouch(0, NON_MONSTER, KILLED_BY_ROTTING);

    if (you.species != SP_GHOUL)
        xom_is_stimulated(hp_loss * 25);

    you.redraw_hit_points = true;
}

void unrot_hp(int hp_recovered)
{
    you.hp_max_temp += hp_recovered;
    if (you.hp_max_temp > 0)
        you.hp_max_temp = 0;

    calc_hp();

    you.redraw_hit_points = true;
}

int player_rotted()
{
    return -you.hp_max_temp;
}

void rot_mp(int mp_loss)
{
    you.mp_max_temp -= mp_loss;
    calc_mp();

    you.redraw_magic_points = true;
}

void inc_max_hp(int hp_gain)
{
    you.hp += hp_gain;
    you.hp_max_perm += hp_gain;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = true;
}

void dec_max_hp(int hp_loss)
{
    you.hp_max_perm -= hp_loss;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = true;
}

// Use of floor: false = hp max, true = hp min. {dlb}
void deflate_hp(int new_level, bool floor)
{
    ASSERT(!crawl_state.game_is_arena());

    if (floor && you.hp < new_level)
        you.hp = new_level;
    else if (!floor && you.hp > new_level)
        you.hp = new_level;

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_hit_points = true;
}

void set_hp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.hp = new_amount;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_hit_points = true;
}

void set_mp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.magic_points = new_amount;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_magic_points = true;
}

// If trans is true, being berserk and/or transformed is taken into account
// here. Else, the base hp is calculated. If rotted is true, calculate the
// real max hp you'd have if the rotting was cured.
int get_real_hp(bool trans, bool rotted)
{
    int hitp;

    hitp  = you.experience_level * 11 / 2;
    hitp += you.hp_max_perm;
    // Important: we shouldn't add Heroism boosts here.
    hitp += (you.experience_level * you.skill(SK_FIGHTING, 10, true)) / 80;

    // Racial modifier.
    hitp *= 10 + species_hp_modifier(you.species);
    hitp /= 10;

    // Frail and robust mutations, divine vigour, and rugged scale mut.
    hitp *= 100 + (player_mutation_level(MUT_ROBUST) * 10)
                + (you.attribute[ATTR_DIVINE_VIGOUR] * 5)
                + (player_mutation_level(MUT_RUGGED_BROWN_SCALES) ?
                   player_mutation_level(MUT_RUGGED_BROWN_SCALES) * 2 + 1 : 0)
                - (player_mutation_level(MUT_FRAIL) * 10);
    hitp /= 100;

    if (!rotted)
        hitp += you.hp_max_temp;

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        if (trans)
            hitp += you.scan_artefacts(ARTP_HP);
    }

    // Being berserk makes you resistant to damage. I don't know why.
    if (trans && you.berserk())
        hitp = hitp * 3 / 2;

    if (trans) // Some transformations give you extra hp.
        hitp = hitp * form_hp_mod() / 10;

    return hitp;
}

int get_real_mp(bool include_items)
{
    int enp = you.experience_level + you.mp_max_perm;
    enp += (you.experience_level * species_mp_modifier(you.species) + 1) / 3;

    int spell_extra = you.skill(SK_SPELLCASTING, you.experience_level, true) / 4;
    int invoc_extra = you.skill(SK_INVOCATIONS, you.experience_level, true) / 6;
    int evoc_extra = you.skill(SK_EVOCATIONS, you.experience_level, true) / 6;

    enp += max(spell_extra, max(invoc_extra, evoc_extra));
    enp = stepdown_value(enp, 9, 18, 45, 100);

    // This is our "rotted" base (applied after scaling):
    enp += you.mp_max_temp;

    // Yes, we really do want this duplication... this is so the stepdown
    // doesn't truncate before we apply the rotted base.  We're doing this
    // the nice way. -- bwr
    enp = min(enp, 50);

    // Analogous to ROBUST/FRAIL
    enp *= 100 + (player_mutation_level(MUT_HIGH_MAGIC) * 10)
               + (you.attribute[ATTR_DIVINE_VIGOUR] * 5)
               - (player_mutation_level(MUT_LOW_MAGIC) * 10);
    enp /= 100;

    if (you.suppressed())
        include_items = false;

    // Now applied after scaling so that power items are more useful -- bwr
    if (include_items)
    {
        enp +=  9 * you.wearing(EQ_RINGS, RING_MAGICAL_POWER);
        enp +=      you.scan_artefacts(ARTP_MAGICAL_POWER);

        if (you.wearing(EQ_STAFF, STAFF_POWER))
            enp += 5 + enp * 2 / 5;
    }

    if (enp > 50)
        enp = 50 + ((enp - 50) / 2);

    if (include_items && you.wearing_ego(EQ_WEAPON, SPWPN_ANTIMAGIC))
        enp /= 3;

    enp = max(enp, 0);

    return enp;
}

int get_contamination_level()
{
    const int glow = you.magic_contamination;

    if (glow > 60000)
        return (glow / 20000 + 3);
    if (glow > 40000)
        return 5;
    if (glow > 25000)
        return 4;
    if (glow > 15000)
        return 3;
    if (glow > 5000)
        return 2;
    if (glow > 0)
        return 1;

    return 0;
}

string describe_contamination(int cont)
{
    if (cont > 5)
        return "You are engulfed in a nimbus of crackling magics!";
    else if (cont == 5)
        return "Your entire body has taken on an eerie glow!";
    else if (cont > 1)
    {
        return (make_stringf("You are %s with residual magics%s",
                   (cont == 4) ? "practically glowing" :
                   (cont == 3) ? "heavily infused" :
                   (cont == 2) ? "contaminated"
                                    : "lightly contaminated",
                   (cont == 4) ? "!" : "."));
    }
    else if (cont == 1)
        return "You are very lightly contaminated with residual magic.";
    else
        return "";
}

// controlled is true if the player actively did something to cause
// contamination (such as drink a known potion of resistance),
// status_only is true only for the status output
void contaminate_player(int change, bool controlled, bool msg)
{
    ASSERT(!crawl_state.game_is_arena());

    int old_amount = you.magic_contamination;
    int old_level  = get_contamination_level();
    int new_level  = 0;

    you.magic_contamination = max(0, min(250000, you.magic_contamination + change));

    new_level = get_contamination_level();

    if (you.magic_contamination != old_amount)
        dprf("change: %d  radiation: %d", change, you.magic_contamination);

    if (msg && new_level >= 1 && old_level <= 1 && new_level != old_level)
        mpr(describe_contamination(new_level));
    else if (msg && new_level != old_level)
    {
        if (old_level == 1 && new_level == 0)
            mpr("Your magical contamination has completely faded away.");
        else
        {
            mprf((change > 0) ? MSGCH_WARN : MSGCH_RECOVERY,
                 "You feel %s contaminated with magical energies.",
                 (change > 0) ? "more" : "less");
        }

        if (change > 0)
            xom_is_stimulated(new_level * 25);

        if (old_level > 1 && new_level <= 1
            && you.duration[DUR_INVIS] && !you.backlit())
        {
            mpr("You fade completely from view now that you are no longer "
                "glowing from magical contamination.");
        }
    }

    if (you.magic_contamination > 0)
        learned_something_new(HINT_GLOWING);

    // Zin doesn't like mutations or mutagenic radiation.
    if (you_worship(GOD_ZIN))
    {
        // Whenever the glow status is first reached, give a warning message.
        if (old_level < 2 && new_level >= 2)
            did_god_conduct(DID_CAUSE_GLOWING, 0, false);
        // If the player actively did something to increase glowing,
        // Zin is displeased.
        else if (controlled && change > 0 && old_level > 1)
            did_god_conduct(DID_CAUSE_GLOWING, 1 + new_level, true);
    }
}

bool confuse_player(int amount, bool resistable)
{
    ASSERT(!crawl_state.game_is_arena());

    if (amount <= 0)
        return false;

    if (resistable && you.clarity())
    {
        mpr("You feel momentarily confused.");
        // Identify the amulet if necessary.
        if (you.wearing(EQ_AMULET, AMU_CLARITY, true))
        {
            item_def* const amu = you.slot_item(EQ_AMULET, false);
            wear_id_type(*amu);
        }
        return false;
    }

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from confusion!");
        return false;
    }

    const int old_value = you.duration[DUR_CONF];
    you.increase_duration(DUR_CONF, amount, 40);

    if (you.duration[DUR_CONF] > old_value)
    {
        you.check_awaken(500);

        mprf(MSGCH_WARN, "You are %sconfused.",
             old_value > 0 ? "more " : "");

        learned_something_new(HINT_YOU_ENCHANTED);

        xom_is_stimulated((you.duration[DUR_CONF] - old_value)
                           / BASELINE_DELAY);
    }

    return true;
}

bool curare_hits_player(int death_source, int amount, const bolt &beam)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_poison() >= 3)
        return false;

    if (!poison_player(amount, beam.get_source_name(), beam.name))
        return false;

    int hurted = 0;

    if (you.res_asphyx() <= 0)
    {
        hurted = roll_dice(2, 6);

        if (hurted)
        {
            you.increase_duration(DUR_BREATH_WEAPON, hurted, 20 + random2(20));
            mpr("You have difficulty breathing.");
            ouch(hurted, death_source, KILLED_BY_CURARE,
                 "curare-induced apnoea");
        }
    }

    potion_effect(POT_SLOWING, 2 + random2(4 + amount));

    return (hurted > 0);
}

void paralyse_player(string source, int amount, int factor)
{
    if (!amount)
        amount = 2 + random2(6 + you.duration[DUR_PARALYSIS] / BASELINE_DELAY);

    amount /= factor;
    you.paralyse(NULL, amount, source);
}

bool poison_player(int amount, string source, string source_aux, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_poison() > 0)
        maybe_id_resist(BEAM_POISON);

    if (player_res_poison() >= 3)
    {
        dprf("Cannot poison, you are immune!");
        return false;
    }

    if (!force && !(amount = _maybe_reduce_poison(amount)))
        return false;

    if (!force && you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from poison!");
        return false;
    }

    const int old_value = you.duration[DUR_POISONING];
    if (player_res_poison() < 0)
        amount *= 2;
    you.duration[DUR_POISONING] += amount;

    if (you.duration[DUR_POISONING] > 40)
        you.duration[DUR_POISONING] = 40;

    if (you.duration[DUR_POISONING] > old_value)
    {
        mprf(MSGCH_WARN, "You are %spoisoned.",
             old_value > 0 ? "more " : "");

        learned_something_new(HINT_YOU_POISON);
    }

    you.props["poisoner"] = source;
    you.props["poison_aux"] = source_aux;

    return amount;
}

void dec_poison_player()
{
    // If Cheibriados has slowed your life processes, there's a
    // chance that your poison level is simply unaffected and
    // you aren't hurt by poison.
    if (GOD_CHEIBRIADOS == you.religion
        && you.piety >= piety_breakpoint(0)
        && coinflip())
    {
        return;
    }

    if (player_res_poison() >= 3)
        return;

    if (x_chance_in_y(you.duration[DUR_POISONING], 5))
    {
        int hurted = 1;
        msg_channel_type channel = MSGCH_PLAIN;
        const char *adj = "";

        if (you.duration[DUR_POISONING] > 10
            && random2(you.duration[DUR_POISONING]) >= 8)
        {
            hurted = random2(10) + 5;
            channel = MSGCH_DANGER;
            adj = "extremely ";
        }
        else if (you.duration[DUR_POISONING] > 5 && coinflip())
        {
            hurted = coinflip() ? 3 : 2;
            channel = MSGCH_WARN;
            adj = "very ";
        }

        int oldhp = you.hp;
        ouch(hurted, NON_MONSTER, KILLED_BY_POISON);
        if (you.hp < oldhp)
            mprf(channel, "You feel %ssick.", adj);

        if ((you.hp == 1 && one_chance_in(3)) || one_chance_in(8))
            reduce_poison_player(1);

        if (!you.duration[DUR_POISONING] && you.hp * 12 < you.hp_max)
            xom_is_stimulated(you.hp == 1 ? 50 : 20);
    }
}

void reduce_poison_player(int amount)
{
    if (amount <= 0)
        return;

    const int old_value = you.duration[DUR_POISONING];
    you.duration[DUR_POISONING] -= amount;

    if (you.duration[DUR_POISONING] <= 0)
    {
        you.duration[DUR_POISONING] = 0;
        you.props.erase("poisoner");
        you.props.erase("poison_aux");
    }

    if (you.duration[DUR_POISONING] < old_value)
    {
        mprf(MSGCH_RECOVERY, "You feel %sbetter.",
             you.duration[DUR_POISONING] > 0 ? "a little " : "");
    }
}

bool miasma_player(string source, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.res_rotting() || you.duration[DUR_DEATHS_DOOR])
        return false;

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from the miasma!");
        return false;
    }

    bool success = poison_player(1, source, source_aux);

    if (you.hp_max > 4 && coinflip())
    {
        rot_hp(1);
        success = true;
    }

    if (one_chance_in(3))
    {
        potion_effect(POT_SLOWING, 5);
        success = true;
    }

    return success;
}

bool napalm_player(int amount, string source, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_sticky_flame() || amount <= 0)
        return false;

    const int old_value = you.duration[DUR_LIQUID_FLAMES];
    you.increase_duration(DUR_LIQUID_FLAMES, amount, 100);

    if (you.duration[DUR_LIQUID_FLAMES] > old_value)
        mpr("You are covered in liquid flames!", MSGCH_WARN);

    you.props["napalmer"] = source;
    you.props["napalm_aux"] = source_aux;

    return true;
}

void dec_napalm_player(int delay)
{
    delay = min(delay, you.duration[DUR_LIQUID_FLAMES]);

    if (feat_is_watery(grd(you.pos())))
    {
        if (you.ground_level())
            mpr("The flames go out!", MSGCH_WARN);
        else
            mpr("You dip into the water, and the flames go out!", MSGCH_WARN);
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.props.erase("napalmer");
        you.props.erase("napalm_aux");
        return;
    }

    mpr("You are covered in liquid flames!", MSGCH_WARN);

    expose_player_to_element(BEAM_NAPALM,
                             div_rand_round(delay * 12, BASELINE_DELAY));

    const int res_fire = player_res_fire();

    if (res_fire > 0)
    {
        ouch((((random2avg(9, 2) + 1) * delay) /
                (1 + (res_fire * res_fire))) / BASELINE_DELAY, NON_MONSTER,
                KILLED_BY_BURNING);
    }

    if (res_fire <= 0)
    {
        ouch(((random2avg(9, 2) + 1) * delay) / BASELINE_DELAY,
             NON_MONSTER, KILLED_BY_BURNING);

        if (res_fire < 0)
        {
            ouch(((random2avg(9, 2) + 1) * delay)
                    / BASELINE_DELAY, NON_MONSTER, KILLED_BY_BURNING);
        }
    }

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();
    if (you.duration[DUR_ICY_ARMOUR] > 0)
        remove_ice_armour();

    you.duration[DUR_LIQUID_FLAMES] -= delay;
    if (you.duration[DUR_LIQUID_FLAMES] <= 0)
    {
        you.props.erase("napalmer");
        you.props.erase("napalm_aux");
    }
}

bool slow_player(int turns)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (stasis_blocks_effect(true, true, "%s rumbles.", 20, "%s rumbles."))
        return false;

    // Doubling these values because moving while slowed takes twice the
    // usual delay.
    turns = haste_mul(turns);
    int threshold = haste_mul(100);

    if (you.duration[DUR_SLOW] >= threshold * BASELINE_DELAY)
        mpr("You already are as slow as you could be.");
    else
    {
        if (you.duration[DUR_SLOW] == 0)
            mpr("You feel yourself slow down.");
        else
            mpr("You feel as though you will be slow longer.");

        you.increase_duration(DUR_SLOW, turns, threshold);
        learned_something_new(HINT_YOU_ENCHANTED);
    }

    return true;
}

void dec_slow_player(int delay)
{
    if (!you.duration[DUR_SLOW])
        return;

    if (you.duration[DUR_SLOW] > BASELINE_DELAY)
    {
        // Make slowing and hasting effects last as long.
        you.duration[DUR_SLOW] -= you.duration[DUR_HASTE]
            ? haste_mul(delay) : delay;
    }
    if (you.duration[DUR_SLOW] <= BASELINE_DELAY)
    {
        mpr("You feel yourself speed up.", MSGCH_DURATION);
        you.duration[DUR_SLOW] = 0;
    }
}

// Exhaustion should last as long as slowing.
void dec_exhaust_player(int delay)
{
    if (!you.duration[DUR_EXHAUSTED])
        return;

    if (you.duration[DUR_EXHAUSTED] > BASELINE_DELAY)
    {
        you.duration[DUR_EXHAUSTED] -= you.duration[DUR_HASTE]
                                       ? haste_mul(delay) : delay;
    }
    if (you.duration[DUR_EXHAUSTED] <= BASELINE_DELAY)
    {
        mpr("You feel less exhausted.", MSGCH_DURATION);
        you.duration[DUR_EXHAUSTED] = 0;
    }
}

bool haste_player(int turns, bool rageext)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (stasis_blocks_effect(true, true, "%s emits a piercing whistle.", 20,
                             "%s makes your neck tingle."))
    {
        return false;
    }

    // Cutting the nominal turns in half since hasted actions take half the
    // usual delay.
    turns = haste_div(turns);
    const int threshold = 40;

    if (!you.duration[DUR_HASTE])
        mpr("You feel yourself speed up.");
    else if (you.duration[DUR_HASTE] > threshold * BASELINE_DELAY)
        mpr("You already have as much speed as you can handle.");
    else if (!rageext)
    {
        mpr("You feel as though your hastened speed will last longer.");
        contaminate_player(1000, true); // always deliberate
    }

    you.increase_duration(DUR_HASTE, turns, threshold);

    return true;
}

void dec_haste_player(int delay)
{
    if (!you.duration[DUR_HASTE])
        return;

    if (you.duration[DUR_HASTE] > BASELINE_DELAY)
    {
        int old_dur = you.duration[DUR_HASTE];

        you.duration[DUR_HASTE] -= delay;

        int threshold = 6 * BASELINE_DELAY;
        // message if we cross the threshold
        if (old_dur > threshold && you.duration[DUR_HASTE] <= threshold)
        {
            mpr("Your extra speed is starting to run out.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_HASTE] -= BASELINE_DELAY;
        }
    }
    else if (you.duration[DUR_HASTE] <= BASELINE_DELAY)
    {
        if (!you.duration[DUR_BERSERK])
            mpr("You feel yourself slow down.", MSGCH_DURATION);
        you.duration[DUR_HASTE] = 0;
    }
}

void dec_disease_player(int delay)
{
    if (you.disease)
    {
        int rr = 50;

        // Extra regeneration means faster recovery from disease.
        // But not if not actually regenerating!
        if (player_mutation_level(MUT_SLOW_HEALING) < 3
            && !(you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING))
        {
            rr += _player_bonus_regen();
        }

        // Trog's Hand.
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            rr += 100;

        // Kobolds get a bonus too.
        if (you.species == SP_KOBOLD)
            rr += 100;

        rr = div_rand_round(rr * delay, 50);

        you.disease -= rr;
        if (you.disease < 0)
            you.disease = 0;

        if (you.disease == 0)
            mpr("You feel your health improve.", MSGCH_RECOVERY);
    }
}

void float_player()
{
    if (you.fishtail)
    {
        mprf("Your tail turns into legs as you fly out of the water.");
        merfolk_stop_swimming();
    }
    else if (you.tengu_flight())
        mpr("You swoop lightly up into the air.");
    else
        mpr("You fly up into the air.");

    if (you.species == SP_TENGU)
        you.redraw_evasion = true;

    // The player hasn't actually taken a step, but in this case, we want
    // neither the message, nor the location effect.
    you.check_clinging(true);
}

void fly_player(int pow, bool already_flying)
{
    if (you.form == TRAN_TREE)
        return mpr("Your roots keep you in place.");

    if (you.duration[DUR_GRASPING_ROOTS])
    {
        mpr("The grasping roots prevent you from becoming airborne.");
        return;
    }

    bool standing = !you.airborne() && !already_flying;
    if (!already_flying)
        mprf(MSGCH_DURATION, "You feel %s buoyant.", standing ? "very" : "more");

    you.increase_duration(DUR_FLIGHT, 25 + random2(pow), 100);

    if (standing)
        float_player();
}

bool land_player(bool quiet)
{
    // there was another source keeping you aloft
    if (you.airborne())
        return false;

    if (!quiet)
        mpr("You float gracefully downwards.");
    if (you.species == SP_TENGU)
        you.redraw_evasion = true;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    // Re-enter the terrain.
    move_player_to_grid(you.pos(), false, true);
    return true;
}

bool is_hovering()
{
    return you.species == SP_DJINNI
           && !feat_has_dry_floor(grd(you.pos()))
           && !you.airborne()
           && !you.is_wall_clinging();
}

bool djinni_floats()
{
    return you.species == SP_DJINNI
           && you.form != TRAN_TREE
           && (you.form != TRAN_SPIDER || !you.is_wall_clinging());
}

static void _end_water_hold()
{
    you.duration[DUR_WATER_HOLD] = 0;
    you.duration[DUR_WATER_HOLD_IMMUNITY] = 1;
    you.props.erase("water_holder");
}

void handle_player_drowning(int delay)
{
    if (you.duration[DUR_WATER_HOLD] == 1)
    {
        if (!you.res_water_drowning())
            mpr("You gasp with relief as air once again reaches your lungs.");
        _end_water_hold();
    }
    else
    {
        monster* mons = monster_by_mid(you.props["water_holder"].get_int());
        if (!mons || mons && !adjacent(mons->pos(), you.pos()))
        {
            if (you.res_water_drowning())
                mpr("The water engulfing you falls away.");
            else
                mpr("You gasp with relief as air once again reaches your lungs.");

            _end_water_hold();

        }
        else if (you.res_water_drowning())
        {
            // Reset so damage doesn't ramp up while able to breathe
            you.duration[DUR_WATER_HOLD] = 10;
        }
        else if (!you.res_water_drowning())
        {
            you.duration[DUR_WATER_HOLD] += delay;
            int dam =
                div_rand_round((28 + stepdown((float)you.duration[DUR_WATER_HOLD], 28.0))
                                * delay,
                                BASELINE_DELAY * 10);
            ouch(dam, mons->mindex(), KILLED_BY_WATER);
            mpr("Your lungs strain for air!", MSGCH_WARN);
        }
    }
}

int count_worn_ego(int which_ego)
{
    int result = 0;
    for (int slot = EQ_MIN_ARMOUR; slot <= EQ_MAX_ARMOUR; ++slot)
    {
        if (you.equip[slot] != -1 && !you.melded[slot]
            && get_armour_ego_type(you.inv[you.equip[slot]]) == which_ego)
        {
            result++;
        }
    }

    return result;
}

player::player()
    : kills(0), m_quiver(0)
{
    init();
}

player::player(const player &other)
    : kills(0), m_quiver(0)
{
    init();

    // why doesn't this do a copy_from?
    player_quiver* saved_quiver = m_quiver;
    delete kills;
    *this = other;
    m_quiver = saved_quiver;

    kills = new KillMaster(*(other.kills));
    *m_quiver = *(other.m_quiver);
}

// why is this not called "operator="?
void player::copy_from(const player &other)
{
    if (this == &other)
        return;

    KillMaster *saved_kills = kills;
    player_quiver* saved_quiver = m_quiver;

    *this = other;

    kills  = saved_kills;
    *kills = *(other.kills);
    m_quiver = saved_quiver;
    *m_quiver = *(other.m_quiver);
}


// player struct initialization
void player::init()
{
    // Permanent data:
    your_name.clear();
    species          = SP_UNKNOWN;
    species_name.clear();
    char_class       = JOB_UNKNOWN;
    class_name.clear();
    type             = MONS_PLAYER;
    mid              = MID_PLAYER;
    position.reset();

#ifdef WIZARD
    wizard = Options.wiz_mode == WIZ_YES;
#else
    wizard = false;
#endif
    birth_time       = time(0);

    // Long-term state:
    elapsed_time     = 0;
    elapsed_time_at_last_input = 0;

    hp               = 0;
    hp_max           = 0;
    hp_max_temp      = 0;
    hp_max_perm      = 0;

    magic_points       = 0;
    max_magic_points   = 0;
    mp_max_temp      = 0;
    mp_max_perm      = 0;

    stat_loss.init(0);
    base_stats.init(0);
    stat_zero.init(0);

    hunger          = HUNGER_DEFAULT;
    hunger_state    = HS_SATIATED;
    disease         = 0;
    max_level       = 1;
    hit_points_regeneration   = 0;
    magic_points_regeneration = 0;
    experience       = 0;
    total_experience = 0;
    experience_level = 1;
    gold             = 0;
    zigs_completed   = 0;
    zig_max          = 0;

    equip.init(-1);
    melded.reset();
    unrand_reacts   = 0;

    symbol          = MONS_PLAYER;
    form            = TRAN_NONE;

    for (int i = 0; i < ENDOFPACK; i++)
        inv[i].clear();
    runes.reset();
    obtainable_runes = 15;

    burden          = 0;
    burden_state    = BS_UNENCUMBERED;
    spells.init(SPELL_NO_SPELL);
    old_vehumet_gifts.clear();
    spell_no        = 0;
    vehumet_gifts.clear();
    char_direction  = GDT_DESCENDING;
    opened_zot      = false;
    royal_jelly_dead = false;
    transform_uncancellable = false;
    fishtail = false;

    pet_target      = MHITNOT;

    duration.init(0);
    rotting         = 0;
    berserk_penalty = 0;
    attribute.init(0);
    quiver.init(ENDOFPACK);
    sacrifice_value.init(0);

    is_undead       = US_ALIVE;

    friendly_pickup = 0;
    dead = false;
    lives = 0;
    deaths = 0;

    temperature = 1; // 1 is min; 15 is max.
    temperature_last = 1;

    xray_vision = false;

    init_skills();

    skill_menu_do = SKM_NONE;
    skill_menu_view = SKM_NONE;

    transfer_from_skill = SK_NONE;
    transfer_to_skill = SK_NONE;
    transfer_skill_points = 0;
    transfer_total_skill_points = 0;

    sage_skills.clear();
    sage_xp.clear();
    sage_bonus.clear();

    skill_cost_level = 1;
    exp_available = 0;
    zot_points = 0;

    item_description.init(255);
    unique_items.init(UNIQ_NOT_EXISTS);
    unique_creatures.reset();
    force_autopickup.init(0);

    if (kills)
        delete kills;
    kills = new KillMaster();

    where_are_you    = BRANCH_MAIN_DUNGEON;
    depth            = 1;

    branch_stairs.init(0);

    religion         = GOD_NO_GOD;
    jiyva_second_name.clear();
    god_name.clear();
    piety            = 0;
    piety_hysteresis = 0;
    gift_timeout     = 0;
    penance.init(0);
    worshipped.init(0);
    num_current_gifts.init(0);
    num_total_gifts.init(0);
    one_time_ability_used.reset();
    piety_max.init(0);
    exp_docked       = 0;
    exp_docked_total = 0;

    mutation.init(0);
    innate_mutations.init(0);
    temp_mutations.init(0);
    demonic_traits.clear();

    magic_contamination = 0;

    had_book.reset();
    seen_spell.reset();
    seen_weapon.init(0);
    seen_armour.init(0);
    seen_misc.reset();

    octopus_king_rings = 0;

    normal_vision    = LOS_RADIUS;
    current_vision   = LOS_RADIUS;

    hell_branch      = BRANCH_MAIN_DUNGEON;
    hell_exit        = 0;

    real_time        = 0;
    num_turns        = 0;
    exploration      = 0;

    last_view_update = 0;

    spell_letter_table.init(-1);
    ability_letter_table.init(ABIL_NON_ABILITY);

    uniq_map_tags.clear();
    uniq_map_names.clear();
    vault_list.clear();

    global_info = PlaceInfo();
    global_info.assert_validity();

    if (m_quiver)
        delete m_quiver;
    m_quiver = new player_quiver;

    props.clear();

    beholders.clear();
    fearmongers.clear();
    dactions.clear();
    level_stack.clear();
    type_ids.init(ID_UNKNOWN_TYPE);
    type_id_props.clear();

    zotdef_wave_name.clear();
    last_mid = 0;
    last_cast_spell = SPELL_NO_SPELL;


    // Non-saved UI state:
    prev_targ        = MHITNOT;
    prev_grd_targ.reset();
    prev_move.reset();

    travel_x         = 0;
    travel_y         = 0;
    travel_z         = level_id();

    running.clear();
    travel_ally_pace = false;
    received_weapon_warning = false;
    received_noskill_warning = false;
    ash_init_bondage(this);

    delay_queue.clear();

    last_keypress_time = time(0);

    action_count.clear();

    branches_left.reset();

    // Volatile (same-turn) state:
    turn_is_over     = false;
    banished         = false;
    banished_by.clear();

    wield_change     = false;
    redraw_quiver    = false;
    redraw_status_flags = 0;
    redraw_hit_points   = false;
    redraw_magic_points = false;
    redraw_stats.init(false);
    redraw_experience   = false;
    redraw_armour_class = false;
    redraw_evasion      = false;
    redraw_title        = false;

    flash_colour        = BLACK;
    flash_where         = nullptr;

    time_taken          = 0;
    shield_blocks       = 0;

    abyss_speed         = 0;

    old_hunger          = hunger;
    transit_stair       = DNGN_UNSEEN;
    entering_level      = false;

    reset_escaped_death();
    on_current_level    = true;
    walking             = 0;
    seen_portals        = 0;
    seen_invis          = false;
    frame_no            = 0;

    save                = 0;
    prev_save_version.clear();

    clear_constricted();
    constricting = 0;

    // Protected fields:
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        branch_info[i].branch = (branch_type)i;
        branch_info[i].assert_validity();
    }
}

void player::init_skills()
{
    auto_training = !(Options.default_manual_training);
    skills.init(0);
    train.init(false);
    train_alt.init(false);
    training.init(0);
    can_train.reset();
    skill_points.init(0);
    ct_skill_points.init(0);
    skill_order.init(MAX_SKILL_ORDER);
    exercises.clear();
    exercises_all.clear();
}

player_save_info& player_save_info::operator=(const player& rhs)
{
    name             = rhs.your_name;
    experience       = rhs.experience;
    experience_level = rhs.experience_level;
    wizard           = rhs.wizard;
    species          = rhs.species;
    species_name     = rhs.species_name;
    class_name       = rhs.class_name;
    religion         = rhs.religion;
    god_name         = rhs.god_name;
    jiyva_second_name= rhs.jiyva_second_name;

    // [ds] Perhaps we should move game type to player?
    saved_game_type  = crawl_state.type;

    return *this;
}

bool player_save_info::operator<(const player_save_info& rhs) const
{
    return experience < rhs.experience
           || (experience == rhs.experience && name < rhs.name);
}

string player_save_info::short_desc() const
{
    ostringstream desc;

    const string qualifier = game_state::game_type_name_for(saved_game_type);
    if (!qualifier.empty())
        desc << "[" << qualifier << "] ";

    desc << name << ", a level " << experience_level << ' '
         << species_name << ' ' << class_name;

    if (religion == GOD_JIYVA)
        desc << " of " << god_name << " " << jiyva_second_name;
    else if (religion != GOD_NO_GOD)
        desc << " of " << god_name;

#ifdef WIZARD
    if (wizard)
        desc << " (WIZ)";
#endif

    return desc.str();
}

player::~player()
{
    delete kills;
    delete m_quiver;
    if (CrawlIsCrashing && save)
    {
        save->abort();
        delete save;
        save = 0;
    }
    ASSERT(!save); // the save file should be closed or deleted
}

flight_type player::flight_mode() const
{
    // Might otherwise be airborne, but currently stuck to the ground
    if (you.duration[DUR_GRASPING_ROOTS])
        return FL_NONE;

    if (duration[DUR_FLIGHT]
        || attribute[ATTR_PERM_FLIGHT]
        || form == TRAN_WISP
        // dragon and bat should be FL_WINGED, but we don't want paralysis
        // instakills over lava
        || form == TRAN_DRAGON
        || form == TRAN_BAT)
    {
        return FL_LEVITATE;
    }

    return FL_NONE;
}

bool player::is_banished() const
{
    return (!alive() && banished);
}

bool player::in_water() const
{
    return (ground_level() && !beogh_water_walk()
            && feat_is_water(grd(pos())));
}

bool player::in_lava() const
{
    return ground_level() && feat_is_lava(grd(pos()));
}

bool player::in_liquid() const
{
    return in_water() || in_lava() || liquefied_ground();
}

bool player::can_swim(bool permanently) const
{
    // Transforming could be fatal if it would cause unequipment of
    // stat-boosting boots or heavy armour.
    return (species == SP_MERFOLK || species == SP_OCTOPODE
            || body_size(PSIZE_BODY) >= SIZE_GIANT
            || !permanently)
                && form_can_swim();
}

int player::visible_igrd(const coord_def &where) const
{
    // shop hack, etc.
    if (where.x == 0)
        return NON_ITEM;

    if (grd(where) == DNGN_LAVA
        || (grd(where) == DNGN_DEEP_WATER
            && species != SP_MERFOLK && species != SP_GREY_DRACONIAN
            && species != SP_OCTOPODE))
    {
        return NON_ITEM;
    }

    return igrd(where);
}

bool player::has_spell(spell_type spell) const
{
    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (spells[i] == spell)
            return true;
    }

    return false;
}

bool player::cannot_speak() const
{
    if (silenced(pos()))
        return true;

    if (cannot_move()) // we allow talking during sleep ;)
        return true;

    // No transform that prevents the player from speaking yet.
    // ... yet setting this would prevent saccing junk and similar activities
    // for no good reason.
    return false;
}

string player::shout_verb() const
{
    switch (form)
    {
    case TRAN_DRAGON:
        return "roar";
    case TRAN_SPIDER:
        return "hiss";
    case TRAN_BAT:
    case TRAN_PORCUPINE:
        return "squeak";
    case TRAN_PIG:
        return coinflip() ? "squeal" : "oink";

    // These forms can't shout.
    case TRAN_FUNGUS:
        return "sporulate";
    case TRAN_TREE:
        return "creak";
    case TRAN_JELLY:
        return "gurgle";
    case TRAN_WISP:
        return "whoosh"; // any wonder why?

    default:
        if (species == SP_FELID)
            return coinflip() ? "meow" : "yowl";
        // depends on SCREAM mutation
        int level = player_mutation_level(MUT_SCREAM);
        if (level <= 1)
            return "shout";
        else if (level == 2)
            return "yell";
        else // level == 3
            return "scream";
    }
}

void player::god_conduct(conduct_type thing_done, int level)
{
    ::did_god_conduct(thing_done, level);
}

void player::banish(actor *agent, const string &who)
{
    ASSERT(!crawl_state.game_is_arena());
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (elapsed_time <= attribute[ATTR_BANISHMENT_IMMUNITY])
    {
        mpr("You resist the pull of the Abyss.");
        return;
    }

    banished    = true;
    banished_by = who;
}

// For semi-undead species (Vampire!) reduce food cost for spells and abilities
// to 50% (hungry, very hungry) or zero (near starving, starving).
int calc_hunger(int food_cost)
{
    if (you.is_undead == US_SEMI_UNDEAD && you.hunger_state < HS_SATIATED)
    {
        if (you.hunger_state <= HS_NEAR_STARVING)
            return 0;

        return food_cost/2;
    }
    return food_cost;
}

bool player::paralysed() const
{
    return duration[DUR_PARALYSIS];
}

bool player::cannot_move() const
{
    return (paralysed() || petrified());
}

bool player::confused() const
{
    return duration[DUR_CONF];
}

bool player::caught() const
{
    return attribute[ATTR_HELD];
}

bool player::petrifying() const
{
    return duration[DUR_PETRIFYING];
}

bool player::petrified() const
{
    return duration[DUR_PETRIFIED];
}

bool player::liquefied_ground() const
{
    return (liquefied(pos())
            && ground_level() && !is_insubstantial());
}

int player::shield_block_penalty() const
{
    return (5 * shield_blocks * shield_blocks);
}

int player::shield_bonus() const
{
    const int shield_class = player_shield_class();
    if (shield_class <= 0)
        return -100;

    return random2avg(shield_class * 2, 2) / 3 - 1;
}

int player::shield_bypass_ability(int tohit) const
{
    return (15 + tohit / 2);
}

void player::shield_block_succeeded(actor *foe)
{
    actor::shield_block_succeeded(foe);

    shield_blocks++;
    practise(EX_SHIELD_BLOCK);
}

int player::missile_deflection() const
{
    if (duration[DUR_DEFLECT_MISSILES])
        return 2;
    if (duration[DUR_REPEL_MISSILES]
        || player_mutation_level(MUT_DISTORTION_FIELD) == 3
        || (!suppressed() && scan_artefacts(ARTP_RMSL, true)))
    {
        return 1;
    }
    return 0;
}

int player::unadjusted_body_armour_penalty() const
{
    const item_def *body_armour = slot_item(EQ_BODY_ARMOUR, false);
    if (!body_armour)
        return 0;

    const int base_ev_penalty = -property(*body_armour, PARM_EVASION);
    return base_ev_penalty;
}

// The EV penalty to the player for their worn body armour.
int player::adjusted_body_armour_penalty(int scale, bool use_size) const
{
    const int base_ev_penalty = unadjusted_body_armour_penalty();
    if (!base_ev_penalty)
        return 0;

    if (use_size)
    {
        const int size = body_size(PSIZE_BODY);

        const int size_bonus_factor = (size - SIZE_MEDIUM) * scale / 4;

        return max(0, scale * base_ev_penalty
                      - size_bonus_factor * base_ev_penalty);
    }

    // New formula for effect of str on aevp: (2/5) * evp^2 / (str+3)
    return (2 * base_ev_penalty * base_ev_penalty
            * (450 - skill(SK_ARMOUR, 10))
            * scale
            / (5 * (strength() + 3))
            / 450);
}

// The EV penalty to the player for wearing their current shield.
int player::adjusted_shield_penalty(int scale) const
{
    const item_def *shield_l = slot_item(EQ_SHIELD, false);
    if (!shield_l)
        return 0;

    const int base_shield_penalty = -property(*shield_l, PARM_EVASION);
    return max(0, (base_shield_penalty * scale - skill(SK_SHIELDS, scale)
                  / max(1, 5 + _player_evasion_size_factor())));
}

int player::armour_tohit_penalty(bool random_factor, int scale) const
{
    return maybe_roll_dice(1, adjusted_body_armour_penalty(scale), random_factor);
}

int player::shield_tohit_penalty(bool random_factor, int scale) const
{
    return maybe_roll_dice(1, adjusted_shield_penalty(scale), random_factor);
}

int player::skill(skill_type sk, int scale, bool real) const
{
    // wizard racechange, or upgraded old save
    if (is_useless_skill(sk))
        return 0;

    int level = skills[sk] * scale + get_skill_progress(sk, scale);
    if (real)
        return level;
    if (duration[DUR_HEROISM] && sk <= SK_LAST_MUNDANE)
        level = min(level + 5 * scale, 27 * scale);
    if (penance[GOD_ASHENZARI])
        level = max(level - min(4 * scale, level / 2), 0);
    else if (religion == GOD_ASHENZARI && piety_rank() > 2)
    {
        if (skill_boost.find(sk) != skill_boost.end()
            && skill_boost.find(sk)->second)
        {
            level = ash_skill_boost(sk, scale);
        }
    }
    if (you.attribute[ATTR_XP_DRAIN])
    {
        level = (int) max(0.0, level - you.attribute[ATTR_XP_DRAIN] / 100.0
                                       * (scale + level/30.0));
    }

    return level;
}

int player_icemail_armour_class()
{
    if (!you.mutation[MUT_ICEMAIL])
        return 0;

    return ICEMAIL_MAX
           - you.duration[DUR_ICEMAIL_DEPLETED]
             * ICEMAIL_MAX / ICEMAIL_TIME;
}

bool player_stoneskin()
{
    // Lava orcs ignore DUR_STONESKIN
    if (you.species == SP_LAVA_ORC)
    {
        // Most transformations conflict with stone skin.
        if (form_changed_physiology() && you.form != TRAN_STATUE)
            return false;

        return temperature_effect(LORC_STONESKIN);
    }
    else
        return you.duration[DUR_STONESKIN];
}

static int _stoneskin_bonus()
{
    if (!player_stoneskin())
        return 0;

    // Max +7.4 base
    int boost = 200;
    if (you.species == SP_LAVA_ORC)
        boost += 20 * you.experience_level;
    else
        boost += you.skill(SK_EARTH_MAGIC, 20);

    // Max additional +7.75 from statue form
    if (you.form == TRAN_STATUE)
    {
        boost += 100;
        if (you.species == SP_LAVA_ORC)
            boost += 25 * you.experience_level;
        else
            boost += you.skill(SK_EARTH_MAGIC, 25);
    }

    return boost;
}

int player::armour_class() const
{
    int AC = 0;

    for (int eq = EQ_MIN_ARMOUR; eq <= EQ_MAX_ARMOUR; ++eq)
    {
        if (eq == EQ_SHIELD)
            continue;

        if (!player_wearing_slot(eq))
            continue;

        const item_def& item   = inv[equip[eq]];
        const int ac_value     = property(item, PARM_AC) * 100;
        const int racial_bonus = _player_armour_racial_bonus(item);

        // [ds] effectively: ac_value * (22 + Arm) / 22, where Arm =
        // Armour Skill + racial_skill_bonus / 2.
        AC += ac_value * (440 + skill(SK_ARMOUR, 20) + racial_bonus * 10) / 440;
        AC += item.plus * 100;

        // The deformed don't fit into body armour very well.
        // (This includes nagas and centaurs.)
        if (eq == EQ_BODY_ARMOUR && (player_mutation_level(MUT_DEFORMED)
            || player_mutation_level(MUT_PSEUDOPODS)))
            AC -= ac_value / 2;
    }

    // All effects negated by magical suppression should go in here.
    if (!suppressed())
    {
        AC += wearing(EQ_RINGS_PLUS, RING_PROTECTION) * 100;

        if (wearing_ego(EQ_WEAPON, SPWPN_PROTECTION))
            AC += 500;

        if (wearing_ego(EQ_SHIELD, SPARM_PROTECTION))
            AC += 300;

        AC += scan_artefacts(ARTP_AC) * 100;
    }

    if (duration[DUR_ICY_ARMOUR])
        AC += 400 + skill(SK_ICE_MAGIC, 100) / 3;    // max 13

    AC += _stoneskin_bonus();

    if (mutation[MUT_ICEMAIL])
        AC += 100 * player_icemail_armour_class();

    if (!player_is_shapechanged()
        || (form == TRAN_DRAGON && player_genus(GENPC_DRACONIAN))
        || (form == TRAN_STATUE && species == SP_GARGOYLE))
    {
        // Being a lich doesn't preclude the benefits of hide/scales -- bwr
        //
        // Note: Even though necromutation is a high level spell, it does
        // allow the character full armour (so the bonus is low). -- bwr
        if (form == TRAN_LICH)
            AC += 600;

        if (player_genus(GENPC_DRACONIAN))
        {
            AC += 400 + 100 * (experience_level / 3);  // max 13
            if (species == SP_GREY_DRACONIAN) // no breath
                AC += 500;
            if (form == TRAN_DRAGON)
                AC += 1000;
        }
        else
        {
            switch (species)
            {
            case SP_NAGA:
                AC += 100 * experience_level / 3;              // max 9
                break;

            case SP_GARGOYLE:
                AC += 200 + 100 * experience_level * 2 / 5     // max 20
                          + 100 * (max(0, experience_level - 7) * 2 / 5);
                if (form == TRAN_STATUE)
                    AC += 1300 + skill(SK_EARTH_MAGIC, 50);
                break;

            default:
                break;
            }
        }
    }
    else
    {
        // transformations:
        switch (form)
        {
        case TRAN_NONE:
        case TRAN_APPENDAGE:
        case TRAN_BLADE_HANDS:
        case TRAN_LICH:  // can wear normal body armour (no bonus)
            break;

        case TRAN_JELLY:  // no bonus
        case TRAN_BAT:
        case TRAN_PIG:
        case TRAN_PORCUPINE:
            break;

        case TRAN_SPIDER: // low level (small bonus), also gets EV
            AC += 200;
            break;

        case TRAN_ICE_BEAST:
            AC += 500 + skill(SK_ICE_MAGIC, 25) + 25;    // max 12

            if (duration[DUR_ICY_ARMOUR])
                AC += 100 + skill(SK_ICE_MAGIC, 25);     // max +7
            break;

        case TRAN_WISP:
            AC += 500 + 50 * experience_level;
            break;
        case TRAN_FUNGUS:
            AC += 1200;
            break;
        case TRAN_DRAGON: // Draconians handled above
            AC += 1600;
            break;

        case TRAN_STATUE: // main ability is armour (high bonus)
            AC += 1700 + skill(SK_EARTH_MAGIC, 50);// max 30
            // Stoneskin bonus already accounted for.
            break;

        case TRAN_TREE: // extreme bonus, no EV
            AC += 2000 + 50 * experience_level;
            break;
        }
    }

    // Scale mutations, etc.  Statues don't get an AC benefit from scales,
    // since the scales are made of the same stone as everything else.
    AC += player_mutation_level(MUT_TOUGH_SKIN)
          ? player_mutation_level(MUT_TOUGH_SKIN) * 100 : 0;                   // +1, +2, +3
    AC += player_mutation_level(MUT_SHAGGY_FUR)
          ? player_mutation_level(MUT_SHAGGY_FUR) * 100 : 0;                   // +1, +2, +3
    AC += player_mutation_level(MUT_GELATINOUS_BODY)
          ? (player_mutation_level(MUT_GELATINOUS_BODY) == 3 ? 200 : 100) : 0; // +1, +1, +2
    AC += _mut_level(MUT_IRIDESCENT_SCALES, MUTACT_FULL)
          ? 200 + _mut_level(MUT_IRIDESCENT_SCALES, MUTACT_FULL) * 200 : 0;    // +4, +6, +8
    AC += _mut_level(MUT_LARGE_BONE_PLATES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_LARGE_BONE_PLATES, MUTACT_FULL) * 100 : 0;    // +2, +3, +4
    AC += _mut_level(MUT_ROUGH_BLACK_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_ROUGH_BLACK_SCALES, MUTACT_FULL) * 300 : 0;   // +4, +7, +10
    AC += _mut_level(MUT_RUGGED_BROWN_SCALES, MUTACT_FULL) * 100;              // +1, +2, +3
    AC += _mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) * 100 +
          (_mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) > 1 ? 100 : 0);        // +1, +3, +4
    AC += _mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) * 100 +
          (_mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) > 1 ? 100 : 0);          // +1, +3, +4
    AC += _mut_level(MUT_SLIMY_GREEN_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_SLIMY_GREEN_SCALES, MUTACT_FULL) * 100 : 0;   // +2, +3, +4
    AC += _mut_level(MUT_THIN_METALLIC_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_THIN_METALLIC_SCALES, MUTACT_FULL) * 100 : 0; // +2, +3, +4
    AC += _mut_level(MUT_YELLOW_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_YELLOW_SCALES, MUTACT_FULL) * 100 : 0;        // +2, +3, +4

    return (AC / 100);
}
 /**
  * Guaranteed damage reduction.
  *
  * The percentage of the damage received that is guaranteed to be reduced
  * by the armour. As the AC roll is done before GDR is applied, GDR is only
  * useful when the AC roll is inferior to it. Therefore a higher GDR means
  * more damage reduced, but also more often.
  *
  * \f[ GDR = 14 \times (base\_AC - 2)^\frac{1}{2} \f]
  *
  * \return GDR as a percentage.
  **/
int player::gdr_perc() const
{
    switch (form)
    {
    case TRAN_DRAGON:
        return 34; // base AC 8
    case TRAN_STATUE:
        return species == SP_GARGOYLE ? 62
                                      : 39; // like plate (AC 10)
    case TRAN_TREE:
        return 48;
    default:
        break;
    }

    const item_def *body_armour = slot_item(EQ_BODY_ARMOUR, false);

    if (!body_armour)
        return 0;

    const int body_base_AC = property(*body_armour, PARM_AC);
    int gdr = 14 * sqrt(max(body_base_AC - 2, 0));

    if (species == SP_GARGOYLE)
        gdr += 24 - (gdr/6);

    return gdr;
}

int player::melee_evasion(const actor *act, ev_ignore_type evit) const
{
    return (player_evasion(evit)
            - (is_constricted() ? 3 : 0)
            - ((!act || act->visible_to(this)
                || (evit & EV_IGNORE_HELPLESS)) ? 0 : 10)
            - (you_are_delayed()
               && !(evit & EV_IGNORE_HELPLESS)
               && !delay_is_run(current_delay_action())? 5 : 0));
}

bool player::heal(int amount, bool max_too)
{
    ASSERT(!max_too);
    ::inc_hp(amount);
    return true; /* TODO Check whether the player was healed. */
}

mon_holy_type player::holiness() const
{
    if (species == SP_GARGOYLE || form == TRAN_STATUE || form == TRAN_WISP
        || petrified())
    {
        return MH_NONLIVING;
    }

    if (is_undead)
        return MH_UNDEAD;

    return MH_NATURAL;
}

bool player::undead_or_demonic() const
{
    // This is only for TSO-related stuff, so demonspawn are included.
    return is_undead || species == SP_DEMONSPAWN;
}

bool player::is_holy(bool check_spells) const
{
    if (is_good_god(religion) && check_spells)
        return true;

    return false;
}

bool player::is_unholy(bool check_spells) const
{
    return species == SP_DEMONSPAWN;
}

bool player::is_evil(bool check_spells) const
{
    if (holiness() == MH_UNDEAD)
        return true;

    if (is_evil_god(religion) && check_spells)
        return true;

    return false;
}

// This is a stub. Check is used only for silver damage. Worship of chaotic
// gods should probably be checked in the non-existing player::is_unclean,
// which could be used for something Zin-related (such as a priestly monster).
bool player::is_chaotic() const
{
    return false;
}

bool player::is_artificial() const
{
    return (species == SP_GARGOYLE || form == TRAN_STATUE || petrified());
}

bool player::is_unbreathing() const
{
    switch (form)
    {
    case TRAN_LICH:
    case TRAN_STATUE:
    case TRAN_FUNGUS:
    case TRAN_TREE:
    case TRAN_JELLY:
    case TRAN_WISP:
        return true;
    default:
        break;
    }

    if (petrified())
        return true;

    return player_mutation_level(MUT_UNBREATHING);
}

bool player::is_insubstantial() const
{
    return form == TRAN_WISP;
}

int player::res_acid(bool calc_unid) const
{
    return player_res_acid(calc_unid);
}

int player::res_fire() const
{
    return player_res_fire();
}

int player::res_holy_fire() const
{
    if (species == SP_DJINNI)
        return 3;
    return actor::res_holy_fire();
}

int player::res_steam() const
{
    return player_res_steam();
}

int player::res_cold() const
{
    return player_res_cold();
}

int player::res_elec() const
{
    return (player_res_electricity() * 2);
}

int player::res_water_drowning() const
{
    int rw = 0;

    if (is_unbreathing()
        || species == SP_MERFOLK && !form_changed_physiology()
        || species == SP_OCTOPODE && !form_changed_physiology()
        || form == TRAN_ICE_BEAST)
    {
        rw++;
    }

    // A fiery lich/hot statue suffers from quenching but not drowning, so
    // neutral resistance sounds ok.
    if (species == SP_DJINNI)
        rw--;

    return rw;
}

int player::res_asphyx() const
{
    // The unbreathing are immune to asphyxiation.
    if (is_unbreathing())
        return 1;

    return 0;
}

int player::res_poison(bool temp) const
{
    return player_res_poison(true, temp);
}

int player::res_rotting(bool temp) const
{
    if (temp && (petrified() || form == TRAN_STATUE || form == TRAN_WISP))
        return 3;

    if (species == SP_GARGOYLE)
        return 3;

    if (mutation[MUT_FOUL_STENCH])
        return 1;

    switch (is_undead)
    {
    default:
    case US_ALIVE:
        return 0;

    case US_HUNGRY_DEAD:
        return 1; // rottable by Zin, not by necromancy

    case US_SEMI_UNDEAD:
        if (temp && hunger_state < HS_SATIATED)
            return 1;
        return 0; // no permanent resistance

    case US_UNDEAD:
        if (!temp && form == TRAN_LICH)
            return 0;
        return 3; // full immunity
    }
}

int player::res_sticky_flame() const
{
    return player_res_sticky_flame();
}

int player::res_holy_energy(const actor *attacker) const
{
    if (undead_or_demonic())
        return -2;

    if (is_evil())
        return -1;

    if (is_holy())
        return 1;

    return 0;
}

int player::res_negative_energy(bool intrinsic_only) const
{
    return player_prot_life(!intrinsic_only, true, !intrinsic_only);
}

int player::res_torment() const
{
    return player_res_torment();
}

int player::res_wind() const
{
    // Full control of the winds around you can negate a hostile tornado.
    return duration[DUR_TORNADO] ? 1 : 0;
}

int player::res_petrify(bool temp) const
{
    if (player_mutation_level(MUT_PETRIFICATION_RESISTANCE))
        return 1;

    if (temp && (form == TRAN_STATUE || form == TRAN_WISP))
        return 1;
    return 0;
}

int player::res_constrict() const
{
    if (form == TRAN_JELLY || form == TRAN_PORCUPINE
        || form == TRAN_WISP)
    {
        return 3;
    }
    return 0;
}

int player::res_magic() const
{
    return player_res_magic();
}

int player_res_magic(bool calc_unid, bool temp)
{
    int rm = 0;

    switch (you.species)
    {
    default:
        rm = you.experience_level * 3;
        break;
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
    case SP_DEEP_ELF:
    case SP_VAMPIRE:
    case SP_DEMIGOD:
    case SP_OGRE:
        rm = you.experience_level * 4;
        break;
    case SP_NAGA:
    case SP_MUMMY:
        rm = you.experience_level * 5;
        break;
    case SP_PURPLE_DRACONIAN:
    case SP_DEEP_DWARF:
    case SP_FELID:
        rm = you.experience_level * 6;
        break;
    case SP_SPRIGGAN:
        rm = you.experience_level * 7;
        break;
    }

    // All effects negated by magical suppression should go in here.
    if (!you.suppressed())
    {
        // randarts
        rm += you.scan_artefacts(ARTP_MAGIC, calc_unid);

        // armour
        rm += 30 * you.wearing_ego(EQ_ALL_ARMOUR, SPARM_MAGIC_RESISTANCE,
                                         calc_unid);

        // rings of magic resistance
        rm += 40 * you.wearing(EQ_RINGS, RING_PROTECTION_FROM_MAGIC, calc_unid);
    }

    // Mutations
    rm += 30 * player_mutation_level(MUT_MAGIC_RESISTANCE);

    // transformations
    if (you.form == TRAN_LICH && temp)
        rm += 50;

    // Trog's Hand
    if (you.attribute[ATTR_DIVINE_REGENERATION] && temp)
        rm += 70;

    // Enchantment effect
    if (you.duration[DUR_LOWERED_MR] && temp)
        rm /= 2;

    if (rm < 0)
        rm = 0;

    return rm;
}

bool player::no_tele(bool calc_unid, bool permit_id, bool blinking) const
{
    if (duration[DUR_DIMENSION_ANCHOR])
        return true;

    if (crawl_state.game_is_sprint() && !blinking)
        return true;

    if (form == TRAN_TREE)
        return true;

    return (has_notele_item(calc_unid)
            || stasis_blocks_effect(calc_unid, permit_id, NULL)
            || crawl_state.game_is_zotdef() && orb_haloed(pos()));
}

bool player::fights_well_unarmed(int heavy_armour_penalty)
{
    return (burden_state == BS_UNENCUMBERED
            && x_chance_in_y(skill(SK_UNARMED_COMBAT, 10), 200)
            && x_chance_in_y(2, 1 + heavy_armour_penalty));
}

bool player::cancellable_flight() const
{
    return duration[DUR_FLIGHT] && !permanent_flight()
           && !attribute[ATTR_FLIGHT_UNCANCELLABLE];
}

bool player::permanent_flight() const
{
    return attribute[ATTR_PERM_FLIGHT];
}

bool player::racial_permanent_flight() const
{
    return (species == SP_TENGU && experience_level >= 15
            || species == SP_BLACK_DRACONIAN && experience_level >= 14
            || species == SP_GARGOYLE && experience_level >= 14);
}

bool player::tengu_flight() const
{
    // Only Tengu get perks for flying.
    return (species == SP_TENGU && flight_mode());
}

bool player::nightvision() const
{
    return (is_undead
            || (religion == GOD_YREDELEMNUL && piety >= piety_breakpoint(2)));
}

reach_type player::reach_range() const
{
    const item_def *wpn = weapon();
    if (wpn)
        return weapon_reach(*wpn);
    return REACH_NONE;
}

monster_type player::mons_species(bool zombie_base) const
{
    return player_species_to_mons_species(species);
}

bool player::poison(actor *agent, int amount, bool force)
{
    return ::poison_player(amount, agent? agent->name(DESC_A, true) : "", "",
                           force);
}

void player::expose_to_element(beam_type element, int _strength,
                               bool damage_inventory, bool slow_cold_blood)
{
    ::expose_player_to_element(element, _strength, damage_inventory,
                               slow_cold_blood);
}

void player::blink(bool allow_partial_control)
{
    random_blink(allow_partial_control);
}

void player::teleport(bool now, bool abyss_shift, bool wizard_tele)
{
    ASSERT(!crawl_state.game_is_arena());

    if (now)
        you_teleport_now(true, abyss_shift, wizard_tele);
    else
        you_teleport();
}

int player::hurt(const actor *agent, int amount, beam_type flavour,
                 bool cleanup_dead, bool attacker_effects)
{
    // We ignore cleanup_dead here.
    if (!agent)
    {
        // FIXME: This can happen if a deferred_damage_fineff does damage
        // to a player from a dead monster.  We should probably not do that,
        // but it could be tricky to fix, so for now let's at least avoid
        // a crash even if it does mean funny death messages.
        ouch(amount, NON_MONSTER, KILLED_BY_MONSTER, "",
             false, "posthumous revenge", attacker_effects);
    }
    else if (agent->is_monster())
    {
        const monster* mon = agent->as_monster();
        ouch(amount, mon->mindex(),
             KILLED_BY_MONSTER, "", mon->visible_to(this), NULL,
             attacker_effects);
    }
    else
    {
        // Should never happen!
        die("player::hurt() called for self-damage");
    }

    if ((flavour == BEAM_NUKE || flavour == BEAM_DISINTEGRATION) && can_bleed())
        blood_spray(pos(), type, amount / 5);

    return amount;
}

void player::drain_stat(stat_type s, int amount, actor *attacker)
{
    if (attacker == NULL)
        lose_stat(s, amount, false, "");
    else if (attacker->is_monster())
        lose_stat(s, amount, attacker->as_monster(), false);
    else if (attacker->is_player())
        lose_stat(s, amount, false, "suicide");
    else
        lose_stat(s, amount, false, "");
}

bool player::rot(actor *who, int amount, int immediate, bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    if (amount <= 0 && immediate <= 0)
        return false;

    if (res_rotting() || duration[DUR_DEATHS_DOOR])
    {
        mpr("You feel terrible.");
        return false;
    }

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from decay!");
        return false;
    }

    if (immediate > 0)
        rot_hp(immediate);

    // Either this, or the actual rotting message should probably
    // be changed so that they're easier to tell apart. -- bwr
    if (!quiet)
    {
        mprf(MSGCH_WARN, "You feel your flesh %s away!",
             (rotting > 0 || immediate) ? "rotting" : "start to rot");
    }

    rotting += amount;

    learned_something_new(HINT_YOU_ROTTING);

    if (one_chance_in(4))
        sicken(50 + random2(100));

    return true;
}

bool player::drain_exp(actor *who, bool quiet, int pow)
{
    return ::drain_exp(!quiet, pow);
}

void player::confuse(actor *who, int str)
{
    confuse_player(str);
}

/*
 * Paralyse the player for str turns.
 *
 *  Duration is capped at 13.
 *
 * @param who Pointer to the actor who paralysed the player.
 * @param str The number of turns the paralysis will last.
 * @param source Description of the source of the paralysis.
 */
void player::paralyse(actor *who, int str, string source)
{
    ASSERT(!crawl_state.game_is_arena());

    // The shock is too mild to do damage.
    if (stasis_blocks_effect(true, true, "%s gives you a mild electric shock."))
        return;

    if (!(who && who->as_monster() && who->as_monster()->type == MONS_RED_WASP)
        && who && (duration[DUR_PARALYSIS] || duration[DUR_PARALYSIS_IMMUNITY]))
    {
        canned_msg(MSG_YOU_RESIST);
        return;
    }

    int &paralysis(duration[DUR_PARALYSIS]);

    if (source.empty() && who)
        source = who->name(DESC_A);

    if (!paralysis && !source.empty())
    {
        take_note(Note(NOTE_PARALYSIS, str, 0, source.c_str()));
        props["paralysed_by"] = source;
    }


    mprf("You %s the ability to move!",
         paralysis ? "still haven't" : "suddenly lose");

    str *= BASELINE_DELAY;
    if (str > paralysis && (paralysis < 3 || one_chance_in(paralysis)))
        paralysis = str;

    if (paralysis > 13 * BASELINE_DELAY)
        paralysis = 13 * BASELINE_DELAY;

    stop_constricting_all();
    end_searing_ray();
}

void player::petrify(actor *who, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_petrify() && !force)
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from petrification!");
        return;
    }

    if (petrifying())
    {
        mpr("Your limbs have turned to stone.");
        duration[DUR_PETRIFYING] = 1;
        return;
    }

    if (petrified())
        return;

    duration[DUR_PETRIFYING] = 3 * BASELINE_DELAY;

    redraw_evasion = true;
    mpr("You are slowing down.", MSGCH_WARN);
}

bool player::fully_petrify(actor *foe, bool quiet)
{
    duration[DUR_PETRIFIED] = 6 * BASELINE_DELAY
                        + random2(4 * BASELINE_DELAY);
    redraw_evasion = true;
    mpr("You have turned to stone.");

    end_searing_ray();

    return true;
}

void player::slow_down(actor *foe, int str)
{
    ::slow_player(str);
}

int player::has_claws(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring claws with them
        if (form == TRAN_DRAGON)
            return 3;

        // blade hands override claws
        if (form == TRAN_BLADE_HANDS)
            return 0;

        // Most forms suppress natural claws.
        if (!form_keeps_mutations())
            return 0;
    }

    if (const int c = species_has_claws(species))
        return c;

    return player_mutation_level(MUT_CLAWS, allow_tran);
}

bool player::has_usable_claws(bool allow_tran) const
{
    return (!player_wearing_slot(EQ_GLOVES) && has_claws(allow_tran));
}

int player::has_talons(bool allow_tran) const
{
    // XXX: Do merfolk in water belong under allow_tran?
    if (fishtail)
        return 0;

    return player_mutation_level(MUT_TALONS, allow_tran);
}

bool player::has_usable_talons(bool allow_tran) const
{
    return (!player_wearing_slot(EQ_BOOTS) && has_talons(allow_tran));
}

int player::has_fangs(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring fangs with them
        if (form == TRAN_DRAGON)
            return 3;
    }

    return player_mutation_level(MUT_FANGS, allow_tran);
}

int player::has_usable_fangs(bool allow_tran) const
{
    return has_fangs(allow_tran);
}

int player::has_tail(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring a tail with them
        if (form == TRAN_DRAGON)
            return 1;

        // Most transformations suppress a tail.
        if (!form_keeps_mutations())
            return 0;
    }

    // XXX: Do merfolk in water belong under allow_tran?
    if (player_genus(GENPC_DRACONIAN)
        || fishtail
        || player_mutation_level(MUT_STINGER, allow_tran))
    {
        return 1;
    }

    return 0;
}

int player::has_usable_tail(bool allow_tran) const
{
    // TSO worshippers don't use their stinger in order
    // to avoid poisoning.
    if (religion == GOD_SHINING_ONE
        && player_mutation_level(MUT_STINGER, allow_tran) > 0)
    {
        return 0;
    }

    return has_tail(allow_tran);
}

// Whether the player has a usable offhand for the
// purpose of punching.
bool player::has_usable_offhand() const
{
    if (player_wearing_slot(EQ_SHIELD))
        return false;

    const item_def* wp = slot_item(EQ_WEAPON);
    return (!wp || hands_reqd(*wp, body_size()) != HANDS_TWO);
}

bool player::has_usable_tentacle() const
{
    return usable_tentacles();
}

int player::usable_tentacles() const
{
    int numtentacle = has_usable_tentacles();

    if (numtentacle == 0)
        return false;

    int free_tentacles = numtentacle - num_constricting();

    if (shield())
        free_tentacles -= 2;

    const item_def* wp = slot_item(EQ_WEAPON);
    if (wp)
    {
        hands_reqd_type hands_req = hands_reqd(*wp, body_size());
        free_tentacles -= 2 * hands_req + 2;
    }

    return free_tentacles;
}

int player::has_pseudopods(bool allow_tran) const
{
    return player_mutation_level(MUT_PSEUDOPODS, allow_tran);
}

int player::has_usable_pseudopods(bool allow_tran) const
{
    return has_pseudopods(allow_tran);
}

int player::has_tentacles(bool allow_tran) const
{
    if (allow_tran)
    {
        // Most transformations suppress tentacles.
        if (!form_keeps_mutations())
            return 0;
    }

    if (species == SP_OCTOPODE)
        return 8;

    return 0;
}

int player::has_usable_tentacles(bool allow_tran) const
{
    return has_tentacles(allow_tran);
}

bool player::sicken(int amount, bool allow_hint, bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_rotting() || amount <= 0)
        return false;

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from disease!");
        return false;
    }

    if (!quiet)
        mpr("You feel ill.");

    disease += amount * BASELINE_DELAY;
    if (disease > 210 * BASELINE_DELAY)
        disease = 210 * BASELINE_DELAY;

    if (allow_hint)
        learned_something_new(HINT_YOU_SICK);
    return true;
}

bool player::can_see_invisible(bool calc_unid, bool items) const
{
    if (crawl_state.game_is_arena())
        return true;

    // All effects negated by magical suppression should go in here.
    if (items && !suppressed())
    {
        if (wearing(EQ_RINGS, RING_SEE_INVISIBLE, calc_unid)
            // armour: (checks head armour only)
            || wearing_ego(EQ_HELMET, SPARM_SEE_INVISIBLE)
            // randart gear
            || scan_artefacts(ARTP_EYESIGHT, calc_unid) > 0)
        {
            return true;
        }
    }

    // Possible to have both with a temp mutation.
    if (player_mutation_level(MUT_ACUTE_VISION)
        && !player_mutation_level(MUT_BLURRY_VISION))
    {
        return true;
    }

    // antennae give sInvis at 3
    if (player_mutation_level(MUT_ANTENNAE) == 3)
        return true;

    if (player_mutation_level(MUT_EYEBALLS) == 3)
        return true;

    if (religion == GOD_ASHENZARI && piety >= piety_breakpoint(2)
        && !player_under_penance())
    {
        return true;
    }

    return false;
}

bool player::can_see_invisible() const
{
    return can_see_invisible(true, true);
}

bool player::invisible() const
{
    return (duration[DUR_INVIS] && !backlit());
}

bool player::misled() const
{
    return duration[DUR_MISLED];
}

bool player::visible_to(const actor *looker) const
{
    if (crawl_state.game_is_arena())
        return false;

    if (this == looker)
        return (can_see_invisible() || !invisible());

    const monster* mon = looker->as_monster();
    return (mons_sense_invis(mon) && distance2(pos(), mon->pos()) <= dist_range(4))
            || (!mon->has_ench(ENCH_BLIND) && (!invisible() || mon->can_see_invisible()));
}

bool player::backlit(bool check_haloed, bool self_halo) const
{
    if (get_contamination_level() > 1 || duration[DUR_CORONA]
        || duration[DUR_LIQUID_FLAMES] || duration[DUR_QUAD_DAMAGE])
    {
        return true;
    }
    if (check_haloed)
        return (!umbraed() && haloed()
                && (self_halo || halo_radius2() == -1));
    return false;
}

bool player::umbra(bool check_haloed, bool self_halo) const
{
    if (backlit())
        return false;

    if (check_haloed)
        return (umbraed() && !haloed()
                && (self_halo || umbra_radius2() == -1));
    return false;
}

bool player::glows_naturally() const
{
    return false;
}

// This is the imperative version.
void player::backlight()
{
    if (!duration[DUR_INVIS])
    {
        if (duration[DUR_CORONA] || glows_naturally())
            mpr("You glow brighter.");
        else
            mpr("You are outlined in light.");

        increase_duration(DUR_CORONA, random_range(15, 35), 250);
    }
    else
    {
        mpr("You feel strangely conspicuous.");

        increase_duration(DUR_CORONA, random_range(3, 5), 250);
    }
}

bool player::has_lifeforce() const
{
    const mon_holy_type holi = holiness();

    return (holi == MH_NATURAL || holi == MH_PLANT);
}

bool player::can_mutate() const
{
    return true;
}

bool player::can_safely_mutate() const
{
    if (!can_mutate())
        return false;

    return (!is_undead
            || is_undead == US_SEMI_UNDEAD
               && hunger_state == HS_ENGORGED);
}

// Is the player too undead to bleed, rage, and polymorph?
bool player::is_lifeless_undead() const
{
    if (is_undead == US_SEMI_UNDEAD)
        return hunger_state <= HS_SATIATED;
    else
        return is_undead != US_ALIVE;
}

bool player::can_polymorph() const
{
    return !(transform_uncancellable || is_lifeless_undead());
}

bool player::can_bleed(bool allow_tran) const
{
    if (allow_tran)
    {
        // These transformations don't bleed. Lichform is handled as undead.
        if (form == TRAN_STATUE || form == TRAN_ICE_BEAST
            || form == TRAN_SPIDER || form == TRAN_TREE
            || form == TRAN_FUNGUS || form == TRAN_PORCUPINE)
        {
            return false;
        }
    }

    if (is_lifeless_undead() || holiness() == MH_NONLIVING)
        return false;

    return true;
}

bool player::malmutate(const string &reason)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_mutate())
        return false;

    if (one_chance_in(5))
    {
        if (mutate(RANDOM_MUTATION, reason))
        {
            learned_something_new(HINT_YOU_MUTATED);
            return true;
        }
    }

    return mutate(RANDOM_BAD_MUTATION, reason);
}

bool player::polymorph(int pow)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_polymorph())
        return false;

    transformation_type f = TRAN_NONE;

    // Be unreliable over lava.  This is not that important as usually when
    // it matters you'll have temp flight and thus that pig will fly (and
    // when flight times out, we'll have roasted bacon).
    for (int tries = 0; tries < 3; tries++)
    {
        // Whole-body transformations only; mere appendage doesn't seem fitting.
        f = random_choose_weighted(
            100, TRAN_BAT,
            100, TRAN_FUNGUS,
            100, TRAN_PIG,
            100, TRAN_TREE,
            100, TRAN_PORCUPINE,
            100, TRAN_WISP,
             20, TRAN_SPIDER,
             20, TRAN_ICE_BEAST,
              5, TRAN_STATUE,
              1, TRAN_DRAGON,
              0);
        // need to do a dry run first, as Zin's protection has a random factor
        if (transform(pow, f, false, true))
            break;
        f = TRAN_NONE;
    }

    if (f && transform(pow, f))
    {
        transform_uncancellable = true;
        return true;
    }
    return false;
}

bool player::is_icy() const
{
    return (form == TRAN_ICE_BEAST);
}

bool player::is_fiery() const
{
    return false;
}

bool player::is_skeletal() const
{
    return false;
}

void player::shiftto(const coord_def &c)
{
    crawl_view.shift_player_to(c);
    set_position(c);
    clear_far_constrictions();
}

void player::reset_prev_move()
{
    prev_move.reset();
}

bool player::asleep() const
{
    return duration[DUR_SLEEP];
}

bool player::cannot_act() const
{
    return (asleep() || cannot_move());
}

bool player::can_throw_large_rocks() const
{
    return (species == SP_OGRE || species == SP_TROLL);
}

bool player::can_smell() const
{
    return (species != SP_MUMMY);
}

void player::hibernate(int)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_hibernate() || duration[DUR_SLEEP_IMMUNITY])
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    stop_constricting_all();
    end_searing_ray();
    mpr("You fall asleep.");

    stop_delay();
    flash_view(DARKGREY);

    // Do this *after* redrawing the view, or viewwindow() will no-op.
    set_duration(DUR_SLEEP, 3 + random2avg(5, 2));
}

void player::put_to_sleep(actor*, int power)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_sleep() || duration[DUR_SLEEP_IMMUNITY])
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    mpr("You fall asleep.");

    stop_constricting_all();
    end_searing_ray();
    stop_delay();
    flash_view(DARKGREY);

    // As above, do this after redraw.
    set_duration(DUR_SLEEP, 5 + random2avg(power/10, 5));
}

void player::awake()
{
    ASSERT(!crawl_state.game_is_arena());

    duration[DUR_SLEEP] = 0;
    duration[DUR_SLEEP_IMMUNITY] = 1;
    mpr("You wake up.");
    flash_view(BLACK);
}

void player::check_awaken(int disturbance)
{
    if (asleep() && x_chance_in_y(disturbance + 1, 50))
        awake();
}

int player::beam_resists(bolt &beam, int hurted, bool doEffects, string source)
{
    return check_your_resists(hurted, beam.flavour, source, &beam, doEffects);
}

void player::set_place_info(PlaceInfo place_info)
{
    place_info.assert_validity();

    if (place_info.is_global())
        global_info = place_info;
    else
        branch_info[place_info.branch] = place_info;
}

vector<PlaceInfo> player::get_all_place_info(bool visited_only,
                                             bool dungeon_only) const
{
    vector<PlaceInfo> list;

    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        if (visited_only && branch_info[i].num_visits == 0
            || dungeon_only && !is_connected_branch((branch_type)i))
        {
            continue;
        }
        list.push_back(branch_info[i]);
    }

    return list;
}

bool player::do_shaft()
{
    dungeon_feature_type force_stair = DNGN_UNSEEN;

    if (!is_valid_shaft_level())
        return false;

    // Handle instances of do_shaft() being invoked magically when
    // the player isn't standing over a shaft.
    if (get_trap_type(pos()) != TRAP_SHAFT)
    {
        switch (grd(pos()))
        {
        case DNGN_FLOOR:
        case DNGN_OPEN_DOOR:
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_MAGICAL:
        case DNGN_TRAP_NATURAL:
        case DNGN_UNDISCOVERED_TRAP:
        case DNGN_ENTER_SHOP:
            break;

        default:
            return false;
        }

        handle_items_on_shaft(pos(), false);

        if (!ground_level() || total_weight() == 0)
            return true;

        force_stair = DNGN_TRAP_NATURAL;
    }

    down_stairs(force_stair);

    return true;
}

bool player::did_escape_death() const
{
    return (escaped_death_cause != NUM_KILLBY);
}

void player::reset_escaped_death()
{
    escaped_death_cause = NUM_KILLBY;
    escaped_death_aux   = "";
}

void player::add_gold(int delta)
{
    set_gold(gold + delta);
}

void player::del_gold(int delta)
{
    set_gold(gold - delta);
}

void player::set_gold(int amount)
{
    ASSERT(amount >= 0);

    if (amount != gold)
    {
        const int old_gold = gold;
        gold = amount;
        shopping_list.gold_changed(old_gold, gold);
    }
}

void player::increase_duration(duration_type dur, int turns, int cap,
                               const char* msg)
{
    if (msg)
        mpr(msg);
    cap *= BASELINE_DELAY;

    duration[dur] += turns * BASELINE_DELAY;
    if (cap && duration[dur] > cap)
        duration[dur] = cap;
}

void player::set_duration(duration_type dur, int turns,
                          int cap, const char * msg)
{
    duration[dur] = 0;
    increase_duration(dur, turns, cap, msg);
}

void player::goto_place(const level_id &lid)
{
    where_are_you = static_cast<branch_type>(lid.branch);
    depth = lid.depth;
    ASSERT_RANGE(depth, 1, brdepth[where_are_you] + 1);
}

bool player::attempt_escape(int attempts)
{
    monster *themonst;

    if (!is_constricted())
        return true;

    themonst = monster_by_mid(constricted_by);
    ASSERT(themonst);
    escape_attempts += attempts;

    // player breaks free if (4+n)d(8+str/4) >= 5d(8+HD/4)
    if (roll_dice(4 + escape_attempts, 8 + div_rand_round(strength(), 4))
        >= roll_dice(5, 8 + div_rand_round(themonst->hit_dice, 4)))
    {
        mprf("You escape %s's grasp.", themonst->name(DESC_THE, true).c_str());

        // Stun the monster to prevent it from constricting again right away.
        themonst->speed_increment -= 5;

        stop_being_constricted(true);

        return true;
    }
    else
    {
        string emsg = "Your attempt to break free from ";
        emsg += themonst->name(DESC_THE, true);
        emsg += " fails, but you feel that another attempt might succeed.";
        mpr(emsg);
        turn_is_over = true;
        return false;
    }
}

void player::sentinel_mark(bool trap)
{
    if (duration[DUR_SENTINEL_MARK])
    {
        mpr("The mark upon you grows brighter.");
        increase_duration(DUR_SENTINEL_MARK, random_range(30, 50), 250);
    }
    else
    {
        mpr("A sentinel's mark forms upon you.", MSGCH_WARN);
        increase_duration(DUR_SENTINEL_MARK, trap ? random_range(35, 55)
                                                  : random_range(50, 80),
                          250);
    }
}

bool player::made_nervous_by(const coord_def &p)
{
    if (form != TRAN_FUNGUS)
        return false;
    monster* mons = monster_at(p);
    if (mons && !mons_is_firewood(mons))
        return false;
    for (monster_iterator mi(get_los()); mi; ++mi)
    {
        if (!mons_is_wandering(*mi)
            && !mi->asleep()
            && !mi->confused()
            && !mi->cannot_act()
            && you.can_see(*mi)
            && !mons_is_firewood(*mi)
            && !mi->wont_attack()
            && !mi->neutral())
        {
            return true;
        }
    }
    return false;
}

void player::weaken(actor *attacker, int pow)
{
    if (!duration[DUR_WEAK])
        mpr("You feel yourself grow feeble.", MSGCH_WARN);
    else
        mpr("You feel as though you will be weak longer.", MSGCH_WARN);

    increase_duration(DUR_WEAK, pow + random2(pow + 3), 50);
}

/*
 * Check if the player is about to die from flight/form expiration.
 *
 * Check whether the player is on a cell which would be deadly if not for some
 * temporary condition, and if such condition is expiring. In that case, we
 * give a strong warning to the player. The actual message printing is done
 * by the caller.
 *
 * @param dur the duration to check for dangerous expiration.
 * @param p the coordinates of the cell to check. Defaults to player position.
 * @return whether the player is in immediate danger.
 */
bool need_expiration_warning(duration_type dur, coord_def p)
{
    if (!is_feat_dangerous(env.grid(p), true) || !dur_expiring(dur))
        return false;

    if (dur == DUR_FLIGHT)
        return true;
    else if (dur == DUR_TRANSFORMATION
             && (!you.airborne() || form_can_fly()))
    {
        return true;
    }
    return false;
}

bool need_expiration_warning(coord_def p)
{
    return need_expiration_warning(DUR_FLIGHT, p)
           || need_expiration_warning(DUR_TRANSFORMATION, p);
}

static string _constriction_description()
{
    string cinfo = "";
    vector<string> c_name;

    const int num_free_tentacles = you.usable_tentacles();
    if (num_free_tentacles)
    {
        cinfo += make_stringf("You have %d tentacle%s available for constriction.",
                              num_free_tentacles,
                              num_free_tentacles > 1 ? "s" : "");
    }
    // name of what this monster is constricted by, if any
    if (you.is_constricted())
    {
        if (!cinfo.empty())
            cinfo += "\n";

        cinfo += make_stringf("You are being %s by %s.",
                      you.held == HELD_MONSTER ? "held" : "constricted",
                      monster_by_mid(you.constricted_by)->name(DESC_A).c_str());
    }

    if (you.constricting && !you.constricting->empty())
    {
        actor::constricting_t::const_iterator i;
        for (i = you.constricting->begin(); i != you.constricting->end(); ++i)
        {
            monster *whom = monster_by_mid(i->first);
            ASSERT(whom);
            c_name.push_back(whom->name(DESC_A));
        }

        if (!cinfo.empty())
            cinfo += "\n";

        cinfo += "You are constricting ";
        cinfo += comma_separated_line(c_name.begin(), c_name.end());
        cinfo += ".";
    }

    return cinfo;
}

void count_action(caction_type type, int subtype)
{
    pair<caction_type, int> pair(type, subtype);
    if (you.action_count.find(pair) == you.action_count.end())
        you.action_count[pair].init(0);
    you.action_count[pair][you.experience_level - 1]++;
}

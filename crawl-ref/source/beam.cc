/**
 * @file
 * @brief Functions related to ranged attacks.
**/

#include "AppHdr.h"

#include "beam.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>

#include "externs.h"
#include "options.h"

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "branch.h"
#include "cio.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "dungeon.h"
#include "dgnevent.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "exercise.h"
#include "godabil.h"
#include "fprop.h"
#include "fight.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "ouch.h"
#include "potion.h"
#include "religion.h"
#include "godconduct.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-transloc.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "xom.h"

#define BEAM_STOP       1000        // all beams stopped by subtracting this
                                    // from remaining range

// Helper functions (some of these should probably be public).
static void _ench_animation(int flavour, const monster* mon = NULL,
                            bool force = false);
static beam_type _chaos_beam_flavour();
static string _beam_type_name(beam_type type);
static bool _ench_flavour_affects_monster(beam_type flavour, const monster* mon,
                                          bool intrinsic_only = false);

tracer_info::tracer_info()
{
    reset();
}

void tracer_info::reset()
{
    count = power = hurt = helped = 0;
    dont_stop = false;
}

const tracer_info& tracer_info::operator+=(const tracer_info &other)
{
    count  += other.count;
    power  += other.power;
    hurt   += other.hurt;
    helped += other.helped;

    dont_stop = dont_stop || other.dont_stop;

    return *this;
}

bool bolt::is_blockable() const
{
    // BEAM_ELECTRICITY is added here because chain lightning is not
    // a true beam (stops at the first target it gets to and redirects
    // from there)... but we don't want it shield blockable.
    return (!is_beam && !is_explosion && flavour != BEAM_ELECTRICITY);
}

void bolt::emit_message(msg_channel_type chan, const char* m)
{
    const string message = m;
    if (message_cache.find(message) == message_cache.end())
        mpr(m, chan);

    message_cache.insert(message);
}

kill_category bolt::whose_kill() const
{
    if (YOU_KILL(thrower))
        return KC_YOU;
    else if (MON_KILL(thrower))
    {
        if (beam_source == ANON_FRIENDLY_MONSTER)
            return KC_FRIENDLY;
        if (!invalid_monster_index(beam_source))
        {
            const monster* mon = &menv[beam_source];
            if (mon->friendly())
                return KC_FRIENDLY;
        }
    }
    return KC_OTHER;
}

// A simple animated flash from Rupert Smith (expanded to be more
// generic).
static void _zap_animation(int colour, const monster* mon = NULL,
                           bool force = false)
{
    coord_def p = you.pos();

    if (mon)
    {
        if (!force && !mon->visible_to(&you))
            return;

        p = mon->pos();
    }

    if (!you.see_cell(p))
        return;

    const coord_def drawp = grid2view(p);

    if (in_los_bounds_v(drawp))
    {
#ifdef USE_TILE
        tiles.add_overlay(p, tileidx_zap(colour));
#endif
#ifndef USE_TILE_LOCAL
        view_update();
        cgotoxy(drawp.x, drawp.y, GOTO_DNGN);
        put_colour_ch(colour, dchar_glyph(DCHAR_FIRED_ZAP));
#endif

        update_screen();

        int zap_delay = 50;
        // Scale delay to match change in arena_delay.
        if (crawl_state.game_is_arena())
        {
            zap_delay *= Options.arena_delay;
            zap_delay /= 600;
        }

        delay(zap_delay);
    }
}

// Special front function for zap_animation to interpret enchantment flavours.
static void _ench_animation(int flavour, const monster* mon, bool force)
{
    element_type elem;
    switch (flavour)
    {
    case BEAM_HEALING:
        elem = ETC_HEAL;
        break;
    case BEAM_PAIN:
        elem = ETC_UNHOLY;
        break;
    case BEAM_DISPEL_UNDEAD:
        elem = ETC_HOLY;
        break;
    case BEAM_POLYMORPH:
    case BEAM_MALMUTATE:
        elem = ETC_MUTAGENIC;
        break;
    case BEAM_CHAOS:
        elem = ETC_RANDOM;
        break;
    case BEAM_TELEPORT:
    case BEAM_BANISH:
    case BEAM_BLINK:
    case BEAM_BLINK_CLOSE:
        elem = ETC_WARP;
        break;
    case BEAM_MAGIC:
        elem = ETC_MAGIC;
        break;
    default:
        elem = ETC_ENCHANT;
        break;
    }

    _zap_animation(element_colour(elem), mon, force);
}

// If needs_tracer is true, we need to check the beam path for friendly
// monsters.
spret_type zapping(zap_type ztype, int power, bolt &pbolt,
                   bool needs_tracer, const char* msg, bool fail)
{
    dprf(DIAG_BEAM, "zapping: power=%d", power);

    pbolt.thrower = KILL_YOU_MISSILE;

    // Check whether tracer goes through friendlies.
    // NOTE: Whenever zapping() is called with a randomised value for power
    // (or effect), player_tracer should be called directly with the highest
    // power possible respecting current skill, experience level, etc.
    if (needs_tracer && !player_tracer(ztype, power, pbolt))
        return SPRET_ABORT;

    fail_check();
    // Fill in the bolt structure.
    zappy(ztype, power, pbolt);

    if (msg)
        mpr(msg);

    if (ztype == ZAP_LIGHTNING_BOLT)
    {
        noisy(25, you.pos(), "You hear a mighty clap of thunder!");
        pbolt.heard = true;
    }

    if (ztype == ZAP_DIG)
        pbolt.aimed_at_spot = false;

    pbolt.fire();

    return SPRET_SUCCESS;
}

// Returns true if the path is considered "safe", and false if there are
// monsters in the way the player doesn't want to hit.
// NOTE: Doesn't check for the player being hit by a rebounding lightning bolt.
bool player_tracer(zap_type ztype, int power, bolt &pbolt, int range)
{
    // Non-controlleable during confusion.
    // (We'll shoot in a different direction anyway.)
    if (you.confused())
        return true;

    zappy(ztype, power, pbolt);

    // Special cases so that tracers behave properly.
    if (pbolt.name != "orb of energy"
        && pbolt.affects_wall(DNGN_TREE) == MB_FALSE
        && pbolt.affects_wall(DNGN_MANGROVE) == MB_FALSE)
    {
        pbolt.name = "unimportant";
    }

    pbolt.is_tracer      = true;
    pbolt.source         = you.pos();
    pbolt.can_see_invis  = you.can_see_invisible();
    pbolt.nightvision    = you.nightvision();
    pbolt.smart_monster  = true;
    pbolt.attitude       = ATT_FRIENDLY;
    pbolt.thrower        = KILL_YOU_MISSILE;

    // Init tracer variables.
    pbolt.friend_info.reset();
    pbolt.foe_info.reset();

    pbolt.foe_ratio        = 100;
    pbolt.beam_cancelled   = false;
    pbolt.dont_stop_player = false;

    // Clear misc
    pbolt.seen          = false;
    pbolt.heard         = false;
    pbolt.reflections   = 0;
    pbolt.bounces       = 0;

    // Save range before overriding it
    const int old_range = pbolt.range;
    if (range)
        pbolt.range = range;

    pbolt.fire();

    if (range)
        pbolt.range = old_range;

    // Should only happen if the player answered 'n' to one of those
    // "Fire through friendly?" prompts.
    if (pbolt.beam_cancelled)
    {
        dprf(DIAG_BEAM, "Beam cancelled.");
        canned_msg(MSG_OK);
        you.turn_is_over = false;
        return false;
    }

    // Set to non-tracing for actual firing.
    pbolt.is_tracer = false;
    return true;
}

template<typename T>
class power_deducer
{
public:
    virtual T operator()(int pow) const = 0;
    virtual ~power_deducer() {}
};

typedef power_deducer<int> tohit_deducer;

template<int adder, int mult_num = 0, int mult_denom = 1>
class tohit_calculator : public tohit_deducer
{
public:
    int operator()(int pow) const
    {
        return adder + (pow * mult_num) / mult_denom;
    }
};

typedef power_deducer<dice_def> dam_deducer;

template<int numdice, int adder, int mult_num, int mult_denom>
class dicedef_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow) const
    {
        return dice_def(numdice, adder + (pow * mult_num) / mult_denom);
    }
};

template<int numdice, int adder, int mult_num, int mult_denom>
class calcdice_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow) const
    {
        return calc_dice(numdice, adder + (pow * mult_num) / mult_denom);
    }
};

struct zap_info
{
    zap_type ztype;
    const char* name;           // NULL means handled specially
    int power_cap;
    dam_deducer* damage;
    tohit_deducer* tohit;       // Enchantments have power modifier here
    int colour;
    bool is_enchantment;
    beam_type flavour;
    dungeon_char_type glyph;
    bool always_obvious;
    bool can_beam;
    bool is_explosion;
    int hit_loudness;
};

static const zap_info zap_data[] = {
#include "zap-data.h"
};

static int zap_index[NUM_ZAPS];

void init_zap_index()
{
    for (int i = 0; i < NUM_ZAPS; ++i)
        zap_index[i] = -1;

    for (unsigned int i = 0; i < ARRAYSZ(zap_data); ++i)
        zap_index[zap_data[i].ztype] = i;
}

static const zap_info* _seek_zap(zap_type z_type)
{
    ASSERT_RANGE(z_type, 0, NUM_ZAPS);
    if (zap_index[z_type] == -1)
        return NULL;
    else
        return &zap_data[zap_index[z_type]];
}

int zap_power_cap(zap_type z_type)
{
    const zap_info* zinfo = _seek_zap(z_type);

    return (zinfo ? zinfo->power_cap : 0);
}

void zappy(zap_type z_type, int power, bolt &pbolt)
{
    const zap_info* zinfo = _seek_zap(z_type);

    // None found?
    if (zinfo == NULL)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_ERROR, "Couldn't find zap type %d", z_type);
#endif
        return;
    }

    // Fill
    pbolt.name           = zinfo->name;
    pbolt.flavour        = zinfo->flavour;
    pbolt.real_flavour   = zinfo->flavour;
    pbolt.colour         = zinfo->colour;
    pbolt.glyph          = dchar_glyph(zinfo->glyph);
    pbolt.obvious_effect = zinfo->always_obvious;
    pbolt.is_beam        = zinfo->can_beam;
    pbolt.is_explosion   = zinfo->is_explosion;

    if (zinfo->power_cap > 0)
        power = min(zinfo->power_cap, power);

    ASSERT(zinfo->is_enchantment == pbolt.is_enchantment());

    if (zinfo->is_enchantment)
    {
        pbolt.ench_power = (zinfo->tohit ? (*zinfo->tohit)(power) : power);
        pbolt.hit = AUTOMATIC_HIT;
    }
    else
    {
        pbolt.hit = (*zinfo->tohit)(power);
        if (you.inaccuracy() && pbolt.hit != AUTOMATIC_HIT)
            pbolt.hit = max(0, pbolt.hit - 5);
    }

    if (zinfo->damage)
        pbolt.damage = (*zinfo->damage)(power);

    // One special case
    if (z_type == ZAP_ICE_STORM)
    {
        pbolt.ench_power = power; // used for radius
        pbolt.ex_size = power > 76 ? 3 : 2; // for tracer, overwritten later
    }
    else if (z_type == ZAP_BREATHE_FROST)
        pbolt.ac_rule = AC_NONE;
    if (pbolt.loudness == 0)
        pbolt.loudness = zinfo->hit_loudness;
}

bool bolt::can_affect_actor(const actor *act) const
{
    map<mid_t, int>::const_iterator cnt = hit_count.find(act->mid);
    if (cnt != hit_count.end() && cnt->second >= 2)
    {
        // Note: this is done for balance, even if it hurts realism a bit.
        // It is arcane knowledge which wall patterns will cause lightning
        // to bounce thrice, double damage for ordinary bounces is enough.
#ifdef DEBUG_DIAGNOSTICS
        if (!quiet_debug)
            dprf(DIAG_BEAM, "skipping beam hit, affected them twice already");
#endif
        return false;
    }
    // If there's a function that checks whether an actor is affected,
    // bypass any generic beam-affects-X logic:
    if (affect_func)
        return (*affect_func)(*this, act);

    return !act->submerged();
}

bool bolt::actor_wall_shielded(const actor *act) const
{
    return act->is_player()? false :
           mons_wall_shielded(act->as_monster());
}

// Affect actor in wall unless it can shield itself using the wall.
// The wall will always shield the actor if the beam bounces off the
// wall, and a monster can't use a metal wall to shield itself from
// electricity.
bool bolt::can_affect_wall_actor(const actor *act) const
{
    if (!can_affect_actor(act))
        return false;

    if (is_enchantment())
        return true;

    const bool superconductor = (feat_is_metal(grd(act->pos()))
                                 && flavour == BEAM_ELECTRICITY);
    if (actor_wall_shielded(act) && !superconductor)
        return false;

    if (!is_explosion && !is_big_cloud)
        return true;

    if (is_bouncy(grd(act->pos())))
        return false;

    return false;
}

static beam_type _chaos_beam_flavour()
{
    const beam_type flavour = random_choose_weighted(
            10, BEAM_FIRE,
            10, BEAM_COLD,
            10, BEAM_ELECTRICITY,
            10, BEAM_POISON,
            10, BEAM_NEG,
            10, BEAM_ACID,
            10, BEAM_HELLFIRE,
            10, BEAM_NAPALM,
            10, BEAM_SLOW,
            10, BEAM_HASTE,
            10, BEAM_MIGHT,
            10, BEAM_BERSERK,
            10, BEAM_HEALING,
            10, BEAM_PARALYSIS,
            10, BEAM_CONFUSION,
            10, BEAM_INVISIBILITY,
            10, BEAM_POLYMORPH,
            10, BEAM_BANISH,
            10, BEAM_DISINTEGRATION,
            10, BEAM_PETRIFY,
             2, BEAM_ENSNARE,
            0);

    return flavour;
}

static void _munge_bounced_bolt(bolt &old_bolt, bolt &new_bolt,
                                ray_def &old_ray, ray_def &new_ray)
{
    if (new_bolt.real_flavour != BEAM_CHAOS)
        return;

    double old_deg = old_ray.get_degrees();
    double new_deg = new_ray.get_degrees();
    double angle   = fabs(old_deg - new_deg);

    if (angle >= 180.0)
        angle -= 180.0;

    double max =  90.0 + (angle / 2.0);
    double min = -90.0 + (angle / 2.0);

    double shift;

    ray_def temp_ray = new_ray;
    for (int tries = 0; tries < 20; tries++)
    {
        shift = random_range(static_cast<int>(min * 10000),
                             static_cast<int>(max * 10000)) / 10000.0;

        if (new_deg < old_deg)
            shift = -shift;
        temp_ray.set_degrees(new_deg + shift);

        // Don't bounce straight into another wall.  Can happen if the beam
        // is shot into an inside corner.
        ray_def test_ray = temp_ray;
        test_ray.advance();
        if (in_bounds(test_ray.pos()) && !cell_is_solid(test_ray.pos()))
            break;

        shift    = 0.0;
        temp_ray = new_ray;
    }

    new_ray = temp_ray;
    dprf(DIAG_BEAM, "chaos beam: old_deg = %5.2f, new_deg = %5.2f, shift = %5.2f",
         static_cast<float>(old_deg), static_cast<float>(new_deg),
         static_cast<float>(shift));

    // Don't use up range in bouncing off walls, so that chaos beams have
    // as many chances as possible to bounce.  They're like demented
    // ping-pong balls on caffeine.
    int range_spent = new_bolt.range_used() - old_bolt.range_used();
    new_bolt.range += range_spent;
}

bool bolt::visible() const
{
    return (glyph != 0 && !is_enchantment());
}

void bolt::initialise_fire()
{
    // Fix some things which the tracer might have set.
    extra_range_used   = 0;
    in_explosion_phase = false;
    use_target_as_pos  = false;
    hit_count.clear();

    if (special_explosion != NULL)
    {
        ASSERT(!is_explosion);
        ASSERT(special_explosion->is_explosion);
        ASSERT(special_explosion->special_explosion == NULL);
        special_explosion->in_explosion_phase = false;
        special_explosion->use_target_as_pos  = false;
    }

    if (chose_ray)
    {
        ASSERT_IN_BOUNDS(ray.pos());

        if (source == coord_def())
            source = ray.pos();
    }

    if (target == source)
    {
        range             = 0;
        aimed_at_feet     = true;
        auto_hit          = true;
        aimed_at_spot     = true;
        use_target_as_pos = true;
    }

    ASSERT_IN_BOUNDS(source);
    ASSERT_RANGE(flavour, BEAM_NONE + 1, BEAM_FIRST_PSEUDO);
    ASSERT(!drop_item || item && item->defined());
    ASSERTM(range >= 0, "beam '%s', source '%s', item '%s'; has range -1",
            name.c_str(),
            ((beam_source == NON_MONSTER && source == you.pos()) ? "player"
             : (!invalid_monster_index(beam_source)
                ? menv[beam_source].name(DESC_PLAIN, true) : "unknown")).c_str(),
            (item ? item->name(DESC_PLAIN, false, true) : "none").c_str());
    ASSERT(!aimed_at_feet || source == target);

    real_flavour = flavour;

    message_cache.clear();

    // seen might be set by caller to suppress this.
    if (!seen && you.see_cell(source) && range > 0 && visible())
    {
        seen = true;
        const monster* mon = monster_at(source);

        if (flavour != BEAM_VISUAL
            && !is_tracer
            && !YOU_KILL(thrower)
            && !crawl_state.is_god_acting()
            && (!mon || !mon->observable()))
        {
            mprf("%s appears from out of thin air!",
                 article_a(name, false).c_str());
        }
    }

    // Visible self-targeted beams are always seen, even though they don't
    // leave a path.
    if (you.see_cell(source) && target == source && visible())
        seen = true;

    // Scale draw_delay to match change in arena_delay.
    if (crawl_state.game_is_arena() && !is_tracer)
    {
        draw_delay *= Options.arena_delay;
        draw_delay /= 600;
    }

#ifdef DEBUG_DIAGNOSTICS
    // Not a "real" tracer, merely a range/reachability check.
    if (quiet_debug)
        return;

    dprf(DIAG_BEAM, "%s%s%s [%s] (%d,%d) to (%d,%d): "
          "gl=%d col=%d flav=%d hit=%d dam=%dd%d range=%d",
          (is_beam) ? "beam" : "missile",
          (is_explosion) ? "*" :
          (is_big_cloud) ? "+" : "",
          (is_tracer) ? " tracer" : "",
          name.c_str(),
          source.x, source.y,
          target.x, target.y,
          glyph, colour, flavour,
          hit, damage.num, damage.size,
          range);
#endif
}

void bolt::apply_beam_conducts()
{
    if (!is_tracer && YOU_KILL(thrower))
    {
        switch (flavour)
        {
        case BEAM_HELLFIRE:
            did_god_conduct(DID_UNHOLY, 2 + random2(3), effect_known);
            break;
        default:
            break;
        }
    }
}

void bolt::choose_ray()
{
    if (!chose_ray || reflections > 0)
    {
        if (!find_ray(source, target, ray, opc_solid_see))
            fallback_ray(source, target, ray);
    }
}

// Draw the bolt at p if needed.
void bolt::draw(const coord_def& p)
{
    if (is_tracer || is_enchantment() || !you.see_cell(p))
        return;

    // We don't clean up the old position.
    // First, most people like to see the full path,
    // and second, it is hard to do it right with
    // respect to killed monsters, cloud trails, etc.

    const coord_def drawpos = grid2view(p);

    if (!in_los_bounds_v(drawpos))
        return;

#ifdef USE_TILE
    if (tile_beam == -1)
        tile_beam = tileidx_bolt(*this);

    if (tile_beam != -1)
    {
        int dist = (p - source).rdist();
        tiles.add_overlay(p, vary_bolt_tile(tile_beam, dist));
    }
#endif
#ifndef USE_TILE_LOCAL
    cgotoxy(drawpos.x, drawpos.y, GOTO_DNGN);
    put_colour_ch(colour == BLACK ? random_colour()
                                  : element_colour(colour),
                  glyph);

    // Get curses to update the screen so we can see the beam.
    update_screen();
#endif
    delay(draw_delay);
}

// Bounce a bolt off a solid feature.
// The ray is assumed to have just been advanced into
// the feature.
void bolt::bounce()
{
    // Don't bounce player tracers off unknown cells, or cells that we
    // incorrectly thought were non-bouncy.
    if (is_tracer && agent() == &you)
    {
        const dungeon_feature_type feat = env.map_knowledge(ray.pos()).feat();

        if (feat == DNGN_UNSEEN || !feat_is_solid(feat) || !is_bouncy(feat))
        {
            ray.regress();
            finish_beam();
            return;
        }
    }

    ray_def old_ray  = ray;
    bolt    old_bolt = *this;

    do
        ray.regress();
    while (feat_is_solid(grd(ray.pos())));

    extra_range_used += range_used(true);
    bounce_pos = ray.pos();
    bounces++;
    reflect_grid rg;
    for (adjacent_iterator ai(ray.pos(), false); ai; ++ai)
        rg(*ai - ray.pos()) = feat_is_solid(grd(*ai));
    ray.bounce(rg);
    extra_range_used += 2;

    ASSERT(!feat_is_solid(grd(ray.pos())));
    _munge_bounced_bolt(old_bolt, *this, old_ray, ray);
}

void bolt::fake_flavour()
{
    if (real_flavour == BEAM_RANDOM)
        flavour = static_cast<beam_type>(random_range(BEAM_FIRE, BEAM_ACID));
    else if (real_flavour == BEAM_CHAOS)
        flavour = _chaos_beam_flavour();
}

void bolt::digging_wall_effect()
{
    const dungeon_feature_type feat = grd(pos());
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_GRATE:
        nuke_wall(pos());
        if (!msg_generated)
        {
            if (!you.see_cell(pos()))
            {
                if (!silenced(you.pos()))
                {
                    mpr("You hear a grinding noise.", MSGCH_SOUND);
                    obvious_effect = true; // You may still see the caster.
                    msg_generated = true;
                }
                break;
            }

            obvious_effect = true;
            msg_generated = true;

            string wall;
            if (feat == DNGN_GRATE)
            {
                // XXX: should this change for monsters?
                mpr("The damaged grate falls apart.");
                return;
            }
            else if (feat == DNGN_SLIMY_WALL)
                wall = "slime";
            else if (player_in_branch(BRANCH_PANDEMONIUM))
                wall = "weird stuff";
            else
                wall = "rock";

            mprf("%s %s shatters into small pieces.",
                 agent() && agent()->is_player() ? "The" : "Some",
                 wall.c_str());
        }
        break;

    default:
        if (feat_is_wall(feat))
            finish_beam();
    }
}

void bolt::fire_wall_effect()
{
    dungeon_feature_type feat = grd(pos());
    // Fire only affects trees.
    if (!feat_is_tree(feat)
        || env.markers.property_at(pos(), MAT_ANY, "veto_fire") == "veto"
        || !is_superhot())
    {
        finish_beam();
        return;
    }

    // Destroy the wall.
    nuke_wall(pos());
    if (you.see_cell(pos()))
        emit_message(MSGCH_PLAIN, "The tree burns like a torch!");
    else if (you.can_smell())
        emit_message(MSGCH_PLAIN, "You smell burning wood.");
    if (whose_kill() == KC_YOU)
        did_god_conduct(DID_KILL_PLANT, 1, effect_known);
    else if (whose_kill() == KC_FRIENDLY && !crawl_state.game_is_arena())
        did_god_conduct(DID_PLANT_KILLED_BY_SERVANT, 1, effect_known);
    ASSERT(agent());
    place_cloud(CLOUD_FOREST_FIRE, pos(), random2(30)+25, agent());
    obvious_effect = true;

    finish_beam();
}

void bolt::elec_wall_effect()
{
    fire_wall_effect();
}

static bool _nuke_wall_msg(dungeon_feature_type feat, const coord_def& p)
{
    string msg;
    msg_channel_type chan = MSGCH_PLAIN;
    bool hear = player_can_hear(p);
    bool see = you.see_cell(p);

    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_GRANITE_STATUE:
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_SEALED_DOOR:
        // XXX: When silenced, features disappear without message.
        // XXX: For doors, we only issue a sound where the beam hit.
        //      If someone wants to improve on the door messaging,
        //      probably best to merge _nuke_wall_msg back into
        //      nuke_wall_effect. [rob]
        if (hear)
        {
            msg = "You hear a grinding noise.";
            chan = MSGCH_SOUND;
        }
        break;

    case DNGN_GRATE:
        if (hear)
        {
            if (see)
                msg = "The grate screeches as it bends and collapses.";
            else
                msg = "You hear the screech of bent metal.";
            chan = MSGCH_SOUND;
        }
        else if (see)
            msg = "The grate bends and collapses.";
        break;

    case DNGN_ORCISH_IDOL:
        if (hear)
        {
            if (see)
                msg = "You hear a hideous screaming!";
            else
                msg = "The idol screams as its substance crumbles away!";
            chan = MSGCH_SOUND;
        }
        else if (see)
            msg = "The idol twists and shakes as its substance crumbles away!";
        break;

    case DNGN_TREE:
    case DNGN_MANGROVE:
        if (see)
            msg = "The tree breaks and falls down!";
        else if (hear)
        {
            msg = "You hear timber falling.";
            chan = MSGCH_SOUND;
        }
        break;

    default:
        break;
    }

    if (!msg.empty())
    {
        mpr(msg, chan);
        return true;
    }
    else
        return false;
}

void bolt::nuke_wall_effect()
{
    if (env.markers.property_at(pos(), MAT_ANY, "veto_disintegrate") == "veto")
    {
        finish_beam();
        return;
    }

    const dungeon_feature_type feat = grd(pos());

    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_GRATE:
    case DNGN_GRANITE_STATUE:
    case DNGN_ORCISH_IDOL:
    case DNGN_TREE:
    case DNGN_MANGROVE:
        nuke_wall(pos());
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_SEALED_DOOR:
    {
        set<coord_def> doors = connected_doors(pos());
        set<coord_def>::iterator it;
        for (it = doors.begin(); it != doors.end(); ++it)
            nuke_wall(*it);
        break;
    }

    default:
        finish_beam();
        return;
    }

    obvious_effect = _nuke_wall_msg(feat, pos());

    if (feat == DNGN_ORCISH_IDOL)
    {
        if (beam_source == NON_MONSTER)
            did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);
    }
    else if (feat_is_tree(feat))
    {
        if (whose_kill() == KC_YOU)
            did_god_conduct(DID_KILL_PLANT, 1);
        else if (whose_kill() == KC_FRIENDLY && !crawl_state.game_is_arena())
            did_god_conduct(DID_PLANT_KILLED_BY_SERVANT, 1, effect_known, 0);
    }

    finish_beam();
}

int bolt::range_used(bool leg_only) const
{
    const int leg_length = pos().range(leg_source());
    return (leg_only ? leg_length : leg_length + extra_range_used);
}

void bolt::finish_beam()
{
    extra_range_used = BEAM_STOP;
}

void bolt::affect_wall()
{
    if (is_tracer)
    {
        if (!can_affect_wall(grd(pos())))
            finish_beam();
        return;
    }

    if (flavour == BEAM_DIGGING)
        digging_wall_effect();
    else if (is_fiery())
        fire_wall_effect();
    else if (flavour == BEAM_ELECTRICITY)
        elec_wall_effect();
    else if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_NUKE)
        nuke_wall_effect();

    if (cell_is_solid(pos()))
        finish_beam();
}

coord_def bolt::pos() const
{
    if (in_explosion_phase || use_target_as_pos)
        return target;
    else
        return ray.pos();
}

bool bolt::need_regress() const
{
    // XXX: The affects_wall check probably makes some of the
    //      others obsolete.
    return ((is_explosion && !in_explosion_phase)
            || drop_item
            || feat_is_solid(grd(pos())) && !can_affect_wall(grd(pos()))
            || origin_spell == SPELL_PRIMAL_WAVE);
}

// Returns true if the beam ended due to hitting the wall.
bool bolt::hit_wall()
{
    const dungeon_feature_type feat = grd(pos());

#ifdef ASSERTS
    if (!feat_is_solid(feat))
        die("beam::hit_wall yet not solid: %s", dungeon_feature_name(feat));
#endif

    if (is_tracer && !is_targetting && YOU_KILL(thrower)
        && in_bounds(target) && !passed_target && pos() != target
        && pos() != source && foe_info.count == 0
        && flavour != BEAM_DIGGING && flavour <= BEAM_LAST_REAL
        && bounces == 0 && reflections == 0 && you.see_cell(target)
        && !feat_is_solid(grd(target)))
    {
        // Okay, with all those tests passed, this is probably an instance
        // of the player manually targetting something whose line of fire
        // is blocked, even though its line of sight isn't blocked.  Give
        // a warning about this fact.
        string prompt = "Your line of fire to ";
        const monster* mon = monster_at(target);

        if (mon && mon->observable())
            prompt += mon->name(DESC_THE);
        else
        {
            prompt += "the targeted "
                    + feature_description_at(target, false, DESC_PLAIN, false);
        }

        prompt += " is blocked by "
                + feature_description_at(pos(), false, DESC_A, false);

        prompt += ". Continue anyway?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            beam_cancelled = true;
            finish_beam();
            return false;
        }

        // Well, we warned them.
    }

    if (in_bounds(pos()) && can_affect_wall(feat))
        affect_wall();
    else if (is_bouncy(feat) && !in_explosion_phase)
        bounce();
    else
    {
        // Regress for explosions: blow up in an open grid (if regressing
        // makes any sense).  Also regress when dropping items.
        if (pos() != source && need_regress())
        {
            do
                ray.regress();
            while (ray.pos() != source && cell_is_solid(ray.pos()));

            // target is where the explosion is centered, so update it.
            if (is_explosion && !is_tracer)
                target = ray.pos();
        }
        finish_beam();

        return true;
    }

    return false;
}

void bolt::affect_cell()
{
    // Shooting through clouds affects accuracy.
    if (env.cgrid(pos()) != EMPTY_CLOUD && hit != AUTOMATIC_HIT)
        hit = max(hit - 2, 0);

    fake_flavour();

    const coord_def old_pos = pos();
    const bool was_solid = feat_is_solid(grd(pos()));

    if (was_solid)
    {
        // Some special casing.
        if (actor *act = actor_at(pos()))
        {
            if (can_affect_wall_actor(act))
                affect_actor(act);
            else if (!is_tracer)
            {
                mprf("The %s protects %s from harm.",
                     raw_feature_description(act->pos()).c_str(),
                     act->name(DESC_THE).c_str());
            }
        }

        // Note that this can change the ray position and the solidity
        // of the wall.
        if (hit_wall())
            // Beam ended due to hitting wall, so don't hit the player
            // or monster with the regressed beam.
            return;
    }

    // If the player can ever walk through walls, this will need
    // special-casing too.
    bool hit_player = found_player();
    if (hit_player && can_affect_actor(&you))
    {
        affect_player();
        if (hit == AUTOMATIC_HIT && !is_beam)
            finish_beam();
    }

    // We don't want to hit a monster in a wall square twice. Also,
    // stop single target beams from affecting a monster if they already
    // affected the player on this square. -cao
    const bool still_wall = (was_solid && old_pos == pos());
    if ((!hit_player || is_beam || is_explosion) && !still_wall)
    {
        monster *m = monster_at(pos());
        if (m && can_affect_actor(m))
        {
            affect_monster(m);
            if ((hit == AUTOMATIC_HIT && !is_beam && !ignores_monster(m))
                && (!is_tracer || m->visible_to(agent())))
            {
                finish_beam();
            }
        }
    }

    if (!feat_is_solid(grd(pos())))
        affect_ground();
}

bool bolt::apply_hit_funcs(actor* victim, int dmg)
{
    bool affected = false;
    for (unsigned int i = 0; i < hit_funcs.size(); ++i)
        affected = (*hit_funcs[i])(*this, victim, dmg) || affected;

    return affected;
}

bool bolt::apply_dmg_funcs(actor* victim, int &dmg, vector<string> &messages)
{
    for (unsigned int i = 0; i < damage_funcs.size(); ++i)
    {
        string dmg_msg;

        if ((*damage_funcs[i])(*this, victim, dmg, dmg_msg))
            return false;
        if (!dmg_msg.empty())
            messages.push_back(dmg_msg);
    }
    return true;
}

static void _undo_tracer(bolt &orig, bolt &copy)
{
    // FIXME: we should have a better idea of what gets changed!
    orig.target           = copy.target;
    orig.source           = copy.source;
    orig.aimed_at_spot    = copy.aimed_at_spot;
    orig.extra_range_used = copy.extra_range_used;
    orig.auto_hit         = copy.auto_hit;
    orig.ray              = copy.ray;
    orig.colour           = copy.colour;
    orig.flavour          = copy.flavour;
    orig.real_flavour     = copy.real_flavour;
    orig.bounces          = copy.bounces;
    orig.bounce_pos       = copy.bounce_pos;
}

// This saves some important things before calling fire().
void bolt::fire()
{
    path_taken.clear();

    if (special_explosion)
        special_explosion->is_tracer = is_tracer;

    if (is_tracer)
    {
        bolt boltcopy = *this;
        if (special_explosion != NULL)
            boltcopy.special_explosion = new bolt(*special_explosion);

        do_fire();

        if (special_explosion != NULL)
        {
            _undo_tracer(*special_explosion, *boltcopy.special_explosion);
            delete boltcopy.special_explosion;
        }

        _undo_tracer(*this, boltcopy);
    }
    else
        do_fire();

    if (special_explosion != NULL)
    {
        seen           = seen  || special_explosion->seen;
        heard          = heard || special_explosion->heard;
    }
}

void bolt::do_fire()
{
    initialise_fire();

    if (range < extra_range_used && range > 0)
    {
#ifdef DEBUG
        dprf(DIAG_BEAM, "fire_beam() called on already done beam "
             "'%s' (item = '%s')", name.c_str(),
             item ? item->name(DESC_PLAIN).c_str() : "none");
#endif
        return;
    }

    apply_beam_conducts();
    cursor_control coff(false);

#ifdef USE_TILE
    tile_beam = -1;

    if (item && !is_tracer && flavour == BEAM_MISSILE)
    {
        const coord_def diff = target - source;
        tile_beam = tileidx_item_throw(get_item_info(*item), diff.x, diff.y);
    }
#endif

    msg_generated = false;
    if (!aimed_at_feet)
    {
        choose_ray();
        // Take *one* step, so as not to hurt the source.
        ray.advance();
    }

    while (map_bounds(pos()))
    {
        if (range_used() > range)
        {
            ray.regress();
            extra_range_used++;
            ASSERT(range_used() >= range);
            break;
        }

        if (!affects_nothing)
            affect_cell();

        if (path_taken.empty() || pos() != path_taken.back())
            path_taken.push_back(pos());

        if (range_used() > range)
            break;

        if (beam_cancelled)
            return;

        // Weapons of returning should find an inverse ray
        // through find_ray and setup_retrace, but they didn't
        // always in the past, and we don't want to crash
        // if they accidentally pass through a corner.
        ASSERT(!feat_is_solid(grd(pos()))
               || is_tracer && can_affect_wall(grd(pos()))
               || affects_nothing); // returning weapons

        const bool was_seen = seen;
        if (!was_seen && range > 0 && visible() && you.see_cell(pos()))
            seen = true;

        if (flavour != BEAM_VISUAL && !was_seen && seen && !is_tracer)
        {
            mprf("%s appears from out of your range of vision.",
                 article_a(name, false).c_str());
        }

        // Reset chaos beams so that it won't be considered an invisible
        // enchantment beam for the purposes of animation.
        if (real_flavour == BEAM_CHAOS)
            flavour = real_flavour;

        // Actually draw the beam/missile/whatever, if the player can see
        // the cell.
        if (animate)
            draw(pos());

        if (pos() == target)
        {
            passed_target = true;
            if (stop_at_target())
                break;
        }

        noise_generated = false;
        ray.advance();
    }

    if (!map_bounds(pos()))
    {
        ASSERT(!aimed_at_spot);

        int tries = max(GXM, GYM);
        while (!map_bounds(ray.pos()) && tries-- > 0)
            ray.regress();

        // Something bizarre happening if we can't get back onto the map.
        ASSERT(map_bounds(pos()));
    }

    // The beam has terminated.
    if (!affects_nothing)
        affect_endpoint();

    // Tracers need nothing further.
    if (is_tracer || affects_nothing)
        return;

    // Canned msg for enchantments that affected no-one, but only if the
    // enchantment is yours (and it wasn't a chaos beam, since with chaos
    // enchantments are entirely random, and if it randomly attempts
    // something which ends up having no obvious effect then the player
    // isn't going to realise it).
    if (!msg_generated && !obvious_effect && is_enchantment()
        && real_flavour != BEAM_CHAOS && YOU_KILL(thrower))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    // Reactions if a monster zapped the beam.
    if (!invalid_monster_index(beam_source))
    {
        if (foe_info.hurt == 0 && friend_info.hurt > 0)
            xom_is_stimulated(100);
        else if (foe_info.helped > 0 && friend_info.helped == 0)
            xom_is_stimulated(100);

        // Allow friendlies to react to projectiles, except when in
        // sanctuary when pet_target can only be explicitly changed by
        // the player.
        const monster* mon = &menv[beam_source];
        if (foe_info.hurt > 0 && !mon->wont_attack() && !crawl_state.game_is_arena()
            && you.pet_target == MHITNOT && env.sanctuary_time <= 0)
        {
            you.pet_target = beam_source;
        }
    }
}

// Returns damage taken by a monster from a "flavoured" (fire, ice, etc.)
// attack -- damage from clouds and branded weapons handled elsewhere.
int mons_adjust_flavoured(monster* mons, bolt &pbolt, int hurted,
                          bool doFlavouredEffects)
{
    // If we're not doing flavoured effects, must be preliminary
    // damage check only.
    // Do not print messages or apply any side effects!
    int resist = 0;
    int original = hurted;

    switch (pbolt.flavour)
    {
    case BEAM_FIRE:
    case BEAM_STEAM:
        hurted = resist_adjust_damage(
                    mons,
                    pbolt.flavour,
                    (pbolt.flavour == BEAM_FIRE) ? mons->res_fire()
                                                 : mons->res_steam(),
                    hurted, true);

        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (original > hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " resists.");
        }
        else if (original < hurted && doFlavouredEffects)
        {
            if (mons->is_icy())
                simple_monster_message(mons, " melts!");
            else if (mons_species(mons->type) == MONS_BUSH
                     && mons->res_fire() < 0)
            {
                simple_monster_message(mons, " is on fire!");
            }
            else if (pbolt.flavour == BEAM_FIRE)
                simple_monster_message(mons, " is burned terribly!");
            else
                simple_monster_message(mons, " is scalded terribly!");
        }
        break;

    case BEAM_WATER:
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_water_drowning(),
                                      hurted, true);
        if (doFlavouredEffects)
        {
            if (!hurted)
                simple_monster_message(mons, " shrugs off the wave.");
        }
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_cold(),
                                      hurted, true);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (original > hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " resists.");
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " is frozen!");
        }
        break;

    case BEAM_ELECTRICITY:
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_elec(),
                                      hurted, true);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        break;

    case BEAM_ACID:
    {
        const int res = mons->res_acid();
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      res, hurted, true);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (res <= 0 && doFlavouredEffects)
            corrode_monster(mons, pbolt.agent());
        break;
    }

    case BEAM_POISON:
    {
        int res = mons->res_poison();
        hurted  = resist_adjust_damage(mons, pbolt.flavour, res,
                                       hurted, true);
        if (!hurted && res > 0)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (res <= 0 && doFlavouredEffects && !one_chance_in(3))
            poison_monster(mons, pbolt.agent());

        break;
    }

    case BEAM_POISON_ARROW:
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_poison(),
                                      hurted);
        if (hurted < original)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons, " partially resists.");

                // Poison arrow can poison any living thing regardless of
                // poison resistance. - bwr
                if (mons->has_lifeforce())
                    poison_monster(mons, pbolt.agent(), 2, true);
            }
        }
        else if (doFlavouredEffects)
            poison_monster(mons, pbolt.agent(), 4);

        break;

    case BEAM_NEG:
        if (mons->res_negative_energy() == 3)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " completely resists.");

            hurted = 0;
        }
        else
        {
            hurted = resist_adjust_damage(mons, pbolt.flavour,
                                          mons->res_negative_energy(),
                                          hurted);

            // Early out if no side effects.
            if (!doFlavouredEffects)
                return hurted;

            if (original > hurted)
                simple_monster_message(mons, " resists.");

            if (mons->observable())
                pbolt.obvious_effect = true;

            mons->drain_exp(pbolt.agent());

            if (YOU_KILL(pbolt.thrower))
                did_god_conduct(DID_NECROMANCY, 2, pbolt.effect_known);
        }
        break;

    case BEAM_MIASMA:
        if (mons->res_rotting())
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " completely resists.");

            hurted = 0;
        }
        else
        {
            // Early out for tracer/no side effects.
            if (!doFlavouredEffects)
                return hurted;

            miasma_monster(mons, pbolt.agent());

            if (YOU_KILL(pbolt.thrower))
                did_god_conduct(DID_UNCLEAN, 2, pbolt.effect_known);
        }
        break;

    case BEAM_HOLY:
    {
        // Cleansing flame.
        const int rhe = mons->res_holy_energy(pbolt.agent());
        if (rhe > 0)
            hurted = 0;
        else if (rhe == 0)
            hurted /= 2;
        else if (rhe < -1)
            hurted = (hurted * 3) / 2;

        if (doFlavouredEffects)
        {
            simple_monster_message(mons,
                                   hurted == 0 ? " appears unharmed."
                                               : " writhes in agony!");
        }
        break;
    }

    case BEAM_BOLT_OF_ZIN:
    {
        int dam = 0;
        // Those naturally chaotic/unclean get hit fully, those who merely
        // dabble in things Zin hates get partial resistance.
        if (mons->is_chaotic() || mons->is_unclean(false))
            dam = 60;
        else if (mons->is_unclean(true))
            dam = 40;
        // a bit of damage to those you can recite against
        else if (mons->is_unholy() || mons->is_evil())
            dam = 20;
        else if (mons->has_ench(ENCH_WRETCHED))
            dam = 3 * mons->get_ench(ENCH_WRETCHED).degree;
        // if non-abstract monster mutations get added, let's handle them too

        hurted = hurted * dam / 60;
        if (doFlavouredEffects)
        {
            simple_monster_message(mons,
                                   hurted == 0 ? " appears unharmed." :
                                   dam > 30    ? " is terribly seared!"
                                               : " is seared!");
        }
        break;
    }

    case BEAM_ICE:
        // ice - about 50% of damage is cold, other 50% is impact and
        // can't be resisted (except by AC, of course)
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_cold(), hurted,
                                      true);
        if (hurted < original)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " partially resists.");
        }
        else if (hurted > original)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " is frozen!");
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(mons, pbolt.flavour,
                                      mons->res_fire(), hurted, true);

        if (hurted < original)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " partially resists.");
        }
        else if (hurted > original)
        {
            if (mons->is_icy())
            {
                if (doFlavouredEffects)
                    simple_monster_message(mons, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(mons, " is burned terribly!");
            }
        }
        break;

    case BEAM_HELLFIRE:
        resist = mons->res_hellfire();
        if (resist > 0)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }

            hurted = 0;
        }
        else if (resist < 0)
        {
            if (mons->is_icy())
            {
                if (doFlavouredEffects)
                    simple_monster_message(mons, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(mons, " is burned terribly!");
            }

            hurted *= 12;       // hellfire
            hurted /= 10;
        }
        break;

    case BEAM_LIGHT:
        if (mons->invisible())
            hurted = 0;
        else if (mons_genus(mons->type) == MONS_VAMPIRE)
            hurted += hurted / 2;
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                if (original > 0)
                    simple_monster_message(mons, " appears unharmed.");
                else if (mons->observable())
                    mprf("The beam of light passes harmlessly through %s.",
                         mons->name(DESC_THE, true).c_str());
            }
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " is burned terribly!");
        }
        break;

    case BEAM_SPORE:
        if (mons->type == MONS_BALLISTOMYCETE)
            hurted = 0;
        break;

    case BEAM_AIR:
        if (mons->res_wind() > 0)
            hurted = 0;
        else if (mons->flight_mode())
            hurted += hurted / 2;
        if (!hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " is harmlessly tossed around.");
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(mons, " gets badly buffeted.");
        }
        break;

    case BEAM_ENSNARE:
        ensnare(mons);
        break;

    case BEAM_GHOSTLY_FLAME:
        if (mons->holiness() == MH_UNDEAD)
            hurted = 0;
        else
        {
            const int res = mons->res_negative_energy();
            hurted = resist_adjust_damage(mons, pbolt.flavour, res, hurted, true);

            if (hurted < original)
            {
                if (doFlavouredEffects)
                    simple_monster_message(mons, " partially resists.");
            }
            if (!hurted)
            {
                if (doFlavouredEffects)
                {
                    simple_monster_message(mons,
                                        (original > 0) ? " completely resists."
                                                       : " appears unharmed.");
                }
            }
        }
        break;

    default:
        break;
    }

    return hurted;
}

static bool _monster_resists_mass_enchantment(monster* mons,
                                              enchant_type wh_enchant,
                                              int pow,
                                              bool* did_msg)
{
    // Assuming that the only mass charm is control undead.
    if (wh_enchant == ENCH_CHARM)
    {
        if (mons->friendly())
            return true;

        if (mons->holiness() != MH_UNDEAD)
            return true;

        int res_margin = mons->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (simple_monster_message(mons,
                    mons_resist_string(mons, res_margin)))
            {
                *did_msg = true;
            }
            return true;
        }
    }
    else if (wh_enchant == ENCH_CONFUSION
             && mons->check_clarity(false))
    {
        *did_msg = true;
        return true;
    }
    else if (wh_enchant == ENCH_CONFUSION
             || wh_enchant == ENCH_INSANE
             || mons->holiness() == MH_NATURAL)
    {
        if (wh_enchant == ENCH_CONFUSION
            && !mons_class_is_confusable(mons->type))
        {
            return true;
        }

        if (wh_enchant == ENCH_FEAR
            && mons->friendly())
        {
            return true;
        }

        if (wh_enchant == ENCH_INSANE
            && !mons->can_go_frenzy())
        {
            return true;
        }

        int res_margin = mons->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (simple_monster_message(mons,
                    mons_resist_string(mons, res_margin)))
            {
                *did_msg = true;
            }
            return true;
        }
    }
    // Mass enchantments around lots of plants/fungi shouldn't cause a flood
    // of "is unaffected" messages. --Eino
    else if (mons_is_firewood(mons))
        return true;
    else  // trying to enchant an unnatural creature doesn't work
    {
        if (simple_monster_message(mons, " is unaffected."))
            *did_msg = true;
        return true;
    }

    // If monster was affected, then there was a message.
    *did_msg = true;
    return false;
}

// Enchants all monsters in player's sight.
// If m_succumbed is non-NULL, will be set to the number of monsters that
// were enchanted. If m_attempted is non-NULL, will be set to the number of
// monsters that we tried to enchant.
spret_type mass_enchantment(enchant_type wh_enchant, int pow, bool fail)
{
    fail_check();
    bool did_msg = false;

    // Give mass enchantments a power multiplier.
    pow *= 3;
    pow /= 2;

    pow = min(pow, 200);

    for (monster_iterator mi; mi; ++mi)
    {
        if (!you.see_cell_no_trans(mi->pos()))
            continue;

        if (mi->has_ench(wh_enchant))
            continue;

        bool resisted = _monster_resists_mass_enchantment(*mi, wh_enchant,
                                                          pow, &did_msg);

        if (resisted)
            continue;

        if ((wh_enchant == ENCH_INSANE && mi->go_frenzy(&you))
            || (wh_enchant != ENCH_INSANE
                && mi->add_ench(mon_enchant(wh_enchant, 0, &you))))
        {
            // Do messaging.
            const char* msg;
            switch (wh_enchant)
            {
            case ENCH_FEAR:      msg = " looks frightened!";      break;
            case ENCH_CONFUSION: msg = " looks rather confused."; break;
            case ENCH_CHARM:     msg = " submits to your will.";  break;
            default:             msg = NULL;                      break;
            }
            if (msg && simple_monster_message(*mi, msg))
                did_msg = true;

            // Extra check for fear (monster needs to reevaluate behaviour).
            if (wh_enchant == ENCH_FEAR)
                behaviour_event(*mi, ME_SCARE, &you);
        }
    }

    if (!did_msg)
        canned_msg(MSG_NOTHING_HAPPENS);

    if (wh_enchant == ENCH_INSANE)
        did_god_conduct(DID_HASTY, 8, true);

    return SPRET_SUCCESS;
}

void bolt::apply_bolt_paralysis(monster* mons)
{
    if (mons->paralysed() || mons->check_stasis(false))
        return;
    // asleep monsters can still be paralysed (and will be always woken by
    // trying to resist); the message might seem wrong but paralysis is
    // always visible.
    if (!mons_is_immotile(mons)
        && simple_monster_message(mons, " suddenly stops moving!"))
    {
        mons->stop_constricting_all();
        obvious_effect = true;
    }

    mons->add_ench(ENCH_PARALYSIS);
    mons_check_pool(mons, mons->pos(), killer(), beam_source);
}

// Petrification works in two stages. First the monster is slowed down in
// all of its actions, and when that times out it remains properly petrified
// (no movement or actions).  The second part is similar to paralysis,
// except that insubstantial monsters can't be affected and damage is
// drastically reduced.
void bolt::apply_bolt_petrify(monster* mons)
{
    if (mons->petrified())
        return;

    if (mons->petrifying())
    {
        // If the petrifying is not yet finished, we can force it to happen
        // right away by casting again. Otherwise, the spell has no further
        // effect.
        mons->del_ench(ENCH_PETRIFYING, true, false);
        // del_ench() would do it, but let's call it ourselves for proper agent
        // blaming and messaging.
        if (mons->fully_petrify(agent()))
            obvious_effect = true;
    }
    else if (mons->add_ench(mon_enchant(ENCH_PETRIFYING, 0, agent())))
    {
        if (!mons_is_immotile(mons)
            && simple_monster_message(mons, " is moving more slowly."))
        {
            obvious_effect = true;
        }
    }
}

static bool _curare_hits_monster(actor *agent, monster* mons, int levels)
{
    if (!mons->alive())
        return false;

    if (mons->res_poison() > 0)
        return false;

    poison_monster(mons, agent, levels, false);

    int hurted = 0;

    if (!mons->res_asphyx())
    {
        hurted = roll_dice(2, 6);

        if (hurted)
        {
            mons->add_ench(mon_enchant(ENCH_BREATH_WEAPON, 0, agent, hurted));
            simple_monster_message(mons, " convulses.");
            mons->hurt(agent, hurted, BEAM_POISON);
        }
    }

    if (mons->alive())
        enchant_monster_with_flavour(mons, agent, BEAM_SLOW);

    // Deities take notice.
    if (agent->is_player())
        did_god_conduct(DID_POISON, 5 + random2(3));

    return (hurted > 0);
}

// Actually poisons a monster (with message).
bool poison_monster(monster* mons, const actor *who, int levels,
                    bool force, bool verbose)
{
    if (!mons->alive() || levels <= 0)
        return false;

    int res = mons->res_poison();
    if (res >= (force ? 3 : 1))
        return false;

    const mon_enchant old_pois = mons->get_ench(ENCH_POISON);
    mons->add_ench(mon_enchant(ENCH_POISON, levels, who));
    const mon_enchant new_pois = mons->get_ench(ENCH_POISON);

    // Actually do the poisoning.  The order is important here.
    if (new_pois.degree > old_pois.degree)
    {
        if (verbose)
        {
            simple_monster_message(mons,
                                   old_pois.degree > 0 ? " looks even sicker."
                                                       : " is poisoned.");
        }
    }

    // Finally, take care of deity preferences.
    if (who && who->is_player())
        did_god_conduct(DID_POISON, 5 + random2(3));

    return (new_pois.duration > old_pois.duration);
}

// Actually poisons, rots, and/or slows a monster with miasma (with
// message).
bool miasma_monster(monster* mons, const actor* who)
{
    if (!mons->alive())
        return false;

    if (mons->res_rotting())
        return false;

    bool success = poison_monster(mons, who);

    if (who && who->is_player()
        && is_good_god(you.religion)
        && !(success && you_worship(GOD_SHINING_ONE))) // already penalized
    {
        did_god_conduct(DID_NECROMANCY, 5 + random2(3));
    }

    if (mons->max_hit_points > 4 && coinflip())
    {
        mons->max_hit_points--;
        mons->hit_points = min(mons->max_hit_points, mons->hit_points);
        success = true;
    }

    if (one_chance_in(3))
    {
        bolt beam;
        beam.flavour = BEAM_SLOW;
        beam.apply_enchantment_to_monster(mons);
        success = true;
    }

    return success;
}

// Actually napalms a monster (with message).
bool napalm_monster(monster* mons, const actor *who, int levels, bool verbose)
{
    if (!mons->alive())
        return false;

    if (mons->res_sticky_flame() || levels <= 0)
        return false;

    const mon_enchant old_flame = mons->get_ench(ENCH_STICKY_FLAME);
    mons->add_ench(mon_enchant(ENCH_STICKY_FLAME, levels, who));
    const mon_enchant new_flame = mons->get_ench(ENCH_STICKY_FLAME);

    // Actually do the napalming.  The order is important here.
    if (new_flame.degree > old_flame.degree)
    {
        if (verbose)
            simple_monster_message(mons, " is covered in liquid flames!");
        ASSERT(who);
        behaviour_event(mons, ME_WHACK, who);
    }

    return (new_flame.degree > old_flame.degree);
}

//  Used by monsters in "planning" which spell to cast. Fires off a "tracer"
//  which tells the monster what it'll hit if it breathes/casts etc.
//
//  The output from this tracer function is written into the
//  tracer_info variables (friend_info and foe_info).
//
//  Note that beam properties must be set, as the tracer will take them
//  into account, as well as the monster's intelligence.
void fire_tracer(const monster* mons, bolt &pbolt, bool explode_only)
{
    // Don't fiddle with any input parameters other than tracer stuff!
    pbolt.is_tracer     = true;
    pbolt.source        = mons->pos();
    pbolt.beam_source   = mons->mindex();
    pbolt.can_see_invis = mons->can_see_invisible();
    pbolt.nightvision   = mons->nightvision();
    pbolt.smart_monster = (mons_intel(mons) >= I_NORMAL);
    pbolt.attitude      = mons_attitude(mons);

    // Init tracer variables.
    pbolt.foe_info.reset();
    pbolt.friend_info.reset();

    // Clear misc
    pbolt.reflections   = 0;
    pbolt.bounces       = 0;

    // If there's a specifically requested foe_ratio, honour it.
    if (!pbolt.foe_ratio)
    {
        pbolt.foe_ratio     = 80;        // default - see mons_should_fire()

        // Foe ratio for summoning greater demons & undead -- they may be
        // summoned, but they're hostile and would love nothing better
        // than to nuke the player and his minions.
        if (mons_att_wont_attack(pbolt.attitude)
            && !mons_att_wont_attack(mons->attitude))
        {
            pbolt.foe_ratio = 25;
        }
    }

    pbolt.in_explosion_phase = false;

    // Fire!
    if (explode_only)
        pbolt.explode(false);
    else
        pbolt.fire();

    // Unset tracer flag (convenience).
    pbolt.is_tracer = false;
}

static coord_def _random_point_hittable_from(const coord_def &c,
                                            int radius,
                                            int margin = 1,
                                            int tries = 5)
{
    while (tries-- > 0)
    {
        const coord_def point = dgn_random_point_from(c, radius, margin);
        if (point.origin())
            continue;
        if (!cell_see_cell(c, point, LOS_SOLID))
            continue;
        return point;
    }
    return coord_def();
}

static void _create_feat_splash(coord_def center,
                                int radius,
                                int nattempts)
{
    // Always affect center, if compatible
    if ((grd(center) == DNGN_FLOOR || grd(center) == DNGN_SHALLOW_WATER))
    {
        temp_change_terrain(center, DNGN_SHALLOW_WATER, 100 + random2(100),
                            TERRAIN_CHANGE_FLOOD);
    }

    for (int i = 0; i < nattempts; ++i)
    {
        const coord_def newp(_random_point_hittable_from(center, radius));
        if (newp.origin() || (grd(newp) != DNGN_FLOOR && grd(newp) != DNGN_SHALLOW_WATER))
            continue;
        temp_change_terrain(newp, DNGN_SHALLOW_WATER, 100 + random2(100),
                            TERRAIN_CHANGE_FLOOD);
    }
}

bool imb_can_splash(coord_def origin, coord_def center,
                    vector<coord_def> path_taken, coord_def target)
{
    // Don't go back along the path of the beam (the explosion doesn't
    // reverse direction). We do this to avoid hitting the caster and
    // also because we don't want aiming one
    // square past a lone monster to be optimal.
    if (origin == target)
        return false;
    for (unsigned int i = 0; i < path_taken.size(); i++)
        if (path_taken[i] == target)
            return false;

    // Don't go far away from the caster (not enough momentum).
    if (distance2(origin, center + (target - center)*2)
        > sqr(you.current_vision) + 1)
    {
        return false;
    }

    return true;
}

static void _imb_explosion(bolt *parent, coord_def center)
{
    const int dist = grid_distance(parent->source, center);
    if (dist == 0 || (!parent->is_tracer && !x_chance_in_y(3, 2 + 2 * dist)))
        return;
    bolt beam;
    beam.name           = "mystic blast";
    beam.aux_source     = "orb of energy";
    beam.beam_source    = parent->beam_source;
    beam.thrower        = parent->thrower;
    beam.attitude       = parent->attitude;
    beam.range          = 3;
    beam.hit            = AUTOMATIC_HIT;
    beam.damage         = parent->damage;
    beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour         = MAGENTA;
    beam.flavour        = BEAM_MMISSILE;
    beam.obvious_effect = true;
    beam.is_beam        = false;
    beam.is_explosion   = false;
    beam.passed_target  = true; // The centre was the target.
    beam.is_tracer      = parent->is_tracer;
    beam.is_targetting  = parent->is_targetting;
    beam.aimed_at_spot  = true;
    if (you.see_cell(center))
        beam.seen = true;
    beam.source_name    = parent->source_name;
    beam.source         = center;
#ifdef DEBUG_DIAGNOSTICS
    beam.quiet_debug    = parent->quiet_debug;
#endif
    bool first = true;
    for (adjacent_iterator ai(center); ai; ++ai)
    {
        if (!imb_can_splash(parent->source, center, parent->path_taken, *ai))
            continue;
        if (beam.is_tracer || x_chance_in_y(3, 4))
        {
            if (first && !beam.is_tracer)
            {
                if (you.see_cell(center))
                    mpr("The orb of energy explodes!");
                noisy(10, center);
                first = false;
            }
            beam.friend_info.reset();
            beam.foe_info.reset();
            beam.friend_info.dont_stop = parent->friend_info.dont_stop;
            beam.foe_info.dont_stop = parent->foe_info.dont_stop;
            beam.target = center + (*ai - center) * 2;
            beam.fire();
            parent->friend_info += beam.friend_info;
            parent->foe_info    += beam.foe_info;
            if (beam.is_tracer)
            {
                if (beam.beam_cancelled)
                {
                    parent->beam_cancelled = true;
                    return;
                }
            }
        }
    }
}

static void _malign_offering_effect(actor* victim, const actor* agent, int damage)
{
    if (!agent || damage < 1)
        return;

    // The position might be trashed if the damage kills the victim,
    // so obtain the LOS now.  The LOS is stored in the monster for
    // memory management reasons, but is not cleared by monster_cleanup,
    // so is still safe to use until invalidated by the next call to
    // get_los_no_trans on that monster.
    const los_base * const victim_los = victim->get_los_no_trans();

    mprf("%s life force is offered up.", victim->name(DESC_ITS).c_str());
    damage = victim->hurt(agent, damage, BEAM_NEG);

    // Actors that had LOS to the victim (blocked by glass, clouds, etc),
    // even if they couldn't actually see each another because of blindness
    // or invisibility.
    for (actor_iterator ai(victim_los); ai; ++ai)
    {
        if (mons_aligned(agent, *ai) && ai->holiness() != MH_NONLIVING)
        {
            if (ai->heal(max(1, damage * 2 / 3)) && you.can_see(*ai))
            {
                mprf("%s %s healed.", ai->name(DESC_THE).c_str(),
                                      ai->conj_verb("are").c_str());
            }
        }
    }
}

bool bolt::is_bouncy(dungeon_feature_type feat) const
{
    if (real_flavour == BEAM_CHAOS && feat_is_solid(feat))
        return true;

    if (is_enchantment())
        return false;

    if (flavour == BEAM_ELECTRICITY && !feat_is_metal(feat)
        && !feat_is_tree(feat))
    {
        return true;
    }

    if ((flavour == BEAM_FIRE || flavour == BEAM_COLD)
        && feat == DNGN_GREEN_CRYSTAL_WALL)
    {
        return true;
    }

    return false;
}

cloud_type bolt::get_cloud_type()
{
    if (name == "noxious blast")
        return CLOUD_MEPHITIC;

    if (name == "blast of poison")
        return CLOUD_POISON;

    if (origin_spell == SPELL_HOLY_BREATH)
        return CLOUD_HOLY_FLAMES;

    if (origin_spell == SPELL_FIRE_BREATH && is_big_cloud)
        return CLOUD_FIRE;

    if (origin_spell == SPELL_CHAOS_BREATH && is_big_cloud)
        return CLOUD_CHAOS;

    if (name == "foul vapour")
    {
        // death drake; swamp drakes handled earlier
        ASSERT(flavour == BEAM_MIASMA);
        return CLOUD_MIASMA;
    }

    if (name == "freezing blast")
        return CLOUD_COLD;

    if (origin_spell == SPELL_GHOSTLY_FLAMES)
        return CLOUD_GHOSTLY_FLAME;

    return CLOUD_NONE;
}

int bolt::get_cloud_pow()
{
    if (name == "freezing blast")
        return random_range(10, 15);

    if (origin_spell == SPELL_GHOSTLY_FLAMES)
        return random_range(12, 20);

    return 0;
}

int bolt::get_cloud_size(bool min, bool max)
{
    if (name == "foul vapour" || name == "freezing blast")
        return 10;

    if (min)
        return 8;
    if (max)
        return 12;

    return 8 + random2(5);
}

void bolt::affect_endpoint()
{
    if (special_explosion)
    {
        special_explosion->refine_for_explosion();
        special_explosion->target = pos();
        special_explosion->explode();

        // XXX: we're significantly overcounting here.
        foe_info      += special_explosion->foe_info;
        friend_info   += special_explosion->friend_info;
        beam_cancelled = beam_cancelled || special_explosion->beam_cancelled;
    }

    // Leave an object, if applicable.
    if (drop_item && item)
        drop_object();

    if (is_explosion)
    {
        refine_for_explosion();
        target = pos();
        explode();
        return;
    }

    cloud_type cloud = get_cloud_type();

    if (is_tracer)
    {
        if (name == "orb of energy")
            _imb_explosion(this, pos());
        if (cloud != CLOUD_NONE)
        {
            targetter_cloud tgt(agent(), range, get_cloud_size(true),
                                                get_cloud_size(false, true));
            tgt.set_aim(pos());
            for (map<coord_def, aff_type>::iterator it = tgt.seen.begin();
                 it != tgt.seen.end(); it++)
            {
                if (it->second != AFF_YES && it->second != AFF_MAYBE)
                    continue;

                if (it->first == you.pos())
                    tracer_affect_player();
                else if (monster* mon = monster_at(it->first))
                    tracer_affect_monster(mon);

                if (agent()->is_player() && beam_cancelled)
                    return;
            }
        }
        return;
    }

    if (!is_explosion && !noise_generated && loudness)
    {
        // Digging can target squares on the map boundary, though it
        // won't remove them of course.
        const coord_def noise_position = clamp_in_bounds(pos());
        noisy(loudness, noise_position, beam_source);
        noise_generated = true;
    }

    if (origin_spell == SPELL_PRIMAL_WAVE) // &&coinflip()
    {
        if (you.see_cell(pos()))
        {
            mprf("The wave splashes down.");
            noisy(25, pos());
        }
        else
            noisy(25, pos(), "You hear a splash.");
        _create_feat_splash(pos(), 2, random_range(3, 12, 2));
    }

    // FIXME: why don't these just have is_explosion set?
    // They don't explode in tracers: why not?
    if  (name == "orb of electricity"
        || name == "metal orb"
        || name == "great blast of cold")
    {
        target = pos();
        refine_for_explosion();
        explode();
    }

    if (cloud != CLOUD_NONE)
        big_cloud(cloud, agent(), pos(), get_cloud_pow(), get_cloud_size());

    if ((name == "fiery breath" && you.species == SP_RED_DRACONIAN)
        || name == "searing blast") // monster and player red draconian breath abilities
    {
        place_cloud(CLOUD_FIRE, pos(), 5 + random2(5), agent());
    }

    if (name == "orb of energy")
        _imb_explosion(this, pos());
}

bool bolt::stop_at_target() const
{
    return (is_explosion || is_big_cloud || aimed_at_spot);
}

void bolt::drop_object()
{
    ASSERT(item != NULL);
    ASSERT(item->defined());

    // Conditions: beam is missile and not tracer.
    if (is_tracer || !was_missile)
        return;

    // Summoned creatures' thrown items disappear.
    if (item->flags & ISFLAG_SUMMONED)
    {
        if (you.see_cell(pos()))
        {
            mprf("%s %s!",
                 item->name(DESC_THE).c_str(),
                 summoned_poof_msg(beam_source, *item).c_str());
        }
        item_was_destroyed(*item, beam_source);
        return;
    }

    if (!thrown_object_destroyed(item, pos()))
    {
        if (item->sub_type == MI_THROWING_NET)
        {
            monster* m = monster_at(pos());
            // Player or monster at position is caught in net.
            if (you.pos() == pos() && you.attribute[ATTR_HELD]
                || m && m->caught())
            {
                // If no trapping net found mark this one.
                if (get_trapping_net(pos(), true) == NON_ITEM)
                    set_item_stationary(*item);
            }
        }

        copy_item_to_grid(*item, pos(), 1);
    }
    else
        item_was_destroyed(*item, NON_MONSTER);
}

// Returns true if the beam hits the player, fuzzing the beam if necessary
// for monsters without see invis firing tracers at the player.
bool bolt::found_player() const
{
    const bool needs_fuzz = (is_tracer && !can_see_invis
                             && you.invisible() && !YOU_KILL(thrower));
    const int dist = needs_fuzz? 2 : 0;

    return (grid_distance(pos(), you.pos()) <= dist);
}

void bolt::affect_ground()
{
    // Explosions only have an effect during their explosion phase.
    // Special cases can be handled here.
    if (is_explosion && !in_explosion_phase)
        return;

    if (is_tracer)
        return;

    // Spore explosions might spawn a fungus.  The spore explosion
    // covers 21 tiles in open space, so the expected number of spores
    // produced is the x in x_chance_in_y() in the conditional below.
    if (is_explosion && flavour == BEAM_SPORE
        && agent() && !agent()->is_summoned())
    {
        if (env.grid(pos()) == DNGN_FLOOR)
            env.pgrid(pos()) |= FPROP_MOLD;

        if (x_chance_in_y(2, 21)
           && !crawl_state.game_is_zotdef() // Turn off in Zotdef
           && mons_class_can_pass(MONS_BALLISTOMYCETE, env.grid(pos()))
           && !actor_at(pos()))
        {
            beh_type beh = attitude_creation_behavior(attitude);

            if (crawl_state.game_is_arena())
                beh = coinflip() ? BEH_FRIENDLY : BEH_HOSTILE;

            const god_type god = agent() ? agent()->deity() : GOD_NO_GOD;

            if (create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                         beh,
                                         NULL,
                                         0,
                                         0,
                                         pos(),
                                         MHITNOT,
                                         MG_FORCE_PLACE,
                                         god)))
            {
                remove_mold(pos());
                if (you.see_cell(pos()))
                    mpr("A fungus suddenly grows.");

            }
        }
    }

    if (affects_items)
    {
        if (is_explosion)
            expose_items_to_element(flavour, pos(), 5);
        affect_place_clouds();
    }
}

bool bolt::is_fiery() const
{
    return (flavour == BEAM_FIRE
            || flavour == BEAM_HELLFIRE
            || flavour == BEAM_LAVA);
}

bool bolt::is_superhot() const
{
    if (!is_fiery() && flavour != BEAM_ELECTRICITY)
        return false;

    return (name == "bolt of fire"
            || name == "bolt of magma"
            || name == "fireball"
            || name == "bolt of lightning"
            || name.find("hellfire") != string::npos
               && in_explosion_phase);
}

maybe_bool bolt::affects_wall(dungeon_feature_type wall) const
{
    // digging
    if (flavour == BEAM_DIGGING
        && (wall == DNGN_ROCK_WALL || wall == DNGN_CLEAR_ROCK_WALL
            || wall == DNGN_SLIMY_WALL || wall == DNGN_GRATE))
    {
        return MB_TRUE;
    }

    if (is_fiery() && feat_is_tree(wall))
        return (is_superhot() ? MB_TRUE : is_beam ? MB_MAYBE : MB_FALSE);

    if (flavour == BEAM_ELECTRICITY && feat_is_tree(wall))
        return (is_superhot() ? MB_TRUE : MB_MAYBE);

    if (flavour == BEAM_DISINTEGRATION && damage.num >= 3
        || flavour == BEAM_NUKE)
    {
        if (feat_is_tree(wall))
            return MB_TRUE;

        if (wall == DNGN_ROCK_WALL
            || wall == DNGN_SLIMY_WALL
            || wall == DNGN_CLEAR_ROCK_WALL
            || wall == DNGN_GRATE
            || wall == DNGN_GRANITE_STATUE
            || wall == DNGN_ORCISH_IDOL
            || wall == DNGN_CLOSED_DOOR
            || wall == DNGN_RUNED_DOOR)
        {
            return MB_TRUE;
        }
    }

    // Lee's Rapid Deconstruction
    if (flavour == BEAM_FRAG)
        return MB_TRUE; // smite targetting, we don't care

    return MB_FALSE;
}

bool bolt::can_affect_wall(dungeon_feature_type feat) const
{
    maybe_bool ret = affects_wall(feat);

    return (ret == MB_TRUE)  ? true :
           (ret == MB_MAYBE) ? is_tracer || coinflip()
                            : false;
}

void bolt::affect_place_clouds()
{
    if (in_explosion_phase)
        affect_place_explosion_clouds();

    const coord_def p = pos();

    // Is there already a cloud here?
    const int cloudidx = env.cgrid(p);
    if (cloudidx != EMPTY_CLOUD)
    {
        cloud_type& ctype = env.cloud[cloudidx].type;

        // fire cancelling cold & vice versa
        if ((ctype == CLOUD_COLD
             && (flavour == BEAM_FIRE || flavour == BEAM_LAVA))
            || (ctype == CLOUD_FIRE && flavour == BEAM_COLD))
        {
            if (player_can_hear(p))
                mpr("You hear a sizzling sound!", MSGCH_SOUND);

            delete_cloud(cloudidx);
            extra_range_used += 5;
        }
        return;
    }

    // No clouds here, free to make new ones.
    const dungeon_feature_type feat = grd(p);

    if (name == "blast of poison")
        place_cloud(CLOUD_POISON, p, random2(4) + 2, agent());

    if (origin_spell == SPELL_HOLY_BREATH)
        place_cloud(CLOUD_HOLY_FLAMES, p, random2(4) + 2, agent());

    if (origin_spell == SPELL_FIRE_BREATH && is_big_cloud)
        place_cloud(CLOUD_FIRE, p,random2(4) + 2, agent());

    // Fire/cold over water/lava
    if (feat == DNGN_LAVA && flavour == BEAM_COLD
        || feat_is_watery(feat) && is_fiery())
    {
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), agent());
    }

    if (feat_is_watery(feat) && flavour == BEAM_COLD
        && damage.num * damage.size > 35)
    {
        place_cloud(CLOUD_COLD, p, damage.num * damage.size / 30 + 1, agent());
    }

    if (name == "great blast of cold")
        place_cloud(CLOUD_COLD, p, random2(5) + 3, agent());

    if (name == "ball of steam")
        place_cloud(CLOUD_STEAM, p, random2(5) + 2, agent());

    if (flavour == BEAM_MIASMA)
        place_cloud(CLOUD_MIASMA, p, random2(5) + 2, agent());

    if (name == "poison gas")
        place_cloud(CLOUD_POISON, p, random2(4) + 3, agent());

    if (name == "blast of choking fumes")
        place_cloud(CLOUD_MEPHITIC, p, random2(4) + 3, agent());

    if (name == "blast of calcifying dust")
        place_cloud(CLOUD_PETRIFY, p, random2(4) + 4, agent());

    if (name == "trail of fire")
        place_cloud(CLOUD_FIRE, p, random2(ench_power) + ench_power, agent());

    if (origin_spell == SPELL_GHOSTLY_FLAMES)
        place_cloud(CLOUD_GHOSTLY_FLAME, p, random2(6) + 5, agent());
}

void bolt::affect_place_explosion_clouds()
{
    const coord_def p = pos();

    // First check: fire/cold over water/lava.
    if (grd(p) == DNGN_LAVA && flavour == BEAM_COLD
        || feat_is_watery(grd(p)) && is_fiery())
    {
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), agent());
        return;
    }

    if (flavour == BEAM_MEPHITIC)
    {
        const coord_def center = (aimed_at_feet ? source : ray.pos());
        if (p == center || x_chance_in_y(125 + ench_power, 225))
        {
            place_cloud(CLOUD_MEPHITIC, p, roll_dice(2, 3 + ench_power / 20),
                        agent());
        }
    }

    // then check for more specific explosion cloud types.
    if (name == "ice storm")
        place_cloud(CLOUD_COLD, p, 2 + random2avg(5,2), agent());

    if (name == "great blast of fire")
    {
        place_cloud(CLOUD_FIRE, p, 2 + random2avg(5,2), agent());

        if (grd(p) == DNGN_FLOOR && !monster_at(p) && one_chance_in(4))
        {
            const god_type god =
                (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                              : GOD_NO_GOD;
            const beh_type att =
                (whose_kill() == KC_OTHER ? BEH_HOSTILE : BEH_FRIENDLY);

            actor* summ = agent();
            mgen_data mg(MONS_FIRE_VORTEX, att, summ, 1, SPELL_FIRE_STORM,
                         p, MHITNOT, 0, god);

            // Spell-summoned monsters need to have a live summoner.
            if (summ == NULL || !summ->alive())
            {
                if (!source_name.empty())
                    mg.non_actor_summoner = source_name;
                else if (god != GOD_NO_GOD)
                    mg.non_actor_summoner = god_name(god);
            }

            mons_place(mg);
        }
    }
}

// A little helper function to handle the calling of ouch()...
void bolt::internal_ouch(int dam)
{
    monster* monst = NULL;
    if (!invalid_monster_index(beam_source))
        monst = &menv[beam_source];

    const char *what = aux_source.empty() ? name.c_str() : aux_source.c_str();

    if (YOU_KILL(thrower) && you.duration[DUR_QUAD_DAMAGE])
        dam *= 4;

    // The order of this is important.
    if (monst && (monst->type == MONS_GIANT_SPORE
                  || monst->type == MONS_BALL_LIGHTNING
                  || monst->type == MONS_HYPERACTIVE_BALLISTOMYCETE))
    {
        ouch(dam, beam_source, KILLED_BY_SPORE, aux_source.c_str(), true,
             source_name.empty() ? NULL : source_name.c_str());
    }
    else if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_NUKE)
    {
        ouch(dam, beam_source, KILLED_BY_DISINT, what, true,
             source_name.empty() ? NULL : source_name.c_str());
    }
    else if (YOU_KILL(thrower) && aux_source.empty())
    {
        if (reflections > 0)
            ouch(dam, reflector, KILLED_BY_REFLECTION, name.c_str());
        else if (bounces > 0)
            ouch(dam, NON_MONSTER, KILLED_BY_BOUNCE, name.c_str());
        else
        {
            if (aimed_at_feet && effect_known)
                ouch(dam, NON_MONSTER, KILLED_BY_SELF_AIMED, name.c_str());
            else
                ouch(dam, NON_MONSTER, KILLED_BY_TARGETTING, name.c_str());
        }
    }
    else if (MON_KILL(thrower) || aux_source == "exploding inner flame")
        ouch(dam, beam_source, KILLED_BY_BEAM, aux_source.c_str(), true,
             source_name.empty() ? NULL : source_name.c_str());
    else // KILL_MISC || (YOU_KILL && aux_source)
        ouch(dam, beam_source, KILLED_BY_WILD_MAGIC, aux_source.c_str());
}

// [ds] Apply a fuzz if the monster lacks see invisible and is trying to target
// an invisible player. This makes invisibility slightly more powerful.
bool bolt::fuzz_invis_tracer()
{
    // Did the monster have a rough idea of where you are?
    int dist = grid_distance(target, you.pos());

    // No, ditch this.
    if (dist > 2)
        return false;

    const actor *beam_src = beam_source_as_target();
    if (beam_src && beam_src->is_monster()
        && mons_sense_invis(beam_src->as_monster()))
    {
        // Monsters that can sense invisible
        return (dist == 0);
    }

    // Apply fuzz now.
    coord_def fuzz(random_range(-2, 2), random_range(-2, 2));
    coord_def newtarget = target + fuzz;

    if (in_bounds(newtarget))
        target = newtarget;

    // Fire away!
    return true;
}

// A first step towards to-hit sanity for beams. We're still being
// very kind to the player, but it should be fairer to monsters than
// 4.0.
static bool _test_beam_hit(int attack, int defence, bool is_beam,
                           int defl, defer_rand &r)
{
    if (attack == AUTOMATIC_HIT)
        return true;

    if (is_beam)
    {
        if (defl > 1)
            attack = r[0].random2(attack * 2) / 3;
        else if (defl && attack >= 2) // don't increase acc of 0
            attack = r[0].random_range((attack + 1) / 2 + 1, attack);
    }
    else if (defl)
        attack = r[0].random2(attack / defl);

    dprf(DIAG_BEAM, "Beam attack: %d, defence: %d", attack, defence);

    attack = r[1].random2(attack);
    defence = r[2].random2avg(defence, 2);

    dprf(DIAG_BEAM, "Beam new attack: %d, defence: %d", attack, defence);

    return (attack >= defence);
}

string bolt::zapper() const
{
    const actor* beam_src = beam_source_as_target();
    if (!beam_src)
        return "";
    else if (beam_src->is_player())
        return "self";
    else
        return beam_src->name(DESC_PLAIN, true);
}

bool bolt::is_harmless(const monster* mon) const
{
    // For enchantments, this is already handled in nasty_to().
    if (is_enchantment())
        return !nasty_to(mon);

    // The others are handled here.
    switch (flavour)
    {
    case BEAM_VISUAL:
    case BEAM_DIGGING:
        return true;

    case BEAM_HOLY:
        return (mon->res_holy_energy(agent()) > 0);

    case BEAM_STEAM:
        return (mon->res_steam() >= 3);

    case BEAM_FIRE:
        return (mon->res_fire() >= 3);

    case BEAM_COLD:
        return (mon->res_cold() >= 3);

    case BEAM_MIASMA:
        return mon->res_rotting();

    case BEAM_NEG:
        return (mon->res_negative_energy() == 3);

    case BEAM_ELECTRICITY:
        return (mon->res_elec() >= 3);

    case BEAM_POISON:
        return (mon->res_poison() >= 3);

    case BEAM_ACID:
        return (mon->res_acid() >= 3);

    case BEAM_PETRIFY:
        return (mon->res_petrify() || mon->petrified());

    case BEAM_MEPHITIC:
        return mon->res_poison() > 0 || mon->is_unbreathing();

    case BEAM_GHOSTLY_FLAME:
        return mon->holiness() == MH_UNDEAD;

    default:
        return false;
    }
}

bool bolt::harmless_to_player() const
{
    dprf(DIAG_BEAM, "beam flavour: %d", flavour);

    switch (flavour)
    {
    case BEAM_VISUAL:
    case BEAM_DIGGING:
        return true;

    // Positive enchantments.
    case BEAM_HASTE:
    case BEAM_HEALING:
    case BEAM_MIGHT:
    case BEAM_INVISIBILITY:
        return true;

    case BEAM_HOLY:
        return is_good_god(you.religion);

    case BEAM_STEAM:
        return (player_res_steam(false) >= 3);

    case BEAM_MIASMA:
        return you.res_rotting();

    case BEAM_NEG:
        return (player_prot_life(false) >= 3);

    case BEAM_POISON:
        return (player_res_poison(false) >= 3
                || is_big_cloud && player_res_poison(false) > 0);

    case BEAM_MEPHITIC:
        return (player_res_poison(false) > 0 || you.clarity(false)
                || you.is_unbreathing());

    case BEAM_ELECTRICITY:
        return player_res_electricity(false);

    case BEAM_PETRIFY:
        return (you.res_petrify() || you.petrified());

    case BEAM_COLD:
    case BEAM_ACID:
        // Fire and ice can destroy inventory items, acid damage equipment.
        return false;

    case BEAM_FIRE:
    case BEAM_HELLFIRE:
    case BEAM_HOLY_FLAME:
    case BEAM_NAPALM:
        return you.species == SP_DJINNI;

    default:
        return false;
    }
}

bool bolt::is_reflectable(const item_def *it) const
{
    if (range_used() > range)
        return false;

    return (it && is_shield(*it) && shield_reflects(*it));
}

coord_def bolt::leg_source() const
{
    if (bounces > 0 && map_bounds(bounce_pos))
        return bounce_pos;
    else
        return source;
}

// Reflect a beam back the direction it came. This is used
// by shields of reflection.
void bolt::reflect()
{
    reflections++;

    target = leg_source();
    source = pos();

    // Reset bounce_pos, so that if we somehow reflect again before reaching
    // the wall that we won't keep heading towards the wall.
    bounce_pos.reset();

    if (pos() == you.pos())
        reflector = NON_MONSTER;
    else if (monster* m = monster_at(pos()))
        reflector = m->mindex();
    else
    {
        reflector = -1;
#ifdef DEBUG
        dprf(DIAG_BEAM, "Bolt reflected by neither player nor "
             "monster (bolt = %s, item = %s)", name.c_str(),
             item ? item->name(DESC_PLAIN).c_str() : "none");
#endif
    }

    flavour = real_flavour;
    choose_ray();
}

void bolt::tracer_affect_player()
{
    // Check whether thrower can see player, unless thrower == player.
    if (YOU_KILL(thrower))
    {
        // Don't ask if we're aiming at ourselves.
        if (!aimed_at_feet && !dont_stop_player && !harmless_to_player())
        {
            string prompt = make_stringf("That %s is likely to hit you. Continue anyway?",
                                         item ? name.c_str() : "beam");

            if (yesno(prompt.c_str(), false, 'n'))
            {
                friend_info.count++;
                friend_info.power += you.experience_level;
                dont_stop_player = true;
            }
            else
            {
                beam_cancelled = true;
                finish_beam();
            }
        }
    }
    else if (can_see_invis || !you.invisible() || fuzz_invis_tracer())
    {
        if (mons_att_wont_attack(attitude))
        {
            friend_info.count++;
            friend_info.power += you.experience_level;
        }
        else
        {
            foe_info.count++;
            foe_info.power += you.experience_level;
        }
    }

    vector<string> messages;
    int dummy = 0;

    apply_dmg_funcs(&you, dummy, messages);

    for (unsigned int i = 0; i < messages.size(); ++i)
        mpr(messages[i].c_str(), MSGCH_WARN);

    apply_hit_funcs(&you, 0);
    extra_range_used += range_used_on_hit();
}

bool bolt::misses_player()
{
    if (is_enchantment())
        return false;

    const bool engulfs = is_explosion || is_big_cloud;

    if (is_explosion || aimed_at_feet || auto_hit)
    {
        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";
        mprf("The %s %s you!", name.c_str(), hit_verb.c_str());
        return false;
    }

    const int dodge = player_evasion();
    const int dodge_less = player_evasion(EV_IGNORE_PHASESHIFT);
    int real_tohit  = hit;

    if (real_tohit != AUTOMATIC_HIT)
    {
        // Monsters shooting at an invisible player are very inaccurate.
        if (you.invisible() && !can_see_invis)
            real_tohit /= 2;

        // Backlit is easier to hit:
        if (you.backlit(true, false))
            real_tohit += 2 + random2(8);

        // Umbra is harder to hit:
        if (!nightvision && you.umbra(true, true))
            real_tohit -= 2 + random2(4);
    }

    bool train_shields_more = false;

    if (is_blockable()
        && (you.shield() || player_mutation_level(MUT_LARGE_BONE_PLATES) > 0)
        && !aimed_at_feet
        && player_shield_class() > 0)
    {
        // We use the original to-hit here.
        const int testhit = random2(hit * 130 / 100
                                    + you.shield_block_penalty());

        const int block = you.shield_bonus();

        dprf(DIAG_BEAM, "Beamshield: hit: %d, block %d", testhit, block);
        if (testhit < block)
        {
            if (is_reflectable(you.shield()))
            {
                mprf("Your %s reflects the %s!",
                      you.shield()->name(DESC_PLAIN).c_str(),
                      name.c_str());
                ident_reflector(you.shield());
                reflect();
            }
            else
            {
                mprf("You block the %s.", name.c_str());
                finish_beam();
            }
            you.shield_block_succeeded(agent());
            return true;
        }

        // Some training just for the "attempt".
        train_shields_more = true;
    }

    if (!aimed_at_feet)
        practise(EX_BEAM_MAY_HIT);

    defer_rand r;
    bool miss = true;

    int defl = you.missile_deflection();
    // rmsl/dmsl works on missiles and bolts, but not light -- otherwise
    // we'd need to deflect Olgreb's Toxic Radiance, halos, etc.
    if (flavour == BEAM_LIGHT)
        defl = 0;

    if (!_test_beam_hit(real_tohit, dodge_less, is_beam, 0, r))
        mprf("The %s misses you.", name.c_str());
    else if (defl && !_test_beam_hit(real_tohit, dodge_less, is_beam, defl, r))
    {
        // active voice to imply stronger effect
        mprf(defl == 1 ? "The %s is repelled." : "You deflect the %s!",
             name.c_str());
    }
    else if (!_test_beam_hit(real_tohit, dodge, is_beam, defl, r))
    {
        mprf("You momentarily phase out as the %s "
             "passes through you.", name.c_str());
    }
    else
    {
        int dodge_more = player_evasion(EV_IGNORE_HELPLESS);

        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";

        if (_test_beam_hit(real_tohit, dodge_more, is_beam, defl, r))
            mprf("The %s %s you!", name.c_str(), hit_verb.c_str());
        else
            mprf("Helpless, you fail to dodge the %s.", name.c_str());

        miss = false;
    }

    if (train_shields_more)
        practise(EX_SHIELD_BEAM_FAIL);

    return miss;
}

void bolt::affect_player_enchantment()
{
    if (flavour != BEAM_MALMUTATE && has_saving_throw()
        && you.check_res_magic(ench_power) > 0)
    {
        // You resisted it.

        // Give a message.
        bool need_msg = true;
        if (thrower != KILL_YOU_MISSILE && !invalid_monster_index(beam_source))
        {
            monster* mon = &menv[beam_source];
            if (!mon->observable())
            {
                mpr("Something tries to affect you, but you resist.");
                need_msg = false;
            }
        }
        if (need_msg)
            canned_msg(MSG_YOU_RESIST);

        // You *could* have gotten a free teleportation in the Abyss,
        // but no, you resisted.
        if (flavour == BEAM_TELEPORT && player_in_branch(BRANCH_ABYSS))
            xom_is_stimulated(200);

        extra_range_used += range_used_on_hit();
        return;
    }

    // You didn't resist it.
    if (animate)
        _ench_animation(effect_known ? real_flavour : BEAM_MAGIC);

    bool nasty = true, nice = false;

    const bool blame_player = effect_known && YOU_KILL(thrower);

    switch (flavour)
    {
    case BEAM_HIBERNATION:
        you.hibernate(ench_power);
        break;

    case BEAM_SLEEP:
        you.put_to_sleep(NULL, ench_power);
        break;

    case BEAM_CORONA:
        you.backlight();
        obvious_effect = true;
        break;

    case BEAM_POLYMORPH:
        obvious_effect = you.polymorph(ench_power);
        break;

    case BEAM_MALMUTATE:
        mpr("Strange energies course through your body.");
        you.malmutate(aux_source.empty() ? get_source_name() :
                      (get_source_name() + "/" + aux_source));
        obvious_effect = true;
        break;

    case BEAM_SLOW:
        potion_effect(POT_SLOWING, ench_power, false, blame_player);
        obvious_effect = true;
        break;

    case BEAM_HASTE:
        potion_effect(POT_SPEED, ench_power, false, blame_player);
        contaminate_player(1, effect_known);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_HEALING:
        potion_effect(POT_HEAL_WOUNDS, ench_power, false, blame_player);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_MIGHT:
        potion_effect(POT_MIGHT, ench_power, false, blame_player);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_INVISIBILITY:
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 1;
        potion_effect(POT_INVISIBILITY, ench_power, false, blame_player);
        contaminate_player(1 + random2(2), effect_known);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_PARALYSIS:
        you.paralyse(agent(), 2 + random2(6));
        obvious_effect = true;
        break;

    case BEAM_PETRIFY:
        you.petrify(agent());
        obvious_effect = true;
        break;

    case BEAM_CONFUSION:
        potion_effect(POT_CONFUSION, ench_power, false, blame_player);
        obvious_effect = true;
        break;

    case BEAM_TELEPORT:
        you_teleport();

        // An enemy helping you escape while in the Abyss, or an
        // enemy stabilizing a teleport that was about to happen.
        if (!mons_att_wont_attack(attitude) && player_in_branch(BRANCH_ABYSS))
            xom_is_stimulated(200);

        obvious_effect = true;
        break;

    case BEAM_BLINK:
        random_blink(false);
        obvious_effect = true;
        break;

    case BEAM_BLINK_CLOSE:
        blink_other_close(&you, source);
        obvious_effect = true;
        break;

    case BEAM_ENSLAVE:
        potion_effect(POT_CONFUSION, ench_power, false, blame_player);
        obvious_effect = true;
        break;     // enslavement - confusion?

    case BEAM_BANISH:
        if (YOU_KILL(thrower))
        {
            mpr("This spell isn't strong enough to banish yourself.");
            break;
        }
        you.banish(agent(), zapper());
        obvious_effect = true;
        break;

    case BEAM_PAIN:
        if (player_res_torment())
        {
            canned_msg(MSG_YOU_UNAFFECTED);
            break;
        }

        if (aux_source.empty())
            aux_source = "by nerve-wracking pain";

        if (name.find("agony") != string::npos)
        {
            if (you.res_negative_energy()) // Agony has no effect with rN.
            {
                canned_msg(MSG_YOU_UNAFFECTED);
                break;
            }

            mpr("Your body is wracked with pain!");

            // On the player, Agony acts like single-target torment.
            internal_ouch(max(0, you.hp / 2 - 1));
        }
        else
        {
            mpr("Pain shoots through your body!");

            internal_ouch(damage.roll());
        }
        obvious_effect = true;
        break;

    case BEAM_DISPEL_UNDEAD:
        if (!you.is_undead)
        {
            canned_msg(MSG_YOU_UNAFFECTED);
            break;
        }

        mpr("You convulse!");

        if (aux_source.empty())
            aux_source = "by dispel undead";

        if (you.is_undead == US_SEMI_UNDEAD)
        {
            if (you.hunger_state == HS_ENGORGED)
                damage.size /= 2;
            else if (you.hunger_state > HS_SATIATED)
            {
                damage.size *= 2;
                damage.size /= 3;
            }
        }
        internal_ouch(damage.roll());
        obvious_effect = true;
        break;

    case BEAM_DISINTEGRATION:
        mpr("You are blasted!");

        if (aux_source.empty())
            aux_source = "disintegration bolt";

        {
            int amt = damage.roll();
            internal_ouch(amt);

            if (you.can_bleed())
                blood_spray(you.pos(), MONS_PLAYER, amt / 5);
        }

        obvious_effect = true;
        break;

    case BEAM_PORKALATOR:
        if (!transform(ench_power, TRAN_PIG, true))
        {
            mpr("You feel like a pig.");
            break;
        }
        obvious_effect = true;
        break;

    case BEAM_BERSERK:
        potion_effect(POT_BERSERK_RAGE, ench_power, false, blame_player);
        obvious_effect = true;
        break;

    case BEAM_SENTINEL_MARK:
        you.sentinel_mark();
        obvious_effect = true;
        break;

    case BEAM_DIMENSION_ANCHOR:
        mprf("You feel %sfirmly anchored in space.",
             you.duration[DUR_DIMENSION_ANCHOR] ? "more " : "");
        you.increase_duration(DUR_DIMENSION_ANCHOR, 12 + random2(15), 50);
        if (you.duration[DUR_TELEPORT])
        {
            you.duration[DUR_TELEPORT] = 0;
            mpr("Your teleport is interrupted.");
        }
        you.duration[DUR_PHASE_SHIFT] = 0;
        you.redraw_evasion = true;
        obvious_effect = true;
        break;

    case BEAM_VULNERABILITY:
        if (!you.duration[DUR_LOWERED_MR])
            mpr("Your magical defenses are stripped away!");
        you.increase_duration(DUR_LOWERED_MR, 12 + random2(18), 50);
        obvious_effect = true;
        break;

    case BEAM_MALIGN_OFFERING:
    {
        int dam = resist_adjust_damage(&you, BEAM_NEG, you.res_negative_energy(),
                                       damage.roll());
        if (dam)
        {
            _malign_offering_effect(&you, agent(), dam);
            obvious_effect = true;
        }
        else
            canned_msg(MSG_YOU_UNAFFECTED);
        break;
    }

    default:
        // _All_ enchantments should be enumerated here!
        mpr("Software bugs nibble your toes!");
        break;
    }

    if (nasty)
    {
        if (mons_att_wont_attack(attitude))
        {
            friend_info.hurt++;
            if (beam_source == NON_MONSTER)
            {
                // Beam from player rebounded and hit player.
                if (!aimed_at_feet)
                    xom_is_stimulated(200);
            }
            else
            {
                // Beam from an ally or neutral.
                xom_is_stimulated(100);
            }
        }
        else
            foe_info.hurt++;
    }
    else if (nice)
    {
        if (mons_att_wont_attack(attitude))
            friend_info.helped++;
        else
        {
            foe_info.helped++;
            xom_is_stimulated(100);
        }
    }

    apply_hit_funcs(&you, 0);

    // Regardless of effect, we need to know if this is a stopper
    // or not - it seems all of the above are.
    extra_range_used += range_used_on_hit();
}

void bolt::affect_actor(actor *act)
{
    if (act->is_monster())
        affect_monster(act->as_monster());
    else
        affect_player();
}

void bolt::affect_player()
{
    hit_count[MID_PLAYER]++;

    // Explosions only have an effect during their explosion phase.
    // Special cases can be handled here.
    if (is_explosion && !in_explosion_phase)
    {
        // Trigger the explosion.
        finish_beam();
        return;
    }

    // Digging -- don't care.
    if (flavour == BEAM_DIGGING)
        return;

    if (is_tracer)
    {
        tracer_affect_player();
        return;
    }

    // Trigger an interrupt, so travel will stop on misses which
    // generate smoke.
    if (!YOU_KILL(thrower))
        interrupt_activity(AI_MONSTER_ATTACKS);

    const bool engulfs = is_explosion || is_big_cloud;

    if (is_enchantment())
    {
        if (real_flavour == BEAM_CHAOS || real_flavour == BEAM_RANDOM)
        {
            if (hit_verb.empty())
                hit_verb = engulfs ? "engulfs" : "hits";
            mprf("The %s %s you!", name.c_str(), hit_verb.c_str());
        }
        affect_player_enchantment();
        return;
    }

    msg_generated = true;

    if (misses_player())
        return;

    // FIXME: Lots of duplicated code here (compare handling of
    // monsters)
    int hurted = 0;
    int burn_power = (is_explosion) ? 5 : (is_beam) ? 3 : 2;

    // Roll the damage.
    hurted += damage.roll();

    int roll = hurted;

    vector<string> messages;
    apply_dmg_funcs(&you, hurted, messages);

    hurted = apply_AC(&you, hurted);

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_BEAM, "Player damage: rolled=%d; after AC=%d", roll, hurted);
#endif

    practise(EX_BEAM_WILL_HIT);

    bool was_affected = false;
    int  old_hp       = you.hp;

    hurted = max(0, hurted);

    // If the beam is an actual missile or of the MMISSILE type (Earth magic)
    // we might bleed on the floor.
    if (!engulfs
        && (flavour == BEAM_MISSILE || flavour == BEAM_MMISSILE))
    {
        // assumes DVORP_PIERCING, factor: 0.5
        int blood = min(you.hp, hurted / 2);
        bleed_onto_floor(you.pos(), MONS_PLAYER, blood, true);
    }

    hurted = check_your_resists(hurted, flavour, "", this);

    if (flavour == BEAM_MIASMA && hurted > 0)
        was_affected = miasma_player(get_source_name(), name);

    if (flavour == BEAM_NUKE) // DISINTEGRATION already handled
        blood_spray(you.pos(), MONS_PLAYER, hurted / 5);

    // Confusion effect for spore explosions
    if (flavour == BEAM_SPORE && hurted
        && you.holiness() != MH_UNDEAD
        && !you.is_unbreathing())
    {
        potion_effect(POT_CONFUSION, 1, false,
                      effect_known && YOU_KILL(thrower));
    }

    // handling of missiles
    if (item && item->base_type == OBJ_MISSILES)
    {
        // SPMSL_POISONED is handled via callback _poison_hit_victim()
        // in item_use.cc.
        if (item->sub_type == MI_THROWING_NET)
        {
            if (player_caught_in_net())
            {
                if (beam_source != NON_MONSTER)
                    xom_is_stimulated(50);
                was_affected = true;
            }
        }
        else if (item->special == SPMSL_CURARE)
        {
            if (x_chance_in_y(90 - 3 * you.armour_class(), 100))
            {
                curare_hits_player(actor_to_death_source(agent()),
                                   1 + random2(3), *this);
                was_affected = true;
            }
        }

        if (you.mutation[MUT_JELLY_MISSILE]
            && you.hp < you.hp_max
            && !you.duration[DUR_DEATHS_DOOR]
            && is_item_jelly_edible(*item)
            && coinflip())
        {
            mprf("Your attached jelly eats %s!", item->name(DESC_THE).c_str());
            inc_hp(random2(hurted / 2));
            mpr("You feel a little better.");
            drop_item = false;
        }
    }

    // Sticky flame.
    if (name == "sticky flame")
    {
        if (!player_res_sticky_flame())
        {
            napalm_player(random2avg(7, 3) + 1, get_source_name(), aux_source);
            was_affected = true;
        }
    }

    // Acid.
    if (flavour == BEAM_ACID)
        splash_with_acid(5, beam_source, affects_items);

    if (flavour == BEAM_ENSNARE)
        was_affected = ensnare(&you) || was_affected;

    // Last resort for characters mainly focusing on AC:
    // Chance of not affecting items if not much damage went through
    if (!x_chance_in_y(hurted, random2avg(roll, 2)))
        affects_items = false;

    if (affects_items)
    {
        // Simple cases for scroll burns.
        if (flavour == BEAM_LAVA || name.find("hellfire") != string::npos)
            expose_player_to_element(BEAM_LAVA, burn_power);

        // More complex (geez..)
        if (flavour == BEAM_FIRE && name != "ball of steam")
            expose_player_to_element(BEAM_FIRE, burn_power);

        // Potions exploding.
        if (flavour == BEAM_COLD)
            expose_player_to_element(BEAM_COLD, burn_power, true, false);

        // Spore pops.
        if (in_explosion_phase && flavour == BEAM_SPORE)
            expose_player_to_element(BEAM_SPORE, burn_power);
    }

    if (origin_spell == SPELL_QUICKSILVER_BOLT)
        antimagic();

    dprf(DIAG_BEAM, "Damage: %d", hurted);

    was_affected = apply_hit_funcs(&you, hurted) || was_affected;

    if (hurted > 0 || old_hp < you.hp || was_affected)
    {
        if (mons_att_wont_attack(attitude))
        {
            friend_info.hurt++;

            // Beam from player rebounded and hit player.
            // Xom's amusement at the player's being damaged is handled
            // elsewhere.
            if (beam_source == NON_MONSTER)
            {
                if (!aimed_at_feet)
                    xom_is_stimulated(200);
            }
            else if (was_affected)
                xom_is_stimulated(100);
        }
        else
            foe_info.hurt++;
    }

    if (hurted > 0)
    {
        for (unsigned int i = 0; i < messages.size(); ++i)
            mpr(messages[i].c_str(), MSGCH_WARN);
    }

    internal_ouch(hurted);

    extra_range_used += range_used_on_hit();

    if ((flavour == BEAM_WATER && origin_spell == SPELL_PRIMAL_WAVE)
         || (name == "chilling blast" && you.airborne())
         || (name == "lance of force" && hurted > 0)
         || (name == "flood of elemental water"))
    {
        beam_hits_actor(&you);
    }
}

int bolt::apply_AC(const actor *victim, int hurted)
{
    switch (flavour)
    {
    case BEAM_HELLFIRE:
        ac_rule = AC_NONE; break;
    case BEAM_ELECTRICITY:
    case BEAM_GHOSTLY_FLAME:
        ac_rule = AC_HALF; break;
    case BEAM_FRAG:
        ac_rule = AC_TRIPLE; break;
    default: ;
    }

    // beams don't obey GDR -> max_damage is 0
    return victim->apply_ac(hurted, 0, ac_rule);
}

const actor* bolt::beam_source_as_target() const
{
    // This looks totally wrong. Preserving old behaviour for now, but it
    // probably needs to be investigated and rewritten (like most of beam
    // blaming). -- 1KB
    if (MON_KILL(thrower) || thrower == KILL_MISCAST)
    {
        if (beam_source == MHITYOU)
            return &you;
        return invalid_monster_index(beam_source) ? 0 : &menv[beam_source];
    }
    return (thrower == KILL_MISC) ? 0 : &you;
}

void bolt::update_hurt_or_helped(monster* mon)
{
    if (!mons_atts_aligned(attitude, mons_attitude(mon)))
    {
        if (nasty_to(mon))
            foe_info.hurt++;
        else if (nice_to(mon))
        {
            foe_info.helped++;
            // Accidentally helped a foe.
            if (!is_tracer && !effect_known)
            {
                const int interest =
                    (flavour == BEAM_INVISIBILITY && can_see_invis) ? 25 : 100;
                xom_is_stimulated(interest);
            }
        }
    }
    else
    {
        if (nasty_to(mon))
        {
            friend_info.hurt++;

            // Harmful beam from this monster rebounded and hit the monster.
            if (!is_tracer && mon->mindex() == beam_source)
                xom_is_stimulated(100);
        }
        else if (nice_to(mon))
            friend_info.helped++;
    }
}

void bolt::tracer_enchantment_affect_monster(monster* mon)
{
    // Only count tracers as hitting creatures they could potentially affect
    if (_ench_flavour_affects_monster(flavour, mon, true))
    {
        // Update friend or foe encountered.
        if (!mons_atts_aligned(attitude, mons_attitude(mon)))
        {
            foe_info.count++;
            foe_info.power += mons_power(mon->type);
        }
        else
        {
            friend_info.count++;
            friend_info.power += mons_power(mon->type);
        }
    }

    handle_stop_attack_prompt(mon);
    if (!beam_cancelled)
    {
        apply_hit_funcs(mon, 0);
        extra_range_used += range_used_on_hit();
    }
}

// Return false if we should skip handling this monster.
bool bolt::determine_damage(monster* mon, int& preac, int& postac, int& final,
                            vector<string>& messages)
{
    preac = postac = final = 0;

    // [ds] Changed how tracers determined damage: the old tracer
    // model took the average damage potential, subtracted the average
    // AC damage reduction and called that the average damage output.
    // This could easily predict an average damage output of 0 for
    // high AC monsters, with the result that monsters often didn't
    // bother using ranged attacks at high AC targets.
    //
    // The new model rounds up average damage at every stage, so it
    // will predict a damage output of 1 even if the average damage
    // expected is much closer to 0. This will allow monsters to use
    // ranged attacks vs high AC targets.
      // [1KB] What ds' code actually does is taking the max damage minus
      // average AC.  This does work well, even using no AC would.  An
      // attack that _usually_ does no damage but can possibly do some means
      // we'll ultimately get it through.  And monsters with weak ranged
      // almost always would do no better in melee.
    //
    // This is not an entirely beneficial change; the old tracer
    // damage system would make monsters with weak ranged attacks
    // close in to their foes, while the new system will make it more
    // likely that such monsters will hang back and make ineffective
    // ranged attacks. Thus the new tracer damage calculation will
    // hurt monsters with low-damage ranged attacks and high-damage
    // melee attacks. I judge this an acceptable compromise (for now).
    //
    const int preac_max_damage = damage.num * damage.size;

    // preac: damage before AC modifier
    // postac: damage after AC modifier
    // final: damage after AC and resists
    // All these are invalid if we return false.

    if (is_tracer)
    {
        // Was mean between min and max;
        preac = preac_max_damage;
    }
    else
        preac = damage.roll();

    if (!apply_dmg_funcs(mon, preac, messages))
        return false;

    int tracer_postac_max = preac_max_damage;

    postac = apply_AC(mon, preac);

    if (is_tracer)
    {
        postac = div_round_up(tracer_postac_max, 2);

        const int adjusted_postac_max =
            mons_adjust_flavoured(mon, *this, tracer_postac_max, false);

        final = div_round_up(adjusted_postac_max, 2);
    }
    else
    {
        postac = max(0, postac);
        // Don't do side effects (beam might miss or be a tracer).
        final = mons_adjust_flavoured(mon, *this, postac, false);
    }

    // Sanity check. Importantly for
    // tracer_nonenchantment_affect_monster, final > 0
    // implies preac > 0.
    ASSERT(0 <= postac);
    ASSERT(postac <= preac);
    ASSERT(0 <= final);
    ASSERT(preac > 0 || final == 0);

    return true;
}

void bolt::handle_stop_attack_prompt(monster* mon)
{
    if ((thrower == KILL_YOU_MISSILE || thrower == KILL_YOU)
        && !is_harmless(mon))
    {
        if (!friend_info.dont_stop || !foe_info.dont_stop)
        {
            const bool autohit_first = (hit == AUTOMATIC_HIT);
            bool prompted = false;

            if (stop_attack_prompt(mon, true, target, autohit_first, &prompted))
            {
                beam_cancelled = true;
                finish_beam();
            }

            if (prompted)
            {
                friend_info.dont_stop = true;
                foe_info.dont_stop = true;
            }
        }
    }
}

void bolt::tracer_nonenchantment_affect_monster(monster* mon)
{
    vector<string> messages;
    int preac, post, final;

    if (!determine_damage(mon, preac, post, final, messages))
        return;

    // Check only if actual damage and the monster is worth caring about.
    if (final > 0 && !mons_is_firewood(mon))
    {
        ASSERT(preac > 0);

        // Monster could be hurt somewhat, but only apply the
        // monster's power based on how badly it is affected.
        // For example, if a fire giant (power 16) threw a
        // fireball at another fire giant, and it only took
        // 1/3 damage, then power of 5 would be applied.

        // Counting foes is only important for monster tracers.
        if (!mons_atts_aligned(attitude, mons_attitude(mon)))
        {
            foe_info.power += 2 * final * mons_power(mon->type) / preac;
            foe_info.count++;
        }
        else
        {
            friend_info.power += 2 * final * mons_power(mon->type) / preac;
            friend_info.count++;
        }
    }

    // Maybe the user wants to cancel at this point.
    handle_stop_attack_prompt(mon);
    if (beam_cancelled)
        return;

    // Check only if actual damage.
    if (!is_tracer && final > 0)
    {
        for (unsigned int i = 0; i < messages.size(); ++i)
            mpr(messages[i].c_str(), MSGCH_MONSTER_DAMAGE);
    }

    apply_hit_funcs(mon, final);

    // Either way, we could hit this monster, so update range used.
    extra_range_used += range_used_on_hit();
}

void bolt::tracer_affect_monster(monster* mon)
{
    // Ignore unseen monsters.
    if (!agent() || !mon->visible_to(agent()))
        return;

    // Trigger explosion on exploding beams.
    if (is_explosion && !in_explosion_phase)
    {
        finish_beam();
        return;
    }

    // Special explosions (current exploding missiles) aren't
    // auto-hit, so we need to explode them at every possible
    // end-point?
    if (special_explosion)
    {
        bolt orig = *special_explosion;
        affect_endpoint();
        *special_explosion = orig;
    }

    if (is_enchantment())
        tracer_enchantment_affect_monster(mon);
    else
        tracer_nonenchantment_affect_monster(mon);

    if (name == "orb of energy")
        _imb_explosion(this, pos());
}

void bolt::enchantment_affect_monster(monster* mon)
{
    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    bool hit_woke_orc = false;

    // Nasty enchantments will annoy the monster, and are considered
    // naughty (even if a monster might resist).
    if (nasty_to(mon))
    {
        if (YOU_KILL(thrower))
        {
            if (is_sanctuary(mon->pos()) || is_sanctuary(you.pos()))
                remove_sanctuary(true);

            set_attack_conducts(conducts, mon, you.can_see(mon));

            if (you_worship(GOD_BEOGH)
                && mons_genus(mon->type) == MONS_ORC
                && mon->asleep() && !player_under_penance()
                && you.piety >= piety_breakpoint(2) && mons_near(mon))
            {
                hit_woke_orc = true;
            }
        }
        behaviour_event(mon, ME_ANNOY, beam_source_as_target());
    }
    else
        behaviour_event(mon, ME_ALERT, beam_source_as_target());

    enable_attack_conducts(conducts);

    // Doing this here so that the player gets to see monsters
    // "flicker and vanish" when turning invisible....
    if (animate)
    {
        _ench_animation(effect_known ? real_flavour
                                     : BEAM_MAGIC,
                        mon, effect_known);
    }

    // Try to hit the monster with the enchantment.
    int res_margin = 0;
    const mon_resist_type ench_result = try_enchant_monster(mon, res_margin);

    if (mon->alive())           // Aftereffects.
    {
        // Message or record the success/failure.
        switch (ench_result)
        {
        case MON_RESIST:
            if (simple_monster_message(mon,
                                   resist_margin_phrase(res_margin)))
            {
                msg_generated = true;
            }
            break;
        case MON_UNAFFECTED:
            if (simple_monster_message(mon, " is unaffected."))
                msg_generated = true;
            break;
        case MON_AFFECTED:
        case MON_OTHER:         // Should this really be here?
            update_hurt_or_helped(mon);
            break;
        }

        if (hit_woke_orc)
            beogh_follower_convert(mon, true);
    }

    apply_hit_funcs(mon, 0);
    extra_range_used += range_used_on_hit();
}

static bool _dazzle_monster(monster* mons, actor* act)
{
    if (mons->holiness() == MH_UNDEAD || mons->holiness() == MH_NONLIVING
        || mons->holiness() == MH_PLANT)
        return false;

    if (x_chance_in_y(85 - mons->hit_dice * 3 , 100))
    {
        simple_monster_message(mons, " is dazzled.");
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, act, 40 + random2(40)));
        return true;
    }
    return false;
}

void bolt::monster_post_hit(monster* mon, int dmg)
{
    if (YOU_KILL(thrower) && mons_near(mon))
        print_wounds(mon);

    // Don't annoy friendlies or good neutrals if the player's beam
    // did no damage.  Hostiles will still take umbrage.
    if (dmg > 0 || !mon->wont_attack() || !YOU_KILL(thrower))
    {
        bool was_asleep = mon->asleep();
        special_missile_type m_brand = SPMSL_FORBID_BRAND;
        if (item && item->base_type == OBJ_MISSILES)
            m_brand = get_ammo_brand(*item);

        // Don't immediately turn insane monsters hostile.
        if (m_brand != SPMSL_FRENZY)
            behaviour_event(mon, ME_ANNOY, beam_source_as_target());

        // Don't allow needles of sleeping to awaken monsters.
        if (m_brand == SPMSL_SLEEP && was_asleep && !mon->asleep())
            mon->put_to_sleep(agent(), 0);
    }

    // Sticky flame.
    if (name == "sticky flame" || name == "splash of liquid fire")
    {
        const int levels = min(4, 1 + random2(mon->hit_dice) / 2);
        napalm_monster(mon, agent(), levels);

        if (name == "splash of liquid fire")
            for (adjacent_iterator ai(source); ai; ++ai)
            {
                // the breath weapon can splash to adjacent people
                if (grid_distance(*ai, target) != 1)
                    continue;
                if (actor *victim = actor_at(*ai))
                {
                    if (you.see_cell(*ai))
                    {
                        mprf("The sticky flame splashes onto %s!",
                             victim->name(DESC_THE).c_str());
                    }
                    if (victim->is_player())
                        napalm_player(levels, get_source_name(), aux_source);
                    else
                        napalm_monster(victim->as_monster(), agent(), levels);
                }
            }
    }

    // Handle missile effects.
    if (item && item->base_type == OBJ_MISSILES)
    {
        // SPMSL_POISONED handled via callback _poison_hit_victim() in
        // item_use.cc
        if (item->special == SPMSL_CURARE && ench_power == AUTOMATIC_HIT)
            _curare_hits_monster(agent(), mon, 2);
    }

    // purple draconian breath
    if (name == "bolt of dispelling energy" || origin_spell == SPELL_QUICKSILVER_BOLT)
        debuff_monster(mon);

    if (dmg)
        beogh_follower_convert(mon, true);

    if ((flavour == BEAM_WATER && origin_spell == SPELL_PRIMAL_WAVE) ||
          (name == "freezing breath" && mon->flight_mode()) ||
          (name == "lance of force" && dmg > 0) ||
          (name == "flood of elemental water"))
    {
        beam_hits_actor(mon);
    }

    if (name == "spray of energy")
        _dazzle_monster(mon, agent());

    if (flavour == BEAM_GHOSTLY_FLAME && mon->holiness() == MH_UNDEAD)
    {
        if (mon->heal(roll_dice(3, 10)))
            simple_monster_message(mon, " is bolstered by the flame.");
    }

}

void bolt::beam_hits_actor(actor *act)
{
    const coord_def oldpos(act->pos());

    const bool drac_breath = (name == "freezing breath" || name == "chilling blast");

    if (knockback_actor(act))
    {
        if (you.can_see(act))
        {
            if (drac_breath)
            {
                mprf("%s %s blown backwards by the freezing wind.",
                     act->name(DESC_THE).c_str(),
                     act->conj_verb("are").c_str());
                knockback_actor(act);
            }
            else
            {
                mprf("%s %s knocked back by the %s.",
                     act->name(DESC_THE).c_str(),
                     act->conj_verb("are").c_str(),
                     name.c_str());
            }
        }

        //Force lance can knockback up to two spaces
        if (name == "lance of force" && coinflip())
            knockback_actor(act);

        // Stun the monster briefly so that it doesn't look as though it wasn't
        // knocked back at all
        if (act->is_monster())
            act->as_monster()->speed_increment -= random2(6) + 4;

        act->apply_location_effects(oldpos, killer(), beam_source);
    }
}

// Return true if the block succeeded (including reflections.)
bool bolt::attempt_block(monster* mon)
{
    const int shield_block = mon->shield_bonus();
    bool rc = false;
    if (shield_block > 0)
    {
        const int ht = random2(hit * 130 / 100 + mon->shield_block_penalty());
        if (ht < shield_block)
        {
            rc = true;
            item_def *shield = mon->mslot_item(MSLOT_SHIELD);
            if (is_reflectable(shield))
            {
                if (mon->observable())
                {
                    mprf("%s reflects the %s off %s %s!",
                         mon->name(DESC_THE).c_str(),
                         name.c_str(),
                         mon->pronoun(PRONOUN_POSSESSIVE).c_str(),
                         shield->name(DESC_PLAIN).c_str());
                    ident_reflector(shield);
                }
                else if (you.see_cell(pos()))
                    mprf("The %s bounces off of thin air!", name.c_str());

                reflect();
            }
            else if (you.see_cell(pos()))
            {
                mprf("%s blocks the %s.",
                     mon->name(DESC_THE).c_str(), name.c_str());
                finish_beam();
            }

            mon->shield_block_succeeded(agent());
        }
    }

    return rc;
}

bool bolt::handle_statue_disintegration(monster* mon)
{
    bool rc = false;
    if ((flavour == BEAM_DISINTEGRATION || flavour == BEAM_NUKE)
        && mons_is_statue(mon->type, true))
    {
        rc = true;
        // Disintegrate the statue.
        if (!silenced(you.pos()))
        {
            if (!you.see_cell(mon->pos()))
                mpr("You hear a hideous screaming!", MSGCH_SOUND);
            else
            {
                mpr("The statue screams as its substance crumbles away!",
                    MSGCH_SOUND);
            }
        }
        else if (you.see_cell(mon->pos()))
        {
            mpr("The statue twists and shakes as its substance "
                "crumbles away!");
        }
        obvious_effect = true;
        update_hurt_or_helped(mon);
        mon->hurt(agent(), INSTANT_DEATH);
        apply_hit_funcs(mon, INSTANT_DEATH);
        // Stop here.
        finish_beam();
    }
    return rc;
}

void bolt::affect_monster(monster* mon)
{
    // Don't hit dead monsters.
    if (!mon->alive())
    {
        apply_hit_funcs(mon, 0);
        return;
    }

    hit_count[mon->mid]++;

    if (fedhas_shoot_through(*this, mon) && !is_tracer)
    {
        // FIXME: Could use a better message, something about
        // dodging that doesn't sound excessively weird would be
        // nice.
        if (you.see_cell(mon->pos()))
        {
            simple_god_message(
                make_stringf(" protects %s plant from harm.",
                    attitude == ATT_FRIENDLY ? "your" : "a").c_str(),
                GOD_FEDHAS);
        }
    }

    if (flavour == BEAM_WATER && mon->type == MONS_WATER_ELEMENTAL && !is_tracer)
    {
        if (you.see_cell(mon->pos()))
            mprf("The %s passes through %s.", name.c_str(), mon->name(DESC_THE).c_str());
    }

    if (ignores_monster(mon))
    {
        apply_hit_funcs(mon, 0);
        return;
    }

    // Handle tracers separately.
    if (is_tracer)
    {
        tracer_affect_monster(mon);
        return;
    }

    // Visual - wake monsters.
    if (flavour == BEAM_VISUAL)
    {
        behaviour_event(mon, ME_DISTURB, agent(), source);
        apply_hit_funcs(mon, 0);
        return;
    }

    // Special case: disintegrate (or Shatter) a statue.
    // Since disintegration is an enchantment, it has to be handled
    // here.
    if (handle_statue_disintegration(mon))
        return;

    // Explosions always 'hit'.
    const bool engulfs = (is_explosion || is_big_cloud);

    if (is_enchantment())
    {
        if (real_flavour == BEAM_CHAOS || real_flavour == BEAM_RANDOM)
        {
            if (hit_verb.empty())
                hit_verb = engulfs ? "engulfs" : "hits";
            if (mons_near(mon))
                mprf("The %s %s %s.", name.c_str(), hit_verb.c_str(),
                     mon->observable() ? mon->name(DESC_THE).c_str()
                                       : "something");
            else if (heard && !noise_msg.empty())
                mprf(MSGCH_SOUND, "%s", noise_msg.c_str());
        }
        // no to-hit check
        enchantment_affect_monster(mon);
        return;
    }

    if (is_explosion && !in_explosion_phase)
    {
        // It hit a monster, so the beam should terminate.
        // Don't actually affect the monster; the explosion
        // will take care of that.
        finish_beam();
        return;
    }

    // We need to know how much the monster _would_ be hurt by this,
    // before we decide if it actually hits.
    vector<string> messages;
    int preac, postac, final;
    if (!determine_damage(mon, preac, postac, final, messages))
        return;

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_BEAM, "Monster: %s; Damage: pre-AC: %d; post-AC: %d; post-resist: %d",
         mon->name(DESC_PLAIN).c_str(), preac, postac, final);
#endif

    // Player beams which hit friendlies or good neutrals will annoy
    // them and be considered naughty if they do damage (this is so as
    // not to penalise players that fling fireballs into a melee with
    // fire elementals on their side - the elementals won't give a sh*t,
    // after all).

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (nasty_to(mon))
    {
        if (YOU_KILL(thrower) && final > 0)
        {
            // It's not the player's fault if he didn't see the monster
            // or the monster was caught in an unexpected blast of
            // ?immolation.
            const bool okay =
                (!you.can_see(mon)
                    || aux_source == "scroll of immolation" && !effect_known);

            if (is_sanctuary(mon->pos()) || is_sanctuary(you.pos()))
                remove_sanctuary(true);

            set_attack_conducts(conducts, mon, !okay);
        }
    }

    if (engulfs && flavour == BEAM_SPORE
        && mon->holiness() == MH_NATURAL
        && !mon->is_unbreathing())
    {
        apply_enchantment_to_monster(mon);
    }

    // Make a copy of the to-hit before we modify it.
    int beam_hit = hit;

    if (beam_hit != AUTOMATIC_HIT)
    {
        if (mon->invisible() && !can_see_invis)
            beam_hit /= 2;

        // Backlit is easier to hit:
        if (mon->backlit(true, false))
            beam_hit += 2 + random2(8);

        // Umbra is harder to hit:
        if (!nightvision && mon->umbra(true, true))
            beam_hit -= 2 + random2(4);
    }

    // The monster may block the beam.
    if (!engulfs && is_blockable() && attempt_block(mon))
        return;

    defer_rand r;
    int rand_ev = random2(mon->ev);
    int defl = mon->missile_deflection();

    // FIXME: We're randomising mon->evasion, which is further
    // randomised inside test_beam_hit.  This is so we stay close to the
    // 4.0 to-hit system (which had very little love for monsters).
    if (!engulfs && !_test_beam_hit(beam_hit, rand_ev, is_beam, defl, r))
    {
        // If the PLAYER cannot see the monster, don't tell them anything!
        if (mon->observable())
        {
            // if it would have hit otherwise...
            if (_test_beam_hit(beam_hit, rand_ev, is_beam, 0, r))
            {
                string deflects = (defl == 2) ? "deflects" : "repels";
                msg::stream << mon->name(DESC_THE) << " "
                            << deflects << " the " << name
                            << '!' << endl;
            }
            else if (mons_class_flag(mon->type, M_PHASE_SHIFT)
                     && _test_beam_hit(beam_hit, rand_ev - random2(8),
                                       is_beam, 0, r))
            {
                msg::stream << mon->name(DESC_THE) << " momentarily phases "
                            << "out as the " << name << " passes through "
                            << mon->pronoun(PRONOUN_OBJECTIVE) << ".\n";
            }
            else
            {
                msg::stream << "The " << name << " misses "
                            << mon->name(DESC_THE) << '.' << endl;
            }
        }
        return;
    }

    update_hurt_or_helped(mon);
    enable_attack_conducts(conducts);

    // We'll say giant spore explosions don't trigger the ally attack conduct
    // for Fedhas worshipers.  Mostly because you can accidentally blow up a
    // group of 8 plants and get placed under penance until the end of time
    // otherwise.  I'd prefer to do this elsewhere but the beam information
    // goes out of scope.
    //
    // Also exempting miscast explosions from this conduct -cao
    if (you_worship(GOD_FEDHAS)
        && (flavour == BEAM_SPORE
            || beam_source == NON_MONSTER
               && aux_source.find("your miscasting") != string::npos))
    {
        conducts[0].enabled = false;
    }

    if (!is_explosion && !noise_generated)
    {
        heard = noisy(loudness, pos(), beam_source) || heard;
        noise_generated = true;
    }

    if (!mon->alive())
        return;

    // The beam hit.
    if (mons_near(mon))
    {
        // Monsters are never currently helpless in ranged combat.
        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";

        mprf("The %s %s %s.",
             name.c_str(),
             hit_verb.c_str(),
             mon->observable() ?
                 mon->name(DESC_THE).c_str() : "something");

    }
    else if (heard && !noise_msg.empty())
        mprf(MSGCH_SOUND, "%s", noise_msg.c_str());
    // The player might hear something, if _they_ fired a missile
    // (not magic beam).
    else if (!silenced(you.pos()) && flavour == BEAM_MISSILE
             && YOU_KILL(thrower))
    {
        mprf(MSGCH_SOUND, "The %s hits something.", name.c_str());
    }

    // handling of missiles
    if (item
        && item->base_type == OBJ_MISSILES
        && item->sub_type == MI_THROWING_NET)
    {
        monster_caught_in_net(mon, *this);
    }

    if (final > 0)
    {
        for (unsigned int i = 0; i < messages.size(); ++i)
            mpr(messages[i].c_str(), MSGCH_MONSTER_DAMAGE);
    }

    // Apply flavoured specials.
    mons_adjust_flavoured(mon, *this, postac, true);

    // mons_adjust_flavoured may kill the monster directly.
    if (mon->alive())
    {
        // If the beam is an actual missile or of the MMISSILE type
        // (Earth magic) we might bleed on the floor.
        if (!engulfs
            && (flavour == BEAM_MISSILE || flavour == BEAM_MMISSILE)
            && !mon->is_summoned())
        {
            // Using raw_damage instead of the flavoured one!
            // assumes DVORP_PIERCING, factor: 0.5
            const int blood = min(postac/2, mon->hit_points);
            bleed_onto_floor(mon->pos(), mon->type, blood, true);
        }
        // Now hurt monster.
        mon->hurt(agent(), final, flavour, false);
    }

    if (mon->alive())
    {
        apply_hit_funcs(mon, final);
        monster_post_hit(mon, final);
    }
    else
    {
        // Preserve name of the source monster if it winds up killing
        // itself.
        if (mon->mindex() == beam_source && source_name.empty())
            source_name = mon->name(DESC_A, true);

        // Prevent spore explosions killing plants from being registered
        // as a Fedhas misconduct.  Deaths can trigger the ally dying or
        // plant dying conducts, but spore explosions shouldn't count
        // for either of those.
        //
        // FIXME: Should be a better way of doing this.  For now, we are
        // just falsifying the death report... -cao
        if (you_worship(GOD_FEDHAS) && flavour == BEAM_SPORE
            && fedhas_protects(mon))
        {
            if (mon->attitude == ATT_FRIENDLY)
                mon->attitude = ATT_HOSTILE;
            monster_die(mon, KILL_MON, /*beam_source_as_target()*/beam_source);
        }
        else
        {
            killer_type ref_killer = thrower;
            if (!YOU_KILL(thrower) && reflector == NON_MONSTER)
                ref_killer = KILL_YOU_MISSILE;
            monster_die(mon, ref_killer, /*beam_source_as_target()*/beam_source);
        }
    }

    extra_range_used += range_used_on_hit();
}

bool bolt::ignores_monster(const monster* mon) const
{
    // Digging doesn't affect monsters (should it harm earth elementals?).
    if (flavour == BEAM_DIGGING)
        return true;

    // The targetters might call us with NULL in the event of a remembered
    // monster that is no longer there. Treat it as opaque.
    if (!mon)
        return false;

    // All kinds of beams go past orbs of destruction and friendly
    // battlespheres. We don't check mon->is_projectile() because that
    // check includes boulder beetles which should be hit.
    if (mons_is_projectile(mon)
        || (mon->type == MONS_BATTLESPHERE || mon->type == MONS_SPECTRAL_WEAPON)
            && mons_aligned(agent(), mon))
    {
        return true;
    }

    // Missiles go past bushes and briar patches, unless aimed directly at them
    if ((mons_species(mon->type) == MONS_BUSH || mon->type == MONS_BRIAR_PATCH)
        && !is_beam && !is_explosion
        && target != mon->pos()
        && name != "sticky flame"
        && name != "splash of liquid fire"
        && name != "lightning arc")
    {
        return true;
    }

    if (fedhas_shoot_through(*this, mon))
        return true;

    // Fire storm creates these, so we'll avoid affecting them.
    if (name == "great blast of fire" && mon->type == MONS_FIRE_VORTEX)
        return true;

    if (flavour == BEAM_WATER && mon->type == MONS_WATER_ELEMENTAL)
        return true;

    return false;
}

bool bolt::has_saving_throw() const
{
    if (aimed_at_feet)
        return false;

    switch (flavour)
    {
    case BEAM_HASTE:
    case BEAM_MIGHT:
    case BEAM_BERSERK:
    case BEAM_HEALING:
    case BEAM_INVISIBILITY:
    case BEAM_DISPEL_UNDEAD:
    case BEAM_ENSLAVE_SOUL:     // has a different saving throw
    case BEAM_BLINK_CLOSE:
    case BEAM_BLINK:
    case BEAM_MALIGN_OFFERING:
        return false;
    case BEAM_VULNERABILITY:
        return !one_chance_in(3);  // Ignores MR 1/3 of the time
    default:
        return true;
    }
}

static bool _ench_flavour_affects_monster(beam_type flavour, const monster* mon,
                                          bool intrinsic_only)
{
    bool rc = true;
    switch (flavour)
    {
    case BEAM_MALMUTATE:
        rc = mon->can_mutate();
        break;

    case BEAM_POLYMORPH:
        rc = mon->can_polymorph();
        break;

    case BEAM_ENSLAVE_SOUL:
        rc = (mon->holiness() == MH_NATURAL && mon->attitude != ATT_FRIENDLY);
        break;

    case BEAM_DISPEL_UNDEAD:
        rc = (mon->holiness() == MH_UNDEAD);
        break;

    case BEAM_PAIN:
        rc = !mon->res_negative_energy(intrinsic_only);
        break;

    case BEAM_HIBERNATION:
        rc = mon->can_hibernate(false, intrinsic_only);
        break;

    case BEAM_PORKALATOR:
        rc = (mon->holiness() == MH_DEMONIC && mon->type != MONS_HELL_HOG)
              || (mon->holiness() == MH_NATURAL && mon->type != MONS_HOG)
              || (mon->holiness() == MH_HOLY && mon->type != MONS_HOLY_SWINE);
        break;

    case BEAM_SENTINEL_MARK:
        rc = false;
        break;

    case BEAM_MALIGN_OFFERING:
        rc = (mon->res_negative_energy(intrinsic_only) < 3);
        break;

    default:
        break;
    }

    return rc;
}

bool enchant_monster_with_flavour(monster* mon, actor *foe,
                                  beam_type flavour, int powc)
{
    bolt dummy;
    dummy.flavour = flavour;
    dummy.ench_power = powc;
    dummy.set_agent(foe);
    dummy.apply_enchantment_to_monster(mon);
    return dummy.obvious_effect;
}

bool enchant_monster_invisible(monster* mon, const string &how)
{
    // Store the monster name before it becomes an "it". - bwr
    const string monster_name = mon->name(DESC_THE);

    if (!mon->has_ench(ENCH_INVIS) && mon->add_ench(ENCH_INVIS))
    {
        if (mons_near(mon))
        {
            const bool is_visible = mon->visible_to(&you);

            // Can't use simple_monster_message() here, since it checks
            // for visibility of the monster (and it's now invisible).
            // - bwr
            mprf("%s %s%s",
                 monster_name.c_str(),
                 how.c_str(),
                 is_visible ? " for a moment."
                            : "!");

            if (!is_visible && !mons_is_safe(mon))
                autotoggle_autopickup(true);
        }

        return true;
    }

    return false;
}

mon_resist_type bolt::try_enchant_monster(monster* mon, int &res_margin)
{
    // Early out if the enchantment is meaningless.
    if (!_ench_flavour_affects_monster(flavour, mon))
        return MON_UNAFFECTED;

    // Check magic resistance.
    if (has_saving_throw())
    {
        if (mons_immune_magic(mon))
            return MON_UNAFFECTED;

        // (Very) ugly things and shapeshifters will never resist
        // polymorph beams.
        if (flavour == BEAM_POLYMORPH
            && (mon->type == MONS_UGLY_THING
                || mon->type == MONS_VERY_UGLY_THING
                || mon->is_shapeshifter()))
        {
            ;
        }
        else
        {
            res_margin = mon->check_res_magic(ench_power);
            if (res_margin > 0)
                return MON_RESIST;
        }
    }

    return apply_enchantment_to_monster(mon);
}

mon_resist_type bolt::apply_enchantment_to_monster(monster* mon)
{
    // Gigantic-switches-R-Us
    switch (flavour)
    {
    case BEAM_TELEPORT:
        if (mon->no_tele())
            return MON_UNAFFECTED;
        if (mon->observable())
            obvious_effect = true;
        monster_teleport(mon, false);
        return MON_AFFECTED;

    case BEAM_BLINK:
        if (mon->no_tele())
            return MON_UNAFFECTED;
        if (mon->observable())
            obvious_effect = true;
        monster_blink(mon);
        return MON_AFFECTED;

    case BEAM_BLINK_CLOSE:
        if (mon->no_tele())
            return MON_UNAFFECTED;
        if (mon->observable())
            obvious_effect = true;
        blink_other_close(mon, source);
        return MON_AFFECTED;

    case BEAM_POLYMORPH:
        if (mon->polymorph(ench_power))
            obvious_effect = true;
        if (YOU_KILL(thrower))
        {
            did_god_conduct(DID_DELIBERATE_MUTATING, 2 + random2(3),
                            effect_known);
        }
        return MON_AFFECTED;

    case BEAM_MALMUTATE:
        if (mon->malmutate("")) // exact source doesn't matter
            obvious_effect = true;
        if (YOU_KILL(thrower))
        {
            did_god_conduct(DID_DELIBERATE_MUTATING, 2 + random2(3),
                            effect_known);
        }
        return MON_AFFECTED;

    case BEAM_BANISH:
        if (player_in_branch(BRANCH_ABYSS) && x_chance_in_y(you.depth, brdepth[BRANCH_ABYSS]))
            simple_monster_message(mon, " wobbles for a moment.");
        else
            mon->banish(agent());
        obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_DISPEL_UNDEAD:
        if (simple_monster_message(mon, " convulses!"))
            obvious_effect = true;
        mon->hurt(agent(), damage.roll());
        return MON_AFFECTED;

    case BEAM_ENSLAVE_SOUL:
    {
        dprf(DIAG_BEAM, "HD: %d; pow: %d", mon->hit_dice, ench_power);

        if (!mons_can_be_zombified(mon) || mons_intel(mon) < I_NORMAL)
            return MON_UNAFFECTED;

        // The monster can be no more than lightly wounded/damaged.
        if (mons_get_damage_level(mon) > MDAM_LIGHTLY_DAMAGED)
        {
            simple_monster_message(mon, "'s soul is too badly injured.");
            return MON_OTHER;
        }

        obvious_effect = true;
        const int duration = you.skill_rdiv(SK_INVOCATIONS, 3, 4) + 2;
        mon->add_ench(mon_enchant(ENCH_SOUL_RIPE, 0, agent(), duration * 10));
        simple_monster_message(mon, "'s soul is now ripe for the taking.");
        return MON_AFFECTED;
    }

    case BEAM_PAIN:             // pain/agony
        if (simple_monster_message(mon, " convulses in agony!"))
            obvious_effect = true;

        if (name.find("agony") != string::npos) // agony
            mon->hurt(agent(), min((mon->hit_points+1)/2, mon->hit_points-1), BEAM_TORMENT_DAMAGE);
        else                    // pain
            mon->hurt(agent(), damage.roll(), flavour);
        return MON_AFFECTED;

    case BEAM_DISINTEGRATION:   // disrupt/disintegrate
        if (simple_monster_message(mon, " is blasted."))
            obvious_effect = true;
        mon->hurt(agent(), damage.roll(), flavour);
        return MON_AFFECTED;

    case BEAM_HIBERNATION:
        if (mon->can_hibernate())
        {
            if (simple_monster_message(mon, " looks drowsy..."))
                obvious_effect = true;
            mon->hibernate();
            return MON_AFFECTED;
        }
        return MON_UNAFFECTED;

    case BEAM_CORONA:
        if (backlight_monsters(mon->pos(), hit, 0))
        {
            obvious_effect = true;
            return MON_AFFECTED;
        }
        return MON_UNAFFECTED;

    case BEAM_SLOW:
        obvious_effect = do_slow_monster(mon, agent());
        return MON_AFFECTED;

    case BEAM_HASTE:
        if (YOU_KILL(thrower))
            did_god_conduct(DID_HASTY, 6, effect_known);

        if (mon->check_stasis(false))
            return MON_AFFECTED;

        if (!mon->has_ench(ENCH_HASTE)
            && !mons_is_stationary(mon)
            && mon->add_ench(ENCH_HASTE))
        {
            if (!mons_is_immotile(mon)
                && simple_monster_message(mon, " seems to speed up."))
            {
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_MIGHT:
        if (!mon->has_ench(ENCH_MIGHT)
            && !mons_is_stationary(mon)
            && mon->add_ench(ENCH_MIGHT))
        {
            if (simple_monster_message(mon, " seems to grow stronger."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_BERSERK:
        if (!mon->berserk_or_insane())
        {
            // currently from potion, hence voluntary
            mon->go_berserk(true);
            // can't return this from go_berserk, unfortunately
            obvious_effect = mons_near(mon);
        }
        return MON_AFFECTED;

    case BEAM_HEALING:
        if (thrower == KILL_YOU || thrower == KILL_YOU_MISSILE)
        {
            // No KILL_YOU_CONF, or we get "You heal ..."
            if (cast_healing(3 + damage.roll(), 3 + damage.num * damage.size,
                             false, mon->pos()) > 0)
            {
                obvious_effect = true;
            }
            msg_generated = true; // to avoid duplicate "nothing happens"
        }
        else if (mon->heal(3 + damage.roll()))
        {
            if (mon->hit_points == mon->max_hit_points)
            {
                if (simple_monster_message(mon, "'s wounds heal themselves!"))
                    obvious_effect = true;
            }
            else if (simple_monster_message(mon, " is healed somewhat."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_PARALYSIS:
        apply_bolt_paralysis(mon);
        return MON_AFFECTED;

    case BEAM_PETRIFY:
        if (mon->res_petrify())
            return MON_UNAFFECTED;

        apply_bolt_petrify(mon);
        return MON_AFFECTED;

    case BEAM_SPORE:
    case BEAM_CONFUSION:
        if (!mons_class_is_confusable(mon->type))
            return MON_UNAFFECTED;

        if (mon->check_clarity(false))
        {
            if (you.can_see(mon) && !mons_is_lurking(mon))
                obvious_effect = true;
            return MON_AFFECTED;
        }

        if (mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, agent())))
        {
            // FIXME: Put in an exception for things you won't notice
            // becoming confused.
            if (simple_monster_message(mon, " appears confused."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_SLEEP:
        if (mon->has_ench(ENCH_SLEEPY))
            return MON_UNAFFECTED;

        mon->put_to_sleep(agent(), 0);
        if (simple_monster_message(mon, " falls asleep!"))
            obvious_effect = true;

        return MON_AFFECTED;

    case BEAM_INVISIBILITY:
    {
        // Mimic or already glowing.
        if (mons_is_mimic(mon->type) || mon->glows_naturally())
            return MON_UNAFFECTED;

        if (enchant_monster_invisible(mon, "flickers and vanishes"))
            obvious_effect = true;

        return MON_AFFECTED;
    }

    case BEAM_ENSLAVE:
        if (player_will_anger_monster(mon))
        {
            simple_monster_message(mon, " is repulsed!");
            obvious_effect = true;
            return MON_OTHER;
        }

        // Being a puppet on magic strings is a nasty thing.
        // Mindless creatures shouldn't probably mind, but because of complex
        // behaviour of enslaved neutrals, let's disallow that for now.
        mon->attitude = ATT_HOSTILE;

        // XXX: Another hackish thing for Pikel's band neutrality.
        if (mons_is_pikel(mon))
            pikel_band_neutralise();

        if (simple_monster_message(mon, " is charmed."))
            obvious_effect = true;
        mon->add_ench(ENCH_CHARM);
        return MON_AFFECTED;

    case BEAM_PORKALATOR:
    {
        // Monsters which use the ghost structure can't be properly
        // restored from hog form.
        if (mons_is_ghost_demon(mon->type))
            return MON_UNAFFECTED;

        monster orig_mon(*mon);
        if (monster_polymorph(mon, mon->holiness() == MH_DEMONIC ?
                      MONS_HELL_HOG : mon->holiness() == MH_HOLY ?
                      MONS_HOLY_SWINE : MONS_HOG))
        {
            obvious_effect = true;

            // Don't restore items to monster if it reverts.
            orig_mon.inv = mon->inv;

            // monsters can't cast spells in hog form either -doy
            mon->spells.clear();

            // For monster reverting to original form.
            mon->props[ORIG_MONSTER_KEY] = orig_mon;
        }


        return MON_AFFECTED;
    }

    case BEAM_INNER_FLAME:
        if (!mon->has_ench(ENCH_INNER_FLAME)
            && !mon->is_summoned()
            && mon->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, agent())))
        {
            if (simple_monster_message(mon,
                                       (mon->body_size(PSIZE_BODY) > SIZE_BIG)
                                        ? " is filled with an intense inner flame!"
                                        : " is filled with an inner flame."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_DIMENSION_ANCHOR:
        if (!mon->has_ench(ENCH_DIMENSION_ANCHOR)
            && mon->add_ench(mon_enchant(ENCH_DIMENSION_ANCHOR, 0, agent())))
        {
            if (simple_monster_message(mon, " is firmly anchored in space."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_VULNERABILITY:
        if (!mon->has_ench(ENCH_LOWERED_MR)
            && mon->add_ench(mon_enchant(ENCH_LOWERED_MR, 0, agent())))
        {
            if (you.can_see(mon))
            {
                mprf("%s magical defenses are stripped away.",
                     mon->name(DESC_ITS).c_str());
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_MALIGN_OFFERING:
    {
        int dam = resist_adjust_damage(mon, BEAM_NEG, mon->res_negative_energy(),
                                       damage.roll());
        if (dam)
        {
            _malign_offering_effect(mon, agent(), dam);
            obvious_effect = true;
        }
        else
            simple_monster_message(mon, " is unaffected.");
    }

    default:
        break;
    }

    return MON_AFFECTED;
}


// Extra range used on hit.
int bolt::range_used_on_hit() const
{
    int used = 0;

    // Non-beams can only affect one thing (player/monster).
    if (!is_beam)
        used = BEAM_STOP;
    else if (is_enchantment())
        used = (flavour == BEAM_DIGGING ? 0 : BEAM_STOP);
    // Hellfire stops for nobody!
    else if (name.find("hellfire") != string::npos)
        used = 0;
    // Generic explosion.
    else if (is_explosion || is_big_cloud)
        used = BEAM_STOP;
    // Plant spit.
    else if (flavour == BEAM_ACID)
        used = BEAM_STOP;
    // Lightning goes through things.
    else if (flavour == BEAM_ELECTRICITY)
        used = 0;
    else
        used = 1;

    // Assume we didn't hit, after all.
    if (is_tracer && beam_source == NON_MONSTER && used > 0
        && hit < AUTOMATIC_HIT)
    {
        return 0;
    }

    if (in_explosion_phase)
        return used;

    for (unsigned int i = 0; i < range_funcs.size(); ++i)
        if ((*range_funcs[i])(*this, used))
            break;

    return used;
}

// Checks whether the beam knocks back the supplied actor. The actor
// should have already failed their EV check, so the save is entirely
// body-mass-based.
bool bolt::knockback_actor(actor *act)
{
    // We can't do knockback if the beam starts and ends on the same space
    if (source == act->pos())
        return false;

    ASSERT(ray.pos() == act->pos());

    const coord_def oldpos(ray.pos());
    const ray_def ray_copy(ray);
    ray.advance();

    const coord_def newpos(ray.pos());
    if (newpos == oldpos
        || actor_at(newpos)
        || (act->is_monster()
            && mons_is_stationary(act->as_monster()))
        || feat_is_solid(grd(newpos))
        || !act->can_pass_through(newpos)
        || !act->is_habitable(newpos)
        // Save is based on target's body weight.
        || random2(2500) < act->body_weight())
    {
        ray = ray_copy;
        return false;
    }

    act->move_to_pos(newpos);

    // Knockback cannot ever kill the actor directly - caller must do
    // apply_location_effects after messaging.
    return true;
}

// Takes a bolt and refines it for use in the explosion function.
// Explosions which do not follow from beams (e.g., scrolls of
// immolation) bypass this function.
void bolt::refine_for_explosion()
{
    ASSERT(!special_explosion);

    const char *seeMsg  = NULL;
    const char *hearMsg = NULL;

    if (ex_size == 0)
        ex_size = 1;

    // Assume that the player can see/hear the explosion, or
    // gets burned by it anyway.  :)
    msg_generated = true;

    // tmp needed so that what c_str() points to doesn't go out of scope
    // before the function ends.
    string tmp;
    if (item != NULL)
    {
        tmp  = "The " + item->name(DESC_PLAIN, false, false, false)
               + " explodes!";

        seeMsg  = tmp.c_str();
        hearMsg = "You hear an explosion!";

        glyph   = dchar_glyph(DCHAR_FIRED_BURST);
    }

    if (name.find("hellfire") != string::npos)
    {
        seeMsg  = "The hellfire explodes!";
        hearMsg = "You hear a strangely unpleasant explosion!";

        glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        flavour = BEAM_HELLFIRE;
    }

    if (name == "fireball")
    {
        seeMsg  = "The fireball explodes!";
        hearMsg = "You hear an explosion!";

        glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        flavour = BEAM_FIRE;
        ex_size = 1;
    }

    if (name == "orb of electricity")
    {
        seeMsg  = "The orb of electricity explodes!";
        hearMsg = "You hear a clap of thunder!";

        glyph      = dchar_glyph(DCHAR_FIRED_BURST);
        flavour    = BEAM_ELECTRICITY;
        colour     = LIGHTCYAN;
        damage.num = 1;
        ex_size    = 2;
    }

    if (name == "metal orb")
    {
        seeMsg  = "The orb explodes into a blast of deadly shrapnel!";
        hearMsg = "You hear an explosion!";

        name    = "blast of shrapnel";
        glyph   = dchar_glyph(DCHAR_FIRED_ZAP);
        flavour = BEAM_FRAG;     // Sets it from pure damage to shrapnel
                                 // (which is absorbed extra by armour).
    }

    if (name == "great blast of fire")
    {
        seeMsg  = "A raging storm of fire appears!";
        hearMsg = "You hear a raging storm!";

        // Everything else is handled elsewhere...
    }

    if (name == "great blast of cold")
    {
        seeMsg  = "The blast explodes into a great storm of ice!";
        hearMsg = "You hear a raging storm!";

        name       = "ice storm";
        glyph      = dchar_glyph(DCHAR_FIRED_ZAP);
        colour     = WHITE;
        ex_size    = is_tracer ? 3 : (2 + (random2(ench_power) > 75));
    }

    if (name == "stinking cloud")
    {
        seeMsg     = "The beam explodes into a vile cloud!";
        hearMsg    = "You hear a loud \'bang\'!";
    }

    if (name == "foul vapour")
    {
        seeMsg     = "The ball explodes into a vile cloud!";
        hearMsg    = "You hear a loud \'bang\'!";
        if (!is_tracer)
            name = "stinking cloud";
    }

    if (name == "silver bolt")
    {
        seeMsg  = "The silver bolt explodes into a blast of light!";
        hearMsg = "The dungeon gets brighter for a moment.";

        glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        flavour = BEAM_BOLT_OF_ZIN;
        ex_size = 1;
    }

    if (name == "ghostly fireball")
    {
        seeMsg  = "The ghostly flame explodes!";
        hearMsg = "You hear the shriek of haunting fire!";

        glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        ex_size = 1;
    }

    if (seeMsg == NULL)
    {
        seeMsg  = "The beam explodes into a cloud of software bugs!";
        hearMsg = "You hear the sound of one hand!";
    }


    if (!is_tracer && *seeMsg && *hearMsg)
    {
        heard = player_can_hear(target);
        // Check for see/hear/no msg.
        if (you.see_cell(target) || target == you.pos())
            mpr(seeMsg);
        else
        {
            if (!heard)
                msg_generated = false;
            else
                mpr(hearMsg, MSGCH_SOUND);
        }
    }
}

typedef vector< vector<coord_def> > sweep_type;

static sweep_type _radial_sweep(int r)
{
    sweep_type result;
    sweep_type::value_type work;

    // Center first.
    work.push_back(coord_def(0,0));
    result.push_back(work);

    for (int rad = 1; rad <= r; ++rad)
    {
        work.clear();

        for (int d = -rad; d <= rad; ++d)
        {
            // Don't put the corners in twice!
            if (d != rad && d != -rad)
            {
                work.push_back(coord_def(-rad, d));
                work.push_back(coord_def(+rad, d));
            }

            work.push_back(coord_def(d, -rad));
            work.push_back(coord_def(d, +rad));
        }
        result.push_back(work);
    }
    return result;
}

#define MAX_EXPLOSION_RADIUS 9
// Returns true if we saw something happening.
bool bolt::explode(bool show_more, bool hole_in_the_middle)
{
    ASSERT(!special_explosion);
    ASSERT(!in_explosion_phase);
    ASSERT(ex_size > 0);

    // explode() can be called manually without setting real_flavour.
    // FIXME: The entire flavour/real_flavour thing needs some
    // rewriting!
    if (real_flavour == BEAM_CHAOS || real_flavour == BEAM_RANDOM)
        flavour = real_flavour;
    else
        real_flavour = flavour;

    const int r = min(ex_size, MAX_EXPLOSION_RADIUS);
    in_explosion_phase = true;
    // being hit by bounces doesn't exempt you from the explosion (not that it
    // currently ever matters)
    hit_count.clear();

    if (is_sanctuary(pos()))
    {
        if (!is_tracer && you.see_cell(pos()) && !name.empty())
        {
            mprf(MSGCH_GOD, "By Zin's power, the %s is contained.",
                 name.c_str());
            return true;
        }
        return false;
    }

#ifdef DEBUG_DIAGNOSTICS
    if (!quiet_debug)
    {
        dprf(DIAG_BEAM, "explosion at (%d, %d) : g=%d c=%d f=%d hit=%d dam=%dd%d r=%d",
             pos().x, pos().y, glyph, colour, flavour, hit, damage.num, damage.size, r);
    }
#endif

    if (!is_tracer)
    {
        loudness = 10 + 5 * r;

        // Lee's Rapid Deconstruction can target the tiles on the map
        // boundary.
        const coord_def noise_position = clamp_in_bounds(pos());
        bool heard_expl = noisy(loudness, noise_position, beam_source);

        heard = heard || heard_expl;

        if (heard_expl && !noise_msg.empty() && !you.see_cell(pos()))
            mprf(MSGCH_SOUND, "%s", noise_msg.c_str());
    }

    // Run DFS to determine which cells are influenced
    explosion_map exp_map;
    exp_map.init(INT_MAX);
    determine_affected_cells(exp_map, coord_def(), 0, r, true, true);

    // We get a bit fancy, drawing all radius 0 effects, then radius
    // 1, radius 2, etc.  It looks a bit better that way.
    const vector< vector<coord_def> > sweep = _radial_sweep(r);
    const coord_def centre(9,9);

    typedef sweep_type::const_iterator siter;
    typedef sweep_type::value_type::const_iterator viter;

    // Draw pass.
    if (!is_tracer)
    {
        for (siter ci = sweep.begin(); ci != sweep.end(); ++ci)
        {
            for (viter cci = ci->begin(); cci != ci->end(); ++cci)
            {
                const coord_def delta = *cci;

                if (delta.origin() && hole_in_the_middle)
                    continue;

                if (exp_map(delta + centre) < INT_MAX)
                    explosion_draw_cell(delta + pos());
            }
            update_screen();

            int explode_delay = 50;
            // Scale delay to match change in arena_delay.
            if (crawl_state.game_is_arena())
            {
                explode_delay *= Options.arena_delay;
                explode_delay /= 600;
            }

            delay(explode_delay);
        }
    }

    // Affect pass.
    int cells_seen = 0;
    for (siter ci = sweep.begin(); ci != sweep.end(); ++ci)
    {
        for (viter cci = ci->begin(); cci != ci->end(); ++cci)
        {
            const coord_def delta = *cci;

            if (delta.origin() && hole_in_the_middle)
                continue;

            if (exp_map(delta + centre) < INT_MAX)
            {
                if (you.see_cell(delta + pos()))
                    ++cells_seen;

                explosion_affect_cell(delta + pos());
            }
        }
    }

    // Delay after entire explosion has been drawn.
    if (!is_tracer && cells_seen > 0 && show_more)
    {
        int explode_delay = 150;
        // Scale delay to match change in arena_delay.
        if (crawl_state.game_is_arena())
        {
            explode_delay *= Options.arena_delay;
            explode_delay /= 600;
        }

        delay(explode_delay);
    }

    return (cells_seen > 0);
}

void bolt::explosion_draw_cell(const coord_def& p)
{
    if (you.see_cell(p))
    {
        const coord_def drawpos = grid2view(p);
#ifdef USE_TILE
        if (in_los_bounds_v(drawpos))
        {
            int dist = (p - source).rdist();
            tileidx_t tile = tileidx_bolt(*this);
            tiles.add_overlay(p, vary_bolt_tile(tile, dist));
        }
#endif
#ifndef USE_TILE_LOCAL
        // bounds check
        if (in_los_bounds_v(drawpos))
        {
            cgotoxy(drawpos.x, drawpos.y, GOTO_DNGN);
            put_colour_ch(colour == BLACK ? random_colour() : colour,
                          dchar_glyph(DCHAR_EXPLOSION));
        }
#endif
    }
}

void bolt::explosion_affect_cell(const coord_def& p)
{
    // pos() = target during an explosion, so restore it after affecting
    // the cell.
    const coord_def orig_pos = target;

    fake_flavour();
    target = p;
    affect_cell();
    flavour = real_flavour;

    target = orig_pos;
}

// Uses DFS
void bolt::determine_affected_cells(explosion_map& m, const coord_def& delta,
                                    int count, int r,
                                    bool stop_at_statues, bool stop_at_walls)
{
    const coord_def centre(9,9);
    const coord_def loc = pos() + delta;

    // A bunch of tests for edge cases.
    if (delta.rdist() > centre.rdist()
        || (delta.abs() > r*(r+1))
        || (count > 10*r)
        || !map_bounds(loc)
        || is_sanctuary(loc))
    {
        return;
    }

    const dungeon_feature_type dngn_feat = grd(loc);

    bool at_wall = false;

    // Check to see if we're blocked by a wall.
    if (feat_is_wall(dngn_feat)
        || feat_is_closed_door(dngn_feat))
    {
        // Special case: explosion originates from rock/statue
        // (e.g. Lee's Rapid Deconstruction) - in this case, ignore
        // solid cells at the center of the explosion.
        if (stop_at_walls && !(delta.origin() && can_affect_wall(dngn_feat)))
            return;
        // But remember that we are at a wall.
        at_wall = true;
    }

    if (feat_is_solid(dngn_feat) && !feat_is_wall(dngn_feat)
        && !(delta.origin() && can_affect_wall(dngn_feat))
        && stop_at_statues)
        return;

    // Check if it passes the callback functions.
    bool hits = true;
    for (unsigned int i = 0; i < aoe_funcs.size(); ++i)
        hits = (*aoe_funcs[i])(*this, loc) && hits;

    if (hits)
    {
        // Hmm, I think we're OK.
        m(delta + centre) = min(count, m(delta + centre));
    }

    // Now recurse in every direction.
    for (int i = 0; i < 8; ++i)
    {
        const coord_def new_delta = delta + Compass[i];

        if (new_delta.rdist() > centre.rdist())
            continue;

        // Is that cell already covered?
        if (m(new_delta + centre) <= count)
            continue;

        // If we were at a wall, only move to visible squares.
        coord_def caster_pos = (invalid_monster_index(beam_source) ? you.pos()
                                                     : menv[beam_source].pos());
        if (at_wall && !cell_see_cell(caster_pos, loc + Compass[i], LOS_NO_TRANS))
            continue;

        int cadd = 5;
        // Circling around the center is always free.
        if (hits && delta.rdist() == 1 && new_delta.rdist() == 1)
            cadd = 0;
        // Otherwise changing direction (e.g. looking around a wall) costs more.
        else if (delta.x * Compass[i].x < 0 || delta.y * Compass[i].y < 0)
            cadd = 17;

        determine_affected_cells(m, new_delta, count + cadd, r,
                                 stop_at_statues, stop_at_walls);
    }
}

// Returns true if the beam is harmful (ignoring monster
// resists) -- mon is given for 'special' cases where,
// for example, "Heal" might actually hurt undead, or
// "Holy Word" being ignored by holy monsters, etc.
//
// Only enchantments should need the actual monster type
// to determine this; non-enchantments are pretty
// straightforward.
bool bolt::nasty_to(const monster* mon) const
{
    // Cleansing flame.
    if (flavour == BEAM_HOLY)
        return (mon->res_holy_energy(agent()) <= 0);

    // The orbs are made of pure disintegration energy.  This also has the side
    // effect of not stopping us from firing further orbs when the previous one
    // is still flying.
    if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_NUKE)
        return (mon->type != MONS_ORB_OF_DESTRUCTION);

    // Take care of other non-enchantments.
    if (!is_enchantment())
        return true;

    // Now for some non-hurtful enchantments.
    if (flavour == BEAM_DIGGING)
        return false;

    // Positive effects.
    if (nice_to(mon))
        return false;

    // Friendly and good neutral monsters don't mind being teleported.
    if (flavour == BEAM_TELEPORT)
        return !mon->wont_attack();

    // enslave soul
    if (flavour == BEAM_ENSLAVE_SOUL)
        return mon->holiness() == MH_NATURAL;

    // sleep
    if (flavour == BEAM_HIBERNATION)
        return mon->can_hibernate(true);

    // dispel undead
    if (flavour == BEAM_DISPEL_UNDEAD)
        return (mon->holiness() == MH_UNDEAD);

    // pain / agony
    if (flavour == BEAM_PAIN)
        return !mon->res_negative_energy();

    if (flavour == BEAM_GHOSTLY_FLAME)
        return mon->holiness() != MH_UNDEAD;

    // everything else is considered nasty by everyone
    return true;
}

// Return true if the bolt is considered nice by mon.
// This is not the inverse of nasty_to(): the bolt needs to be
// actively positive.
bool bolt::nice_to(const monster* mon) const
{
    // Polymorphing a (very) ugly thing will mutate it into a different
    // (very) ugly thing.
    if (flavour == BEAM_POLYMORPH)
    {
        return (mon->type == MONS_UGLY_THING
                || mon->type == MONS_VERY_UGLY_THING);
    }

    if (flavour == BEAM_HASTE
        || flavour == BEAM_HEALING
        || flavour == BEAM_MIGHT
        || flavour == BEAM_INVISIBILITY)
    {
        return true;
    }

    if (flavour == BEAM_GHOSTLY_FLAME && mon->holiness() == MH_UNDEAD)
        return true;

    return false;
}

////////////////////////////////////////////////////////////////////////////
// bolt

// A constructor for bolt to help guarantee that we start clean (this has
// caused way too many bugs).  Putting it here since there's no good place to
// put it, and it doesn't do anything other than initialise its members.
//
// TODO: Eventually it'd be nice to have a proper factory for these things
// (extended from setup_mons_cast() and zapping() which act as limited ones).
bolt::bolt() : origin_spell(SPELL_NO_SPELL),
               range(-2), glyph('*'), colour(BLACK), flavour(BEAM_MAGIC),
               real_flavour(BEAM_MAGIC), drop_item(false), item(NULL),
               source(), target(), damage(0, 0), ench_power(0), hit(0),
               thrower(KILL_MISC), ex_size(0), beam_source(MHITNOT),
               source_name(), name(), short_name(), hit_verb(),
               loudness(0), noise_msg(), is_beam(false), is_explosion(false),
               is_big_cloud(false), aimed_at_spot(false), aux_source(),
               affects_nothing(false), affects_items(true), effect_known(true),
               draw_delay(15), special_explosion(NULL), animate(true),
               ac_rule(AC_NORMAL),
#ifdef DEBUG_DIAGNOSTICS
               quiet_debug(false),
#endif
               range_funcs(),
               damage_funcs(), hit_funcs(), aoe_funcs(), affect_func(NULL),
               obvious_effect(false), seen(false), heard(false),
               path_taken(), extra_range_used(0), is_tracer(false),
               is_targetting(false), aimed_at_feet(false), msg_generated(false),
               noise_generated(false), passed_target(false),
               in_explosion_phase(false), smart_monster(false),
               can_see_invis(false), nightvision(false), attitude(ATT_HOSTILE), foe_ratio(0),
               chose_ray(false), beam_cancelled(false),
               dont_stop_player(false), bounces(false), bounce_pos(),
               reflections(0), reflector(-1), auto_hit(false)
{
}

killer_type bolt::killer() const
{
    if (flavour == BEAM_BANISH)
        return KILL_BANISHED;

    switch (thrower)
    {
    case KILL_YOU:
    case KILL_YOU_MISSILE:
        return (flavour == BEAM_PARALYSIS
                || flavour == BEAM_PETRIFY) ? KILL_YOU : KILL_YOU_MISSILE;

    case KILL_MON:
    case KILL_MON_MISSILE:
        return KILL_MON_MISSILE;

    case KILL_YOU_CONF:
        return KILL_YOU_CONF;

    default:
        return KILL_MON_MISSILE;
    }
}

void bolt::set_target(const dist &d)
{
    if (!d.isValid)
        return;

    target = d.target;

    chose_ray = d.choseRay;
    if (d.choseRay)
        ray = d.ray;

    if (d.isEndpoint)
        aimed_at_spot = true;
}

void bolt::setup_retrace()
{
    if (pos().x && pos().y)
        target = pos();

    swap(source, target);
    chose_ray        = false;
    affects_nothing  = true;
    aimed_at_spot    = true;
    extra_range_used = 0;
}

void bolt::set_agent(actor *actor)
{
    // NULL actor is fine by us.
    if (!actor)
        return;

    if (actor->is_player())
        thrower = KILL_YOU_MISSILE;
    else
    {
        thrower = KILL_MON_MISSILE;
        beam_source = actor->mindex();
    }
}

actor* bolt::agent(bool ignore_reflection) const
{
    killer_type nominal_ktype = thrower;
    int nominal_source = beam_source;

    // If the beam was reflected report a different point of origin
    if (reflections > 0 && !ignore_reflection)
    {
        if (reflector == NON_MONSTER)
            nominal_ktype = KILL_YOU_MISSILE;
        nominal_source = reflector;
    }
    if (YOU_KILL(nominal_ktype))
        return &you;
    else if (!invalid_monster_index(nominal_source))
        return &menv[nominal_source];
    else
        return NULL;
}

bool bolt::is_enchantment() const
{
    return (flavour >= BEAM_FIRST_ENCHANTMENT
            && flavour <= BEAM_LAST_ENCHANTMENT);
}

string bolt::get_short_name() const
{
    if (!short_name.empty())
        return short_name;

    if (item != NULL && item->defined())
        return item->name(DESC_A, false, false, false, false,
                          ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK
                          | ISFLAG_RACIAL_MASK);

    if (real_flavour == BEAM_RANDOM || real_flavour == BEAM_CHAOS)
        return _beam_type_name(real_flavour);

    if (flavour == BEAM_FIRE && name == "sticky fire")
        return "sticky fire";

    if (flavour == BEAM_ELECTRICITY && is_beam)
        return "lightning";

    if (flavour == BEAM_NONE || flavour == BEAM_MISSILE
        || flavour == BEAM_MMISSILE)
    {
        return name;
    }

    return _beam_type_name(flavour);
}

static string _beam_type_name(beam_type type)
{
    switch (type)
    {
    case BEAM_NONE:                  return "none";
    case BEAM_MISSILE:               return "missile";
    case BEAM_MMISSILE:              return "magic missile";
    case BEAM_FIRE:                  return "fire";
    case BEAM_COLD:                  return "cold";
    case BEAM_WATER:                 return "water";
    case BEAM_MAGIC:                 return "magic";
    case BEAM_ELECTRICITY:           return "electricity";
    case BEAM_MEPHITIC:              return "noxious fumes";
    case BEAM_POISON:                return "poison";
    case BEAM_NEG:                   return "negative energy";
    case BEAM_ACID:                  return "acid";
    case BEAM_MIASMA:                return "miasma";
    case BEAM_SPORE:                 return "spores";
    case BEAM_POISON_ARROW:          return "poison arrow";
    case BEAM_HELLFIRE:              return "hellfire";
    case BEAM_NAPALM:                return "sticky fire";
    case BEAM_STEAM:                 return "steam";
    case BEAM_ENERGY:                return "energy";
    case BEAM_HOLY:                  return "holy energy";
    case BEAM_FRAG:                  return "fragments";
    case BEAM_LAVA:                  return "magma";
    case BEAM_ICE:                   return "ice";
    case BEAM_NUKE:                  return "nuke";
    case BEAM_LIGHT:                 return "light";
    case BEAM_RANDOM:                return "random";
    case BEAM_CHAOS:                 return "chaos";
    case BEAM_GHOSTLY_FLAME:         return "ghostly flame";
    case BEAM_SLOW:                  return "slow";
    case BEAM_HASTE:                 return "haste";
    case BEAM_MIGHT:                 return "might";
    case BEAM_HEALING:               return "healing";
    case BEAM_PARALYSIS:             return "paralysis";
    case BEAM_CONFUSION:             return "confusion";
    case BEAM_INVISIBILITY:          return "invisibility";
    case BEAM_DIGGING:               return "digging";
    case BEAM_TELEPORT:              return "teleportation";
    case BEAM_POLYMORPH:             return "polymorph";
    case BEAM_MALMUTATE:             return "malmutation";
    case BEAM_ENSLAVE:               return "enslave";
    case BEAM_BANISH:                return "banishment";
    case BEAM_ENSLAVE_SOUL:          return "enslave soul";
    case BEAM_PAIN:                  return "pain";
    case BEAM_DISPEL_UNDEAD:         return "dispel undead";
    case BEAM_DISINTEGRATION:        return "disintegration";
    case BEAM_BLINK:                 return "blink";
    case BEAM_BLINK_CLOSE:           return "blink close";
    case BEAM_PETRIFY:               return "petrify";
    case BEAM_CORONA:                return "backlight";
    case BEAM_PORKALATOR:            return "porkalator";
    case BEAM_HIBERNATION:           return "hibernation";
    case BEAM_SLEEP:                 return "sleep";
    case BEAM_BERSERK:               return "berserk";
    case BEAM_VISUAL:                return "visual effects";
    case BEAM_TORMENT_DAMAGE:        return "torment damage";
    case BEAM_DEVOUR_FOOD:           return "devour food";
#if TAG_MAJOR_VERSION == 34
    case BEAM_GLOOM:                 return "gloom";
#endif
    case BEAM_INK:                   return "ink";
    case BEAM_HOLY_FLAME:            return "cleansing flame";
    case BEAM_HOLY_LIGHT:            return "holy light";
    case BEAM_AIR:                   return "air";
    case BEAM_INNER_FLAME:           return "inner flame";
    case BEAM_PETRIFYING_CLOUD:      return "calcifying dust";
    case BEAM_BOLT_OF_ZIN:           return "silver light";
    case BEAM_ENSNARE:               return "magic web";
    case BEAM_SENTINEL_MARK:         return "sentinel's mark";
    case BEAM_DIMENSION_ANCHOR:      return "dimension anchor";
    case BEAM_VULNERABILITY:         return "vulnerability";
    case BEAM_MALIGN_OFFERING:       return "malign offering";

    case NUM_BEAMS:                  die("invalid beam type");
    }
    die("unknown beam type");
}

string bolt::get_source_name() const
{
    if (!source_name.empty())
        return source_name;
    const actor *a = agent();
    if (a)
        return a->name(DESC_A, true);
    return "";
}

void clear_zap_info_on_exit()
{
    for (unsigned int i = 0; i < ARRAYSZ(zap_data); ++i)
    {
        delete zap_data[i].damage;
        delete zap_data[i].tohit;
    }
}

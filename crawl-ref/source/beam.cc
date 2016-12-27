/**
 * @file
 * @brief Functions related to ranged attacks.
**/

#include "AppHdr.h"

#include "beam.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "exercise.h"
#include "fight.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::convert_orcs
#include "item-use.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "potion.h"
#include "prompt.h"
#include "ranged-attack.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

#define SAP_MAGIC_CHANCE() x_chance_in_y(7, 10)

// Helper functions (some of these should probably be public).
static void _ench_animation(int flavour, const monster* mon = nullptr,
                            bool force = false);
static beam_type _chaos_beam_flavour(bolt* beam);
static string _beam_type_name(beam_type type);
static void _explosive_bolt_explode(bolt *parent, coord_def pos);
int _ench_pow_to_dur(int pow);

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
    return !pierce && !is_explosion && flavour != BEAM_ELECTRICITY
           && hit != AUTOMATIC_HIT && flavour != BEAM_VISUAL;
}

/// Can 'omnireflection' (from the Warlock's Mirror) potentially reflect this?
bool bolt::is_omnireflectable() const
{
    return !is_explosion && flavour != BEAM_VISUAL
            && origin_spell != SPELL_GLACIATE;
}

void bolt::emit_message(const char* m)
{
    const string message = m;
    if (!message_cache.count(message))
        mpr(m);

    message_cache.insert(message);
}

kill_category bolt::whose_kill() const
{
    if (YOU_KILL(thrower) || source_id == MID_YOU_FAULTLESS)
        return KC_YOU;
    else if (MON_KILL(thrower))
    {
        if (source_id == MID_ANON_FRIEND)
            return KC_FRIENDLY;
        const monster* mon = monster_by_mid(source_id);
        if (mon && mon->friendly())
            return KC_FRIENDLY;
    }
    return KC_OTHER;
}

// A simple animated flash from Rupert Smith (expanded to be more
// generic).
static void _zap_animation(int colour, const monster* mon = nullptr,
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

        scaled_delay(50);
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
    case BEAM_INFESTATION:
    case BEAM_PAIN:
    case BEAM_AGONY:
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
    case BEAM_BECKONING:
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
    zappy(ztype, power, false, pbolt);

    if (msg)
        mpr(msg);

    if (ztype == ZAP_LIGHTNING_BOLT)
    {
        noisy(spell_effect_noise(SPELL_LIGHTNING_BOLT),
               you.pos(), "You hear a mighty clap of thunder!");
        pbolt.heard = true;
    }

    if (ztype == ZAP_DIG)
        pbolt.aimed_at_spot = false;

    pbolt.fire();

    return SPRET_SUCCESS;
}

// Returns true if the path is considered "safe", and false if there are
// monsters in the way the player doesn't want to hit.
bool player_tracer(zap_type ztype, int power, bolt &pbolt, int range)
{
    // Non-controlleable during confusion.
    // (We'll shoot in a different direction anyway.)
    if (you.confused())
        return true;

    zappy(ztype, power, false, pbolt);

    pbolt.is_tracer     = true;
    pbolt.source        = you.pos();
    pbolt.source_id     = MID_PLAYER;
    pbolt.attitude      = ATT_FRIENDLY;
    pbolt.thrower       = KILL_YOU_MISSILE;


    // Init tracer variables.
    pbolt.friend_info.reset();
    pbolt.foe_info.reset();

    pbolt.foe_ratio        = 100;
    pbolt.beam_cancelled   = false;
    pbolt.dont_stop_player = false;
    pbolt.dont_stop_trees  = false;

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
        you.turn_is_over = false;
        return false;
    }

    // Set to non-tracing for actual firing.
    pbolt.is_tracer = false;
    return true;
}

// Returns true if the player wants / needs to abort based on god displeasure
// with targeting this target with this spell. Returns false otherwise.
static bool _stop_because_god_hates_target_prompt(monster* mon,
                                                  spell_type spell,
                                                  beam_type flavour,
                                                  item_def *item)
{
    if (spell == SPELL_TUKIMAS_DANCE)
    {
        const item_def * const first = mon->weapon(0);
        const item_def * const second = mon->weapon(1);
        bool prompt = first && god_hates_item(*first)
                      || second && god_hates_item(*second);
        if (prompt
            && !yesno("Animating this weapon would place you under penance. "
            "Really cast this spell?", false, 'n'))
        {
            return true;
        }
    }

    return false;
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
    int operator()(int pow) const override
    {
        return adder + pow * mult_num / mult_denom;
    }
};

typedef power_deducer<dice_def> dam_deducer;

template<int numdice, int adder, int mult_num, int mult_denom>
class dicedef_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow) const override
    {
        return dice_def(numdice, adder + pow * mult_num / mult_denom);
    }
};

template<int numdice, int adder, int mult_num, int mult_denom>
class calcdice_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow) const override
    {
        return calc_dice(numdice, adder + pow * mult_num / mult_denom);
    }
};

struct zap_info
{
    zap_type ztype;
    const char* name;           // nullptr means handled specially
    int player_power_cap;
    dam_deducer* player_damage;
    tohit_deducer* player_tohit;    // Enchantments have power modifier here
    dam_deducer* monster_damage;
    tohit_deducer* monster_tohit;
    colour_t colour;
    bool is_enchantment;
    beam_type flavour;
    dungeon_char_type glyph;
    bool always_obvious;
    bool can_beam;
    bool is_explosion;
    int hit_loudness;
};

#include "zap-data.h"

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
        return nullptr;
    else
        return &zap_data[zap_index[z_type]];
}

int zap_power_cap(zap_type z_type)
{
    const zap_info* zinfo = _seek_zap(z_type);

    return zinfo ? zinfo->player_power_cap : 0;
}

int zap_ench_power(zap_type z_type, int pow, bool is_monster)
{
    const zap_info* zinfo = _seek_zap(z_type);
    if (!zinfo)
        return pow;

    if (zinfo->player_power_cap > 0 && !is_monster)
        pow = min(zinfo->player_power_cap, pow);

    tohit_deducer* ench_calc = is_monster ? zinfo->monster_tohit
                                          : zinfo->player_tohit;
    if (zinfo->is_enchantment && ench_calc)
        return (*ench_calc)(pow);
    else
        return pow;
}

void zappy(zap_type z_type, int power, bool is_monster, bolt &pbolt)
{
    const zap_info* zinfo = _seek_zap(z_type);

    // None found?
    if (zinfo == nullptr)
    {
        dprf("Couldn't find zap type %d", z_type);
        return;
    }

    // Fill
    pbolt.name           = zinfo->name;
    pbolt.flavour        = zinfo->flavour;
    pbolt.real_flavour   = zinfo->flavour;
    pbolt.colour         = zinfo->colour;
    pbolt.glyph          = dchar_glyph(zinfo->glyph);
    pbolt.obvious_effect = zinfo->always_obvious;
    pbolt.pierce         = zinfo->can_beam;
    pbolt.is_explosion   = zinfo->is_explosion;

    if (zinfo->player_power_cap > 0 && !is_monster)
        power = min(zinfo->player_power_cap, power);

    ASSERT(zinfo->is_enchantment == pbolt.is_enchantment());

    pbolt.ench_power = zap_ench_power(z_type, power, is_monster);

    if (zinfo->is_enchantment)
        pbolt.hit = AUTOMATIC_HIT;
    else
    {
        tohit_deducer* hit_calc = is_monster ? zinfo->monster_tohit
                                             : zinfo->player_tohit;
        ASSERT(hit_calc);
        pbolt.hit = (*hit_calc)(power);
        if (pbolt.hit != AUTOMATIC_HIT && !is_monster)
            pbolt.hit = max(0, pbolt.hit - 5 * you.inaccuracy());
    }

    dam_deducer* dam_calc = is_monster ? zinfo->monster_damage
                                       : zinfo->player_damage;
    if (dam_calc)
        pbolt.damage = (*dam_calc)(power);

    pbolt.origin_spell = zap_to_spell(z_type);

    if (z_type == ZAP_BREATHE_FIRE && you.species == SP_RED_DRACONIAN
        && !is_monster)
    {
        pbolt.origin_spell = SPELL_SEARING_BREATH;
    }

    if (pbolt.loudness == 0)
        pbolt.loudness = zinfo->hit_loudness;
}

bool bolt::can_affect_actor(const actor *act) const
{
    // Blinkbolt doesn't hit its caster, since they are the bolt.
    if (origin_spell == SPELL_BLINKBOLT && act->mid == source_id)
        return false;
    auto cnt = hit_count.find(act->mid);
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

    return !act->submerged();
}

static beam_type _chaos_beam_flavour(bolt* beam)
{
    beam_type flavour;
    do
    {
        flavour = random_choose_weighted(
            10, BEAM_FIRE,
            10, BEAM_COLD,
            10, BEAM_ELECTRICITY,
            10, BEAM_POISON,
            10, BEAM_NEG,
            10, BEAM_ACID,
            10, BEAM_DAMNATION,
            10, BEAM_STICKY_FLAME,
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
            10, BEAM_AGILITY,
             2, BEAM_ENSNARE);
    }
    while (beam->origin_spell == SPELL_CHAIN_OF_CHAOS
           && (flavour == BEAM_BANISH
               || flavour == BEAM_POLYMORPH));

    return flavour;
}

bool bolt::visible() const
{
    return !is_tracer && glyph != 0 && !is_enchantment();
}

void bolt::initialise_fire()
{
    // Fix some things which the tracer might have set.
    extra_range_used   = 0;
    in_explosion_phase = false;
    use_target_as_pos  = false;
    hit_count.clear();

    if (special_explosion != nullptr)
    {
        ASSERT(!is_explosion);
        ASSERT(special_explosion->is_explosion);
        ASSERT(special_explosion->special_explosion == nullptr);
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
            (source_id == MID_PLAYER ? "player" :
                          monster_by_mid(source_id) ?
                             monster_by_mid(source_id)->name(DESC_PLAIN, true) :
                          "unknown").c_str(),
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

    // The agent may die during the beam's firing, need to save these now.
    // If the beam was reflected, assume it can "see" anything, since neither
    // the reflector nor the original source was particularly aiming for this
    // target. WARNING: if you change this logic, keep in mind that
    // menv[YOU_FAULTLESS] cannot be safely queried for properties like
    // can_see_invisible.
    if (reflections > 0)
        nightvision = can_see_invis = true;
    else
    {
        // XXX: Should non-agents count as seeing invisible?
        nightvision = agent() && agent()->nightvision();
        can_see_invis = agent() && agent()->can_see_invisible();
    }

#ifdef DEBUG_DIAGNOSTICS
    // Not a "real" tracer, merely a range/reachability check.
    if (quiet_debug)
        return;

    dprf(DIAG_BEAM, "%s%s%s [%s] (%d,%d) to (%d,%d): "
          "gl=%d col=%d flav=%d hit=%d dam=%dd%d range=%d",
          (pierce) ? "beam" : "missile",
          (is_explosion) ? "*" :
          (is_big_cloud()) ? "+" : "",
          (is_tracer) ? " tracer" : "",
          name.c_str(),
          source.x, source.y,
          target.x, target.y,
          glyph, colour, flavour,
          hit, damage.num, damage.size,
          range);
#endif
}

// Catch the bolt part of explosive bolt.
static bool _is_explosive_bolt(const bolt *beam)
{
    return beam->origin_spell == SPELL_EXPLOSIVE_BOLT
           && !beam->in_explosion_phase;
}

void bolt::apply_beam_conducts()
{
    if (!is_tracer && YOU_KILL(thrower))
    {
        switch (flavour)
        {
        case BEAM_DAMNATION:
            did_god_conduct(DID_EVIL, 2 + random2(3), god_cares());
            break;
        case BEAM_FIRE:
        case BEAM_STICKY_FLAME:
            did_god_conduct(DID_FIRE,
                            pierce || is_explosion ? 6 + random2(3)
                                                   : 2 + random2(3),
                            god_cares());
            break;
        default:
            // Fire comes from a side-effect of the beam, not the beam itself.
            if (_is_explosive_bolt(this))
                did_god_conduct(DID_FIRE, 6 + random2(3), god_cares());
            break;
        }
    }
}

void bolt::choose_ray()
{
    if ((!chose_ray || reflections > 0)
        && !find_ray(source, target, ray, opc_solid_see)
        // If fire is blocked, at least try a visible path so the
        // error message is better.
        && !find_ray(source, target, ray, opc_default))
    {
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
    put_colour_ch(colour == BLACK ? random_colour(true)
                                  : element_colour(colour),
                  glyph);

    // Get curses to update the screen so we can see the beam.
    update_screen();
#endif
    scaled_delay(draw_delay);
}

// Bounce a bolt off a solid feature.
// The ray is assumed to have just been advanced into
// the feature.
void bolt::bounce()
{
    ASSERT(cell_is_solid(ray.pos()));
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

    do
    {
        ray.regress();
    }
    while (cell_is_solid(ray.pos()));

    extra_range_used += range_used(true);
    bounce_pos = ray.pos();
    bounces++;
    reflect_grid rg;
    for (adjacent_iterator ai(ray.pos(), false); ai; ++ai)
        rg(*ai - ray.pos()) = cell_is_solid(*ai);
    ray.bounce(rg);
    extra_range_used += 2;

    ASSERT(!cell_is_solid(ray.pos()));
}

void bolt::fake_flavour()
{
    if (real_flavour == BEAM_RANDOM)
        flavour = static_cast<beam_type>(random_range(BEAM_FIRE, BEAM_ACID));
    else if (real_flavour == BEAM_CHAOS)
        flavour = _chaos_beam_flavour(this);
    else if (real_flavour == BEAM_CRYSTAL && flavour == BEAM_CRYSTAL)
    {
        flavour = random_choose(BEAM_FIRE, BEAM_COLD);
        hit_verb = (flavour == BEAM_FIRE) ? "burns" :
                   (flavour == BEAM_COLD) ? "freezes"
                                          : "bugs";
    }
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
        destroy_wall(pos());
        if (!msg_generated)
        {
            if (!you.see_cell(pos()))
            {
                if (!silenced(you.pos()))
                {
                    mprf(MSGCH_SOUND, "You hear a grinding noise.");
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

void bolt::burn_wall_effect()
{
    dungeon_feature_type feat = grd(pos());
    // Fire only affects trees.
    if (!feat_is_tree(feat)
        || env.markers.property_at(pos(), MAT_ANY, "veto_fire") == "veto"
        || !can_burn_trees()) // sanity
    {
        finish_beam();
        return;
    }

    // Destroy the wall.
    destroy_wall(pos());
    if (you.see_cell(pos()))
    {
        if (player_in_branch(BRANCH_SWAMP))
            emit_message("The tree smoulders and burns.");
        else
            emit_message("The tree burns like a torch!");
    }
    else if (you.can_smell())
        emit_message("You smell burning wood.");
    if (whose_kill() == KC_YOU)
    {
        did_god_conduct(DID_KILL_PLANT, 1, god_cares());
        did_god_conduct(DID_FIRE, 6, god_cares()); // guaranteed penance
    }
    else if (whose_kill() == KC_FRIENDLY && !crawl_state.game_is_arena())
        did_god_conduct(DID_KILL_PLANT, 1, god_cares());

    // Trees do not burn so readily in a wet environment.
    if (player_in_branch(BRANCH_SWAMP))
        place_cloud(CLOUD_FIRE, pos(), random2(12)+5, agent());
    else
        place_cloud(CLOUD_FOREST_FIRE, pos(), random2(30)+25, agent());
    obvious_effect = true;

    if (_is_explosive_bolt(this))
        _explosive_bolt_explode(this, pos());
    finish_beam();
}

static bool _destroy_wall_msg(dungeon_feature_type feat, const coord_def& p)
{
    string msg = "";
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
        if (see)
        {
            msg = (feature_description_at(p, false, DESC_THE, false)
                   + " explodes into countless fragments.");
        }
        else if (hear)
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
                msg = "The idol screams as its substance crumbles away!";
            else
                msg = "You hear a hideous screaming!";
            chan = MSGCH_SOUND;
        }
        else if (see)
            msg = "The idol twists and shakes as its substance crumbles away!";
        break;

    case DNGN_TREE:
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
        mprf(chan, "%s", msg.c_str());
        return true;
    }
    else
        return false;
}

void bolt::destroy_wall_effect()
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
        destroy_wall(pos());
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    {
        set<coord_def> doors;
        find_connected_identical(pos(), doors);
        for (coord_def c : doors)
            destroy_wall(c);
        break;
    }

    default:
        finish_beam();
        return;
    }

    obvious_effect = _destroy_wall_msg(feat, pos());

    if (feat_is_tree(feat))
    {
        if (whose_kill() == KC_YOU)
            did_god_conduct(DID_KILL_PLANT, 1);
        else if (whose_kill() == KC_FRIENDLY && !crawl_state.game_is_arena())
            did_god_conduct(DID_KILL_PLANT, 1, god_cares(), 0);
    }

    finish_beam();
}

int bolt::range_used(bool leg_only) const
{
    const int leg_length = pos().distance_from(leg_source());
    return leg_only ? leg_length : leg_length + extra_range_used;
}

void bolt::finish_beam()
{
    extra_range_used = BEAM_STOP;
}

void bolt::affect_wall()
{
    if (is_tracer)
    {
        if (!can_affect_wall(pos()))
            finish_beam();

        // potentially warn about offending your god by burning/disinting trees
        const bool burns_trees = can_burn_trees();
        const bool god_relevant = you.religion == GOD_DITHMENOS && burns_trees
                                  || you.religion == GOD_FEDHAS;
        const string veto_key = burns_trees ? "veto_fire" : "veto_disintegrate";
        const bool vetoed = env.markers.property_at(pos(), MAT_ANY, veto_key)
                            == "veto";
        // XXX: should check env knowledge for feat_is_tree()
        if (god_relevant && feat_is_tree(grd(pos())) && !vetoed
            && !is_targeting && YOU_KILL(thrower) && !dont_stop_trees)
        {
            const string prompt =
                make_stringf("Are you sure you want to %s %s?",
                             burns_trees ? "burn" : "destroy",
                             feature_description_at(pos(), false, DESC_THE,
                                                    false).c_str());

            if (yesno(prompt.c_str(), false, 'n'))
                dont_stop_trees = true;
            else
            {
                canned_msg(MSG_OK);
                beam_cancelled = true;
                finish_beam();
            }
        }

        // The only thing that doesn't stop at walls.
        if (flavour != BEAM_DIGGING)
            finish_beam();
        return;
    }
    if (in_bounds(pos()))
    {
        if (flavour == BEAM_DIGGING)
            digging_wall_effect();
        else if (can_burn_trees())
            burn_wall_effect();
        else if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_DEVASTATION)
            destroy_wall_effect();
    }
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
    return (is_explosion && !in_explosion_phase)
           || drop_item
           || cell_is_solid(pos()) && !can_affect_wall(pos())
           || origin_spell == SPELL_PRIMAL_WAVE;
}

void bolt::affect_cell()
{
    // Shooting through clouds affects accuracy.
    if (cloud_at(pos()) && hit != AUTOMATIC_HIT)
        hit = max(hit - 2, 0);

    fake_flavour();

    // Note that this can change the solidity of the wall.
    if (cell_is_solid(pos()))
        affect_wall();

    // If the player can ever walk through walls, this will need
    // special-casing too.
    bool hit_player = found_player();
    if (hit_player && can_affect_actor(&you))
    {
        const int prev_reflections = reflections;
        affect_player();
        if (reflections != prev_reflections)
            return;
        if (hit == AUTOMATIC_HIT && !pierce)
            finish_beam();
    }

    // Stop single target beams from affecting a monster if they already
    // affected the player on this square. -cao
    if (!hit_player || pierce || is_explosion)
    {
        monster *m = monster_at(pos());
        if (m && can_affect_actor(m))
        {
            const bool ignored = ignores_monster(m);
            affect_monster(m);
            if (hit == AUTOMATIC_HIT && !pierce && !ignored
                // Assumes tracers will always have an agent!
                && (!is_tracer || m->visible_to(agent())))
            {
                finish_beam();
            }
        }
    }

    if (!cell_is_solid(pos()))
        affect_ground();
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
        if (special_explosion != nullptr)
            boltcopy.special_explosion = new bolt(*special_explosion);

        do_fire();

        if (special_explosion != nullptr)
        {
            _undo_tracer(*special_explosion, *boltcopy.special_explosion);
            delete boltcopy.special_explosion;
        }

        _undo_tracer(*this, boltcopy);
    }
    else
        do_fire();

    //XXX: suspect, but code relies on path_taken being non-empty
    if (path_taken.empty())
        path_taken.push_back(source);

    if (special_explosion != nullptr)
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

    if (item && !is_tracer && (flavour == BEAM_MISSILE
                               || flavour == BEAM_VISUAL))
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

    // Note: nothing but this loop should be changing the ray.
    while (map_bounds(pos()))
    {
        if (range_used() > range)
        {
            ray.regress();
            extra_range_used++;
            ASSERT(range_used() >= range);
            break;
        }

        const dungeon_feature_type feat = grd(pos());

        if (in_bounds(target)
            // We ran into a solid wall with a real beam...
            && (feat_is_solid(feat)
                && flavour != BEAM_DIGGING && flavour <= BEAM_LAST_REAL
                && !cell_is_solid(target)
            // or visible firewood with a non-penetrating beam...
                || !pierce
                   && monster_at(pos())
                   && you.can_see(*monster_at(pos()))
                   && !ignores_monster(monster_at(pos()))
                   && mons_is_firewood(*monster_at(pos())))
            // and it's a player tracer...
            // (!is_targeting so you don't get prompted while adjusting the aim)
            && is_tracer && !is_targeting && YOU_KILL(thrower)
            // and we're actually between you and the target...
            && !passed_target && pos() != target && pos() != source
            // ?
            && foe_info.count == 0 && bounces == 0 && reflections == 0
            // and you aren't shooting out of LOS.
            && you.see_cell(target))
        {
            // Okay, with all those tests passed, this is probably an instance
            // of the player manually targeting something whose line of fire
            // is blocked, even though its line of sight isn't blocked. Give
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
                    + (feat_is_solid(feat) ?
                        feature_description_at(pos(), false, DESC_A, false) :
                        monster_at(pos())->name(DESC_A));

            prompt += ". Continue anyway?";

            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                beam_cancelled = true;
                finish_beam();
                return;
            }

            // Well, we warned them.
        }

        if (feat_is_solid(feat) && !can_affect_wall(pos()))
        {
            if (is_bouncy(feat))
            {
                bounce();
                // see comment in bounce(); the beam will be cancelled if this
                // is a tracer and showing the bounce would be an info leak.
                // In that case, we have to break early to avoid adding this
                // square to path_taken twice, which would make it look like a
                // a bounce ANYWAY.
                if (range_used() > range)
                    break;
            }
            else
            {
                // Regress for explosions: blow up in an open grid (if regressing
                // makes any sense). Also regress when dropping items.
                if (pos() != source && need_regress())
                {
                    do
                    {
                        ray.regress();
                    }
                    while (ray.pos() != source && cell_is_solid(ray.pos()));

                    // target is where the explosion is centered, so update it.
                    if (is_explosion && !is_tracer)
                        target = ray.pos();
                }
                break;
            }
        }

        path_taken.push_back(pos());

        if (!affects_nothing)
            affect_cell();

        if (range_used() > range)
            break;

        if (beam_cancelled)
            return;

        // Weapons of returning should find an inverse ray
        // through find_ray and setup_retrace, but they didn't
        // always in the past, and we don't want to crash
        // if they accidentally pass through a corner.
        ASSERT(!cell_is_solid(pos())
               || is_tracer && can_affect_wall(pos())
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
        && real_flavour != BEAM_CHAOS
        && YOU_KILL(thrower))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    // Reactions if a monster zapped the beam.
    if (monster_by_mid(source_id))
    {
        if (foe_info.hurt == 0 && friend_info.hurt > 0)
            xom_is_stimulated(100);
        else if (foe_info.helped > 0 && friend_info.helped == 0)
            xom_is_stimulated(100);

        // Allow friendlies to react to projectiles, except when in
        // sanctuary when pet_target can only be explicitly changed by
        // the player.
        const monster* mon = monster_by_mid(source_id);
        if (foe_info.hurt > 0 && !mon->wont_attack() && !crawl_state.game_is_arena()
            && you.pet_target == MHITNOT && env.sanctuary_time <= 0)
        {
            you.pet_target = mon->mindex();
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
    int original = hurted;

    switch (pbolt.flavour)
    {
    case BEAM_FIRE:
    case BEAM_STEAM:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);

        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (original > hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " resists.");
        }
        else if (original < hurted && doFlavouredEffects)
        {
            if (mons->is_icy())
                simple_monster_message(*mons, " melts!");
            else if (mons_species(mons->type) == MONS_BUSH
                     && mons->res_fire() < 0)
            {
                simple_monster_message(*mons, " is on fire!");
            }
            else if (pbolt.flavour == BEAM_FIRE)
                simple_monster_message(*mons, " is burned terribly!");
            else
                simple_monster_message(*mons, " is scalded terribly!");
        }
        break;

    case BEAM_WATER:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (doFlavouredEffects)
        {
            if (!hurted)
                simple_monster_message(*mons, " shrugs off the wave.");
            else if (hurted > original)
                simple_monster_message(*mons, " is doused terribly!");
        }
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (original > hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " resists.");
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " is frozen!");
        }
        break;

    case BEAM_ELECTRICITY:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (original > hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " resists.");
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " is electrocuted!");
        }
        break;

    case BEAM_ACID:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (!hurted)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                       (original > 0) ? " completely resists."
                                                      : " appears unharmed.");
            }
        }
        else if (mons->res_acid() <= 0 && doFlavouredEffects)
            mons->splash_with_acid(pbolt.agent());
        break;
    }

    case BEAM_POISON:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);

        if (!hurted && doFlavouredEffects)
        {
            simple_monster_message(*mons,
                                   (original > 0) ? " completely resists."
                                                  : " appears unharmed.");
        }
        else if (doFlavouredEffects && !one_chance_in(3))
            poison_monster(mons, pbolt.agent());

        break;
    }

    case BEAM_POISON_ARROW:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (hurted < original)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons, " partially resists.");

                poison_monster(mons, pbolt.agent(), 2, true);
            }
        }
        else if (doFlavouredEffects)
            poison_monster(mons, pbolt.agent(), 4, true);

        break;

    case BEAM_NEG:
        if (mons->res_negative_energy() == 3)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");

            hurted = 0;
        }
        else
        {
            hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);

            // Early out if no side effects.
            if (!doFlavouredEffects)
                return hurted;

            if (original > hurted)
                simple_monster_message(*mons, " resists.");
            else if (original < hurted)
                simple_monster_message(*mons, " is drained terribly!");

            if (mons->observable())
                pbolt.obvious_effect = true;

            mons->drain_exp(pbolt.agent());

            if (YOU_KILL(pbolt.thrower))
                did_god_conduct(DID_EVIL, 2, pbolt.god_cares());
        }
        break;

    case BEAM_MIASMA:
        if (mons->res_rotting())
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");

            hurted = 0;
        }
        else
        {
            // Early out for tracer/no side effects.
            if (!doFlavouredEffects)
                return hurted;

            miasma_monster(mons, pbolt.agent());

            if (YOU_KILL(pbolt.thrower))
                did_god_conduct(DID_UNCLEAN, 2, pbolt.god_cares());
        }
        break;

    case BEAM_HOLY:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (doFlavouredEffects && (!hurted || hurted != original))
        {
            simple_monster_message(*mons,
                                       original == 0 ? " appears unharmed." :
                                         hurted == 0 ? " completely resists." :
                                   hurted < original ? " resists." :
                                   hurted > original ? " writhes in agony!" :
                                   " suffers an impossible damage scaling amount!");

        }
        break;
    }

    case BEAM_ICE:
        // ice - 40% of damage is cold, other 60% is impact and
        // can't be resisted (except by AC, of course)
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (hurted < original)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " partially resists.");
        }
        else if (hurted > original)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " is frozen!");
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);

        if (hurted < original)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " partially resists.");
        }
        else if (hurted > original)
        {
            if (mons->is_icy())
            {
                if (doFlavouredEffects)
                    simple_monster_message(*mons, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(*mons, " is burned terribly!");
            }
        }
        break;

    case BEAM_DAMNATION:
        if (mons->res_damnation())
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                       hurted ? " completely resists."
                                              : " appears unharmed.");
            }

            hurted = 0;
        }
        break;

    case BEAM_MEPHITIC:
        if (mons->res_poison() > 0)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons,
                                        hurted ? " completely resists."
                                               : " appears unharmed.");
            }

            hurted = 0;
        }
        break;

    case BEAM_SPORE:
        if (mons->type == MONS_BALLISTOMYCETE)
            hurted = 0;
        break;

    case BEAM_AIR:
        if (mons->res_wind())
            hurted = 0;
        else if (mons->airborne())
            hurted += hurted / 2;
        if (!hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " is harmlessly tossed around.");
        }
        else if (original < hurted)
        {
            if (doFlavouredEffects)
                simple_monster_message(*mons, " gets badly buffeted.");
        }
        break;

    case BEAM_ENSNARE:
        if (doFlavouredEffects)
            ensnare(mons);
        break;

    default:
        break;
    }

    if (doFlavouredEffects)
    {
        const int burn_power = (pbolt.is_explosion) ? 5 :
                               (pbolt.pierce)       ? 3
                                                    : 2;
        mons->expose_to_element(pbolt.flavour, burn_power, false);
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
        if (player_mutation_level(MUT_NO_LOVE))
            return true;

        if (mons->friendly())
            return true;

        if (!(mons->holiness() & MH_UNDEAD))
            return true;

        int res_margin = mons->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (simple_monster_message(*mons,
                    mons->resist_margin_phrase(res_margin).c_str()))
            {
                *did_msg = true;
            }
            return true;
        }
    }
    else if (wh_enchant == ENCH_INSANE
             || mons->holiness() & MH_NATURAL)
    {
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
            if (simple_monster_message(*mons,
                    mons->resist_margin_phrase(res_margin).c_str()))
            {
                *did_msg = true;
            }
            return true;
        }
    }
    // Mass enchantments around lots of plants/fungi shouldn't cause a flood
    // of "is unaffected" messages. --Eino
    else if (mons_is_firewood(*mons))
        return true;
    else  // trying to enchant an unnatural creature doesn't work
    {
        if (simple_monster_message(*mons, " is unaffected."))
            *did_msg = true;
        return true;
    }

    // If monster was affected, then there was a message.
    *did_msg = true;
    return false;
}

// Enchants all monsters in player's sight.
// If m_succumbed is non-nullptr, will be set to the number of monsters that
// were enchanted. If m_attempted is not nullptr, will be set to the number of
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
            || (wh_enchant == ENCH_CHARM && mi->has_ench(ENCH_HEXED))
            || (wh_enchant != ENCH_INSANE
                && mi->add_ench(mon_enchant(wh_enchant, 0, &you))))
        {
            // Do messaging.
            const char* msg;
            switch (wh_enchant)
            {
            case ENCH_FEAR:      msg = " looks frightened!";      break;
            case ENCH_CHARM:     msg = " submits to your will.";  break;
            default:             msg = nullptr;                   break;
            }
            if (msg && simple_monster_message(**mi, msg))
                did_msg = true;

            // Reassert control over hexed undead.
            if (wh_enchant == ENCH_CHARM && mi->has_ench(ENCH_HEXED))
                mi->del_ench(ENCH_HEXED);

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
    if (mons->paralysed() || mons->stasis())
        return;
    // asleep monsters can still be paralysed (and will be always woken by
    // trying to resist); the message might seem wrong but paralysis is
    // always visible.
    if (!mons_is_immotile(*mons)
        && simple_monster_message(*mons, " suddenly stops moving!"))
    {
        mons->stop_constricting_all();
        obvious_effect = true;
    }

    mons->add_ench(mon_enchant(ENCH_PARALYSIS, 0, agent(),
                               _ench_pow_to_dur(ench_power)));
}

// Petrification works in two stages. First the monster is slowed down in
// all of its actions, and when that times out it remains properly petrified
// (no movement or actions). The second part is similar to paralysis,
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
        if (!mons_is_immotile(*mons)
            && simple_monster_message(*mons, " is moving more slowly."))
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

    if (!mons->is_unbreathing())
    {
        hurted = roll_dice(levels, 6);

        if (hurted)
        {
            simple_monster_message(*mons, " convulses.");
            mons->hurt(agent, hurted, BEAM_POISON);
        }
    }

    if (mons->alive())
    {
        if (!mons->cannot_move())
        {
            simple_monster_message(*mons, mons->has_ench(ENCH_SLOW)
                                         ? " seems to be slow for longer."
                                         : " seems to slow down.");
        }
        // FIXME: calculate the slow duration more cleanly
        mon_enchant me(ENCH_SLOW, 0, agent);
        levels -= 2;
        while (levels > 0)
        {
            mon_enchant me2(ENCH_SLOW, 0, agent);
            me.set_duration(mons, &me2);
            levels -= 2;
        }
        mons->add_ench(me);
    }

    return hurted > 0;
}

// Actually poisons a monster (with message).
bool poison_monster(monster* mons, const actor *who, int levels,
                    bool force, bool verbose)
{
    if (!mons->alive() || levels <= 0)
        return false;

    if (monster_resists_this_poison(*mons, force))
        return false;

    const mon_enchant old_pois = mons->get_ench(ENCH_POISON);
    mons->add_ench(mon_enchant(ENCH_POISON, levels, who));
    const mon_enchant new_pois = mons->get_ench(ENCH_POISON);

    // Actually do the poisoning. The order is important here.
    if (new_pois.degree > old_pois.degree
        || new_pois.degree >= MAX_ENCH_DEGREE_DEFAULT)
    {
        if (verbose)
        {
            const char* msg;
            if (new_pois.degree >= MAX_ENCH_DEGREE_DEFAULT)
                msg = " looks as sick as possible!";
            else if (old_pois.degree > 0)
                msg = " looks even sicker.";
            else
                msg = " is poisoned.";

            simple_monster_message(*mons, msg);
        }
    }

    return new_pois.duration > old_pois.duration;
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
        did_god_conduct(DID_EVIL, 5 + random2(3));
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

    if (mons->res_sticky_flame() || levels <= 0 || mons->has_ench(ENCH_WATER_HOLD))
        return false;

    const mon_enchant old_flame = mons->get_ench(ENCH_STICKY_FLAME);
    mons->add_ench(mon_enchant(ENCH_STICKY_FLAME, levels, who));
    const mon_enchant new_flame = mons->get_ench(ENCH_STICKY_FLAME);

    // Actually do the napalming. The order is important here.
    if (new_flame.degree > old_flame.degree)
    {
        if (verbose)
            simple_monster_message(*mons, " is covered in liquid flames!");
        ASSERT(who);
        behaviour_event(mons, ME_WHACK, who);
    }

    return new_flame.degree > old_flame.degree;
}

static bool _curare_hits_player(actor* agent, int levels, string name,
                                string source_name)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_poison() >= 3
        || player_res_poison() > 0 && !one_chance_in(3))
    {
        return false;
    }

    poison_player(roll_dice(levels, 12) + 1, source_name, name);

    int hurted = 0;

    if (!you.is_unbreathing())
    {
        hurted = roll_dice(levels, 6);

        if (hurted)
        {
            mpr("You have difficulty breathing.");
            ouch(hurted, KILLED_BY_CURARE, agent->mid,
                 "curare-induced apnoea");
        }
    }

    slow_player(10 + random2(levels + random2(3 * levels)));

    return hurted > 0;
}


bool curare_actor(actor* source, actor* target, int levels, string name,
                  string source_name)
{
    if (target->is_player())
        return _curare_hits_player(source, levels, name, source_name);
    else
        return _curare_hits_monster(source, target->as_monster(), levels);
}

// XXX: This is a terrible place for this, but it at least does go with
// curare_actor().
int silver_damages_victim(actor* victim, int damage, string &dmg_msg)
{
    int ret = 0;
    if (victim->how_chaotic()
        || victim->is_player() && player_is_shapechanged())
    {
        ret = damage * 3 / 4;
    }
    else if (victim->is_player())
    {
        // For mutation damage, we want to count innate mutations for
        // demonspawn but not other species.
        int multiplier = 5 * how_mutated(you.species == SP_DEMONSPAWN, true);
        if (multiplier == 0)
            return 0;

        if (multiplier > 75)
            multiplier = 75;

        ret = damage * multiplier / 100;
    }
    else
        return 0;

    dmg_msg = "The silver sears " + victim->name(DESC_THE) + "!";
    return ret;
}

//  Used by monsters in "planning" which spell to cast. Fires off a "tracer"
//  which tells the monster what it'll hit if it breathes/casts etc.
//
//  The output from this tracer function is written into the
//  tracer_info variables (friend_info and foe_info).
//
//  Note that beam properties must be set, as the tracer will take them
//  into account, as well as the monster's intelligence.
void fire_tracer(const monster* mons, bolt &pbolt, bool explode_only,
                 bool explosion_hole)
{
    // Don't fiddle with any input parameters other than tracer stuff!
    pbolt.is_tracer     = true;
    pbolt.source        = mons->pos();
    pbolt.source_id     = mons->mid;
    pbolt.attitude      = mons_attitude(*mons);

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

        if (mons_is_hepliaklqana_ancestor(mons->type))
            pbolt.foe_ratio = 100; // do not harm the player!
        // Foe ratio for summoning greater demons & undead -- they may be
        // summoned, but they're hostile and would love nothing better
        // than to nuke the player and his minions.
        else if (mons_att_wont_attack(pbolt.attitude)
            && !mons_att_wont_attack(mons->attitude))
        {
            pbolt.foe_ratio = 25;
        }
    }

    pbolt.in_explosion_phase = false;

    // Fire!
    if (explode_only)
        pbolt.explode(false, explosion_hole);
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

void create_feat_splash(coord_def center,
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
    if (find(begin(path_taken), end(path_taken), target) != end(path_taken))
        return false;

    // Don't go far away from the caster (not enough momentum).
    if (grid_distance(origin, center + (target - center)*2)
        > you.current_vision)
    {
        return false;
    }

    return true;
}

void bolt_parent_init(const bolt &parent, bolt &child)
{
    child.name           = parent.name;
    child.short_name     = parent.short_name;
    child.aux_source     = parent.aux_source;
    child.source_id      = parent.source_id;
    child.origin_spell   = parent.origin_spell;
    child.glyph          = parent.glyph;
    child.colour         = parent.colour;

    child.flavour        = parent.flavour;
    child.origin_spell   = parent.origin_spell;

    // We don't copy target since that is often overriden.
    child.thrower        = parent.thrower;
    child.source         = parent.source;
    child.source_name    = parent.source_name;
    child.attitude       = parent.attitude;

    child.pierce         = parent.pierce ;
    child.is_explosion   = parent.is_explosion;
    child.ex_size        = parent.ex_size;
    child.foe_ratio      = parent.foe_ratio;

    child.is_tracer      = parent.is_tracer;
    child.is_targeting   = parent.is_targeting;

    child.range          = parent.range;
    child.hit            = parent.hit;
    child.damage         = parent.damage;
    if (parent.ench_power != -1)
        child.ench_power = parent.ench_power;

    child.friend_info.dont_stop = parent.friend_info.dont_stop;
    child.foe_info.dont_stop    = parent.foe_info.dont_stop;
    child.dont_stop_player      = parent.dont_stop_player;
    child.dont_stop_trees       = parent.dont_stop_trees;

#ifdef DEBUG_DIAGNOSTICS
    child.quiet_debug    = parent.quiet_debug;
#endif
}

static void _maybe_imb_explosion(bolt *parent, coord_def center)
{
    if (parent->origin_spell != SPELL_ISKENDERUNS_MYSTIC_BLAST
        || parent->in_explosion_phase)
    {
        return;
    }
    const int dist = grid_distance(parent->source, center);
    if (dist == 0 || (!parent->is_tracer && !x_chance_in_y(3, 2 + 2 * dist)))
        return;
    bolt beam;

    bolt_parent_init(*parent, beam);
    beam.name           = "mystic blast";
    beam.aux_source     = "orb of energy";
    beam.range          = 3;
    beam.hit            = AUTOMATIC_HIT;
    beam.colour         = MAGENTA;
    beam.obvious_effect = true;
    beam.pierce         = false;
    beam.is_explosion   = false;
    // So as not to recur infinitely
    beam.origin_spell = SPELL_NO_SPELL;
    beam.passed_target  = true; // The centre was the target.
    beam.aimed_at_spot  = true;
    if (you.see_cell(center))
        beam.seen = true;
    beam.source         = center;

    bool first = true;
    for (adjacent_iterator ai(center); ai; ++ai)
    {
        if (!imb_can_splash(parent->source, center, parent->path_taken, *ai))
            continue;
        if (!beam.is_tracer && one_chance_in(4))
            continue;

        if (first && !beam.is_tracer)
        {
            if (you.see_cell(center))
                mpr("The orb of energy explodes!");
            noisy(spell_effect_noise(SPELL_ISKENDERUNS_MYSTIC_BLAST),
                  center);
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
        if (beam.is_tracer && beam.beam_cancelled)
        {
            parent->beam_cancelled = true;
            return;
        }
    }
}

static void _malign_offering_effect(actor* victim, const actor* agent, int damage)
{
    if (!agent || damage < 1)
        return;

    // The victim may die.
    coord_def c = victim->pos();

    mprf("%s life force is offered up.", victim->name(DESC_ITS).c_str());
    damage = victim->hurt(agent, damage, BEAM_MALIGN_OFFERING, KILLED_BY_BEAM,
                          "", "by a malign offering");

    // Actors that had LOS to the victim (blocked by glass, clouds, etc),
    // even if they couldn't actually see each other because of blindness
    // or invisibility.
    for (actor_near_iterator ai(c, LOS_NO_TRANS); ai; ++ai)
    {
        if (mons_aligned(agent, *ai) && !(ai->holiness() & MH_NONLIVING)
            && *ai != victim)
        {
            if (ai->heal(max(1, damage * 2 / 3)) && you.can_see(**ai))
            {
                mprf("%s %s healed.", ai->name(DESC_THE).c_str(),
                                      ai->conj_verb("are").c_str());
            }
        }
    }
}

static void _explosive_bolt_explode(bolt *parent, coord_def pos)
{
    bolt beam;

    bolt_parent_init(*parent, beam);
    beam.name         = "fiery explosion";
    beam.aux_source   = "explosive bolt";
    beam.damage       = dice_def(3, 9 + parent->ench_power / 6);
    beam.colour       = RED;
    beam.flavour      = BEAM_FIRE;
    beam.is_explosion = true;
    beam.source       = pos;
    beam.target       = pos;
    beam.origin_spell = SPELL_EXPLOSIVE_BOLT;
    beam.refine_for_explosion();
    beam.explode();
    parent->friend_info += beam.friend_info;
    parent->foe_info    += beam.foe_info;
    if (beam.is_tracer && beam.beam_cancelled)
        parent->beam_cancelled = true;
}

/**
 * Turn a BEAM_UNRAVELLING beam into a BEAM_UNRAVELLED_MAGIC beam, and make
 * it explode appropriately.
 *
 * @param[in,out] beam      The beam being turned into an explosion.
 */
static void _unravelling_explode(bolt &beam)
{
    beam.damage       = dice_def(3, 3 + div_rand_round(beam.ench_power, 6));
    beam.colour       = ETC_MUTAGENIC;
    beam.flavour      = BEAM_UNRAVELLED_MAGIC;
    beam.ex_size      = 1;
    beam.is_explosion = true;
    // and it'll explode 'naturally' a little later.
}

bool bolt::is_bouncy(dungeon_feature_type feat) const
{
    if (real_flavour == BEAM_CHAOS
        && feat_is_solid(feat))
    {
        return true;
    }

    if ((flavour == BEAM_CRYSTAL || real_flavour == BEAM_CRYSTAL
         || flavour == BEAM_BOUNCY_TRACER)
        && feat_is_solid(feat)
        && !feat_is_tree(feat))
    {
        return true;
    }

    if (is_enchantment())
        return false;

    if (flavour == BEAM_ELECTRICITY && !feat_is_metal(feat)
        && !feat_is_tree(feat))
    {
        return true;
    }

    if ((flavour == BEAM_FIRE || flavour == BEAM_COLD)
        && feat == DNGN_CRYSTAL_WALL)
    {
        return true;
    }

    return false;
}

cloud_type bolt::get_cloud_type() const
{
    if (origin_spell == SPELL_NOXIOUS_CLOUD)
        return CLOUD_MEPHITIC;

    if (origin_spell == SPELL_POISONOUS_CLOUD)
        return CLOUD_POISON;

    if (origin_spell == SPELL_HOLY_BREATH)
        return CLOUD_HOLY;

    if (origin_spell == SPELL_FLAMING_CLOUD)
        return CLOUD_FIRE;

    if (origin_spell == SPELL_CHAOS_BREATH)
        return CLOUD_CHAOS;

    if (origin_spell == SPELL_MIASMA_BREATH)
        return CLOUD_MIASMA;

    if (origin_spell == SPELL_FREEZING_CLOUD)
        return CLOUD_COLD;

    if (origin_spell == SPELL_SPECTRAL_CLOUD)
        return CLOUD_SPECTRAL;

    return CLOUD_NONE;
}

int bolt::get_cloud_pow() const
{
    if (origin_spell == SPELL_FREEZING_CLOUD
        || origin_spell == SPELL_POISONOUS_CLOUD)
    {
        return random_range(10, 15);
    }

    if (origin_spell == SPELL_SPECTRAL_CLOUD)
        return random_range(12, 20);

    return 0;
}

int bolt::get_cloud_size(bool min, bool max) const
{
    if (origin_spell == SPELL_MEPHITIC_CLOUD
        || origin_spell == SPELL_MIASMA_BREATH
        || origin_spell == SPELL_FREEZING_CLOUD)
    {
        return 10;
    }

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
        special_explosion->target = pos();
        special_explosion->refine_for_explosion();
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
        target = pos();
        refine_for_explosion();
        explode();
        return;
    }

    _maybe_imb_explosion(this, pos());

    const cloud_type cloud = get_cloud_type();

    if (is_tracer)
    {
        if (cloud == CLOUD_NONE)
            return;

        targetter_cloud tgt(agent(), range, get_cloud_size(true),
                            get_cloud_size(false, true));
        tgt.set_aim(pos());
        for (const auto &entry : tgt.seen)
        {
            if (entry.second != AFF_YES && entry.second != AFF_MAYBE)
                continue;

            if (entry.first == you.pos())
                tracer_affect_player();
            else if (monster* mon = monster_at(entry.first))
                tracer_affect_monster(mon);

            if (agent()->is_player() && beam_cancelled)
                return;
        }

        return;
    }

    if (!is_explosion && !noise_generated && loudness)
    {
        // Digging can target squares on the map boundary, though it
        // won't remove them of course.
        const coord_def noise_position = clamp_in_bounds(pos());
        noisy(loudness, noise_position, source_id);
        noise_generated = true;
    }

    if (cloud != CLOUD_NONE)
        big_cloud(cloud, agent(), pos(), get_cloud_pow(), get_cloud_size());

    // you like special cases, right?
    switch (origin_spell)
    {
    case SPELL_PRIMAL_WAVE:
        if (you.see_cell(pos()))
        {
            mpr("The wave splashes down.");
            noisy(spell_effect_noise(SPELL_PRIMAL_WAVE), pos());
        }
        else
        {
            noisy(spell_effect_noise(SPELL_PRIMAL_WAVE),
                  pos(), "You hear a splash.");
        }
        create_feat_splash(pos(), 2, random_range(3, 12, 2));
        break;

    case SPELL_BLINKBOLT:
    {
        actor *act = agent(true); // use orig actor even when reflected
        if (!act || !act->alive())
            return;

        for (vector<coord_def>::reverse_iterator citr = path_taken.rbegin();
             citr != path_taken.rend(); ++citr)
        {
            if (act->is_habitable(*citr) && act->blink_to(*citr, false))
                return;
        }
        return;
    }

    case SPELL_SEARING_BREATH:
        if (!path_taken.empty())
            place_cloud(CLOUD_FIRE, pos(), 5 + random2(5), agent());

    default:
        break;
    }
}

bool bolt::stop_at_target() const
{
    return is_explosion || is_big_cloud() || aimed_at_spot;
}

void bolt::drop_object()
{
    ASSERT(item != nullptr);
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
                 summoned_poof_msg(agent() ? agent()->as_monster() : nullptr,
                                   *item).c_str());
        }
        item_was_destroyed(*item);
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
                    set_net_stationary(*item);
            }
        }

        copy_item_to_grid(*item, pos(), 1);
    }
    else
        item_was_destroyed(*item);
}

// Returns true if the beam hits the player, fuzzing the beam if necessary
// for monsters without see invis firing tracers at the player.
bool bolt::found_player() const
{
    const bool needs_fuzz = (is_tracer && !can_see_invis
                             && you.invisible() && !YOU_KILL(thrower));
    const int dist = needs_fuzz? 2 : 0;

    return grid_distance(pos(), you.pos()) <= dist;
}

void bolt::affect_ground()
{
    // Explosions only have an effect during their explosion phase.
    // Special cases can be handled here.
    if (is_explosion && !in_explosion_phase)
        return;

    if (is_tracer)
        return;

    // Spore explosions might spawn a fungus. The spore explosion
    // covers 21 tiles in open space, so the expected number of spores
    // produced is the x in x_chance_in_y() in the conditional below.
    if (is_explosion && flavour == BEAM_SPORE
        && agent() && !agent()->is_summoned())
    {
        if (env.grid(pos()) == DNGN_FLOOR)
            env.pgrid(pos()) |= FPROP_MOLD;

        if (x_chance_in_y(2, 21)
           && mons_class_can_pass(MONS_BALLISTOMYCETE, env.grid(pos()))
           && !actor_at(pos()))
        {
            beh_type beh = attitude_creation_behavior(attitude);
            // A friendly spore or hyperactive can exist only with Fedhas
            // in which case the inactive ballistos spawned should be
            // good_neutral to avoid hidden piety costs of Fedhas abilities
            if (beh == BEH_FRIENDLY)
                beh = BEH_GOOD_NEUTRAL;

            const god_type god = agent() ? agent()->deity() : GOD_NO_GOD;

            if (create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                         beh,
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

    affect_place_clouds();
}

bool bolt::is_fiery() const
{
    return flavour == BEAM_FIRE || flavour == BEAM_LAVA;
}

/// Can this bolt burn trees it hits?
bool bolt::can_burn_trees() const
{
    // XXX: rethink this
    return origin_spell == SPELL_LIGHTNING_BOLT
           || origin_spell == SPELL_BOLT_OF_FIRE
           || origin_spell == SPELL_BOLT_OF_MAGMA
           || origin_spell == SPELL_FIREBALL
           || origin_spell == SPELL_EXPLOSIVE_BOLT
           || origin_spell == SPELL_INNER_FLAME;
}

bool bolt::can_affect_wall(const coord_def& p) const
{
    dungeon_feature_type wall = grd(p);

    // Temporary trees (from Summon Forest) can't be burned/distintegrated.
    if (feat_is_tree(wall) && is_temp_terrain(p))
        return false;

    // digging
    if (flavour == BEAM_DIGGING
        && (wall == DNGN_ROCK_WALL || wall == DNGN_CLEAR_ROCK_WALL
            || wall == DNGN_SLIMY_WALL || wall == DNGN_GRATE))
    {
        return true;
    }

    if (can_burn_trees())
        return feat_is_tree(wall);

    if (flavour == BEAM_DISINTEGRATION && damage.num >= 3
        || flavour == BEAM_DEVASTATION)
    {
        return wall == DNGN_ROCK_WALL
               || wall == DNGN_SLIMY_WALL
               || wall == DNGN_CLEAR_ROCK_WALL
               || wall == DNGN_GRATE
               || wall == DNGN_CLOSED_DOOR
               || wall == DNGN_RUNED_DOOR
               || feat_is_statuelike(wall)
               || feat_is_tree(wall);
    }

    // Lee's Rapid Deconstruction
    if (flavour == BEAM_FRAG)
        return true; // smite targeting, we don't care

    return false;
}

void bolt::affect_place_clouds()
{
    if (in_explosion_phase)
        affect_place_explosion_clouds();

    const coord_def p = pos();

    // Is there already a cloud here?
    if (cloud_struct* cloud = cloud_at(p))
    {
        // fire cancelling cold & vice versa
        if ((cloud->type == CLOUD_COLD
             && (flavour == BEAM_FIRE || flavour == BEAM_LAVA))
            || (cloud->type == CLOUD_FIRE && flavour == BEAM_COLD))
        {
            if (player_can_hear(p))
                mprf(MSGCH_SOUND, "You hear a sizzling sound!");

            delete_cloud(p);
            extra_range_used += 5;
        }
        return;
    }

    // No clouds here, free to make new ones.
    const dungeon_feature_type feat = grd(p);

    if (origin_spell == SPELL_POISONOUS_CLOUD)
        place_cloud(CLOUD_POISON, p, random2(5) + 3, agent());

    if (origin_spell == SPELL_HOLY_BREATH)
        place_cloud(CLOUD_HOLY, p, random2(4) + 2, agent());

    if (origin_spell == SPELL_FLAMING_CLOUD)
        place_cloud(CLOUD_FIRE, p, random2(4) + 2, agent());

    // Fire/cold over water/lava
    if (feat == DNGN_LAVA && flavour == BEAM_COLD
        || feat_is_watery(feat) && is_fiery())
    {
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), agent(), 11);
    }

    if (feat_is_watery(feat) && flavour == BEAM_COLD
        && damage.num * damage.size > 35)
    {
        place_cloud(CLOUD_COLD, p, damage.num * damage.size / 30 + 1, agent());
    }

    if (flavour == BEAM_MIASMA)
        place_cloud(CLOUD_MIASMA, p, random2(5) + 2, agent());

    //XXX: these use the name for a gameplay effect.
    if (name == "ball of steam")
        place_cloud(CLOUD_STEAM, p, random2(5) + 2, agent());

    if (name == "poison gas")
        place_cloud(CLOUD_POISON, p, random2(4) + 3, agent());

    if (name == "blast of choking fumes")
        place_cloud(CLOUD_MEPHITIC, p, random2(4) + 3, agent());

    if (name == "trail of fire")
        place_cloud(CLOUD_FIRE, p, random2(ench_power) + ench_power, agent());

    if (origin_spell == SPELL_PETRIFYING_CLOUD)
        place_cloud(CLOUD_PETRIFY, p, random2(4) + 4, agent());

    if (origin_spell == SPELL_SPECTRAL_CLOUD)
        place_cloud(CLOUD_SPECTRAL, p, random2(6) + 5, agent());

    if (origin_spell == SPELL_DEATH_RATTLE)
        place_cloud(CLOUD_MIASMA, p, random2(4) + 4, agent());
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

    if (origin_spell == SPELL_FIRE_STORM)
    {
        place_cloud(CLOUD_FIRE, p, 2 + random2avg(5,2), agent());

        // XXX: affect other open spaces?
        if (grd(p) == DNGN_FLOOR && !monster_at(p) && one_chance_in(4))
        {
            const god_type god =
                (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                              : GOD_NO_GOD;
            const beh_type att =
                (whose_kill() == KC_OTHER ? BEH_HOSTILE : BEH_FRIENDLY);

            actor* summ = agent();
            mgen_data mg(MONS_FIRE_VORTEX, att, p, MHITNOT, MG_NONE, god);
            mg.set_summoned(summ, 1, SPELL_FIRE_STORM);

            // Spell-summoned monsters need to have a live summoner.
            if (summ == nullptr || !summ->alive())
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
    monster* monst = nullptr;
    monst = monster_by_mid(source_id);

    const char *what = aux_source.empty() ? name.c_str() : aux_source.c_str();

    if (YOU_KILL(thrower) && you.duration[DUR_QUAD_DAMAGE])
        dam *= 4;

    // The order of this is important.
    if (monst && monst->type == MONS_PLAYER_SHADOW
        && !monst->mname.empty())
    {
        ouch(dam, KILLED_BY_DIVINE_WRATH, MID_NOBODY,
             aux_source.empty() ? nullptr : aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
    else if (monst && (monst->type == MONS_BALLISTOMYCETE_SPORE
                       || monst->type == MONS_BALL_LIGHTNING
                       || monst->type == MONS_HYPERACTIVE_BALLISTOMYCETE
                       || monst->type == MONS_FULMINANT_PRISM
                       || monst->type == MONS_BENNU // death flames
                       ))
    {
        ouch(dam, KILLED_BY_SPORE, source_id,
             aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
    else if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_DEVASTATION)
    {
        ouch(dam, KILLED_BY_DISINT, source_id, what, true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
    else if (YOU_KILL(thrower) && aux_source.empty())
    {
        if (reflections > 0)
            ouch(dam, KILLED_BY_REFLECTION, reflector, name.c_str());
        else if (bounces > 0)
            ouch(dam, KILLED_BY_BOUNCE, MID_PLAYER, name.c_str());
        else
        {
            if (aimed_at_feet && effect_known)
                ouch(dam, KILLED_BY_SELF_AIMED, MID_PLAYER, name.c_str());
            else
                ouch(dam, KILLED_BY_TARGETING, MID_PLAYER, name.c_str());
        }
    }
    else if (MON_KILL(thrower) || aux_source == "exploding inner flame")
        ouch(dam, KILLED_BY_BEAM, source_id,
             aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    else // KILL_MISC || (YOU_KILL && aux_source)
        ouch(dam, KILLED_BY_WILD_MAGIC, source_id, aux_source.c_str());
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
static bool _test_beam_hit(int attack, int defence, bool pierce,
                           int defl, defer_rand &r)
{
    if (attack == AUTOMATIC_HIT)
        return true;

    if (pierce)
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

    return attack >= defence;
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
        return mon->res_holy_energy() >= 3;

    case BEAM_STEAM:
        return mon->res_steam() >= 3;

    case BEAM_FIRE:
        return mon->res_fire() >= 3;

    case BEAM_COLD:
        return mon->res_cold() >= 3;

    case BEAM_MIASMA:
        return mon->res_rotting();

    case BEAM_NEG:
        return mon->res_negative_energy() == 3;

    case BEAM_ELECTRICITY:
        return mon->res_elec() >= 3;

    case BEAM_POISON:
        return mon->res_poison() >= 3;

    case BEAM_ACID:
        return mon->res_acid() >= 3;

    case BEAM_PETRIFY:
        return mon->res_petrify() || mon->petrified();

    case BEAM_MEPHITIC:
        return mon->res_poison() > 0 || mon->is_unbreathing();

    default:
        return false;
    }
}

// N.b. only called for player-originated beams; if that is changed,
// be sure to adjust various assumptions based on the spells/abilities
// available to the player.
bool bolt::harmless_to_player() const
{
    dprf(DIAG_BEAM, "beam flavour: %d", flavour);

    if (have_passive(passive_t::cloud_immunity) && is_big_cloud())
        return true;

    switch (flavour)
    {
    case BEAM_VISUAL:
    case BEAM_DIGGING:
        return true;

    // Positive enchantments.
    case BEAM_HASTE:
    case BEAM_HEALING:
    case BEAM_MIGHT:
    case BEAM_AGILITY:
    case BEAM_INVISIBILITY:
    case BEAM_RESISTANCE:
        return true;

    case BEAM_HOLY:
        return you.res_holy_energy() >= 3;

    case BEAM_MIASMA:
        return you.res_rotting();

    case BEAM_NEG:
        return player_prot_life(false) >= 3;

    case BEAM_POISON:
        return player_res_poison(false) >= 3
               || is_big_cloud() && player_res_poison(false) > 0;

    case BEAM_MEPHITIC:
        // With clarity, meph still does a tiny amount of damage (1d3 - 1).
        // Normally we'd just ignore it, but we shouldn't let a player
        // kill themselves without a warning.
        return player_res_poison(false) > 0 || you.is_unbreathing()
            || you.clarity(false) && you.hp > 2;

    case BEAM_ELECTRICITY:
        return player_res_electricity(false);

    case BEAM_PETRIFY:
        return you.res_petrify() || you.petrified();

    case BEAM_COLD:
        return is_big_cloud() && you.mutation[MUT_FREEZING_CLOUD_IMMUNITY];

#if TAG_MAJOR_VERSION == 34
    case BEAM_FIRE:
    case BEAM_STICKY_FLAME:
        return you.species == SP_DJINNI;
#endif

    case BEAM_VIRULENCE:
        return player_res_poison(false) >= 3;

    default:
        return false;
    }
}

bool bolt::is_reflectable(const actor &whom) const
{
    if (range_used() > range)
        return false;

    const item_def *it = whom.shield();
    return (it && is_shield(*it) && shield_reflects(*it)) || whom.reflection();
}

bool bolt::is_big_cloud() const
{
    return get_spell_flags(origin_spell) & SPFLAG_CLOUD;
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
    {
        reflector = MID_PLAYER;
        count_action(CACT_BLOCK, -1, BLOCK_REFLECT);
    }
    else if (monster* m = monster_at(pos()))
        reflector = m->mid;
    else
    {
        reflector = MID_NOBODY;
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
    if (flavour == BEAM_UNRAVELLING && player_is_debuffable())
        is_explosion = true;

    // Check whether thrower can see player, unless thrower == player.
    if (YOU_KILL(thrower))
    {
        if (!dont_stop_player && !harmless_to_player())
        {
            string prompt = make_stringf("That %s is likely to hit you. Continue anyway?",
                                         item ? name.c_str() : "beam");

            if (yesno(prompt.c_str(), false, 'n'))
            {
                friend_info.count++;
                friend_info.power += you.experience_level;
                // Don't ask about aiming at ourself twice.
                dont_stop_player = true;
            }
            else
            {
                canned_msg(MSG_OK);
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

    extra_range_used += range_used_on_hit();

    if (_is_explosive_bolt(this))
        _explosive_bolt_explode(this, you.pos());
}

bool bolt::misses_player()
{
    if (flavour == BEAM_VISUAL)
        return true;

    const bool engulfs = is_explosion || is_big_cloud();

    if (is_explosion || aimed_at_feet || auto_hit)
    {
        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";
        if (flavour != BEAM_VISUAL && !is_enchantment())
            mprf("The %s %s you!", name.c_str(), hit_verb.c_str());
        return false;
    }

    const int dodge = you.evasion();
    int real_tohit  = hit;

    if (real_tohit != AUTOMATIC_HIT)
    {
        // Monsters shooting at an invisible player are very inaccurate.
        if (you.invisible() && !can_see_invis)
            real_tohit /= 2;

        // Backlit is easier to hit:
        if (you.backlit(false))
            real_tohit += 2 + random2(8);

        // Umbra is harder to hit:
        if (!nightvision && you.umbra())
            real_tohit -= 2 + random2(4);
    }

    const int SH = player_shield_class();
    if ((player_omnireflects() && is_omnireflectable()
         || is_blockable())
        && you.shielded()
        && !aimed_at_feet
        && SH > 0)
    {
        // We use the original to-hit here.
        // (so that effects increasing dodge chance don't increase block...?)
        const int testhit = random2(hit * 130 / 100
                                    + you.shield_block_penalty());

        const int block = you.shield_bonus();

        // 50% chance of blocking ench-type effects at 20 displayed sh
        const bool omnireflected
            = hit == AUTOMATIC_HIT
              && x_chance_in_y(SH, omnireflect_chance_denom(SH));

        dprf(DIAG_BEAM, "Beamshield: hit: %d, block %d", testhit, block);
        if ((testhit < block && hit != AUTOMATIC_HIT) || omnireflected)
        {
            bool penet = false;

            const string refl_name = name.empty() && origin_spell ?
                                     spell_title(origin_spell) :
                                     name;

            const item_def *shield = you.shield();
            if (is_reflectable(you))
            {
                if (shield && is_shield(*shield) && shield_reflects(*shield))
                {
                    mprf("Your %s reflects the %s!",
                            shield->name(DESC_PLAIN).c_str(),
                            refl_name.c_str());
                }
                else
                {
                    mprf("The %s reflects off an invisible shield around you!",
                            refl_name.c_str());
                }
                reflect();
            }
            else
            {
                mprf("You block the %s.", name.c_str());
                finish_beam();
            }
            you.shield_block_succeeded(agent());
            if (!penet)
                return true;
        }

        // Some training just for the "attempt".
        practise_shield_block(false);
    }

    if (is_enchantment())
        return false;

    if (!aimed_at_feet)
        practise_being_shot_at();

    defer_rand r;
    bool miss = true;

    int defl = you.missile_deflection();

    if (!_test_beam_hit(real_tohit, dodge, pierce, 0, r))
    {
        mprf("The %s misses you.", name.c_str());
        count_action(CACT_DODGE, DODGE_EVASION);
    }
    else if (defl && !_test_beam_hit(real_tohit, dodge, pierce, defl, r))
    {
        // active voice to imply stronger effect
        mprf(defl == 1 ? "The %s is repelled." : "You deflect the %s!",
             name.c_str());
        you.ablate_deflection();
        count_action(CACT_DODGE, DODGE_DEFLECT);
    }
    else
    {
        int dodge_more = you.evasion(EV_IGNORE_HELPLESS);

        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";

        if (_test_beam_hit(real_tohit, dodge_more, pierce, defl, r))
            mprf("The %s %s you!", name.c_str(), hit_verb.c_str());
        else
            mprf("Helpless, you fail to dodge the %s.", name.c_str());

        miss = false;
    }

    return miss;
}

void bolt::affect_player_enchantment(bool resistible)
{
    if (resistible
        && has_saving_throw()
        && you.check_res_magic(ench_power) > 0)
    {
        // You resisted it.

        // Give a message.
        bool need_msg = true;
        if (thrower != KILL_YOU_MISSILE)
        {
            const monster* mon = monster_by_mid(source_id);
            if (mon && !mon->observable())
            {
                mprf("Something tries to affect you, but you %s.",
                     you.res_magic() == MAG_IMMUNE ? "are unaffected"
                                                   : "resist");
                need_msg = false;
            }
        }
        if (need_msg)
        {
            if (you.res_magic() == MAG_IMMUNE)
                canned_msg(MSG_YOU_UNAFFECTED);
            else
            {
                // the message reflects the level of difficulty resisting.
                const int margin = you.res_magic() - ench_power;
                mprf("You%s", you.resist_margin_phrase(margin).c_str());
            }
        }
        // You *could* have gotten a free teleportation in the Abyss,
        // but no, you resisted.
        if (flavour == BEAM_TELEPORT && player_in_branch(BRANCH_ABYSS))
            xom_is_stimulated(200);

        extra_range_used += range_used_on_hit();
        return;
    }

    // Never affects the player.
    if (flavour == BEAM_INFESTATION)
        return;

    // You didn't resist it.
    if (animate)
        _ench_animation(effect_known ? real_flavour : BEAM_MAGIC);

    bool nasty = true, nice = false;

    const bool blame_player = god_cares() && YOU_KILL(thrower);

    switch (flavour)
    {
    case BEAM_HIBERNATION:
    case BEAM_SLEEP:
        you.put_to_sleep(nullptr, ench_power, flavour == BEAM_HIBERNATION);
        break;

    case BEAM_CORONA:
        you.backlight();
        obvious_effect = true;
        break;

    case BEAM_POLYMORPH:
        obvious_effect = you.polymorph(ench_power);
        break;

    case BEAM_MALMUTATE:
    case BEAM_UNRAVELLED_MAGIC:
        mpr("Strange energies course through your body.");
        you.malmutate(aux_source.empty() ? get_source_name() :
                      (get_source_name() + "/" + aux_source));
        obvious_effect = true;
        break;

    case BEAM_SLOW:
        slow_player(10 + random2(ench_power));
        obvious_effect = true;
        break;

    case BEAM_HASTE:
        haste_player(40 + random2(ench_power));
        did_god_conduct(DID_HASTY, 10, blame_player);
        contaminate_player(750 + random2(500), blame_player);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_HEALING:
        potionlike_effect(POT_HEAL_WOUNDS, ench_power, true);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_MIGHT:
        potionlike_effect(POT_MIGHT, ench_power);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_INVISIBILITY:
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 1;
        potionlike_effect(POT_INVISIBILITY, ench_power);
        contaminate_player(1000 + random2(1000), blame_player);
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
        confuse_player(5 + random2(3));
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
        uncontrolled_blink();
        obvious_effect = true;
        break;

    case BEAM_BLINK_CLOSE:
        blink_other_close(&you, source);
        obvious_effect = true;
        break;

    case BEAM_BECKONING:
        obvious_effect = beckon(you, *this);
        break;

    case BEAM_ENSLAVE:
        mprf(MSGCH_WARN, "Your will is overpowered!");
        confuse_player(5 + random2(3));
        obvious_effect = true;
        break;     // enslavement - confusion?

    case BEAM_BANISH:
        if (YOU_KILL(thrower))
        {
            mpr("This spell isn't strong enough to banish yourself.");
            break;
        }
        you.banish(agent(), get_source_name(),
                   agent()->get_experience_level());
        obvious_effect = true;
        break;

    case BEAM_PAIN:
    {
        if (aux_source.empty())
            aux_source = "by nerve-wracking pain";

        const int dam = resist_adjust_damage(&you, flavour, damage.roll());
        if (dam)
        {
            mpr("Pain shoots through your body!");
            internal_ouch(dam);
            obvious_effect = true;
        }
        else
            canned_msg(MSG_YOU_UNAFFECTED);
        break;
    }

    case BEAM_AGONY:
        torment_player(agent(), TORMENT_AGONY);
        obvious_effect = true;
        break;

    case BEAM_DISPEL_UNDEAD:
        if (you.undead_state() == US_ALIVE)
        {
            canned_msg(MSG_YOU_UNAFFECTED);
            break;
        }

        mpr("You convulse!");

        if (aux_source.empty())
            aux_source = "by dispel undead";

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
            mpr("You feel a momentary urge to oink.");
            break;
        }

        you.transform_uncancellable = true;
        obvious_effect = true;
        break;

    case BEAM_BERSERK:
        you.go_berserk(blame_player, true);
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
        const int dam = resist_adjust_damage(&you, flavour, damage.roll());
        if (dam)
        {
            _malign_offering_effect(&you, agent(), dam);
            obvious_effect = true;
        }
        else
            canned_msg(MSG_YOU_UNAFFECTED);
        break;
    }

    case BEAM_VIRULENCE:
        // Those completely immune cannot be made more susceptible this way
        if (you.res_poison(false) >= 3)
        {
            canned_msg(MSG_YOU_UNAFFECTED);
            break;
        }

        mpr("You feel yourself grow more vulnerable to poison.");
        you.increase_duration(DUR_POISON_VULN, 12 + random2(18), 50);
        obvious_effect = true;
        break;

    case BEAM_AGILITY:
        potionlike_effect(POT_AGILITY, ench_power);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_SAP_MAGIC:
        if (!SAP_MAGIC_CHANCE())
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            break;
        }
        mprf(MSGCH_WARN, "Your magic feels %stainted.",
             you.duration[DUR_SAP_MAGIC] ? "more " : "");
        you.increase_duration(DUR_SAP_MAGIC, random_range(20, 30), 50);
        break;

    case BEAM_DRAIN_MAGIC:
    {
        int amount = min(you.magic_points, random2avg(ench_power / 8, 3));
        if (!amount)
            break;
        mprf(MSGCH_WARN, "You feel your power leaking away.");
        drain_mp(amount);
        if (agent() && (agent()->type == MONS_EYE_OF_DRAINING
                        || agent()->type == MONS_GHOST_MOTH))
        {
            agent()->heal(amount);
        }
        obvious_effect = true;
        break;
    }

    case BEAM_TUKIMAS_DANCE:
        cast_tukimas_dance(ench_power, &you);
        obvious_effect = true;
        break;

    case BEAM_RESISTANCE:
        potionlike_effect(POT_RESISTANCE, min(ench_power, 200));
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_UNRAVELLING:
        if (!player_is_debuffable())
            break;

        debuff_player();
        _unravelling_explode(*this);
        obvious_effect = true;
        break;

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
            if (source_id == MID_PLAYER)
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
    {
        if (agent() && agent()->is_monster())
            interrupt_activity(AI_MONSTER_ATTACKS, agent()->as_monster());
        else
            interrupt_activity(AI_MONSTER_ATTACKS);
    }

    if (flavour == BEAM_MISSILE && item)
    {
        ranged_attack attk(agent(true), &you, item, use_target_as_pos, agent());
        attk.attack();
        // fsim purposes - throw_it detects if an attack connected through
        // hit_verb
        if (attk.ev_margin >= 0 && hit_verb.empty())
            hit_verb = attk.attack_verb;
        if (attk.reflected)
            reflect();
        extra_range_used += attk.range_used;
        return;
    }

    if (misses_player())
        return;

    const bool engulfs = is_explosion || is_big_cloud();

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

    // FIXME: Lots of duplicated code here (compare handling of
    // monsters)
    int hurted = 0;

    // Roll the damage.
    if (!(origin_spell == SPELL_FLASH_FREEZE && you.duration[DUR_FROZEN]))
        hurted += damage.roll();

#ifdef DEBUG_DIAGNOSTICS
    const int preac = hurted;
#endif

    hurted = apply_AC(&you, hurted);

#ifdef DEBUG_DIAGNOSTICS
    const int postac = hurted;
    dprf(DIAG_BEAM, "Player damage: before AC=%d; after AC=%d",
                    preac, postac);
#endif

    practise_being_shot();

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
        was_affected = miasma_player(agent(), name);

    if (flavour == BEAM_DEVASTATION) // DISINTEGRATION already handled
        blood_spray(you.pos(), MONS_PLAYER, hurted / 5);

    // Confusion effect for spore explosions
    if (flavour == BEAM_SPORE && hurted
        && !(you.holiness() & MH_UNDEAD)
        && !you.is_unbreathing())
    {
        confuse_player(2 + random2(3));
    }

    if (flavour == BEAM_UNRAVELLED_MAGIC)
        affect_player_enchantment();

    // handling of missiles
    if (item && item->base_type == OBJ_MISSILES)
    {
        if (item->sub_type == MI_THROWING_NET)
        {
            if (player_caught_in_net())
            {
                if (monster_by_mid(source_id))
                    xom_is_stimulated(50);
                was_affected = true;
            }
        }
        else if (item->brand == SPMSL_CURARE)
        {
            if (x_chance_in_y(90 - 3 * you.armour_class(), 100))
            {
                curare_actor(agent(), (actor*) &you, 2, name, source_name);
                was_affected = true;
            }
        }

        if (you.mutation[MUT_JELLY_MISSILE]
            && you.hp < you.hp_max
            && !you.duration[DUR_DEATHS_DOOR]
            && item_is_jelly_edible(*item)
            && coinflip())
        {
            mprf("Your attached jelly eats %s!", item->name(DESC_THE).c_str());
            inc_hp(random2(hurted / 2));
            canned_msg(MSG_GAIN_HEALTH);
            drop_item = false;
        }
    }

    // Sticky flame.
    if (origin_spell == SPELL_STICKY_FLAME
        || origin_spell == SPELL_STICKY_FLAME_RANGE)
    {
        if (!player_res_sticky_flame())
        {
            napalm_player(random2avg(7, 3) + 1, get_source_name(), aux_source);
            was_affected = true;
        }
    }

    // need to trigger qaz resists after reducing damage from ac/resists.
    //    for some reason, strength 2 is the standard. This leads to qaz's resists triggering 2 in 5 times at max piety.
    //    perhaps this should scale with damage?
    // what to do for hybrid damage?  E.g. bolt of magma, icicle, poison arrow?  Right now just ignore the physical component.
    // what about acid?
    you.expose_to_element(flavour, 2, false);


    // Manticore spikes
    if (origin_spell == SPELL_THROW_BARBS && hurted > 0)
    {
        mpr("The barbed spikes become lodged in your body.");
        if (!you.duration[DUR_BARBS])
            you.set_duration(DUR_BARBS,  random_range(3, 6));
        else
            you.increase_duration(DUR_BARBS, random_range(2, 4), 12);

        if (you.attribute[ATTR_BARBS_POW])
            you.attribute[ATTR_BARBS_POW] = min(6, you.attribute[ATTR_BARBS_POW]++);
        else
            you.attribute[ATTR_BARBS_POW] = 4;
    }

    if (flavour == BEAM_ENSNARE)
        was_affected = ensnare(&you) || was_affected;


    if (origin_spell == SPELL_QUICKSILVER_BOLT)
        debuff_player();

    dprf(DIAG_BEAM, "Damage: %d", hurted);

    if (hurted > 0 || old_hp < you.hp || was_affected)
    {
        if (mons_att_wont_attack(attitude))
        {
            friend_info.hurt++;

            // Beam from player rebounded and hit player.
            // Xom's amusement at the player's being damaged is handled
            // elsewhere.
            if (source_id == MID_PLAYER)
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

    internal_ouch(hurted);

    // Acid. (Apply this afterward, to avoid bad message ordering.)
    if (flavour == BEAM_ACID)
        you.splash_with_acid(agent(), 5, true);

    extra_range_used += range_used_on_hit();

    knockback_actor(&you, hurted);

    if (_is_explosive_bolt(this))
        _explosive_bolt_explode(this, you.pos());
    else if (origin_spell == SPELL_FLASH_FREEZE
             || name == "blast of ice"
             || origin_spell == SPELL_GLACIATE && !is_explosion)
    {
        if (you.duration[DUR_FROZEN])
        {
            if (origin_spell == SPELL_FLASH_FREEZE)
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        else
        {
            mprf(MSGCH_WARN, "You are encased in ice.");
            you.duration[DUR_FROZEN] = (2 + random2(3)) * BASELINE_DELAY;
        }
    }
    else if (origin_spell == SPELL_DAZZLING_SPRAY
             && !(you.holiness() & (MH_UNDEAD | MH_NONLIVING | MH_PLANT)))
    {
        if (x_chance_in_y(85 - you.experience_level * 3 , 100))
            you.confuse(agent(), 5 + random2(3));
    }
}

int bolt::apply_AC(const actor *victim, int hurted)
{
    switch (flavour)
    {
    case BEAM_DAMNATION:
        ac_rule = AC_NONE; break;
    case BEAM_ELECTRICITY:
        ac_rule = AC_HALF; break;
    case BEAM_FRAG:
        ac_rule = AC_TRIPLE; break;
    default: ;
    }

    // beams don't obey GDR -> max_damage is 0
    return victim->apply_ac(hurted, 0, ac_rule, 0, !is_tracer);
}

void bolt::update_hurt_or_helped(monster* mon)
{
    if (!mons_atts_aligned(attitude, mons_attitude(*mon)))
    {
        if (nasty_to(mon))
            foe_info.hurt++;
        else if (nice_to(monster_info(mon)))
        {
            foe_info.helped++;
            // Accidentally helped a foe.
            if (!is_tracer && !effect_known && mons_is_threatening(*mon))
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
            if (!is_tracer && mon->mid == source_id)
                xom_is_stimulated(100);
        }
        else if (nice_to(monster_info(mon)))
            friend_info.helped++;
    }
}

void bolt::tracer_enchantment_affect_monster(monster* mon)
{
    // Only count tracers as hitting creatures they could potentially affect
    if (ench_flavour_affects_monster(flavour, mon, true)
        && !(has_saving_throw() && mons_immune_magic(*mon)))
    {
        // Update friend or foe encountered.
        if (!mons_atts_aligned(attitude, mons_attitude(*mon)))
        {
            foe_info.count++;
            foe_info.power += mon->get_experience_level();
        }
        else
        {
            friend_info.count++;
            friend_info.power += mon->get_experience_level();
        }
    }

    handle_stop_attack_prompt(mon);
    if (!beam_cancelled)
        extra_range_used += range_used_on_hit();
}

// Return false if we should skip handling this monster.
bool bolt::determine_damage(monster* mon, int& preac, int& postac, int& final,
                            vector<string>& messages)
{
    preac = postac = final = 0;

    const bool freeze_immune =
        origin_spell == SPELL_FLASH_FREEZE && mon->has_ench(ENCH_FROZEN);

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
      // average AC. This does work well, even using no AC would. An
      // attack that _usually_ does no damage but can possibly do some means
      // we'll ultimately get it through. And monsters with weak ranged
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
    const int preac_max_damage =
        (freeze_immune) ? 0
                        : damage.num * damage.size;

    // preac: damage before AC modifier
    // postac: damage after AC modifier
    // final: damage after AC and resists
    // All these are invalid if we return false.

    if (is_tracer)
    {
        // Was mean between min and max;
        preac = preac_max_damage;
    }
    else if (!freeze_immune)
        preac = damage.roll();

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
    if (thrower != KILL_YOU_MISSILE && thrower != KILL_YOU
        || is_harmless(mon)
        || friend_info.dont_stop && foe_info.dont_stop)
    {
        return;
    }

    bool prompted = false;

    if (stop_attack_prompt(mon, true, target, &prompted)
        || _stop_because_god_hates_target_prompt(mon, origin_spell, flavour,
                                                 item))
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

void bolt::tracer_nonenchantment_affect_monster(monster* mon)
{
    vector<string> messages;
    int preac, post, final;

    if (!determine_damage(mon, preac, post, final, messages))
        return;

    // Check only if actual damage and the monster is worth caring about.
    if (final > 0 && mons_is_threatening(*mon))
    {
        ASSERT(preac > 0);

        // Monster could be hurt somewhat, but only apply the
        // monster's power based on how badly it is affected.
        // For example, if a fire giant (power 16) threw a
        // fireball at another fire giant, and it only took
        // 1/3 damage, then power of 5 would be applied.

        // Counting foes is only important for monster tracers.
        if (!mons_atts_aligned(attitude, mons_attitude(*mon)))
        {
            foe_info.power += 2 * final * mon->get_experience_level() / preac;
            foe_info.count++;
        }
        else
        {
            friend_info.power
                += 2 * final * mon->get_experience_level() / preac;
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
        for (const string &msg : messages)
            mprf(MSGCH_MONSTER_DAMAGE, "%s", msg.c_str());
    }

    // Either way, we could hit this monster, so update range used.
    extra_range_used += range_used_on_hit();

    if (_is_explosive_bolt(this))
        _explosive_bolt_explode(this, mon->pos());
}

void bolt::tracer_affect_monster(monster* mon)
{
    // Ignore unseen monsters.
    if (!agent() || !agent()->can_see(*mon))
        return;

    if (flavour == BEAM_UNRAVELLING && monster_is_debuffable(*mon))
        is_explosion = true;

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

    _maybe_imb_explosion(this, pos());
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

            set_attack_conducts(conducts, mon, you.can_see(*mon));

            if (have_passive(passive_t::convert_orcs)
                && mons_genus(mon->type) == MONS_ORC
                && mon->asleep() && you.see_cell(mon->pos()))
            {
                hit_woke_orc = true;
            }
        }
        behaviour_event(mon, ME_ANNOY, agent());
    }
    else if (flavour != BEAM_HIBERNATION || !mon->asleep())
        behaviour_event(mon, ME_ALERT, agent());

    enable_attack_conducts(conducts);

    // Doing this here so that the player gets to see monsters
    // "flicker and vanish" when turning invisible....
    if (animate)
    {
        _ench_animation(effect_known ? real_flavour
                                     : BEAM_MAGIC,
                        mon, effect_known);
    }

    // Try to hit the monster with the enchantment. The behaviour_event above
    // may have caused a pacified monster to leave the level, so only try to
    // enchant it if it's still here. If the monster did leave the level, set
    // obvious_effect so we don't get "Nothing appears to happen".
    int res_margin = 0;
    const mon_resist_type ench_result = mon->alive()
                                      ? try_enchant_monster(mon, res_margin)
                                      : (obvious_effect = true, MON_OTHER);

    if (mon->alive())           // Aftereffects.
    {
        // Message or record the success/failure.
        switch (ench_result)
        {
        case MON_RESIST:
            if (simple_monster_message(*mon,
                                   mon->resist_margin_phrase(res_margin).c_str()))
            {
                msg_generated = true;
            }
            break;
        case MON_UNAFFECTED:
            if (simple_monster_message(*mon, " is unaffected."))
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

    extra_range_used += range_used_on_hit();
}

static bool _dazzle_monster(monster* mons, actor* act)
{
    if (!mons_can_be_dazzled(mons->type))
        return false;

    if (x_chance_in_y(95 - mons->get_hit_dice() * 5 , 100))
    {
        simple_monster_message(*mons, " is dazzled.");
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, act,
                       random_range(4, 8) * BASELINE_DELAY));
        return true;
    }

    return false;
}

static void _glaciate_freeze(monster* mon, killer_type englaciator,
                             int kindex)
{
    const coord_def where = mon->pos();
    const monster_type pillar_type =
        mons_is_zombified(*mon) ? mons_zombie_base(*mon)
                                : mons_species(mon->type);
    const int hd = mon->get_experience_level();

    simple_monster_message(*mon, " is frozen into a solid block of ice!");

    // If the monster leaves a corpse when it dies, destroy the corpse.
    item_def* corpse = monster_die(mon, englaciator, kindex);
    if (corpse)
        destroy_item(corpse->index());

    if (monster *pillar = create_monster(
                        mgen_data(MONS_BLOCK_OF_ICE,
                                  BEH_HOSTILE,
                                  where,
                                  MHITNOT,
                                  MG_FORCE_PLACE).set_base(pillar_type),
                                  false))
    {
        // Enemies with more HD leave longer-lasting blocks of ice.
        int time_left = (random2(8) + hd) * BASELINE_DELAY;
        mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
        pillar->update_ench(temp_en);
    }
}

void bolt::monster_post_hit(monster* mon, int dmg)
{
    // Suppress the message for scattershot.
    if (YOU_KILL(thrower) && you.see_cell(mon->pos())
        && name != "burst of metal fragments")
    {
        print_wounds(*mon);
    }

    // Don't annoy friendlies or good neutrals if the player's beam
    // did no damage. Hostiles will still take umbrage.
    if (dmg > 0 || !mon->wont_attack() || !YOU_KILL(thrower))
    {
        bool was_asleep = mon->asleep();
        special_missile_type m_brand = SPMSL_FORBID_BRAND;
        if (item && item->base_type == OBJ_MISSILES)
            m_brand = get_ammo_brand(*item);

        // Don't immediately turn insane monsters hostile.
        if (m_brand != SPMSL_FRENZY)
        {
            behaviour_event(mon, ME_ANNOY, agent());
            // behaviour_event can make a monster leave the level or vanish.
            if (!mon->alive())
                return;
        }


        // Don't allow needles of sleeping to awaken monsters.
        if (m_brand == SPMSL_SLEEP && was_asleep && !mon->asleep())
            mon->put_to_sleep(agent(), 0);
    }

    if (YOU_KILL(thrower) && !mon->wont_attack() && !mons_is_firewood(*mon))
        you.pet_target = mon->mindex();

    // Sticky flame.
    if (origin_spell == SPELL_STICKY_FLAME
        || origin_spell == SPELL_STICKY_FLAME_RANGE)
    {
        const int levels = min(4, 1 + random2(dmg) / 2);
        napalm_monster(mon, agent(), levels);
    }

    // Acid splash from yellow draconians / acid dragons
    if (origin_spell == SPELL_ACID_SPLASH)
    {
        mon->splash_with_acid(agent(), 3);

        for (adjacent_iterator ai(source); ai; ++ai)
        {
            // the acid can splash onto adjacent targets
            if (grid_distance(*ai, target) != 1)
                continue;
            if (actor *victim = actor_at(*ai))
            {
                if (you.see_cell(*ai))
                {
                    mprf("The acid splashes onto %s!",
                         victim->name(DESC_THE).c_str());
                }

                victim->splash_with_acid(agent(), 3);
            }
        }
    }

    // Handle missile effects.
    if (item && item->base_type == OBJ_MISSILES
        && item->brand == SPMSL_CURARE && ench_power == AUTOMATIC_HIT)
    {
        curare_actor(agent(), mon, 2, name, source_name);
    }

    // purple draconian breath
    if (origin_spell == SPELL_QUICKSILVER_BOLT)
        debuff_monster(*mon);

    if (dmg)
        beogh_follower_convert(mon, true);

    knockback_actor(mon, dmg);
    if (_is_explosive_bolt(this))
        _explosive_bolt_explode(this, mon->pos());

    if (origin_spell == SPELL_DAZZLING_SPRAY)
        _dazzle_monster(mon, agent());
    else if (origin_spell == SPELL_FLASH_FREEZE
             || name == "blast of ice"
             || origin_spell == SPELL_GLACIATE && !is_explosion)
    {
        if (mon->has_ench(ENCH_FROZEN))
        {
            if (origin_spell == SPELL_FLASH_FREEZE)
                simple_monster_message(*mon, " is unaffected.");
        }
        else
        {
            simple_monster_message(*mon, " is flash-frozen.");
            mon->add_ench(ENCH_FROZEN);
        }
    }

    if (origin_spell == SPELL_THROW_BARBS && dmg > 0
        && !(mon->is_insubstantial() || mons_genus(mon->type) == MONS_JELLY))
    {
        mon->add_ench(mon_enchant(ENCH_BARBS, 1, agent(),
                                  random_range(5, 7) * BASELINE_DELAY));
    }
}

void bolt::knockback_actor(actor *act, int dam)
{
    if (!can_knockback(act, dam))
        return;

    const int distance =
        (origin_spell == SPELL_FORCE_LANCE)
            ? 1 + div_rand_round(ench_power, 40) :
        (origin_spell == SPELL_CHILLING_BREATH) ? 2 : 1;

    const int roll = origin_spell == SPELL_FORCE_LANCE
                     ? 7 + 0.27 * ench_power
                     : 17;
    const int weight = max_corpse_chunks(act->is_monster() ? act->type :
                                   player_species_to_mons_species(you.species));

    const coord_def oldpos = act->pos();

    if (act->is_stationary())
        return;
    // We can't do knockback if the beam starts and ends on the same space
    if (source == oldpos)
        return;
    ASSERT(ray.pos() == oldpos);

    coord_def newpos = oldpos;
    for (int dist_travelled = 0; dist_travelled < distance; ++dist_travelled)
    {
        if (x_chance_in_y(weight, roll))
            continue;

        const ray_def oldray(ray);

        ray.advance();

        newpos = ray.pos();
        if (newpos == oldray.pos()
            || cell_is_solid(newpos)
            || actor_at(newpos)
            || !act->can_pass_through(newpos)
            || !act->is_habitable(newpos))
        {
            ray = oldray;
            break;
        }

        act->move_to_pos(newpos);
        if (act->is_player())
            stop_delay(true);
    }

    if (newpos == oldpos)
        return;

    if (you.can_see(*act))
    {
        if (origin_spell == SPELL_CHILLING_BREATH)
        {
            mprf("%s %s blown backwards by the freezing wind.",
                 act->name(DESC_THE).c_str(),
                 act->conj_verb("are").c_str());
        }
        else
        {
            mprf("%s %s knocked back by the %s.",
                 act->name(DESC_THE).c_str(),
                 act->conj_verb("are").c_str(),
                 name.c_str());
        }
    }

    if (act->pos() != newpos)
        act->collide(newpos, agent(), ench_power);

    // Stun the monster briefly so that it doesn't look as though it wasn't
    // knocked back at all
    if (act->is_monster())
        act->as_monster()->speed_increment -= random2(6) + 4;

    act->apply_location_effects(oldpos, killer(),
                                actor_to_death_source(agent()));
}

// Return true if the player's god will be unforgiving about the effects
// of this beam.
bool bolt::god_cares() const
{
    return effect_known || effect_wanton;
}

// Return true if the block succeeded (including reflections.)
bool bolt::attempt_block(monster* mon)
{
    const int shield_block = mon->shield_bonus();
    if (shield_block <= 0)
        return false;

    const int sh_hit = random2(hit * 130 / 100 + mon->shield_block_penalty());
    if (sh_hit >= shield_block)
        return false;

    item_def *shield = mon->mslot_item(MSLOT_SHIELD);
    if (is_reflectable(*mon))
    {
        if (mon->observable())
        {
            if (shield && is_shield(*shield) && shield_reflects(*shield))
            {
                mprf("%s reflects the %s off %s %s!",
                     mon->name(DESC_THE).c_str(),
                     name.c_str(),
                     mon->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     shield->name(DESC_PLAIN).c_str());
                ident_reflector(shield);
            }
            else
            {
                mprf("The %s bounces off an invisible shield around %s!",
                     name.c_str(),
                     mon->name(DESC_THE).c_str());

                item_def *amulet = mon->mslot_item(MSLOT_JEWELLERY);
                if (amulet)
                    ident_reflector(amulet);
            }
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
    return true;
}

/// Is the given monster a bush or bush-like 'monster', and can the given beam
/// travel through it without harm?
bool bolt::bush_immune(const monster &mons) const
{
    return
        (mons_species(mons.type) == MONS_BUSH || mons.type == MONS_BRIAR_PATCH)
        && !pierce && !is_explosion
        && !is_enchantment()
        && target != mons.pos()
        && origin_spell != SPELL_STICKY_FLAME
        && origin_spell != SPELL_STICKY_FLAME_RANGE
        && origin_spell != SPELL_CHAIN_LIGHTNING;
}

void bolt::affect_monster(monster* mon)
{
    // Don't hit dead monsters.
    if (!mon->alive() || mon->type == MONS_PLAYER_SHADOW)
        return;

    hit_count[mon->mid]++;

    if (shoot_through_monster(*this, mon) && !is_tracer)
    {
        // FIXME: Could use a better message, something about
        // dodging that doesn't sound excessively weird would be
        // nice.
        if (you.see_cell(mon->pos()))
        {
            if (testbits(mon->flags, MF_DEMONIC_GUARDIAN))
                mpr("Your demonic guardian avoids your attack.");
            else if (!bush_immune(*mon))
            {
                simple_god_message(
                    make_stringf(" protects %s plant from harm.",
                        attitude == ATT_FRIENDLY ? "your" : "a").c_str(),
                    GOD_FEDHAS);
            }
        }
    }

    if (flavour == BEAM_WATER && mon->type == MONS_WATER_ELEMENTAL && !is_tracer)
    {
        if (you.see_cell(mon->pos()))
            mprf("The %s passes through %s.", name.c_str(), mon->name(DESC_THE).c_str());
    }

    if (ignores_monster(mon))
        return;

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
        return;
    }

    if (flavour == BEAM_MISSILE && item)
    {
        ranged_attack attk(agent(true), mon, item, use_target_as_pos, agent());
        attk.attack();
        // fsim purposes - throw_it detects if an attack connected through
        // hit_verb
        if (attk.ev_margin >= 0 && hit_verb.empty())
            hit_verb = attk.attack_verb;
        if (attk.reflected)
            reflect();
        extra_range_used += attk.range_used;
        return;
    }

    // Explosions always 'hit'.
    const bool engulfs = (is_explosion || is_big_cloud());

    if (is_enchantment())
    {
        if (real_flavour == BEAM_CHAOS || real_flavour == BEAM_RANDOM)
        {
            if (hit_verb.empty())
                hit_verb = engulfs ? "engulfs" : "hits";
            if (you.see_cell(mon->pos()))
            {
                mprf("The %s %s %s.", name.c_str(), hit_verb.c_str(),
                     mon->name(DESC_THE).c_str());
            }
            else if (heard && !hit_noise_msg.empty())
                mprf(MSGCH_SOUND, "%s", hit_noise_msg.c_str());
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
            if (is_sanctuary(mon->pos()) || is_sanctuary(you.pos()))
                remove_sanctuary(true);

            // It's not the player's fault if the monster couldn't be seen
            set_attack_conducts(conducts, mon, you.can_see(*mon));
        }
    }

    if (engulfs && flavour == BEAM_SPORE // XXX: engulfs is redundant?
        && mon->holiness() & MH_NATURAL
        && !mon->is_unbreathing())
    {
        apply_enchantment_to_monster(mon);
    }

    if (flavour == BEAM_UNRAVELLED_MAGIC)
    {
        int unused; // res_margin
        try_enchant_monster(mon, unused);
    }

    // Make a copy of the to-hit before we modify it.
    int beam_hit = hit;

    if (beam_hit != AUTOMATIC_HIT)
    {
        if (mon->invisible() && !can_see_invis)
            beam_hit /= 2;

        // Backlit is easier to hit:
        if (mon->backlit(false))
            beam_hit += 2 + random2(8);

        // Umbra is harder to hit:
        if (!nightvision && mon->umbra())
            beam_hit -= 2 + random2(4);
    }

    // The monster may block the beam.
    if (!engulfs && is_blockable() && attempt_block(mon))
        return;

    defer_rand r;
    int rand_ev = random2(mon->evasion());
    int defl = mon->missile_deflection();

    // FIXME: We're randomising mon->evasion, which is further
    // randomised inside test_beam_hit. This is so we stay close to the
    // 4.0 to-hit system (which had very little love for monsters).
    if (!engulfs && !_test_beam_hit(beam_hit, rand_ev, pierce, defl, r))
    {
        const bool deflected = _test_beam_hit(beam_hit, rand_ev, pierce, 0, r);
        // If the PLAYER cannot see the monster, don't tell them anything!
        if (mon->observable() && name != "burst of metal fragments")
        {
            // if it would have hit otherwise...
            if (_test_beam_hit(beam_hit, rand_ev, pierce, 0, r))
            {
                string deflects = (defl == 2) ? "deflects" : "repels";
                msg::stream << mon->name(DESC_THE) << " "
                            << deflects << " the " << name
                            << '!' << endl;
            }
            else
            {
                msg::stream << "The " << name << " misses "
                            << mon->name(DESC_THE) << '.' << endl;
            }
        }
        if (deflected)
            mon->ablate_deflection();
        return;
    }

    update_hurt_or_helped(mon);
    enable_attack_conducts(conducts);

    // We'll say ballistomycete spore explosions don't trigger the ally attack conduct
    // for Fedhas worshipers. Mostly because you can accidentally blow up a
    // group of 8 plants and get placed under penance until the end of time
    // otherwise. I'd prefer to do this elsewhere but the beam information
    // goes out of scope.
    //
    // Also exempting miscast explosions from this conduct -cao
    if (you_worship(GOD_FEDHAS)
        && (flavour == BEAM_SPORE
            || source_id == MID_PLAYER
               && aux_source.find("your miscasting") != string::npos))
    {
        conducts[0].enabled = false;
    }

    if (!is_explosion && !noise_generated)
    {
        heard = noisy(loudness, pos(), source_id) || heard;
        noise_generated = true;
    }

    if (!mon->alive())
        return;

    // The beam hit.
    if (you.see_cell(mon->pos()))
    {
        // Monsters are never currently helpless in ranged combat.
        if (hit_verb.empty())
            hit_verb = engulfs ? "engulfs" : "hits";

        mprf("The %s %s %s.",
             name.c_str(),
             hit_verb.c_str(),
             mon->name(DESC_THE).c_str());

    }
    else if (heard && !hit_noise_msg.empty())
        mprf(MSGCH_SOUND, "%s", hit_noise_msg.c_str());
    // The player might hear something, if _they_ fired a missile
    // (not magic beam).
    else if (!silenced(you.pos()) && flavour == BEAM_MISSILE
             && YOU_KILL(thrower))
    {
        mprf(MSGCH_SOUND, "The %s hits something.", name.c_str());
    }

    if (final > 0)
    {
        for (const string &msg : messages)
            mprf(MSGCH_MONSTER_DAMAGE, "%s", msg.c_str());
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
        mon->hurt(agent(), final, flavour, KILLED_BY_BEAM, "", "", false);
    }

    if (mon->alive())
        monster_post_hit(mon, final);
    // The monster (e.g. a spectral weapon) might have self-destructed in its
    // behaviour_event called from mon->hurt() above. If that happened, it
    // will have been cleaned up already (and is therefore invalid now).
    else if (!invalid_monster(mon))
    {
        // Preserve name of the source monster if it winds up killing
        // itself.
        if (mon->mid == source_id && source_name.empty())
            source_name = mon->name(DESC_A, true);

        int kindex = actor_to_death_source(agent());
        if (origin_spell == SPELL_GLACIATE
            && !mon->is_insubstantial()
            && x_chance_in_y(3, 5))
        {
            // Includes monster_die as part of converting to block of ice.
            _glaciate_freeze(mon, thrower, kindex);
        }
        // Prevent spore explosions killing plants from being registered
        // as a Fedhas misconduct. Deaths can trigger the ally dying or
        // plant dying conducts, but spore explosions shouldn't count
        // for either of those.
        //
        // FIXME: Should be a better way of doing this. For now, we are
        // just falsifying the death report... -cao
        else if (you_worship(GOD_FEDHAS) && flavour == BEAM_SPORE
            && fedhas_protects(*mon))
        {
            if (mon->attitude == ATT_FRIENDLY)
                mon->attitude = ATT_HOSTILE;
            monster_die(mon, KILL_MON, kindex);
        }
        else
        {
            killer_type ref_killer = thrower;
            if (!YOU_KILL(thrower) && reflector == MID_PLAYER)
            {
                ref_killer = KILL_YOU_MISSILE;
                kindex = YOU_FAULTLESS;
            }
            monster_die(mon, ref_killer, kindex);
        }
    }

    extra_range_used += range_used_on_hit();
}

bool bolt::ignores_monster(const monster* mon) const
{
    // Digging doesn't affect monsters (should it harm earth elementals?).
    if (flavour == BEAM_DIGGING)
        return true;

    // The targetters might call us with nullptr in the event of a remembered
    // monster that is no longer there. Treat it as opaque.
    if (!mon)
        return false;

    // All kinds of beams go past orbs of destruction and friendly
    // battlespheres.
    if (mons_is_projectile(*mon)
        || (mons_is_avatar(mon->type) && mons_aligned(agent(), mon)))
    {
        return true;
    }

    // Missiles go past bushes and briar patches, unless aimed directly at them
    if (bush_immune(*mon))
        return true;

    if (shoot_through_monster(*this, mon))
        return true;

    // Fire storm creates these, so we'll avoid affecting them.
    if (origin_spell == SPELL_FIRE_STORM && mon->type == MONS_FIRE_VORTEX)
        return true;

    // Don't blow up blocks of ice with the spell that creates them.
    if (origin_spell == SPELL_GLACIATE && mon->type == MONS_BLOCK_OF_ICE)
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
    case BEAM_ENSLAVE_SOUL:
    case BEAM_BLINK_CLOSE:
    case BEAM_BLINK:
    case BEAM_BECKONING:
    case BEAM_MALIGN_OFFERING:
    case BEAM_AGILITY:
    case BEAM_RESISTANCE:
    case BEAM_MALMUTATE:
    case BEAM_SAP_MAGIC:
    case BEAM_UNRAVELLING:
    case BEAM_UNRAVELLED_MAGIC:
    case BEAM_INFESTATION:
    case BEAM_IRRESISTIBLE_CONFUSION:
        return false;
    case BEAM_VULNERABILITY:
        return !one_chance_in(3);  // Ignores MR 1/3 of the time
    case BEAM_PARALYSIS:        // Giant eyeball paralysis is irresistible
        return !(agent() && agent()->type == MONS_FLOATING_EYE);
    default:
        return true;
    }
}

bool ench_flavour_affects_monster(beam_type flavour, const monster* mon,
                                          bool intrinsic_only)
{
    bool rc = true;
    switch (flavour)
    {
    case BEAM_MALMUTATE:
    case BEAM_UNRAVELLED_MAGIC:
        rc = mon->can_mutate();
        break;

    case BEAM_SLOW:
    case BEAM_HASTE:
    case BEAM_PARALYSIS:
        rc = !mon->stasis();
        break;

    case BEAM_POLYMORPH:
        rc = mon->can_polymorph();
        break;

    case BEAM_ENSLAVE_SOUL:
        rc = (mon->holiness() & MH_NATURAL
              || mon->holiness() & MH_DEMONIC
              || mon->holiness() & MH_HOLY)
             && !mon->is_summoned()
             && !mons_enslaved_body_and_soul(*mon)
             && mon->attitude != ATT_FRIENDLY
             && mons_intel(*mon) >= I_HUMAN
             && mon->type != MONS_PANDEMONIUM_LORD;
        break;

    case BEAM_DISPEL_UNDEAD:
        rc = bool(mon->holiness() & MH_UNDEAD);
        break;

    case BEAM_PAIN:
        rc = mon->res_negative_energy(intrinsic_only) < 3;
        break;

    case BEAM_AGONY:
        rc = !mon->res_torment();
        break;

    case BEAM_HIBERNATION:
        rc = mon->can_hibernate(false, intrinsic_only);
        break;

    case BEAM_PORKALATOR:
        rc = (mon->holiness() & MH_DEMONIC && mon->type != MONS_HELL_HOG)
              || (mon->holiness() & MH_NATURAL && mon->type != MONS_HOG)
              || (mon->holiness() & MH_HOLY && mon->type != MONS_HOLY_SWINE);
        break;

    case BEAM_SENTINEL_MARK:
        rc = false;
        break;

    case BEAM_MALIGN_OFFERING:
        rc = (mon->res_negative_energy(intrinsic_only) < 3);
        break;

    case BEAM_VIRULENCE:
        rc = (mon->res_poison() < 3);
        break;

    case BEAM_DRAIN_MAGIC:
        rc = mon->antimagic_susceptible();
        break;

    case BEAM_INNER_FLAME:
        rc = !(mon->is_summoned() || mon->has_ench(ENCH_INNER_FLAME));
        break;

    case BEAM_INFESTATION:
        rc = mons_gives_xp(*mon, you) && !mon->has_ench(ENCH_INFESTATION);
        break;

    default:
        break;
    }

    return rc;
}

bool enchant_actor_with_flavour(actor* victim, const actor *foe,
                                beam_type flavour, int powc)
{
    bolt dummy;
    dummy.flavour = flavour;
    dummy.ench_power = powc;
    dummy.set_agent(foe);
    dummy.animate = false;
    if (victim->is_player())
        dummy.affect_player_enchantment(false);
    else
        dummy.apply_enchantment_to_monster(victim->as_monster());
    return dummy.obvious_effect;
}

bool enchant_monster_invisible(monster* mon, const string &how)
{
    // Store the monster name before it becomes an "it". - bwr
    const string monster_name = mon->name(DESC_THE);
    const bool could_see = you.can_see(*mon);

    if (mon->has_ench(ENCH_INVIS) || !mon->add_ench(ENCH_INVIS))
        return false;

    if (could_see)
    {
        const bool is_visible = mon->visible_to(&you);

        // Can't use simple_monster_message(*) here, since it checks
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

mon_resist_type bolt::try_enchant_monster(monster* mon, int &res_margin)
{
    // Early out if the enchantment is meaningless.
    if (!ench_flavour_affects_monster(flavour, mon))
        return MON_UNAFFECTED;

    // Check magic resistance.
    if (has_saving_throw())
    {
        if (mons_immune_magic(*mon))
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
        // Chaos effects don't get a resistance check to match melee chaos.
        else if (real_flavour != BEAM_CHAOS)
        {
            if (mon->check_res_magic(ench_power) > 0)
            {
                // Note only actually used by messages in this case.
                res_margin = mon->res_magic() - ench_power_stepdown(ench_power);
                return MON_RESIST;
            }
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

    case BEAM_BECKONING:
        obvious_effect = beckon(*mon, *this);
        return obvious_effect ? MON_AFFECTED : MON_OTHER; // ?

    case BEAM_POLYMORPH:
        if (mon->polymorph(ench_power))
            obvious_effect = true;
        if (YOU_KILL(thrower))
        {
            did_god_conduct(DID_DELIBERATE_MUTATING, 2 + random2(3),
                            god_cares());
        }
        return MON_AFFECTED;

    case BEAM_MALMUTATE:
    case BEAM_UNRAVELLED_MAGIC:
        if (mon->malmutate("")) // exact source doesn't matter
            obvious_effect = true;
        if (YOU_KILL(thrower))
        {
            did_god_conduct(DID_DELIBERATE_MUTATING, 2 + random2(3),
                            god_cares());
        }
        return MON_AFFECTED;

    case BEAM_BANISH:
        mon->banish(agent());
        obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_DISPEL_UNDEAD:
        if (simple_monster_message(*mon, " convulses!"))
            obvious_effect = true;
        mon->hurt(agent(), damage.roll());
        return MON_AFFECTED;

    case BEAM_ENSLAVE_SOUL:
    {
        if (!ench_flavour_affects_monster(flavour, mon))
            return MON_UNAFFECTED;

        obvious_effect = true;
        const int duration = you.skill_rdiv(SK_INVOCATIONS, 3, 4) + 2;
        mon->add_ench(mon_enchant(ENCH_SOUL_RIPE, 0, agent(),
                                  duration * BASELINE_DELAY));
        simple_monster_message(*mon, "'s soul is now ripe for the taking.");
        return MON_AFFECTED;
    }

    case BEAM_PAIN:
    {
        const int dam = resist_adjust_damage(mon, flavour, damage.roll());
        if (dam)
        {
            if (simple_monster_message(*mon, " writhes in agony!"))
                obvious_effect = true;
            mon->hurt(agent(), dam, flavour);
            return MON_AFFECTED;
        }
        return MON_UNAFFECTED;
    }

    case BEAM_AGONY:
        torment_cell(mon->pos(), agent(), TORMENT_AGONY);
        obvious_effect = you.can_see(*mon);
        return MON_AFFECTED;

    case BEAM_DISINTEGRATION:   // disrupt/disintegrate
        if (simple_monster_message(*mon, " is blasted."))
            obvious_effect = true;
        mon->hurt(agent(), damage.roll(), flavour);
        return MON_AFFECTED;

    case BEAM_HIBERNATION:
        if (mon->can_hibernate())
        {
            if (simple_monster_message(*mon, " looks drowsy..."))
                obvious_effect = true;
            mon->put_to_sleep(agent(), ench_power, true);
            return MON_AFFECTED;
        }
        return MON_UNAFFECTED;

    case BEAM_CORONA:
        if (backlight_monster(mon))
        {
            obvious_effect = true;
            return MON_AFFECTED;
        }
        return MON_UNAFFECTED;

    case BEAM_SLOW:
        obvious_effect = do_slow_monster(*mon, agent(),
                                         ench_power * BASELINE_DELAY);
        return MON_AFFECTED;

    case BEAM_HASTE:
        if (YOU_KILL(thrower))
            did_god_conduct(DID_HASTY, 6, god_cares());

        if (mon->stasis())
            return MON_AFFECTED;

        if (!mon->has_ench(ENCH_HASTE)
            && !mon->is_stationary()
            && mon->add_ench(ENCH_HASTE))
        {
            if (!mons_is_immotile(*mon)
                && simple_monster_message(*mon, " seems to speed up."))
            {
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_MIGHT:
        if (!mon->has_ench(ENCH_MIGHT)
            && !mon->is_stationary()
            && mon->add_ench(ENCH_MIGHT))
        {
            if (simple_monster_message(*mon, " seems to grow stronger."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_BERSERK:
        if (!mon->berserk_or_insane())
        {
            // currently from potion, hence voluntary
            mon->go_berserk(true);
            // can't return this from go_berserk, unfortunately
            obvious_effect = you.can_see(*mon);
        }
        return MON_AFFECTED;

    case BEAM_HEALING:
        // No KILL_YOU_CONF, or we get "You heal ..."
        if (thrower == KILL_YOU || thrower == KILL_YOU_MISSILE)
        {
            const int pow = min(50, 3 + damage.roll());
            const int amount = pow + roll_dice(2, pow) - 2;
            if (heal_monster(*mon, amount))
                obvious_effect = true;
            msg_generated = true; // to avoid duplicate "nothing happens"
        }
        else if (mon->heal(3 + damage.roll()))
        {
            if (mon->hit_points == mon->max_hit_points)
            {
                if (simple_monster_message(*mon, "'s wounds heal themselves!"))
                    obvious_effect = true;
            }
            else if (simple_monster_message(*mon, " is healed somewhat."))
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
    case BEAM_IRRESISTIBLE_CONFUSION:
        if (mon->check_clarity(false))
        {
            if (you.can_see(*mon))
                obvious_effect = true;
            return MON_AFFECTED;
        }
        {
            // irresistible confusion has a shorter duration and is weaker
            // against strong monsters
            int dur = ench_power;
            if (flavour == BEAM_IRRESISTIBLE_CONFUSION)
                dur = max(10, dur - mon->get_hit_dice());
            else
                dur = _ench_pow_to_dur(dur);

            if (mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, agent(), dur)))
            {
                // FIXME: Put in an exception for things you won't notice
                // becoming confused.
                if (simple_monster_message(*mon, " appears confused."))
                    obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_SLEEP:
        if (mons_just_slept(*mon))
            return MON_UNAFFECTED;

        mon->put_to_sleep(agent(), ench_power);
        if (simple_monster_message(*mon, " falls asleep!"))
            obvious_effect = true;

        return MON_AFFECTED;

    case BEAM_INVISIBILITY:
    {
        if (enchant_monster_invisible(mon, "flickers and vanishes"))
            obvious_effect = true;

        return MON_AFFECTED;
    }

    case BEAM_ENSLAVE:
        if (agent() && agent()->is_monster())
        {
            enchant_type good = (agent()->wont_attack()) ? ENCH_CHARM
                                                         : ENCH_HEXED;
            enchant_type bad  = (agent()->wont_attack()) ? ENCH_HEXED
                                                         : ENCH_CHARM;

            const bool could_see = you.can_see(*mon);
            if (mon->has_ench(bad))
            {
                obvious_effect = mon->del_ench(bad);
                return MON_AFFECTED;
            }
            if (simple_monster_message(*mon, " is enslaved!"))
                obvious_effect = true;
            mon->add_ench(mon_enchant(good, 0, agent()));
            if (!obvious_effect && could_see && !you.can_see(*mon))
                obvious_effect = true;
            return MON_AFFECTED;
        }

        // Being a puppet on magic strings is a nasty thing.
        // Mindless creatures shouldn't probably mind, but because of complex
        // behaviour of enslaved neutrals, let's disallow that for now.
        mon->attitude = ATT_HOSTILE;

        // XXX: Another hackish thing for Pikel's band neutrality.
        if (mons_is_mons_class(mon, MONS_PIKEL))
            pikel_band_neutralise();

        if (simple_monster_message(*mon, " is charmed."))
            obvious_effect = true;
        mon->add_ench(ENCH_CHARM);
        if (you.can_see(*mon))
            obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_PORKALATOR:
    {
        // Monsters which use the ghost structure can't be properly
        // restored from hog form.
        if (mons_is_ghost_demon(mon->type))
            return MON_UNAFFECTED;

        monster orig_mon(*mon);
        if (monster_polymorph(mon, mon->holiness() & MH_DEMONIC ?
                      MONS_HELL_HOG : mon->holiness() & MH_HOLY ?
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
            if (simple_monster_message(*mon,
                                       (mon->body_size(PSIZE_BODY) > SIZE_BIG)
                                        ? " is filled with an intense inner flame!"
                                        : " is filled with an inner flame."))
            {
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_DIMENSION_ANCHOR:
        if (!mon->has_ench(ENCH_DIMENSION_ANCHOR)
            && mon->add_ench(mon_enchant(ENCH_DIMENSION_ANCHOR, 0, agent(),
                                         random_range(20, 30) * BASELINE_DELAY)))
        {
            if (simple_monster_message(*mon, " is firmly anchored in space."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_VULNERABILITY:
        if (!mon->has_ench(ENCH_LOWERED_MR)
            && mon->add_ench(mon_enchant(ENCH_LOWERED_MR, 0, agent(),
                                         random_range(20, 30) * BASELINE_DELAY)))
        {
            if (you.can_see(*mon))
            {
                mprf("%s magical defenses are stripped away.",
                     mon->name(DESC_ITS).c_str());
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_MALIGN_OFFERING:
    {
        const int dam = resist_adjust_damage(mon, flavour, damage.roll());
        if (dam)
        {
            _malign_offering_effect(mon, agent(), dam);
            obvious_effect = true;
            return MON_AFFECTED;
        }
        else
        {
            simple_monster_message(*mon, " is unaffected.");
            return MON_UNAFFECTED;
        }
    }

    case BEAM_VIRULENCE:
        if (!mon->has_ench(ENCH_POISON_VULN)
            && mon->add_ench(mon_enchant(ENCH_POISON_VULN, 0, agent(),
                                         random_range(20, 30) * BASELINE_DELAY)))
        {
            if (simple_monster_message(*mon,
                                       " grows more vulnerable to poison."))
            {
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_AGILITY:
        if (!mon->has_ench(ENCH_AGILE)
            && !mon->is_stationary()
            && mon->add_ench(ENCH_AGILE))
        {
            if (simple_monster_message(*mon, " suddenly seems more agile."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_SAP_MAGIC:
        if (!SAP_MAGIC_CHANCE())
        {
            if (you.can_see(*mon))
                canned_msg(MSG_NOTHING_HAPPENS);
            break;
        }
        if (!mon->has_ench(ENCH_SAP_MAGIC)
            && mon->add_ench(mon_enchant(ENCH_SAP_MAGIC, 0, agent())))
        {
            if (you.can_see(*mon))
            {
                mprf("%s seems less certain of %s magic.",
                     mon->name(DESC_THE).c_str(), mon->pronoun(PRONOUN_POSSESSIVE).c_str());
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_DRAIN_MAGIC:
    {
        if (!mon->antimagic_susceptible())
            break;

        const int dur =
            random2(div_rand_round(ench_power, mon->get_hit_dice()) + 1)
                    * BASELINE_DELAY;
        mon->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                  agent(), // doesn't matter
                                  dur));
        if (you.can_see(*mon))
        {
            mprf("%s magic leaks into the air.",
                 apostrophise(mon->name(DESC_THE)).c_str());
        }

        if (agent() && (agent()->type == MONS_EYE_OF_DRAINING
                        || agent()->type == MONS_GHOST_MOTH))
        {
            agent()->heal(dur / BASELINE_DELAY);
        }
        obvious_effect = true;
        break;
    }

    case BEAM_TUKIMAS_DANCE:
        cast_tukimas_dance(ench_power, mon);
        obvious_effect = true;
        break;

    case BEAM_RESISTANCE:
        if (!mon->has_ench(ENCH_RESISTANCE)
            && mon->add_ench(ENCH_RESISTANCE))
        {
            if (simple_monster_message(*mon, " suddenly seems more resistant."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_UNRAVELLING:
        if (!monster_is_debuffable(*mon))
            return MON_UNAFFECTED;

        debuff_monster(*mon);
        _unravelling_explode(*this);
        return MON_AFFECTED;

    case BEAM_INFESTATION:
    {
        const int dur = (5 + random2avg(ench_power / 2, 2)) * BASELINE_DELAY;
        mon->add_ench(mon_enchant(ENCH_INFESTATION, 0, &you, dur));
        if (simple_monster_message(*mon, " is infested!"))
            obvious_effect = true;
        return MON_AFFECTED;
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
    if (!pierce)
        used = BEAM_STOP;
    else if (is_enchantment() && name != "line pass")
        used = (flavour == BEAM_DIGGING ? 0 : BEAM_STOP);
    // Damnation stops for nobody!
    else if (flavour == BEAM_DAMNATION)
        used = 0;
    // Generic explosion.
    else if (is_explosion || is_big_cloud())
        used = BEAM_STOP;
    // Lightning goes through things.
    else if (flavour == BEAM_ELECTRICITY)
        used = 0;
    else
        used = 1;

    // Assume we didn't hit, after all.
    if (is_tracer && source_id == MID_PLAYER && used > 0
        && hit < AUTOMATIC_HIT)
    {
        return 0;
    }

    if (in_explosion_phase)
        return used;

    return used;
}

// Information for how various explosions look & sound.
struct explosion_sfx
{
    // A message printed when the player sees the explosion.
    const char *seeMsg;
    // What the player hears when the explosion goes off unseen.
    const char *sound;
};

// A map from origin_spells to special explosion info for each.
const map<spell_type, explosion_sfx> spell_explosions = {
    { SPELL_HURL_DAMNATION, {
        "The sphere of damnation explodes!",
        "the wailing of the damned",
    } },
    { SPELL_CALL_DOWN_DAMNATION, {
        "The sphere of damnation explodes!",
        "the wailing of the damned",
    } },
    { SPELL_FIREBALL, {
        "The fireball explodes!",
        "an explosion",
    } },
    { SPELL_ORB_OF_ELECTRICITY, {
        "The orb of electricity explodes!",
        "a clap of thunder",
    } },
    { SPELL_FIRE_STORM, {
        "A raging storm of fire appears!",
        "a raging storm",
    } },
    { SPELL_MEPHITIC_CLOUD, {
        "The ball explodes into a vile cloud!",
        "a loud \'bang\'",
    } },
    { SPELL_GHOSTLY_FIREBALL, {
        "The ghostly flame explodes!",
        "the shriek of haunting fire",
    } },
    { SPELL_EXPLOSIVE_BOLT, {
        "The explosive bolt releases an explosion!",
        "an explosion",
    } },
    { SPELL_VIOLENT_UNRAVELLING, {
        "The enchantments explode!",
        "a sharp crackling", // radiation = geiger counter
    } },
    { SPELL_ICEBLAST, {
        "The mass of ice explodes!",
        "an explosion",
    } },
    { SPELL_GHOSTLY_SACRIFICE, {
        "The ghostly flame explodes!",
        "the shriek of haunting fire",
    } },
};

// Takes a bolt and refines it for use in the explosion function.
// Explosions which do not follow from beams bypass this function.
void bolt::refine_for_explosion()
{
    ASSERT(!special_explosion);

    string seeMsg;
    string hearMsg;

    if (ex_size == 0)
        ex_size = 1;
    glyph   = dchar_glyph(DCHAR_FIRED_BURST);

    // Assume that the player can see/hear the explosion, or
    // gets burned by it anyway.  :)
    msg_generated = true;

    if (item != nullptr)
    {
        seeMsg  = "The " + item->name(DESC_PLAIN, false, false, false)
                  + " explodes!";
        hearMsg = "You hear an explosion!";
    }
    else
    {
        const explosion_sfx *explosion = map_find(spell_explosions,
                                                  origin_spell);
        if (explosion)
        {
            seeMsg = explosion->seeMsg;
            hearMsg = make_stringf("You hear %s!", explosion->sound);
        }
        else
        {
            seeMsg  = "The beam explodes into a cloud of software bugs!";
            hearMsg = "You hear the sound of one hand!";
        }
    }

    if (origin_spell == SPELL_ORB_OF_ELECTRICITY)
    {
        colour     = LIGHTCYAN;
        ex_size    = 2;
    }

    if (!is_tracer && !seeMsg.empty() && !hearMsg.empty())
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
                mprf(MSGCH_SOUND, "%s", hearMsg.c_str());
        }
    }
}

typedef vector< vector<coord_def> > sweep_type;

static sweep_type _radial_sweep(int r)
{
    sweep_type result;

    // Center first.
    result.emplace_back(1, coord_def(0,0));

    for (int rad = 1; rad <= r; ++rad)
    {
        sweep_type::value_type work;

        for (int d = -rad; d <= rad; ++d)
        {
            // Don't put the corners in twice!
            if (d != rad && d != -rad)
            {
                work.emplace_back(-rad, d);
                work.emplace_back(+rad, d);
            }

            work.emplace_back(d, -rad);
            work.emplace_back(d, +rad);
        }
        result.push_back(work);
    }
    return result;
}

/** How much noise does an explosion this big make?
 *
 *  @param the size of the explosion (radius, not diamater)
 *  @returns how much noise it would make.
 */
int explosion_noise(int rad)
{
    return 10 + rad * 5;
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
    if (real_flavour == BEAM_CHAOS
        || real_flavour == BEAM_RANDOM
        || real_flavour == BEAM_CRYSTAL)
    {
        flavour = real_flavour;
    }
    else
        real_flavour = flavour;

    const int r = min(ex_size, MAX_EXPLOSION_RADIUS);
    in_explosion_phase = true;
    // being hit by bounces doesn't exempt you from the explosion (not that it
    // currently ever matters)
    hit_count.clear();

    if (is_sanctuary(pos()) && flavour != BEAM_VISUAL)
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
        loudness = explosion_noise(r);

        // Not an "explosion", but still a bit noisy at the target location.
        if (origin_spell == SPELL_INFESTATION)
            loudness = spell_effect_noise(SPELL_INFESTATION);

        // Lee's Rapid Deconstruction can target the tiles on the map
        // boundary.
        const coord_def noise_position = clamp_in_bounds(pos());
        bool heard_expl = noisy(loudness, noise_position, source_id);

        heard = heard || heard_expl;

        if (heard_expl && !explode_noise_msg.empty() && !you.see_cell(pos()))
            mprf(MSGCH_SOUND, "%s", explode_noise_msg.c_str());
    }

    // Run DFS to determine which cells are influenced
    explosion_map exp_map;
    exp_map.init(INT_MAX);
    determine_affected_cells(exp_map, coord_def(), 0, r, true, true);

    // We get a bit fancy, drawing all radius 0 effects, then radius
    // 1, radius 2, etc. It looks a bit better that way.
    const vector< vector<coord_def> > sweep = _radial_sweep(r);
    const coord_def centre(9,9);

    // Draw pass.
    if (!is_tracer)
    {
        for (const auto &line : sweep)
        {
            bool pass_visible = false;
            for (const coord_def delta : line)
            {
                if (delta.origin() && hole_in_the_middle)
                    continue;

                if (exp_map(delta + centre) < INT_MAX)
                    pass_visible |= explosion_draw_cell(delta + pos());
            }
            if (pass_visible)
            {
                update_screen();
                scaled_delay(explode_delay);
            }
        }
    }

    // Affect pass.
    int cells_seen = 0;
    for (const auto &line : sweep)
    {
        for (const coord_def delta : line)
        {
            if (delta.origin() && hole_in_the_middle)
                continue;

            if (exp_map(delta + centre) < INT_MAX)
            {
                if (you.see_cell(delta + pos()))
                    ++cells_seen;

                explosion_affect_cell(delta + pos());

                if (beam_cancelled) // don't spam prompts
                    return false;
            }
        }
    }

    // Delay after entire explosion has been drawn.
    if (!is_tracer && cells_seen > 0 && show_more)
        scaled_delay(explode_delay * 3);

    return cells_seen > 0;
}

/**
 * Draw one tile of an explosion, if that cell is visible.
 *
 * @param p The cell to draw, in grid coordinates.
 * @return True if the cell was actually drawn.
 */
bool bolt::explosion_draw_cell(const coord_def& p)
{
    if (you.see_cell(p))
    {
        const coord_def drawpos = grid2view(p);
        // bounds check
        if (in_los_bounds_v(drawpos))
        {
#ifdef USE_TILE
            int dist = (p - source).rdist();
            tileidx_t tile = tileidx_bolt(*this);
            tiles.add_overlay(p, vary_bolt_tile(tile, dist));
#endif
#ifndef USE_TILE_LOCAL
            cgotoxy(drawpos.x, drawpos.y, GOTO_DNGN);
            put_colour_ch(colour == BLACK ? random_colour(true)
                                          : element_colour(colour, false, p),
                          dchar_glyph(DCHAR_EXPLOSION));
#endif
            return true;
        }
    }
    return false;
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
        || delta.rdist() > r
        || count > 10*r
        || !map_bounds(loc)
        || is_sanctuary(loc) && flavour != BEAM_VISUAL)
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
        if (stop_at_walls && !(delta.origin() && can_affect_wall(loc)))
            return;
        // But remember that we are at a wall.
        at_wall = true;
    }

    if (feat_is_solid(dngn_feat) && !feat_is_wall(dngn_feat)
        && !can_affect_wall(loc) && stop_at_statues)
    {
        return;
    }

    m(delta + centre) = min(count, m(delta + centre));

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
        coord_def caster_pos = actor_by_mid(source_id) ?
                                   actor_by_mid(source_id)->pos() :
                                   you.pos();

        if (at_wall && !cell_see_cell(caster_pos, loc + Compass[i], LOS_NO_TRANS))
            continue;

        int cadd = 5;
        // Circling around the center is always free.
        if (delta.rdist() == 1 && new_delta.rdist() == 1)
            cadd = 0;
        // Otherwise changing direction (e.g. looking around a wall) costs more.
        else if (delta.x * Compass[i].x < 0 || delta.y * Compass[i].y < 0)
            cadd = 17;

        determine_affected_cells(m, new_delta, count + cadd, r,
                                 stop_at_statues, stop_at_walls);
    }
}

// Returns true if the beam is harmful ((mostly) ignoring monster
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
        return mon->res_holy_energy() < 3;

    // The orbs are made of pure disintegration energy. This also has the side
    // effect of not stopping us from firing further orbs when the previous one
    // is still flying.
    if (flavour == BEAM_DISINTEGRATION || flavour == BEAM_DEVASTATION)
        return mon->type != MONS_ORB_OF_DESTRUCTION;

    // Take care of other non-enchantments.
    if (!is_enchantment())
        return true;

    // Now for some non-hurtful enchantments.
    if (flavour == BEAM_DIGGING)
        return false;

    // Positive effects.
    if (nice_to(monster_info(mon)))
        return false;

    // Co-aligned inner flame is fine.
    if (flavour == BEAM_INNER_FLAME && mons_aligned(mon, agent()))
        return false;

    // Friendly and good neutral monsters don't mind being teleported.
    if (flavour == BEAM_TELEPORT)
        return !mon->wont_attack();

    if (flavour == BEAM_ENSLAVE_SOUL || flavour == BEAM_INFESTATION)
        return ench_flavour_affects_monster(flavour, mon);

    // sleep
    if (flavour == BEAM_HIBERNATION)
        return mon->can_hibernate();

    if (flavour == BEAM_SLOW || flavour == BEAM_PARALYSIS)
        return !mon->stasis();

    // dispel undead
    if (flavour == BEAM_DISPEL_UNDEAD)
        return bool(mon->holiness() & MH_UNDEAD);

    if (flavour == BEAM_PAIN)
        return mon->res_negative_energy() < 3;

    if (flavour == BEAM_AGONY)
        return !mon->res_torment();

    if (flavour == BEAM_TUKIMAS_DANCE)
        return tukima_affects(*mon);

    if (flavour == BEAM_UNRAVELLING)
        return monster_is_debuffable(*mon);

    // everything else is considered nasty by everyone
    return true;
}

// Return true if the bolt is considered nice by mon.
// This is not the inverse of nasty_to(): the bolt needs to be
// actively positive.
bool bolt::nice_to(const monster_info& mi) const
{
    // Polymorphing a (very) ugly thing will mutate it into a different
    // (very) ugly thing.
    if (flavour == BEAM_POLYMORPH)
    {
        return mi.type == MONS_UGLY_THING
               || mi.type == MONS_VERY_UGLY_THING;
    }

    if (flavour == BEAM_HASTE
        || flavour == BEAM_HEALING
        || flavour == BEAM_MIGHT
        || flavour == BEAM_AGILITY
        || flavour == BEAM_INVISIBILITY
        || flavour == BEAM_RESISTANCE)
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
// bolt
// TODO: Eventually it'd be nice to have a proper factory for these things
// (extended from setup_mons_cast() and zapping() which act as limited ones).

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

void bolt::set_agent(const actor *actor)
{
    // nullptr actor is fine by us.
    if (!actor)
        return;

    source_id = actor->mid;

    if (actor->is_player())
        thrower = KILL_YOU_MISSILE;
    else
        thrower = KILL_MON_MISSILE;
}

/**
 * Who caused this beam?
 *
 * @param ignore_reflection If true, look all the way back to the original
 *                          source; if false (the default), treat the latest
 *                          actor to reflect this as the source.
 * @returns The actor that can be treated as the source. May be null if
 *          it's a now-dead monster, or if neither the player nor a monster
 *          caused it (for example, divine retribution).
 */
actor* bolt::agent(bool ignore_reflection) const
{
    killer_type nominal_ktype = thrower;
    mid_t nominal_source = source_id;

    // If the beam was reflected report a different point of origin
    if (reflections > 0 && !ignore_reflection)
    {
        if (reflector == MID_PLAYER || source_id == MID_PLAYER)
            return &menv[YOU_FAULTLESS];
        nominal_source = reflector;
    }
    if (YOU_KILL(nominal_ktype))
        return &you;
    else
        return actor_by_mid(nominal_source);
}

bool bolt::is_enchantment() const
{
    return flavour >= BEAM_FIRST_ENCHANTMENT
           && flavour <= BEAM_LAST_ENCHANTMENT;
}

string bolt::get_short_name() const
{
    if (!short_name.empty())
        return short_name;

    if (item != nullptr && item->defined())
    {
        return item->name(DESC_A, false, false, false, false,
                          ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK);
    }

    if (real_flavour == BEAM_RANDOM
        || real_flavour == BEAM_CHAOS
        || real_flavour == BEAM_CRYSTAL)
    {
        return _beam_type_name(real_flavour);
    }

    if (flavour == BEAM_FIRE
        && (origin_spell == SPELL_STICKY_FLAME
            || origin_spell == SPELL_STICKY_FLAME_RANGE))
    {
        return "sticky fire";
    }

    if (flavour == BEAM_ELECTRICITY && pierce)
        return "lightning";

    if (origin_spell == SPELL_ISKENDERUNS_MYSTIC_BLAST
        || origin_spell == SPELL_DAZZLING_SPRAY)
    {
        return "energy";
    }

    if (name == "bolt of dispelling energy")
        return "dispelling energy";

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
    case BEAM_DAMNATION:             return "damnation";
    case BEAM_STICKY_FLAME:          return "sticky fire";
    case BEAM_STEAM:                 return "steam";
    case BEAM_ENERGY:                return "energy";
    case BEAM_HOLY:                  return "cleansing flame";
    case BEAM_FRAG:                  return "fragments";
    case BEAM_LAVA:                  return "magma";
    case BEAM_ICE:                   return "ice";
    case BEAM_DEVASTATION:           return "devastation";
    case BEAM_RANDOM:                return "random";
    case BEAM_CHAOS:                 return "chaos";
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
    case BEAM_AGONY:                 return "agony";
    case BEAM_DISPEL_UNDEAD:         return "dispel undead";
    case BEAM_DISINTEGRATION:        return "disintegration";
    case BEAM_BLINK:                 return "blink";
    case BEAM_BLINK_CLOSE:           return "blink close";
    case BEAM_BECKONING:             return "beckoning";
    case BEAM_PETRIFY:               return "petrify";
    case BEAM_CORONA:                return "backlight";
    case BEAM_PORKALATOR:            return "porkalator";
    case BEAM_HIBERNATION:           return "hibernation";
    case BEAM_SLEEP:                 return "sleep";
    case BEAM_BERSERK:               return "berserk";
    case BEAM_VISUAL:                return "visual effects";
    case BEAM_TORMENT_DAMAGE:        return "torment damage";
    case BEAM_AIR:                   return "air";
    case BEAM_INNER_FLAME:           return "inner flame";
    case BEAM_PETRIFYING_CLOUD:      return "calcifying dust";
    case BEAM_ENSNARE:               return "magic web";
    case BEAM_SENTINEL_MARK:         return "sentinel's mark";
    case BEAM_DIMENSION_ANCHOR:      return "dimension anchor";
    case BEAM_VULNERABILITY:         return "vulnerability";
    case BEAM_MALIGN_OFFERING:       return "malign offering";
    case BEAM_VIRULENCE:             return "virulence";
    case BEAM_AGILITY:               return "agility";
    case BEAM_SAP_MAGIC:             return "sap magic";
    case BEAM_CRYSTAL:               return "crystal bolt";
    case BEAM_DRAIN_MAGIC:           return "drain magic";
    case BEAM_TUKIMAS_DANCE:         return "tukima's dance";
    case BEAM_BOUNCY_TRACER:         return "bouncy tracer";
    case BEAM_DEATH_RATTLE:          return "breath of the dead";
    case BEAM_RESISTANCE:            return "resistance";
    case BEAM_UNRAVELLING:           return "unravelling";
    case BEAM_UNRAVELLED_MAGIC:      return "unravelled magic";
    case BEAM_SHARED_PAIN:           return "shared pain";
    case BEAM_IRRESISTIBLE_CONFUSION:return "confusion";
    case BEAM_INFESTATION:           return "infestation";

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

/**
 * Can this bolt knock back an actor?
 *
 * The bolts that knockback flying actors or actors only when damage
 * is dealt will return when.
 *
 * @param act The target actor. If not-nullptr, check if the actor is flying for
 *            bolts that knockback flying actors.
 * @param dam The damage dealt. If non-negative, check that dam > 0 for bolts
 *             like force bolt that only push back upon damage.
 * @return True if the bolt could knockback the actor, false otherwise.
*/
bool bolt::can_knockback(const actor *act, int dam) const
{
    return flavour == BEAM_WATER && origin_spell == SPELL_PRIMAL_WAVE
           || origin_spell == SPELL_CHILLING_BREATH
              && (!act || act->airborne())
           || origin_spell == SPELL_FORCE_LANCE && dam;
}

void clear_zap_info_on_exit()
{
    for (const zap_info &zap : zap_data)
    {
        delete zap.player_damage;
        delete zap.player_tohit;
        delete zap.monster_damage;
        delete zap.monster_tohit;
    }
}

int ench_power_stepdown(int pow)
{
    return stepdown_value(pow, 30, 40, 100, 120);
}

/// Translate a given ench power to a duration, in aut.
int _ench_pow_to_dur(int pow)
{
    // ~15 turns at 25 pow, ~21 turns at 50 pow, ~27 turns at 100 pow
    return stepdown(pow * BASELINE_DELAY, 70);
}

// Can a particular beam go through a particular monster?
// Fedhas worshipers can shoot through non-hostile plants,
// and players can shoot through their demonic guardians.
bool shoot_through_monster(const bolt& beam, const monster* victim)
{
    actor *originator = beam.agent();
    if (!victim || !originator)
        return false;

    bool origin_worships_fedhas;
    mon_attitude_type origin_attitude;
    if (originator->is_player())
    {
        origin_worships_fedhas = have_passive(passive_t::shoot_through_plants);
        origin_attitude = ATT_FRIENDLY;
    }
    else
    {
        monster* temp = originator->as_monster();
        if (!temp)
            return false;
        origin_worships_fedhas = temp->god == GOD_FEDHAS;
        origin_attitude = temp->attitude;
    }

    return (origin_worships_fedhas
            && fedhas_protects(*victim))
           || (originator->is_player()
               && testbits(victim->flags, MF_DEMONIC_GUARDIAN))
           && !beam.is_enchantment()
           && beam.origin_spell != SPELL_CHAIN_LIGHTNING
           && (mons_atts_aligned(victim->attitude, origin_attitude)
               || victim->neutral());
}

/**
 * Given some shield value, what is the chance that omnireflect will activate
 * on an AUTOMATIC_HIT attack?
 *
 * E.g., if 40 is returned, there is a SH in 40 chance of a given attack being
 * reflected.
 *
 * @param SH        The SH (shield) value of the omnireflect user.
 * @return          A denominator to the chance of omnireflect activating.
 */
int omnireflect_chance_denom(int SH)
{
    return SH + 40;
}

/// Set up a beam aiming from the given monster to their target.
bolt setup_targetting_beam(const monster &mons)
{
    bolt beem;

    beem.source    = mons.pos();
    beem.target    = mons.target;
    beem.source_id = mons.mid;

    return beem;
}

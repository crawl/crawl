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
#include "attack.h"
#include "attitude-change.h"
#include "bloodspatter.h"
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
#include "fineff.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::convert_orcs
#include "item-use.h"
#include "item-prop.h"
#include "items.h"
#include "killer-type.h"
#include "libutil.h"
#include "losglobal.h"
#include "los.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-explode.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "options.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "ranged-attack.h"
#include "religion.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-monench.h"
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
#include "tiles-build-specific.h"
#include "transform.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

// Helper functions (some of these should probably be public).
static void _ench_animation(int flavour, const monster* mon = nullptr,
                            bool force = false);
static beam_type _chaos_beam_flavour(bolt* beam);
static string _beam_type_name(beam_type type);
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

bolt::bolt() : animate(bool(Options.use_animations & UA_BEAM)) {}

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
            && origin_spell != SPELL_GLACIATE
            && flavour != BEAM_VAMPIRIC_DRAINING; // buggy :(
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
        view_add_tile_overlay(p, tileidx_zap(colour));
#endif
        view_add_glyph_overlay(p, {dchar_glyph(DCHAR_FIRED_ZAP),
                                   static_cast<unsigned short>(colour)});
        animation_delay(50, true);
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
    case BEAM_CURSE_OF_AGONY:
    case BEAM_VILE_CLUTCH:
    case BEAM_VAMPIRIC_DRAINING:
    case BEAM_SOUL_SPLINTER:
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
    case BEAM_ROOTS:
        elem = ETC_EARTH;
        break;
    default:
        elem = ETC_ENCHANT;
        break;
    }

    _zap_animation(element_colour(elem), mon, force);
}

// If needs_tracer is true, we need to check the beam path for friendly
// monsters.
spret zapping(zap_type ztype, int power, bolt &pbolt,
                   bool needs_tracer, const char* msg, bool fail)
{
    dprf(DIAG_BEAM, "zapping: power=%d", power);

    pbolt.thrower = KILL_YOU_MISSILE;

    // Check whether tracer goes through friendlies.
    // NOTE: Whenever zapping() is called with a randomised value for power
    // (or effect), player_tracer should be called directly with the highest
    // power possible respecting current skill, experience level, etc.
    if (needs_tracer && !player_tracer(ztype, power, pbolt))
        return spret::abort;

    fail_check();
    // Fill in the bolt structure.
    zappy(ztype, power, false, pbolt);

    if (msg)
        mpr(msg);

    if (ztype == ZAP_DIG)
        pbolt.aimed_at_spot = false;

    pbolt.fire();

    return spret::success;
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
    pbolt.overshoot_prompt = false;
    pbolt.passed_target = false;

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
    pbolt.loudness      = 0;

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

    if (pbolt.friendly_past_target)
        pbolt.aimed_at_spot = true;

    // Set to non-tracing for actual firing.
    pbolt.is_tracer = false;
    return true;
}

// Returns true if the player wants / needs to abort based on god displeasure
// with targeting this target with this spell. Returns false otherwise.
static bool _stop_because_god_hates_target_prompt(monster* mon,
                                                  spell_type spell)
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
    virtual T operator()(int pow, bool random = true) const = 0;
    virtual ~power_deducer() {}
};

typedef power_deducer<int> tohit_deducer;

template<int adder, int mult_num = 0, int mult_denom = 1>
class tohit_calculator : public tohit_deducer
{
public:
    int operator()(int pow, bool /*random*/) const override
    {
        return adder + pow * mult_num / mult_denom;
    }
};

typedef power_deducer<dice_def> dam_deducer;

template<int numdice, int adder, int mult_num, int mult_denom>
class dicedef_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow, bool /*random*/) const override
    {
        return dice_def(numdice, adder + pow * mult_num / mult_denom);
    }
};

template<int numdice, int adder, int mult_num, int mult_denom>
class calcdice_calculator : public dam_deducer
{
public:
    dice_def operator()(int pow, bool random) const override
    {
        return calc_dice(numdice, adder + pow * mult_num / mult_denom, random);
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
    bool can_beam;
    bool is_explosion;
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

bool zap_explodes(zap_type z_type)
{
    const zap_info* zinfo = _seek_zap(z_type);
    return zinfo && zinfo->is_explosion;
}

bool zap_is_enchantment(zap_type z_type)
{
    const zap_info* zinfo = _seek_zap(z_type);
    return zinfo && zinfo->is_enchantment;
}

int zap_to_hit(zap_type z_type, int power, bool is_monster)
{
    const zap_info* zinfo = _seek_zap(z_type);
    if (!zinfo)
        return 0;
    const tohit_deducer* hit_calc = is_monster ? zinfo->monster_tohit
                                               : zinfo->player_tohit;
    if (zinfo->is_enchantment)
        return 0;
    ASSERT(hit_calc);
    const int hit = (*hit_calc)(power);
    if (hit != AUTOMATIC_HIT && !is_monster && crawl_state.need_save)
        return max(0, hit - you.inaccuracy_penalty());
    return hit;
}

dice_def zap_damage(zap_type z_type, int power, bool is_monster, bool random)
{
    const zap_info* zinfo = _seek_zap(z_type);
    if (!zinfo)
        return dice_def(0,0);
    const dam_deducer* dam_calc = is_monster ? zinfo->monster_damage
                                             : zinfo->player_damage;
    return dam_calc ? (*dam_calc)(power, random) : dice_def(0,0);
}

colour_t zap_colour(zap_type z_type)
{
    const zap_info* zinfo = _seek_zap(z_type);
    if (!zinfo)
        return BLACK;
    return zinfo->colour;
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

static int _zap_loudness(zap_type zap, spell_type spell)
{
    const zap_info* zinfo = _seek_zap(zap);
    const int noise = spell_effect_noise(spell);
    const spell_flags flags = get_spell_flags(spell);

    // Explosions have noise handled separately.
    if (zinfo->is_explosion || testbits(flags, spflag::silent))
        return 0;
    else if (noise > 0)
        return noise;
    // Enchantments are (usually) silent.
    else if (zinfo->is_enchantment)
        return 0;
    else if (spell != SPELL_NO_SPELL)
        return spell_difficulty(spell);

    return 0;
}

#ifdef WIZARD
static bool _needs_monster_zap(const zap_info &zinfo)
{
    // for enchantments we can't know from this data
    if (zinfo.is_enchantment)
        return false;
    // if player tohit is missing from a non-enchantment, then it has to use
    // a fake monster cast
    if (!zinfo.player_tohit)
        return true;

    // otherwise, it should be possible to use a player zap (damage may or may
    // not be defined)
    return false;
}
#endif

void zappy(zap_type z_type, int power, bool is_monster, bolt &pbolt)
{
    const zap_info* zinfo = _seek_zap(z_type);

    // None found?
    if (zinfo == nullptr)
    {
        dprf("Couldn't find zap type %d", z_type);
        return;
    }

#ifdef WIZARD
    // we are in a wizmode cast scenario: use monster zap data to avoid crashes.
    // N.b. this suppresses some player effects, such as inaccuracy.
    if (!is_monster && you.wizard && _needs_monster_zap(*zinfo))
        is_monster = true;
#endif

    // Fill
    pbolt.name           = zinfo->name;
    pbolt.flavour        = zinfo->flavour;
    pbolt.real_flavour   = zinfo->flavour;
    pbolt.colour         = zinfo->colour;
    pbolt.glyph          = dchar_glyph(zinfo->glyph);
    pbolt.pierce         = zinfo->can_beam;
    pbolt.is_explosion   = zinfo->is_explosion;

    if (zinfo->player_power_cap > 0 && !is_monster)
        power = min(zinfo->player_power_cap, power);

    ASSERT(zinfo->is_enchantment == pbolt.is_enchantment());

    pbolt.ench_power = zap_ench_power(z_type, power, is_monster);

    if (zinfo->is_enchantment)
        pbolt.hit = AUTOMATIC_HIT;
    else
        pbolt.hit = zap_to_hit(z_type, power, is_monster);

    pbolt.damage = zap_damage(z_type, power, is_monster);

    if (pbolt.origin_spell == SPELL_NO_SPELL)
        pbolt.origin_spell = zap_to_spell(z_type);

    if (pbolt.loudness == 0)
        pbolt.loudness = _zap_loudness(z_type, pbolt.origin_spell);
}

bool bolt::can_affect_actor(const actor *act) const
{
    // Blinkbolt doesn't hit its caster, since they are the bolt.
    if (origin_spell == SPELL_BLINKBOLT && act->mid == source_id)
        return false;
    // Damnation doesn't blast the one firing.
    else if (item
            && item->props.exists(DAMNATION_BOLT_KEY)
            && act->mid == source_id)
    {
        return false;
    }
    // Xak'krixis' prisms are smart enough not to affect friendlies
    else if (origin_spell == SPELL_FULMINANT_PRISM && thrower == KILL_MON
        && act->temp_attitude() == attitude)
    {
        return false;
    }
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

    return true;
}

// Choose the beam effect for BEAM_CHAOS that's analogous to the effect used by
// SPWPN_CHAOS, with weightings similar to those use by that brand. XXX: Rework
// this and SPWPN_CHAOS to use the same tables.
static beam_type _chaos_beam_flavour(bolt* beam)
{
    UNUSED(beam);

    beam_type flavour;
    flavour = random_choose_weighted(
         // SPWPN_CHAOS randomizes to brands analogous to these beam effects
         // with similar weights.
         80, BEAM_FIRE,
         80, BEAM_COLD,
         80, BEAM_ELECTRICITY,
         80, BEAM_POISON,
         // Combined weight from drain + vamp.
         80, BEAM_NEG,
         40, BEAM_HOLY,
         40, BEAM_DRAIN_MAGIC,
         16, BEAM_FOUL_FLAME,
         // From here are beam effects analogous to effects that happen when
         // SPWPN_CHAOS chooses itself again as the ego (roughly 1/7 chance).
         // Weights similar to those from chaos_effects in attack.cc
          5, BEAM_BERSERK,
         12, BEAM_HASTE,
         12, BEAM_MIGHT,
         10, BEAM_RESISTANCE,
         10, BEAM_SLOW,
         12, BEAM_CONFUSION,
         10, BEAM_WEAKNESS,
         10, BEAM_VULNERABILITY,
         10, BEAM_ACID,
          5, BEAM_VITRIFY,
          5, BEAM_ENSNARE,
          3, BEAM_BLINK,
          3, BEAM_PARALYSIS,
          3, BEAM_PETRIFY,
          3, BEAM_SLEEP,
         // Combined weight for poly and clone effects.
          4, BEAM_POLYMORPH,
          5, BEAM_LIGHT);

    return flavour;
}

dice_def combustion_breath_damage(int pow, bool allow_random)
{
    if (allow_random)
        return dice_def(3, 4 + div_rand_round(pow * 10, 9));
    else
        return dice_def(3, 4 + pow * 10 / 9);
}

static void _combustion_breath_explode(bolt *parent, coord_def pos)
{
    bolt beam;

    bolt_parent_init(*parent, beam);
    beam.name         = "fiery explosion";
    beam.aux_source   = "combustion breath";
    beam.damage       = combustion_breath_damage(parent->ench_power);
    beam.colour       = RED;
    beam.flavour      = BEAM_FIRE;
    beam.is_explosion = true;
    beam.source       = pos;
    beam.target       = pos;
    beam.origin_spell = SPELL_COMBUSTION_BREATH;
    beam.refine_for_explosion();
    beam.explode();
    parent->friend_info += beam.friend_info;
    parent->foe_info    += beam.foe_info;
    if (beam.is_tracer && beam.beam_cancelled)
        parent->beam_cancelled = true;
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
    // env.mons[YOU_FAULTLESS] cannot be safely queried for properties like
    // can_see_invisible.
    if (reflections > 0)
        nightvision = can_see_invis = true;
    else
        precalc_agent_properties();

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

void bolt::precalc_agent_properties()
{
    const actor* a = agent();
    // XXX: Should non-agents count as seeing invisible?
    if (!a) return;
    nightvision = a->nightvision();
    can_see_invis = a->can_see_invisible();
}

void bolt::apply_beam_conducts()
{
    if (!is_tracer && YOU_KILL(thrower))
    {
        switch (flavour)
        {
        case BEAM_DAMNATION:
        {
            const int level = 2 + random2(3);
            did_god_conduct(DID_EVIL, level, god_cares());
            break;
        }
        default:
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
void bolt::draw(const coord_def& p, bool force_refresh)
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
        view_add_tile_overlay(p, vary_bolt_tile(tile_beam, dist));
    }
#endif
    const unsigned short c = colour == BLACK ? random_colour(true)
                                             : element_colour(colour);
    view_add_glyph_overlay(p, {glyph, c});

    // If reduce_animations is set, the redraw is unnecessary and
    // should be done only once outside the loop calling the bolt::draw
    if (Options.reduce_animations)
        return;

    // to avoid redraws, set force_refresh = false, and draw_delay = 0. This
    // will still force a refresh if there is a draw_delay regardless of the
    // param, because a delay on drawing is pretty meaningless without a
    // redraw.
    if (force_refresh || draw_delay > 0)
        animation_delay(draw_delay, true);
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

    // Keep length of bounced spells constant despite reduced los (scarf of
    // shadows, Robe of Night)
    if (bounces == 1)
    {
        extra_range_used -= spell_range(origin_spell, ench_power, true, true)
                            - range;
    }

    ASSERT(!cell_is_solid(ray.pos()));
}

void bolt::fake_flavour()
{
    if (real_flavour == BEAM_RANDOM)
        flavour = static_cast<beam_type>(random_range(BEAM_FIRST_RANDOM, BEAM_LAST_RANDOM));
    else if (real_flavour == BEAM_CHAOS)
        flavour = _chaos_beam_flavour(this);
}

void bolt::digging_wall_effect()
{
    if (env.markers.property_at(pos(), MAT_ANY, "veto_destroy") == "veto")
    {
        finish_beam();
        return;
    }

    const dungeon_feature_type feat = env.grid(pos());
    if (feat_is_diggable(feat))
    {
        // If this is temporary terrain and the underlying terrain is *not*
        // diggable, just revert it to whatever it was before. But if the
        // original terrain was also diggable, simply destroy it.
        if (is_temp_terrain(pos()))
        {
            revert_terrain_change(pos());
            if (feat_is_diggable(env.grid(pos())))
                destroy_wall(pos());
        }
        else
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
                return;
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
    }
    else if (feat_is_wall(feat))
        finish_beam();
}

void bolt::burn_wall_effect()
{
    dungeon_feature_type feat = env.grid(pos());
    // Fire only affects things that can burn.
    if (!feat_is_flammable(feat)
        || env.markers.property_at(pos(), MAT_ANY, "veto_destroy") == "veto"
        || !can_burn_trees()) // sanity
    {
        finish_beam();
        return;
    }

    if (have_passive(passive_t::shoot_through_plants))
    {
        emit_message("Fedhas protects the tree from harm.");
        finish_beam();
        return;
    }

    if (you.see_cell(pos()))
    {
        if (feat == DNGN_TREE)
            emit_message("The tree burns like a torch!");
        else if (feat == DNGN_MANGROVE)
            emit_message("The mangrove smoulders and burns.");
        else if (feat == DNGN_DEMONIC_TREE)
            emit_message("The demonic tree burns, releasing chaotic energy.");
    }
    else if (you.can_smell())
        emit_message("You smell burning wood.");

    // If the tree we're destroying is temporary, immediately revert
    // terrain changes on this tile rather than permanently changing it.
    if (is_temp_terrain(pos()))
        revert_terrain_change(pos());
    else
        destroy_wall(pos());

    // But burning a temporary tree that was created on open ground can still
    // start a fire.
    if (!cell_is_solid(pos()))
    {
        if (feat == DNGN_TREE)
            place_cloud(CLOUD_FOREST_FIRE, pos(), random2(30)+25, agent());
        // Mangroves do not burn so readily.
        else if (feat == DNGN_MANGROVE)
            place_cloud(CLOUD_FIRE, pos(), random2(12)+5, agent());
        // Demonic trees produce a chaos cloud instead of fire.
        else if (feat == DNGN_DEMONIC_TREE)
            place_cloud(CLOUD_CHAOS, pos(), random2(30)+25, agent());
    }
    // If a tree turned back into a wall, place some fire around it to simulate
    // the normal burning effect without removing the wall.
    else if (feat == DNGN_TREE)
    {
        for (adjacent_iterator ai(pos()); ai; ++ai)
        {
            if (!in_bounds(*ai) || cloud_at(*ai) || is_sanctuary(*ai)
                || cell_is_solid(*ai) || !cell_see_cell(*ai, source, LOS_NO_TRANS))
            {
                continue;
            }

            if (one_chance_in(3))
                place_cloud(CLOUD_FIRE, *ai, random_range(11, 25), agent());
        }
    }

    obvious_effect = true;

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
        if (!in_bounds(pos()) || !can_affect_wall(pos(), true))
            finish_beam();

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
    fake_flavour();

    // Note that this can change the solidity of the wall.
    if (cell_is_solid(pos()))
        affect_wall();

    if (origin_spell == SPELL_CHAIN_LIGHTNING && pos() != target)
        return;

    // If the player can ever walk through walls, this will need
    // special-casing too.
    bool hit_player = found_player() && !ignores_player();
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
                && (!is_tracer || (agent() && m->visible_to(agent()))))
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
    orig.aimed_at_feet    = copy.aimed_at_feet;
    orig.extra_range_used = copy.extra_range_used;
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
        tile_beam = tileidx_item_throw(
                                get_item_known_info(*item), diff.x, diff.y);
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

        const dungeon_feature_type feat = env.grid(pos());

        if (in_bounds(target)
            // Starburst beams are essentially untargeted; some might even hit
            // a victim if others have LOF blocked.
            && origin_spell != SPELL_STARBURST
            // We ran into a solid wall with a real beam...
            && (feat_is_solid(feat)
                && flavour != BEAM_DIGGING && flavour <= BEAM_LAST_REAL
                && !cell_is_solid(target)
            // Or hit a monster that'll stop our beam...
                || at_blocking_monster())
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

            string blockee;
            if (mon && mon->observable())
                blockee = mon->name(DESC_THE);
            else
            {
                blockee = "the targeted "
                        + feature_description_at(target, false, DESC_PLAIN);
            }

            const string blocker = feat_is_solid(feat) ?
                        feature_description_at(pos(), false, DESC_A) :
                        monster_at(pos())->name(DESC_A);

            mprf("Your line of fire to %s is blocked by %s.",
                 blockee.c_str(), blocker.c_str());
            beam_cancelled = true;
            finish_beam();
            return;
        }

        // digging is taken care of in affect_cell
        if (feat_is_solid(feat) && !can_affect_wall(pos())
                                                    && flavour != BEAM_DIGGING)
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

        // Roots only have an effect during explosions.
        if (flavour == BEAM_ROOTS)
        {
            if (cell_is_solid(pos()))
                affect_wall();
            const actor *victim = actor_at(pos());
            if (victim
                && !ignores_monster(victim->as_monster())
                && (!is_tracer || agent()->can_see(*victim)))
            {
                finish_beam();
            }
        }
        else if (!affects_nothing)
            affect_cell();

        if (range_used() > range)
            break;

        if (beam_cancelled)
            return;

        // Weapons of returning should find an inverse ray
        // through find_ray and setup_retrace, but they didn't
        // always in the past, and we don't want to crash
        // if they accidentally pass through a corner.
        // Dig tracers continue through unseen cells.
        ASSERT(!cell_is_solid(pos())
               || is_tracer && can_affect_wall(pos(), true)
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
            draw(pos(), redraw_per_cell);

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

    // final delay for any draws that happened in the above loop
    if (animate && Options.reduce_animations)
        animation_delay(15 + draw_delay, true);

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
            if (original > 0 && doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");
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
        if (hurted > original && doFlavouredEffects)
            simple_monster_message(*mons, " is doused terribly!");
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (!hurted)
        {
            if (original > 0 && doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");
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

    case BEAM_STUN_BOLT:
    case BEAM_ELECTRICITY:
    case BEAM_THUNDER:
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (!hurted)
        {
            if (original > 0 && doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");
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
            if (original > 0 && doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");
        }
        else if (mons->res_acid() <= 0 && doFlavouredEffects)
            mons->acid_corrode(5);
        break;
    }

    case BEAM_POISON:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);

        if (!hurted && doFlavouredEffects && original > 0)
            simple_monster_message(*mons, " completely resists.");
        else if (doFlavouredEffects && !one_chance_in(3))
        {
            if (pbolt.origin_spell == SPELL_SPIT_POISON &&
                pbolt.agent(true)->is_monster() &&
                pbolt.agent(true)->as_monster()->has_ench(ENCH_CONCENTRATE_VENOM)
                && mons->res_poison() <= 0)
            {
                // set the cause to the reflected agent for piety &c.
                //
                // (7 bonus poison will apply exactly 1 additional level of
                // poison to monsters, mirroring the normal effect of BEAM_POISON)
                curare_actor(pbolt.agent(), mons, "concentrated venom",
                             pbolt.agent(true)->name(DESC_PLAIN), 7);
            }
            else
                poison_monster(mons, pbolt.agent());
        }

        break;
    }

    case BEAM_POISON_ARROW:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        const int stacks = pbolt.origin_spell == SPELL_STING ? 1 : 4;
        if (hurted < original)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message(*mons, " partially resists.");
                poison_monster(mons, pbolt.agent(), div_rand_round(stacks, 2),
                               true);
            }
        }
        else if (doFlavouredEffects)
            poison_monster(mons, pbolt.agent(), stacks, true);

        break;
    }

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

            mons->drain(pbolt.agent());

            if (YOU_KILL(pbolt.thrower))
                did_god_conduct(DID_EVIL, 2, pbolt.god_cares());
        }
        break;

    case BEAM_MIASMA:
        if (mons->res_miasma())
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
    case BEAM_FOUL_FLAME:
    {
        hurted = resist_adjust_damage(mons, pbolt.flavour, hurted);
        if (doFlavouredEffects && original > 0
            && (!hurted || hurted != original))
        {
            simple_monster_message(*mons, hurted == 0 ? " completely resists." :
                                    hurted < original ? " resists." :
                                    " writhes in agony!");
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
                simple_monster_message(*mons, " completely resists.");

            hurted = 0;
        }
        break;

    case BEAM_MEPHITIC:
        if (mons->res_poison() > 0)
        {
            if (original > 0 && doFlavouredEffects)
                simple_monster_message(*mons, " completely resists.");

            hurted = 0;
        }
        break;

    case BEAM_SPORE:
        if (mons->type == MONS_BALLISTOMYCETE)
            hurted = 0;
        break;

    case BEAM_ENSNARE:
        if (doFlavouredEffects)
            ensnare(mons);
        break;

    case BEAM_DEVASTATION:
        if (doFlavouredEffects)
            mons->strip_willpower(pbolt.agent(), random_range(20, 30));
        break;

    case BEAM_CRYSTALLIZING:
        if (doFlavouredEffects && x_chance_in_y(2, 3))
        {
            bool had_status = mons->has_ench(ENCH_VITRIFIED);
            mons->add_ench(mon_enchant(ENCH_VITRIFIED, 0, pbolt.agent(),
                           random_range(8, 18) * BASELINE_DELAY));
            {
                if (you.can_see(*mons))
                {
                    if (had_status)
                    {
                        mprf("%s looks even more glass-like.",
                             mons->name(DESC_THE).c_str());
                    }
                    else
                    {
                        mprf("%s becomes as fragile as glass!",
                             mons->name(DESC_THE).c_str());
                    }
                }
            }
        }
        break;

    case BEAM_UMBRAL_TORCHLIGHT:
        if (mons->holiness() & ~(MH_NATURAL | MH_DEMONIC | MH_HOLY))
        {
            if (doFlavouredEffects && !mons_aligned(mons, pbolt.agent(true)))
                simple_monster_message(*mons, " completely resists.");

            hurted = 0;
        }
        break;

    case BEAM_WARPING:
        if (doFlavouredEffects
            && x_chance_in_y(get_warp_space_chance(pbolt.ench_power), 100))
        {
            monster_blink(mons);
        }
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

// XXX: change mons to a reference
static bool _monster_resists_mass_enchantment(monster* mons,
                                              enchant_type wh_enchant,
                                              int pow,
                                              bool* did_msg)
{
    // Mass enchantments around lots of plants/fungi shouldn't cause a flood
    // of "is unaffected" messages. --Eino
    if (mons_is_firewood(*mons))
        return true;
    // Jiyva protects from mass enchantments.
    if (have_passive(passive_t::neutral_slimes) && god_protects(*mons))
        return true;

    switch (wh_enchant)
    {
    case ENCH_FEAR:
        if (mons->friendly())
            return true;

        if (!mons->can_feel_fear(true))
        {
            if (simple_monster_message(*mons, " is unaffected."))
                *did_msg = true;
            return true;
        }
        break;
    case ENCH_FRENZIED:
        if (!mons->can_go_frenzy())
        {
            if (simple_monster_message(*mons, " is unaffected."))
                *did_msg = true;
            return true;
        }
        break;
    case ENCH_ANGUISH:
        if (mons->friendly())
            return true;
        if (mons_intel(*mons) <= I_BRAINLESS)
        {
            if (simple_monster_message(*mons, " is unaffected."))
                *did_msg = true;
            return true;
        }
        break;
    default:
        break;
    }

    int res_margin = mons->check_willpower(&you, pow);
    if (res_margin > 0)
    {
        behaviour_event(mons, ME_ALERT, &you);
        if (simple_monster_message(*mons,
                mons->resist_margin_phrase(res_margin).c_str()))
        {
            *did_msg = true;
        }
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
spret mass_enchantment(enchant_type wh_enchant, int pow, bool fail)
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

        if ((wh_enchant == ENCH_FRENZIED && mi->go_frenzy(&you))
            || (wh_enchant != ENCH_FRENZIED
                && mi->add_ench(mon_enchant(wh_enchant, 0, &you))))
        {
            // Do messaging.
            const char* msg = nullptr;
            if (wh_enchant == ENCH_FEAR)
                msg = " looks frightened!";
            else if (wh_enchant == ENCH_ANGUISH)
                msg = " is haunted by guilt!";

            if (msg && simple_monster_message(**mi, msg))
                did_msg = true;

            // Extra check for fear (monster needs to reevaluate behaviour).
            if (wh_enchant == ENCH_FEAR)
                behaviour_event(*mi, ME_SCARE, &you);
            else if (wh_enchant == ENCH_ANGUISH)
                behaviour_event(*mi, ME_ALERT, &you);
        }
    }

    if (!did_msg)
        canned_msg(MSG_NOTHING_HAPPENS);

    if (wh_enchant == ENCH_FRENZIED)
        did_god_conduct(DID_HASTY, 8, true);

    return spret::success;
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

static bool _curare_hits_monster(actor *agent, monster* mons, int bonus_poison)
{
    if (!mons->alive())
        return false;

    if (mons->res_poison() > 0)
        return false;

    poison_monster(mons, agent, 2 + (bonus_poison / 7), true);

    simple_monster_message(*mons, " struggles to breathe.");
    mons->hurt(agent, roll_dice(2, 6), BEAM_POISON);

    if (mons->alive())
    {
        if (!mons->cannot_act())
        {
            simple_monster_message(*mons, mons->has_ench(ENCH_SLOW)
                                         ? " seems to be slow for longer."
                                         : " seems to slow down.");
        }
        mons->add_ench(mon_enchant (ENCH_SLOW, 0, agent));
    }

    return true;
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

// Actually poisons and/or slows a monster with miasma (with message).
bool miasma_monster(monster* mons, const actor* who)
{
    if (!mons->alive())
        return false;

    if (mons->res_miasma())
        return false;

    bool success = false;
    if (poison_monster(mons, who))
    {
        // Do additional impact damage to monsters who fail an rPois check
        mons->hurt(who, roll_dice(2, 4), BEAM_MMISSILE, KILLED_BY_CLOUD);
        success = true;
    }

    if (who && who->is_player() && is_good_god(you.religion))
        did_god_conduct(DID_EVIL, 5 + random2(3));

    if (mons->alive() && one_chance_in(3))
    {
        do_slow_monster(*mons, who, random_range(8, 12) * BASELINE_DELAY);
        success = true;
    }

    return success;
}

// Actually applies sticky flame to a monster (with message).
bool sticky_flame_monster(monster* mons, const actor *who, int dur, bool verbose)
{
    if (!mons->alive())
        return false;

    if (mons->res_sticky_flame() || dur <= 0 || mons->has_ench(ENCH_WATER_HOLD))
        return false;

    const mon_enchant old_flame = mons->get_ench(ENCH_STICKY_FLAME);
    mons->add_ench(mon_enchant(ENCH_STICKY_FLAME, 0, who, dur * BASELINE_DELAY));
    const mon_enchant new_flame = mons->get_ench(ENCH_STICKY_FLAME);

    // Actually do the napalming. The order is important here.
    if (new_flame.degree > old_flame.degree)
    {
        if (verbose)
            simple_monster_message(*mons, " is covered in liquid fire!");
        if (who)
            behaviour_event(mons, ME_WHACK, who);
    }

    return new_flame.degree > old_flame.degree;
}

static bool _curare_hits_player(actor* agent, string name,
                                string source_name, int bonus_poison)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_poison() >= 3
        || player_res_poison() > 0 && !one_chance_in(3))
    {
        canned_msg(MSG_YOU_RESIST);
        return false;
    }

    // We force apply this poison, since the 1-in-3 chance was already passed
    poison_player(roll_dice(2, 12) + bonus_poison + 1, source_name, name, true);

    mpr("You have difficulty breathing.");
    ouch(roll_dice(2, 6), KILLED_BY_CURARE, agent->mid, "curare-induced apnoea");
    slow_player(10 + random2(2 + random2(6)));

    return true;
}


bool curare_actor(actor* source, actor* target, string name,
                  string source_name, int bonus_poison)
{
    if (target->is_player())
        return _curare_hits_player(source, name, source_name, bonus_poison);
    else
        return _curare_hits_monster(source, target->as_monster(), bonus_poison);
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
        int multiplier = 5 * you.how_mutated(you.species == SP_DEMONSPAWN, true);
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
    // If this ASSERT triggers, your spell's setup code probably is doing
    // something bad when setup_mons_cast is called with check_validity=true.
    ASSERTM(crawl_state.game_started || crawl_state.test || crawl_state.script
        || crawl_state.game_is_arena(),
        "invalid game state for tracer '%s'!", pbolt.name.c_str());

    // Don't fiddle with any input parameters other than tracer stuff!
    pbolt.is_tracer     = true;
    pbolt.source        = mons->pos();
    pbolt.source_id     = mons->mid;
    pbolt.attitude      = mons_attitude(*mons);
    pbolt.precalc_agent_properties();

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

set<coord_def> create_feat_splash(coord_def center,
                                int radius,
                                int number,
                                int duration,
                                dungeon_feature_type new_feat)
{
    set<coord_def> splash_coords;

    for (distance_iterator di(center, true, false, radius); di && number > 0; ++di)
    {
        const dungeon_feature_type feat = env.grid(*di);
        if ((feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER)
            && cell_see_cell(center, *di, LOS_NO_TRANS))
        {
            number--;
            int time = random_range(duration, duration * 3 / 2) - (di.radius() * 20);
            temp_change_terrain(*di, new_feat, time,
                                TERRAIN_CHANGE_FLOOD);
            splash_coords.insert(*di);
        }
    }

    return splash_coords;
}

void bolt_parent_init(const bolt &parent, bolt &child)
{
    child.name           = parent.name;
    child.short_name     = parent.short_name;
    child.hit_verb       = parent.hit_verb;
    child.aux_source     = parent.aux_source;
    child.source_id      = parent.source_id;
    child.origin_spell   = parent.origin_spell;
    child.glyph          = parent.glyph;
    child.colour         = parent.colour;

    child.flavour        = parent.flavour;

    // We don't copy target since that is often overridden.
    child.thrower        = parent.thrower;
    child.source         = parent.source;
    child.source_name    = parent.source_name;
    child.attitude       = parent.attitude;

    child.loudness       = parent.loudness;
    child.pierce         = parent.pierce;
    child.aimed_at_spot  = parent.aimed_at_spot;
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

#ifdef DEBUG_DIAGNOSTICS
    child.quiet_debug    = parent.quiet_debug;
#endif
}

static void _malign_offering_effect(actor* victim, const actor* agent, int damage)
{
    if (!agent || damage < 1)
        return;

    // The victim may die.
    coord_def c = victim->pos();

    if (you.see_cell(c) || you.see_cell(agent->pos()))
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

static void _vampiric_draining_effect(actor& victim, actor& agent, int damage)
{
    if (damage < 1 || !actor_is_susceptible_to_vampirism(victim))
        return;

    if (you.can_see(victim) || you.can_see(agent))
    {
        mprf("%s %s life force from %s%s",
             agent.name(DESC_THE).c_str(),
             agent.conj_verb("draw").c_str(),
             victim.name(DESC_THE).c_str(),
             attack_strength_punctuation(damage).c_str());
    }

    const int drain_amount = victim.hurt(&agent, damage,
                                         BEAM_VAMPIRIC_DRAINING,
                                         KILLED_BY_BEAM, "",
                                         "by vampiric draining");

    if (agent.is_monster())
    {
        const int hp_gain = drain_amount * 2 / 3;
        if (agent.heal(hp_gain))
            simple_monster_message(*agent.as_monster(), " is healed by the life force!");
    }
    else
    {
        if (you.duration[DUR_DEATHS_DOOR] || you.hp == you.hp_max)
            return;

        const int hp_gain = div_rand_round(drain_amount, 2);
        if (hp_gain)
        {
            mprf("You feel life coursing into your body%s",
                 attack_strength_punctuation(hp_gain).c_str());
            inc_hp(hp_gain);
        }
    }
}

/**
 * Turn a BEAM_UNRAVELLING beam into a BEAM_UNRAVELLED_MAGIC beam, and make
 * it explode appropriately.
 *
 * @param[in,out] beam      The beam being turned into an explosion.
 */
static void _unravelling_explode(bolt &beam)
{
    beam.colour       = ETC_MUTAGENIC;
    beam.flavour      = BEAM_UNRAVELLED_MAGIC;
    beam.ex_size      = 1;
    beam.is_explosion = true;
    // and it'll explode 'naturally' a little later.
}

bool bolt::is_bouncy(dungeon_feature_type feat) const
{
    // Don't bounce off open sea.
    if (feat_is_endless(feat))
        return false;

    if (real_flavour == BEAM_CHAOS
        && feat_is_solid(feat))
    {
        return true;
    }

    if ((origin_spell == SPELL_REBOUNDING_BLAZE ||
        origin_spell == SPELL_REBOUNDING_CHILL)
        && feat_is_solid(feat)
        && !feat_is_tree(feat))
    {
        return true;
    }

    if (is_enchantment())
        return false;

    if (flavour == BEAM_ELECTRICITY
        && origin_spell != SPELL_BLINKBOLT
        && origin_spell != SPELL_MAGNAVOLT
        && !feat_is_metal(feat)
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

static void _waterlog_mon(monster &mon, int ench_pow)
{
    if (!mon.alive() || mon.res_water_drowning() || mon.has_ench(ENCH_WATERLOGGED))
        return;

    simple_monster_message(mon, " is engulfed in water.");
    const int min_dur = ench_pow + 20;
    const int dur = random_range(min_dur, min_dur * 3 / 2);
    mon.add_ench(mon_enchant(ENCH_WATERLOGGED, 0, &you, dur));
}

void bolt::affect_endpoint()
{
    // Test if this shot should trigger Dimensional Bullseye.
    bool use_bullseye = false;
    if (can_trigger_bullseye)
    {
        // We've already verified that at least one valid triggering target was
        // aimed at, but we need to know that we didn't ALSO aim at the bullseye
        // target (and that it's still in range)
        monster* bullseye_targ = monster_by_mid(you.props[BULLSEYE_TARGET_KEY].get_int());
        if (bullseye_targ && hit_count.count(bullseye_targ->mid) == 0
            && you.can_see(*bullseye_targ))
        {
            use_bullseye = true;
        }
    }

    // hack: we use hit_verb to communicate whether a ranged
    // attack hit. (And ranged attacks should only explode if
    // they hit the target, to avoid silliness with . targeting.)
    if (special_explosion && (is_tracer || !item || !hit_verb.empty()))
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
    if (item && !is_tracer && was_missile)
    {
        ASSERT(item->defined());
        if (item->flags & ISFLAG_SUMMONED || item_mulches)
            item_was_destroyed(*item);
        // Dimensional bullseye should make objects drop at the bullseye target
        // instead of the end of the ray
        else if (drop_item && !use_bullseye)
            drop_object();
    }

    if (is_explosion)
    {
        target = pos();
        refine_for_explosion();
        explode();
        return;
    }

    const cloud_type cloud = get_cloud_type();

    if (is_tracer)
    {
        if (cloud == CLOUD_NONE)
            return;

        targeter_cloud tgt(agent(), cloud, range, get_cloud_size(true),
                           get_cloud_size(false, true));
        tgt.set_aim(pos());
        for (const auto &entry : tgt.seen)
        {
            if (entry.second != AFF_YES && entry.second != AFF_MAYBE)
                continue;

            if (entry.first == you.pos())
                tracer_affect_player();
            else if (monster* mon = monster_at(entry.first))
                if (!ignores_monster(mon))
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

    if (use_bullseye)
    {
        monster* bullseye_targ = monster_by_mid(you.props[BULLSEYE_TARGET_KEY].get_int());
        mpr("Your projectile teleports!");
        use_target_as_pos = true;
        target = bullseye_targ->pos();
        affect_monster(bullseye_targ);
        if (drop_item)
        {
            // This is a little hacky, but the idea is to make the beam think we only
            // aimed at our bullseye target before dropping the item (so it will appear
            // beneath them). I think this should work fine?
            drop_object();
        }
    }

    // you like special cases, right?
    switch (origin_spell)
    {
    case SPELL_PRIMAL_WAVE:
    {
        if (you.see_cell(pos()))
            mpr("The wave splashes down.");
        else
            mpr("You hear a splash.");

        const bool is_player = agent(true) && agent(true)->is_player();
        const int num = is_player ? div_rand_round(ench_power * 3, 20) + 3 + random2(7)
                                  : random_range(3, 12, 2);
        const int dur = div_rand_round(ench_power * 4, 3) + 66;
        set<coord_def> splash_coords = create_feat_splash(pos(), is_player ? 2 : 1, num, dur);
        dprf(DIAG_BEAM, "Creating pool at %d,%d with %d tiles of water for %d auts.", pos().x, pos().y, num, dur);

        // Waterlog anything at the center, even if a pool wasn't generated there
        splash_coords.insert(pos());

        if (!is_player)
            break;
        for (const coord_def &coord : splash_coords)
        {
            monster* mons = monster_at(coord);
            if (mons)
                _waterlog_mon(*mons, ench_power);
        }
        break;
    }
    case SPELL_HURL_SLUDGE:
    {
        // short duration to limit the ability of the player to use it
        // against enemies
        const int dur = random_range(20, 30);
        // TODO: deduplicate with noxious_bog_cell
        if (env.grid(pos()) == DNGN_DEEP_WATER || env.grid(pos()) == DNGN_LAVA)
            break;

        // intentionally don't use TERRAIN_CHANGE_BOG to avoid it vanishing
        // when out of LOS
        temp_change_terrain(pos(), DNGN_TOXIC_BOG, dur,
                            TERRAIN_CHANGE_FLOOD,
                            agent() ? agent()->mid : MID_NOBODY);
        break;
    }
    case SPELL_BLINKBOLT:
    {
        actor *act = agent(true); // use orig actor even when reflected
        if (!act || !act->alive())
            return;

        for (vector<coord_def>::reverse_iterator citr = path_taken.rbegin();
             citr != path_taken.rend(); ++citr)
        {
            if (act->is_habitable(*citr) && (!act->is_player() || !monster_at(*citr))
                && act->blink_to(*citr, false))
            {
                return;
            }
        }
        return;
    }

    case SPELL_SEARING_BREATH:
        if (!path_taken.empty())
            place_cloud(CLOUD_FIRE, pos(), 5 + random2(5), agent());
        break;

    case SPELL_MUD_BREATH:
    {
        mpr("The mud splashes down.");
        const int num = 3 + div_rand_round(ench_power * 3, 7) + random2(4);
        const int dur = (div_rand_round(ench_power * 1, 3) + 15) * BASELINE_DELAY;
        const int rad = (num > 20) ? 3 : (num > 9) ? 2 : 1;
        create_feat_splash(pos(), rad, num, dur, DNGN_MUD);
    }
    break;

    case SPELL_FLASHING_BALESTRA:
    {
        // If the initial bolt is reflected or redirected back at the summoner,
        // we can crash on trying to create the summon.
        if (!agent(true) || !agent(true)->alive())
            break;

        coord_def spot;
        int num_found = 0;
        for (adjacent_iterator ai(pos()); ai; ++ai)
        {
            if (feat_is_solid(env.grid(*ai)) || actor_at(*ai))
                continue;

            if (one_chance_in(++num_found))
                spot = *ai;
        }

        if (!spot.origin())
        {
            monster* blade = create_monster(mgen_data(MONS_DANCING_WEAPON,
                                            SAME_ATTITUDE(agent(true)->as_monster()),
                                            spot, agent(true)->as_monster()->foe,
                                            MG_FORCE_PLACE)
                            .set_summoned(agent(true), 1, SPELL_FLASHING_BALESTRA, GOD_NO_GOD));

            if (blade)
                blade->add_ench(ENCH_MIGHT);
        }
    }
    break;

    case SPELL_GREATER_ENSNARE:
    {
        int count = random_range(3, 6);
        for (distance_iterator di(pos(), true, true, 1); di && count > 0; ++di)
        {
            trap_def *trap = trap_at(*di);
            if (trap && trap->type != TRAP_WEB
                || !trap && env.grid(*di) != DNGN_FLOOR)
            {
                continue;
            }

            if (actor_at(*di))
                ensnare(actor_at(*di));
            else
            {
                temp_change_terrain(*di, DNGN_TRAP_WEB, random_range(60, 110),
                                    TERRAIN_CHANGE_WEBS);
            }
            count--;
        }
        break;
    }

    default:
        break;
    }
}

bool bolt::stop_at_target() const
{
    // the pos check is to avoid a ray.cc assert for a ray that goes nowhere
    return is_explosion || is_big_cloud() ||
            (source_id == MID_PLAYER && origin_spell == SPELL_BLINKBOLT) ||
            (aimed_at_spot && (pos() == source || flavour != BEAM_DIGGING));
}

void bolt::drop_object()
{
    ASSERT(item != nullptr);
    ASSERT(item->defined());

    const int idx = copy_item_to_grid(*item, pos(), 1);

    if (idx != NON_ITEM
        && idx != -1
        && item->sub_type == MI_THROWING_NET)
    {
        monster* m = monster_at(pos());
        // Player or monster at position is caught in net.
        // Don't catch anything if the creature was already caught.
        if (get_trapping_net(pos(), true) == NON_ITEM
            && (you.pos() == pos() && you.attribute[ATTR_HELD]
            || m && m->caught()))
        {
            maybe_split_nets(env.item[idx], pos());
        }
    }
}

// Returns true if the beam hits the player, fuzzing the beam if necessary
// for monsters without see invis firing tracers at the player.
bool bolt::found_player() const
{
    const bool needs_fuzz = is_tracer
            && !YOU_KILL(thrower)
            && !can_see_invis && you.invisible()
            && (!agent()
                || (agent()->is_monster()
                    && !agent()->as_monster()->friendly()
                    && agent()->as_monster()->attitude != ATT_MARIONETTE))
            // No point in fuzzing to a position that could never be hit.
            && you.see_cell_no_trans(pos());
    const int dist = needs_fuzz? 2 : 0;

    return grid_distance(pos(), you.pos()) <= dist;
}

void bolt::affect_ground()
{
    // Explosions only have an effect during their explosion phase.
    // Special cases can be handled here.
    if (is_explosion && !in_explosion_phase)
        return;

    // XXX: This feels like an ugly place to put this, but it doesn't do any
    //      damage and the cloud placement code is otherwise skipped entirely
    //      for tracers.
    if (is_tracer && origin_spell == SPELL_NOXIOUS_BREATH && ench_power > 10)
    {
        for (adjacent_iterator ai(pos()); ai; ++ai)
        {
            if (monster_at(*ai))
                handle_stop_attack_prompt(monster_at(*ai));
        }
    }

    if (is_tracer)
        return;

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
    switch (origin_spell)
    {
    case SPELL_LIGHTNING_BOLT:
    case SPELL_BOLT_OF_FIRE:
    case SPELL_REBOUNDING_BLAZE:
    case SPELL_BOLT_OF_MAGMA:
    case SPELL_FIREBALL:
    case SPELL_FIRE_STORM:
    case SPELL_IGNITION:
    case SPELL_INNER_FLAME:
    case SPELL_STARBURST:
    case SPELL_FLAME_WAVE:
    case SPELL_SUMMON_BLAZEHEART_GOLEM: // core breach!
    case SPELL_HELLFIRE_MORTAR:
        return true;
    case SPELL_UNLEASH_DESTRUCTION:
        return flavour == BEAM_FIRE || flavour == BEAM_LAVA;
    default:
        return false;
    }
}

bool bolt::can_affect_wall(const coord_def& p, bool map_knowledge) const
{
    dungeon_feature_type wall = env.grid(p);

    // digging might affect unseen squares, as far as the player knows
    if (map_knowledge && flavour == BEAM_DIGGING &&
                                        !env.map_knowledge(pos()).seen())
    {
        return true;
    }

    // digging
    if (flavour == BEAM_DIGGING && feat_is_diggable(wall))
        return true;

    if (can_burn_trees())
        return feat_is_flammable(wall);

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
        if ((cloud->type == CLOUD_COLD && is_fiery())
            || (cloud->type == CLOUD_FIRE && flavour == BEAM_COLD))
        {
            if (player_can_hear(p))
                mprf(MSGCH_SOUND, "You hear a sizzling sound!");

            delete_cloud(p);
            extra_range_used += 5;
            return;
        }
        // blastmote explosions
        if (cloud->type == CLOUD_BLASTMOTES && is_fiery())
            explode_blastmotes_at(p);
    }

    const dungeon_feature_type feat = env.grid(p);

    if (origin_spell == SPELL_POISONOUS_CLOUD)
        place_cloud(CLOUD_POISON, p, random2(5) + 3, agent());

    if (origin_spell == SPELL_HOLY_BREATH)
        place_cloud(CLOUD_HOLY, p, random2(4) + 2, agent());

    if (origin_spell == SPELL_FLAMING_CLOUD)
        place_cloud(CLOUD_FIRE, p, random2(4) + 2, agent());

    // Fire/cold over water/lava
    if (feat == DNGN_LAVA && flavour == BEAM_COLD
        || feat_is_water(feat) && is_fiery())
    {
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), agent(), 11);
    }

    if (feat_is_water(feat) && flavour == BEAM_COLD
        && damage.num * damage.size > 35)
    {
        place_cloud(CLOUD_COLD, p, damage.num * damage.size / 30 + 1, agent());
    }

    if (flavour == BEAM_MIASMA)
        place_cloud(CLOUD_MIASMA, p, random2(5) + 2, agent());


    if (flavour == BEAM_STEAM)
    {
        if (origin_spell == SPELL_STEAM_BREATH)
            place_cloud(CLOUD_STEAM, p, div_rand_round(ench_power, 3) + 3, agent());
        else
            place_cloud(CLOUD_STEAM, p, random2(5) + 2, agent());
    }

    //XXX: these use the name for a gameplay effect.
    if (name == "poison gas")
        place_cloud(CLOUD_POISON, p, random2(4) + 3, agent());

    if (origin_spell == SPELL_PETRIFYING_CLOUD)
        place_cloud(CLOUD_PETRIFY, p, random2(4) + 4, agent());

    if (origin_spell == SPELL_SPECTRAL_CLOUD)
        place_cloud(CLOUD_SPECTRAL, p, random2(6) + 5, agent());

    if (origin_spell == SPELL_DEATH_RATTLE)
        place_cloud(CLOUD_MIASMA, p, random2(4) + 4, agent());

    if (origin_spell == SPELL_CAUSTIC_BREATH)
    {
        if (!actor_at(p))
        {
            place_cloud(CLOUD_ACID, p, 3 + random_range(ench_power / 2,
                                                        ench_power * 4 / 5),
                        agent());
        }
    }

    if (origin_spell == SPELL_NOXIOUS_BREATH)
    {
        place_cloud(CLOUD_MEPHITIC, p,
                    2 + random_range(ench_power / 2, ench_power * 4 / 5), agent());

        for (adjacent_iterator ai(pos()); ai; ++ai)
        {
            if (feat_is_solid(env.grid(*ai)))
                continue;

            if (x_chance_in_y(max(0, ench_power - 10), ench_power - 5))
            {
                place_cloud(CLOUD_MEPHITIC, *ai,
                        2 + random_range(ench_power / 3, ench_power / 2), agent());
            }
        }
    }

    // Only place clouds on creatures who are immune to them
    if (origin_spell == SPELL_MOURNING_WAIL)
    {
        if (!actor_at(p) || actor_at(p)->res_negative_energy() == 3)
            place_cloud(CLOUD_MISERY, p, random2(4) + 2, agent());
    }

    if (origin_spell == SPELL_MARCH_OF_SORROWS)
    {
        place_cloud(CLOUD_MISERY, p, random2(5) + 8, agent());

        if (actor_at(p) && !mons_aligned(actor_at(p), agent()))
        {
            for (adjacent_iterator ai(pos()); ai; ++ai)
            {
                if (feat_is_solid(env.grid(*ai)))
                    continue;

                place_cloud(CLOUD_MISERY, *ai, random2(4) + 2, agent());
            }
        }
    }

}

void bolt::affect_place_explosion_clouds()
{
    const coord_def p = pos();

    // First check: fire/cold over water/lava.
    if (env.grid(p) == DNGN_LAVA && flavour == BEAM_COLD
        || feat_is_water(env.grid(p)) && is_fiery())
    {
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), agent());
        return;
    }

    if (flavour == BEAM_MEPHITIC)
    {
        const coord_def center = (aimed_at_feet ? source : ray.pos());
        if (p == center || x_chance_in_y(125 + ench_power, 225))
        {
            place_cloud(CLOUD_MEPHITIC, p, roll_dice(2,
                        8 + div_rand_round(ench_power, 20)), agent());
        }
    }

    // Blazeheart core detonation
    if (origin_spell == SPELL_SUMMON_BLAZEHEART_GOLEM)
        place_cloud(CLOUD_FIRE, p, 2 + random2avg(5,2), agent());

    if (origin_spell == SPELL_FIRE_STORM)
    {
        place_cloud(CLOUD_FIRE, p, 2 + random2avg(5,2), agent());

        // XXX: affect other open spaces?
        if (env.grid(p) == DNGN_FLOOR && !monster_at(p) && one_chance_in(4))
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
    monster* monst = monster_by_mid(source_id);

    const char *what = aux_source.empty() ? name.c_str() : aux_source.c_str();

    if (YOU_KILL(thrower) && you.duration[DUR_QUAD_DAMAGE])
        dam *= 4;

    // The order of this is important.
    if (monst && mons_is_wrath_avatar(*monst))
    {
        ouch(dam, KILLED_BY_DIVINE_WRATH, MID_NOBODY,
             aux_source.empty() ? nullptr : aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
    else if (is_death_effect)
    {
        ouch(dam, KILLED_BY_DEATH_EXPLOSION, source_id,
             aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
    else if (flavour == BEAM_MINDBURST || flavour == BEAM_DESTRUCTION)
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
            if (self_targeted() && effect_known)
                ouch(dam, KILLED_BY_SELF_AIMED, MID_PLAYER, name.c_str());
            else
                ouch(dam, KILLED_BY_TARGETING, MID_PLAYER, name.c_str());
        }
    }
    else if (MON_KILL(thrower))
    {
        ouch(dam, KILLED_BY_BEAM, source_id,
             aux_source.c_str(), true,
             source_name.empty() ? nullptr : source_name.c_str());
    }
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
    coord_def fuzz;
    fuzz.x = random_range(-2, 2);
    fuzz.y = random_range(-2, 2);
    coord_def newtarget = target + fuzz;

    if (in_bounds(newtarget))
        target = newtarget;

    // Fire away!
    return true;
}

// A first step towards to-hit sanity for beams. We're still being
// very kind to the player, but it should be fairer to monsters than
// 4.0.
static int _test_beam_hit(int attack, int defence, defer_rand &r)
{
    if (attack == AUTOMATIC_HIT)
        return true;

    attack = r[1].random2(attack);
    defence = r[2].random2avg(defence, 2);

    dprf(DIAG_BEAM, "Beam attack: %d, defence: %d", attack, defence);

    return attack - defence;
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

    case BEAM_FOUL_FLAME:
        return mon->res_foul_flame() >= 3;

    case BEAM_STEAM:
        return mon->res_steam() >= 3;

    case BEAM_FIRE:
        return mon->res_fire() >= 3;

    case BEAM_COLD:
        return mon->res_cold() >= 3;

    case BEAM_MIASMA:
        return mon->res_miasma();

    case BEAM_NEG:
        return mon->res_negative_energy() == 3;

    case BEAM_ELECTRICITY:
        return mon->res_elec() >= 3;

    case BEAM_POISON:
        return mon->res_poison() >= 3;

    case BEAM_ACID:
        return mon->res_acid() >= 3;

    case BEAM_MEPHITIC:
        return mon->res_poison() > 0 || mon->clarity();

    case BEAM_UMBRAL_TORCHLIGHT:
        return (bool)!(mon->holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY));

    default:
        return false;
    }
}

// N.b. only called for player-originated beams and those used by the
// player's allies. This may not currently be comprehensive with ally spells
// that could affect some, but not all, players.
bool bolt::harmless_to_player() const
{
    dprf(DIAG_BEAM, "beam flavour: %d", flavour);

    // Marionettes can't hurt the player with anything, so don't worry about it.
    if (agent() && agent()->real_attitude() == ATT_MARIONETTE)
        return true;

    if (you.cloud_immune() && is_big_cloud())
        return true;

    if (origin_spell == SPELL_COMBUSTION_BREATH
        || origin_spell == SPELL_NULLIFYING_BREATH
        || origin_spell == SPELL_RIMEBLIGHT)
    {
        return true;
    }

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

    case BEAM_FOUL_FLAME:
        return you.res_foul_flame() >= 3;

    case BEAM_MIASMA:
        return you.res_miasma();

    case BEAM_NEG:
        return player_prot_life(false) >= 3;

    case BEAM_POISON:
        return player_res_poison(false) >= 3
               || is_big_cloud() && player_res_poison(false) > 0;

    case BEAM_MEPHITIC:
        return player_res_poison(false) > 0 || you.clarity();

    case BEAM_PETRIFY:
        return you.res_petrify() || you.petrified();

#if TAG_MAJOR_VERSION == 34
    case BEAM_COLD:
        return is_big_cloud() && you.has_mutation(MUT_FREEZING_CLOUD_IMMUNITY);
#endif

    case BEAM_VIRULENCE:
        return player_res_poison(false) >= 3;

    case BEAM_ROOTS:
        return mons_att_wont_attack(attitude) || !agent()->can_constrict(you, CONSTRICT_ROOTS);

    case BEAM_VILE_CLUTCH:
        return mons_att_wont_attack(attitude) || !agent()->can_constrict(you, CONSTRICT_BVC);

    case BEAM_UMBRAL_TORCHLIGHT:
        return agent(true)->is_player()
               || (bool)!(you.holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY));

    case BEAM_QAZLAL:
        return true;

    default:
        return false;
    }
}

bool bolt::is_reflectable(const actor &whom) const
{
    if (range_used() > range)
        return false;

    return whom.reflection();
}

bool bolt::is_big_cloud() const
{
    return testbits(get_spell_flags(origin_spell), spflag::cloud);
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

    const actor* ag = agent();

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
    else if (can_see_invis
             || !you.invisible()
             || ag && ag->as_monster()->friendly()
             || fuzz_invis_tracer())
    {
        if (mons_att_wont_attack(attitude) && !harmless_to_player())
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
}

int bolt::apply_lighting(int base_hit, const actor &targ) const
{
    if (targ.invisible() && !can_see_invis)
        base_hit /= 2;

    // We multiply these lighting effects by 2, since normally they're applied post-roll
    // where the effect (on average) counts doubled

    if (targ.backlit(false))
        base_hit += BACKLIGHT_TO_HIT_BONUS * 2;

    // Malus is already negative so must still be ADDED to the base_hit
    if (!nightvision && targ.umbra())
        base_hit += UMBRA_TO_HIT_MALUS * 2;

    return base_hit;
}

/* Determine whether the beam hit or missed the player, and tell them if it
 * missed.
 *
 * @return  true if the beam missed, false if the beam hit the player.
 */
bool bolt::misses_player()
{
    if (flavour == BEAM_VISUAL)
        return true;

    if ((is_explosion || aimed_at_feet)
        && origin_spell != SPELL_CALL_DOWN_LIGHTNING
        && origin_spell != SPELL_MOMENTUM_STRIKE)
    {
        return false;
    }

    int dodge = you.evasion();
    int real_tohit  = hit;

    if (real_tohit != AUTOMATIC_HIT)
        real_tohit = apply_lighting(real_tohit, you);

    const int SH = player_shield_class();
    if ((player_omnireflects() && is_omnireflectable()
         || is_blockable())
        && you.shielded()
        && !you.shield_exhausted()
        && !aimed_at_feet
        && (SH > 0 || you.duration[DUR_DIVINE_SHIELD]))
    {
        bool blocked = false;
        if (hit == AUTOMATIC_HIT)
        {
            // 50% chance of blocking ench-type effects at 10 displayed sh
            blocked = x_chance_in_y(SH, omnireflect_chance_denom(SH));

            dprf(DIAG_BEAM, "%smnireflected: %d/%d chance",
                 blocked ? "O" : "Not o", SH, omnireflect_chance_denom(SH));
        }
        else
        {
            // We use the original to-hit here.
            // (so that effects increasing dodge chance don't increase
            // block...?)
            const int testhit = random2(hit * 130 / 100);

            const int block = you.shield_bonus();

            dprf(DIAG_BEAM, "Beamshield: hit: %d, block %d", testhit, block);
            blocked = testhit < block;
        }

        // Divine shield only blocks conventionally blockable things, even if
        // the player is using the Warlock's Mirror.
        if (blocked || (you.duration[DUR_DIVINE_SHIELD] && is_blockable()))
        {
            const string refl_name = name.empty() &&
                                     origin_spell != SPELL_NO_SPELL ?
                                        spell_title(origin_spell) :
                                        name;

            const item_def *shield = you.shield();
            if (is_reflectable(you))
            {
                if (shield && shield_reflects(*shield))
                {
                    mprf("Your %s blocks the %s... and reflects it back!",
                            shield->name(DESC_PLAIN).c_str(),
                            refl_name.c_str());
                }
                else
                {
                    mprf("You block the %s... and reflect it back!",
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

            // Use up a charge of Divine Shield, if active.
            tso_expend_divine_shield_charge();

            return true;
        }

        // Some training just for the "attempt".
        practise_shield_block(false);
    }

    if (is_enchantment())
        return false;

    if (!self_targeted())
        practise_being_shot_at();

    defer_rand r;

    const int repel = you.missile_repulsion() ? REPEL_MISSILES_EV_BONUS : 0;
    dodge += repel;

    const int hit_margin = _test_beam_hit(real_tohit, dodge, r);
    if (hit_margin < 0)
    {
        if (hit_margin > -repel)
        {
            mprf("The %s is repelled.", name.c_str());
            count_action(CACT_DODGE, DODGE_REPEL);
        }
        else
        {
            mprf("The %s misses you.", name.c_str());
            count_action(CACT_DODGE, DODGE_EVASION);
        }
    }
    else
        return false;

    return true;
}

void bolt::affect_player_enchantment(bool resistible)
{
    if (resistible
        && has_saving_throw()
        && you.check_willpower(agent(true), ench_power) > 0)
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
                     you.willpower() == WILL_INVULN ? "are unaffected"
                                                   : "resist");
                need_msg = false;
            }
        }
        if (need_msg)
        {
            if (you.willpower() == WILL_INVULN)
                canned_msg(MSG_YOU_UNAFFECTED);
            else
            {
                // the message reflects the level of difficulty resisting.
                const int margin = you.willpower() - ench_power;
                mprf("You%s", you.resist_margin_phrase(margin).c_str());
            }
        }

        // punish the monster if we're a willful demon
        if (you.get_mutation_level(MUT_DEMONIC_WILL))
        {
            monster* mon = monster_by_mid(source_id);
            if (mon && mon->alive())
            {
                const int dam = 3 + random2(1 + div_rand_round(ench_power, 10));
                mprf("Your will lashes out at %s%s",
                     mon->name(DESC_THE).c_str(),
                     attack_strength_punctuation(dam).c_str());
                mon->hurt(&you, dam);
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
    if (flavour == BEAM_INFESTATION
        || flavour == BEAM_ENFEEBLE)
    {
        return;
    }

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
    case BEAM_SHADOW_TORPOR:
        slow_player(10 + random2(ench_power));
        obvious_effect = true;
        break;

    case BEAM_HASTE:
        haste_player(40 + random2(ench_power));
        did_god_conduct(DID_HASTY, 10, blame_player);
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
        potionlike_effect(POT_INVISIBILITY, ench_power);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_PARALYSIS:
        you.paralyse(agent(), random_range(2, 5));
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

    case BEAM_WEAKNESS:
        you.weaken(agent(), 8 + random2(4));
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

    case BEAM_CHARM:
        mprf(MSGCH_WARN, "Your will is overpowered!");
        confuse_player(5 + random2(3));
        obvious_effect = true;
        break;     // charming - confusion?

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

    case BEAM_MINDBURST:
        mpr("Your mind is blasted!");

        if (aux_source.empty())
            aux_source = "mindburst bolt";

        {
            int amt = damage.roll();
            internal_ouch(amt);

            if (you.has_blood())
                blood_spray(you.pos(), MONS_PLAYER, amt / 5);
        }

        obvious_effect = true;
        break;

    case BEAM_PORKALATOR:
        if (!transform(ench_power, transformation::pig, true))
        {
            mpr("You feel a momentary urge to oink.");
            break;
        }

        you.transform_uncancellable = true;
        obvious_effect = true;
        break;

    case BEAM_BERSERK:
        you.go_berserk(blame_player);
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
        you.strip_willpower(agent(), 12 + random2(18));
        obvious_effect = true;
        break;

    case BEAM_VITRIFY:
        if (!you.duration[DUR_VITRIFIED])
            mpr("Your body becomes as fragile as glass!");
        else
            mpr("You feel your fragility will last longer.");
        you.increase_duration(DUR_VITRIFIED, random_range(8, 18), 50);
        obvious_effect = true;
        break;

    case BEAM_VITRIFYING_GAZE:
        if (!you.duration[DUR_VITRIFIED])
            mpr("Your body becomes as fragile as glass!");
        else
            mpr("You feel your fragility will last longer.");
        you.increase_duration(DUR_VITRIFIED, random_range(4, 8), 50);
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

    case BEAM_ROOTS:
    {
        const int turns = 1 + random_range(div_rand_round(ench_power, 20),
                                           div_rand_round(ench_power, 12) + 1);
        if (start_ranged_constriction(*agent(), you, turns, CONSTRICT_ROOTS))
            obvious_effect = true;
        break;
    }

    case BEAM_VILE_CLUTCH:
    {
        const int turns = 3 + random_range(div_rand_round(ench_power, 50),
                                           div_rand_round(ench_power, 35) + 1);
        if (start_ranged_constriction(*agent(), you, turns, CONSTRICT_BVC))
            obvious_effect = true;
        break;
    }

    case BEAM_VAMPIRIC_DRAINING:
    {
        const int dam = resist_adjust_damage(&you, flavour, damage.roll());
        if (dam && actor_is_susceptible_to_vampirism(you))
        {
            _vampiric_draining_effect(you, *agent(), dam);
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
        you.be_agile(ench_power);
        obvious_effect = true;
        nasty = false;
        nice  = true;
        break;

    case BEAM_SAP_MAGIC:
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

        // If the player is unravelling themselves voluntarily, allow it to work.
        debuff_player(agent() && agent()->is_player());
        _unravelling_explode(*this);
        obvious_effect = true;
        break;

    case BEAM_SOUL_SPLINTER:
        obvious_effect = true;
        if (you.holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY))
            make_soul_wisp(*agent(), you);
        else
            canned_msg(MSG_YOU_UNAFFECTED);
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
                if (!self_targeted())
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

    handle_petrify_chaining(you.pos());

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

struct pie_effect
{
    const char* desc;
    function<bool(const actor& def)> valid;
    function<void (actor& def, const bolt &beam)> effect;
    int weight;
};

static const vector<pie_effect> pie_effects = {
    {
        "plum",
        [](const actor &defender) {
            return defender.is_player();
        },
        [](actor &/*defender*/, const bolt &/*beam*/) {
            if (you.duration[DUR_VERTIGO])
                mpr("You feel your light-headedness will last longer.");
            else
                mpr("You feel light-headed.");

            you.increase_duration(DUR_VERTIGO, 10 + random2(11), 50);
        },
        10
    },
    {
        "lemon",
        [](const actor &defender) {
            return defender.is_player() && (you.can_drink()
                                            || you.duration[DUR_NO_POTIONS]); // allow stacking
        },
        [](actor &/*defender*/, const bolt &/*beam*/) {
            if (you.duration[DUR_NO_POTIONS])
                mpr("You feel your inability to drink will last longer.");
            else
                mpr("You feel unable to drink.");

            you.increase_duration(DUR_NO_POTIONS, 10 + random2(11), 50);
        },
        10
    },
    {
        "blueberry",
        nullptr,
        [](actor &defender, const bolt &beam) {
            if (defender.is_monster())
            {
                monster *mons = defender.as_monster();
                simple_monster_message(*mons, " loses the ability to speak.");
                mons->add_ench(mon_enchant(ENCH_MUTE, 0, beam.agent(),
                            4 + random2(7) * BASELINE_DELAY));
            }
            else
            {
                if (you.duration[DUR_SILENCE])
                    mpr("You feel your silence will last longer.");
                else
                    mpr("An unnatural silence engulfs you.");

                you.increase_duration(DUR_SILENCE, 4 + random2(7), 10);
                invalidate_agrid(true);

                if (you.beheld())
                    you.update_beholders();
            }
        },
        10
    },
    {
        "raspberry",
        [](const actor &defender) {
            return defender.is_player();
        },
        [](actor &/*defender*/, const bolt &/*beam*/) {
            for (int i = 0; i < NUM_STATS; ++i)
                lose_stat(static_cast<stat_type>(i), 1 + random2(3));
        },
        10
    },
    {
        "cherry",
        [](const actor &defender) {
            return defender.is_player() || defender.res_fire() < 3;
        },
        [](actor &defender, const bolt &beam) {
            if (defender.is_monster())
            {
                monster *mons = defender.as_monster();
                simple_monster_message(*mons,
                        " looks more vulnerable to fire.");
                mons->add_ench(mon_enchant(ENCH_FIRE_VULN, 0,
                             beam.agent(),
                             15 + random2(11) * BASELINE_DELAY));
            }
            else
            {
                if (you.duration[DUR_FIRE_VULN])
                {
                    mpr("You feel your vulnerability to fire will last "
                        "longer.");
                }
                else
                    mpr("Cherry-coloured flames burn away your fire "
                        "resistance!");

                you.increase_duration(DUR_FIRE_VULN, 15 + random2(11), 50);
            }
        },
        6
    },
    {
        "peanut brittle",
        nullptr,
        [](actor &defender, const bolt &beam) {
            if (defender.is_monster())
            {
                monster *mons = defender.as_monster();
                simple_monster_message(*mons,
                    " becomes as fragile as glass!");

                mons->add_ench(mon_enchant(ENCH_VITRIFIED, 0, beam.agent(),
                                           random_range(16, 36) * BASELINE_DELAY));
            }
            else
            {
                if (you.duration[DUR_VITRIFIED])
                    mpr("You feel your fragility will last longer.");
                else
                    mpr("Your body becomes as fragile as glass!");

                you.increase_duration(DUR_VITRIFIED, random_range(16, 36), 50);
            }
        },
        4
    },
    {
        "glitter",
        [](const actor &defender) {
            return defender.can_be_dazzled();
        },
        [](actor &defender, const bolt &beam) {
            if (defender.is_player())
                blind_player(random_range(16, 36), ETC_GLITTER);
            else
                dazzle_target(&defender, beam.agent(), 149);
        },
        5
    },
};

static pie_effect _random_pie_effect(const actor &defender)
{
    vector<pair<const pie_effect&, int>> weights;
    for (const pie_effect &effect : pie_effects)
        if (!effect.valid || effect.valid(defender))
            weights.push_back({effect, effect.weight});

    ASSERT(!weights.empty());

    return *random_choose_weighted(weights);
}

void bolt::affect_player()
{
    hit_count[MID_PLAYER]++;

    if (ignores_player())
        return;

    // If this is a friendly monster, firing a penetrating beam in the player's
    // direction, always stop immediately before them if this attack wouldn't
    // be harmless to them.
    if (agent() && agent()->is_monster() && mons_att_wont_attack(attitude)
        && !harmless_to_player() && pierce && !is_explosion)
    {
        ray.regress();
        finish_beam();
        return;
    }

    // Explosions only have an effect during their explosion phase.
    // Special cases can be handled here.
    if (is_explosion && !in_explosion_phase)
    {
        // Trigger the explosion.
        finish_beam();
        return;
    }

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
        {
            interrupt_activity(activity_interrupt::monster_attacks,
                               agent()->as_monster());
        }
        else
            interrupt_activity(activity_interrupt::monster_attacks);
    }

    if (flavour == BEAM_MISSILE && item)
    {
        ranged_attack attk(agent(true), &you, launcher,
                           item, use_target_as_pos,
                           agent(), item_mulches);
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

    // Visible beams reveal invisible monsters; otherwise animations confer
    // an information advantage for sighted players
    if (visible() && agent() && agent()->is_monster())
        agent()->as_monster()->unseen_pos = agent()->pos();

    if (misses_player())
        return;

    if (!is_explosion && !noise_generated)
    {
        heard = noisy(loudness, pos(), source_id) || heard;
        noise_generated = true;
    }

    const bool engulfs = is_explosion || is_big_cloud();

    if (is_enchantment())
    {
        if (real_flavour == BEAM_CHAOS || real_flavour == BEAM_RANDOM)
        {
            if (hit_verb.empty())
                hit_verb = engulfs ? "engulfs" : "hits";
            mprf("The %s %s %s!", name.c_str(), hit_verb.c_str(),
                you.hp > 0 ? "you" : "your lifeless body");
        }

        affect_player_enchantment();
        return;
    }

    msg_generated = true;

    // FIXME: Lots of duplicated code here (compare handling of
    // monsters)
    int pre_ac_dam = 0;

    // Roll the damage.
    pre_ac_dam += damage.roll();
    int pre_res_dam = apply_AC(&you, pre_ac_dam);

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_BEAM, "Player damage: before AC=%d; after AC=%d",
                    pre_ac_dam, pre_res_dam);
#endif

    practise_being_shot();

    bool was_affected = false;
    int  old_hp       = you.hp;

    pre_res_dam = max(0, pre_res_dam);

    // If the beam is of the MMISSILE type (Earth magic) we might bleed on the
    // floor.
    if (!engulfs && flavour == BEAM_MMISSILE)
    {
        // assumes DVORP_PIERCING, factor: 0.5
        int blood = min(you.hp, pre_res_dam / 2);
        bleed_onto_floor(you.pos(), MONS_PLAYER, blood, true);
    }

    // Apply resistances to damage, but don't print "You resist" messages yet
    int final_dam = check_your_resists(pre_res_dam, flavour, "", this, false);

    // Tell the player the beam hit
    if (hit_verb.empty())
        hit_verb = engulfs ? "engulfs" : "hits";

    if (flavour != BEAM_VISUAL && !is_enchantment())
    {
        mprf("The %s %s %s%s%s", name.c_str(), hit_verb.c_str(),
             you.hp > 0 ? "you" : "your lifeless body",
             final_dam || damage.num == 0 ? "" : " but does no damage",
             attack_strength_punctuation(final_dam).c_str());
    }

    // Now print the messages associated with checking resistances, so that
    // these come after the beam actually hitting.
    // Note that this must be called with the pre-resistance damage, so that
    // poison effects etc work properly.
    check_your_resists(pre_res_dam, flavour, "", this, true);

    if (flavour == BEAM_STUN_BOLT
        && !you.duration[DUR_PARALYSIS]
        && (you.res_elec() <= 0 || coinflip()))
    {
        const bool was_parad = you.duration[DUR_PARALYSIS];
        you.paralyse(agent(), 1);
        was_affected = (!was_parad && you.duration[DUR_PARALYSIS]) || was_affected;

    }

    if (flavour == BEAM_LIGHT && you.can_be_dazzled())
        blind_player(random_range(7, 12), WHITE);

    if (flavour == BEAM_MIASMA && final_dam > 0)
        was_affected = miasma_player(agent(), name);

    if (flavour == BEAM_DESTRUCTION) // MINDBURST already handled
        blood_spray(you.pos(), MONS_PLAYER, final_dam / 5);

    // Confusion effect for spore explosions
    if (flavour == BEAM_SPORE && final_dam
        && !(you.holiness() & MH_UNDEAD)
        && !you.is_unbreathing())
    {
        confuse_player(2 + random2(3));
    }

    if (flavour == BEAM_UNRAVELLED_MAGIC)
        affect_player_enchantment();

    // Sticky flame.
    if (origin_spell == SPELL_STICKY_FLAME
        || flavour == BEAM_STICKY_FLAME)
    {
        // ench_power here is equal to 12 * caster HD for monsters, btw
        const int intensity = 2 + ench_power / 14;

        if (!player_res_sticky_flame())
        {
            sticky_flame_player(intensity, random_range(11, 21),
                                get_source_name(), aux_source);
            was_affected = true;
        }
    }

    // need to trigger qaz resists after reducing damage from ac/resists.
    //    for some reason, strength 2 is the standard. This leads to qaz's
    //    resists triggering 2 in 5 times at max piety.
    //    perhaps this should scale with damage?
    // what to do for hybrid damage?  E.g. bolt of magma, icicle, poison arrow?
    // Right now just ignore the physical component.
    // what about acid?
    you.expose_to_element(flavour, 2, false);

    // Manticore spikes
    if (origin_spell == SPELL_THROW_BARBS && final_dam > 0)
        barb_player(random_range(4, 8), 4);

    if (origin_spell == SPELL_GRAVE_CLAW)
    {
        mpr("You are skewered in place!");
        you.increase_duration(DUR_NO_MOMENTUM, random_range(2, 4));
    }

    if (flavour == BEAM_ENSNARE)
        was_affected = ensnare(&you) || was_affected;

    if (origin_spell == SPELL_QUICKSILVER_BOLT)
        debuff_player();

    if (origin_spell == SPELL_NULLIFYING_BREATH)
    {
        debuff_player();
        int amount = min(you.magic_points, random2avg(ench_power, 3));
        if (amount > 0)
        {
            mprf(MSGCH_WARN, "You feel your power leaking away.");
            drain_mp(amount);
        }
    }

    if (origin_spell == SPELL_THROW_PIE && final_dam > 0)
    {
        const pie_effect effect = _random_pie_effect(you);
        mprf("%s!", effect.desc);
        effect.effect(you, *this);
    }

    dprf(DIAG_BEAM, "Damage: %d", final_dam);

    if (final_dam > 0 || old_hp < you.hp || was_affected)
    {
        if (mons_att_wont_attack(attitude))
        {
            friend_info.hurt++;

            // Beam from player rebounded and hit player.
            // Xom's amusement at the player's being damaged is handled
            // elsewhere.
            if (source_id == MID_PLAYER)
            {
                if (!self_targeted())
                    xom_is_stimulated(200);
            }
            else if (was_affected)
                xom_is_stimulated(100);
        }
        else
            foe_info.hurt++;
    }

    internal_ouch(final_dam);

    // Acid. (Apply this afterward, to avoid bad message ordering.)
    if (flavour == BEAM_ACID)
        you.acid_corrode(5);

    extra_range_used += range_used_on_hit();

    knockback_actor(&you, final_dam);
    pull_actor(&you, final_dam);

    if (origin_spell == SPELL_FLASH_FREEZE
        || origin_spell == SPELL_CREEPING_FROST
        || name == "blast of ice"
        || origin_spell == SPELL_HOARFROST_BULLET
        || origin_spell == SPELL_GLACIATE && !is_explosion)
    {
        if (!you.duration[DUR_FROZEN])
        {
            mprf(MSGCH_WARN, "You are encased in ice.");
            you.duration[DUR_FROZEN] = (random_range(3, 5)) * BASELINE_DELAY;
        }
    }
}

bool bolt::ignores_player() const
{
    // Digging -- don't care.
    if (flavour == BEAM_DIGGING)
        return true;

    if (origin_spell == SPELL_COMBUSTION_BREATH
        || origin_spell == SPELL_NULLIFYING_BREATH
        || origin_spell == SPELL_RIMEBLIGHT
        || origin_spell == SPELL_SHADOW_PRISM)
    {
        return true;
    }

    if (origin_spell == SPELL_HOARFROST_BULLET && is_explosion
        && agent() && agent()->wont_attack())
    {
        return true;
    }

    if (agent() && agent()->is_monster()
        && (mons_is_hepliaklqana_ancestor(agent()->as_monster()->type)
            || mons_is_player_shadow(*agent()->as_monster())
            || agent()->real_attitude() == ATT_MARIONETTE))
    {
        // friends!
        return true;
    }

    if (source_id == MID_PLAYER_SHADOW_DUMMY)
        return true;

    if (flavour == BEAM_ROOTS || flavour == BEAM_VILE_CLUTCH)
    {
        return agent()->wont_attack()
               || !agent()->can_constrict(you, flavour == BEAM_ROOTS ? CONSTRICT_ROOTS
                                                                     : CONSTRICT_BVC);
    }

    if (flavour == BEAM_QAZLAL || flavour == BEAM_HAEMOCLASM)
        return true;

    // The player is immune to their own Destruction (both from Carnage / Annihilation
    // explosions, but also reflected lightning bolts or shots from hellmouths)
    if (origin_spell == SPELL_UNLEASH_DESTRUCTION
        && (!agent() || agent()->wont_attack()))
    {
        return true;
    }

    return false;
}

int bolt::apply_AC(const actor *victim, int hurted)
{
    switch (flavour)
    {
    case BEAM_DAMNATION:
        ac_rule = ac_type::none; break;
    case BEAM_COLD:
        if (origin_spell == SPELL_PERMAFROST_ERUPTION)
            ac_rule = ac_type::none;
        break;
    case BEAM_ELECTRICITY:
    case BEAM_THUNDER:
        ac_rule = ac_type::half; break;
    case BEAM_FRAG:
        ac_rule = ac_type::triple; break;
    default: ;
    }

    // beams don't obey GDR -> max_damage is 0
    return victim->apply_ac(hurted, 0, ac_rule, !is_tracer);
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
    if (ench_flavour_affects_monster(agent(), flavour, mon, true)
        && !(has_saving_throw() && mons_invuln_will(*mon)))
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
bool bolt::determine_damage(monster* mon, int& preac, int& postac, int& final)
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

    // If prompts for overshooting the target are disabled, instead
    // just let the caller know that there was something there. They
    // should be responsible and keep the player from shooting friends.
    // (We skip this for explosions, since stopping at our target is not
    // guaranteed to spare allies behind the target)
    if (passed_target && !overshoot_prompt && you.can_see(*mon)
       && !is_explosion)
    {
        string adj, suffix;
        bool penance;
        if (bad_attack(mon, adj, suffix, penance, target))
            friendly_past_target = true;
        return;
    }

    bool prompted = false;

    if (stop_attack_prompt(mon, true, target, &prompted)
        || _stop_because_god_hates_target_prompt(mon, origin_spell))
    {
        beam_cancelled = true;
        finish_beam();
    }
    // Handle charming monsters when a nasty dur is up: give a prompt for
    // attempting to charm monsters that might be affected.
    else if (flavour == BEAM_CHARM)
    {
        monclass_flags_t flags = monster_info(mon).airborne() ? M_FLIES
                                                              : M_NO_FLAGS;
        const string verb = make_stringf("charm %s",
                                         mon->name(DESC_THE).c_str());
        if (stop_summoning_prompt(monster_info(mon).resists(), flags,
            verb))
        {
            beam_cancelled = true;
            finish_beam();
            prompted = true;
        }
    }

    if (prompted)
    {
        friend_info.dont_stop = true;
        foe_info.dont_stop = true;
    }
}

// Whether or not this non-enchantment effect would have a relevant non-damage
// side effect when used on a given monster.
//
// XXX: This function is a stop-gap! It is knowingly incomplete, but addresses
//      the immediate problem of monsters refusing to use certain spells on
//      other monsters, even though they would be useful.
// TODO: Expand and unify this with actually applying beam side-effects
bool bolt::has_relevant_side_effect(monster* mon)
{
    if (flavour == BEAM_STICKY_FLAME
        && !mon->res_sticky_flame() && mon->res_fire() < 3)
    {
        return true;
    }
    else if (flavour == BEAM_ENSNARE || flavour == BEAM_LIGHT)
        return true;

    if ((origin_spell == SPELL_NOXIOUS_CLOUD || origin_spell == SPELL_POISONOUS_CLOUD
         || origin_spell == SPELL_NOXIOUS_BREATH)
        && mon->res_poison() < 1)
    {
        return true;
    }

    return false;
}

void bolt::tracer_nonenchantment_affect_monster(monster* mon)
{
    int preac = 0, post = 0, final = 0;

    bool side_effect = has_relevant_side_effect(mon);

    // The projectile applying sticky flame often does no damage, but this
    // doesn't mean it's harmless.
    if (!determine_damage(mon, preac, post, final) && !side_effect)
        return;

    // Check only if actual damage and the monster is worth caring about.
    // Living spells do count as threats, but are fine being collateral damage.
    if ((final > 0 || side_effect)
        && (mons_is_threatening(*mon) || mons_class_is_test(mon->type))
        && mon->type != MONS_LIVING_SPELL)
    {
        ASSERT(side_effect || preac > 0);

        // Add some 'fake damage' for a relevant side-effect, so tracer power
        // will actually see it.
        // XXX: This is ugly and numerically arbitrary...
        if (side_effect)
        {
            final += 10;
            preac += 10;
        }

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
            // Discourage summoned monsters firing on their summoner.
            const monster* mon_source = agent() ? agent()->as_monster() : nullptr;
            if (mon_source && mon_source->summoner == mon->mid)
                friend_info.power = 100;
            // Marionettes will avoid harming other 'allies', but are
            // deliberately reckless about themselves.
            else if (!(mon_source == mon && mon_source->attitude == ATT_MARIONETTE))
            {
                friend_info.power
                    += 2 * final * mon->get_experience_level() / preac;
            }
            friend_info.count++;
        }
    }

    // Maybe the user wants to cancel at this point.
    handle_stop_attack_prompt(mon);
    if (beam_cancelled)
        return;

    // Either way, we could hit this monster, so update range used.
    extra_range_used += range_used_on_hit();

    if (origin_spell == SPELL_COMBUSTION_BREATH && !in_explosion_phase)
        _combustion_breath_explode(this, mon->pos());
}

void bolt::tracer_affect_monster(monster* mon)
{
    // Ignore unseen monsters.
    if ((agent() && !agent()->can_see(*mon))
        || !cell_see_cell(source, mon->pos(), LOS_NO_TRANS))
    {
        return;
    }

    if (flavour == BEAM_UNRAVELLING && monster_can_be_unravelled(*mon))
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
}

void bolt::enchantment_affect_monster(monster* mon)
{
    god_conduct_trigger conducts[3];

    bool hit_woke_orc = false;

    if (nasty_to(mon))
    {
        if (YOU_KILL(thrower))
        {
            set_attack_conducts(conducts, *mon, you.can_see(*mon));

            if (have_passive(passive_t::convert_orcs)
                && mons_genus(mon->type) == MONS_ORC
                && mon->asleep() && you.see_cell(mon->pos()))
            {
                hit_woke_orc = true;
            }
            if (flavour != BEAM_HIBERNATION)
                you.pet_target = mon->mindex();
        }
    }

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
                // Hack: assume that BEAM_BANISH comes from Lugonu's Banishment
                // and hence causes malmutation on resist.
                if (real_flavour == BEAM_BANISH && agent() && agent()->is_player())
                    mon->malmutate("");
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

    handle_petrify_chaining(mon->pos());

    extra_range_used += range_used_on_hit();

    // Nasty enchantments will annoy the monster, and are considered
    // naughty (even if a monster resisted).
    if (mon && mon->alive() && nasty_to(mon))
        behaviour_event(mon, ME_ANNOY, agent());
    else
        behaviour_event(mon, ME_ALERT, agent());
}

static void _add_petrify_chain_candidates(const bolt& beam, coord_def pos,
                                          set<coord_def> &candidates)
{
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!cell_see_cell(beam.source, *ai, LOS_NO_TRANS))
            continue;

        actor* act = actor_at(*ai);

        // Ignore friendlies, firewood, and things we can fire through.
        if (!act || mons_aligned(beam.agent(), act))
            continue;

        monster *mon = act->as_monster();
        if (mon && (shoot_through_monster(beam, mon)
                    || mons_is_firewood(*mon)))
        {
            continue;
        }

        candidates.insert(*ai);
    }
}

void fill_petrify_chain_targets(const bolt& beam, coord_def centre,
                                vector<coord_def> &targs, bool random)
{
    set<coord_def> candidates;
    _add_petrify_chain_candidates(beam, centre, candidates);
    if (candidates.empty())
        return;

    const size_t MAX_PETRIFY_BOUNCES = 2;
    if (candidates.size() >= MAX_PETRIFY_BOUNCES)
    {
        for (coord_def candidate : candidates)
            targs.push_back(candidate);
        if (!random)
            return;

        shuffle_array(targs);
        targs.resize(MAX_PETRIFY_BOUNCES);
        return;
    }

    // OK. Can we bounce further out?
    set<coord_def> scs;
    for (coord_def candidate : candidates) // only runs once
    {
        targs.push_back(candidate);
        // Ensure adjacent targets show up before later-ring ones.
        _add_petrify_chain_candidates(beam, candidate, scs);
    }
    if (random)
        shuffle_array(targs);

    scs.erase(centre);
    if (scs.empty())
        return;

    // OK, we found some later bounces.
    if (random)
    {
        vector<coord_def> subcand_array(scs.begin(), scs.end());
        // Assumes MAX_PETRIFY_BOUNCES = 2. Sorry!
        targs.push_back(*random_iterator(subcand_array));
        return;
    }

    for (coord_def sc : scs)
        targs.push_back(sc);
}

void bolt::handle_petrify_chaining(coord_def centre)
{
    // Handle ray bounces
    if (origin_spell != SPELL_PETRIFY || hit_count.size() != 1)
        return;

    vector<coord_def> chain_targs;
    fill_petrify_chain_targets(*this, centre, chain_targs, true);
    if (chain_targs.empty())
        return;

    // Bounces are at 2/3 power
    unwind_var<int> ep(ench_power, ench_power * 2 / 3);
    for (coord_def c : chain_targs)
        affect_actor(actor_at(c));
}

static void _print_pre_death_message(const monster &mon, bool goldify,
                                     spell_type origin_spell)
{
    if (mon.is_insubstantial()
        || (origin_spell != SPELL_GLACIATE && origin_spell != SPELL_GLACIAL_BREATH))
    {
        return;
    }
    if (goldify)
        simple_monster_message(mon, " shatters and turns to gold!");
    else
        simple_monster_message(mon, " is frozen into a solid block of ice!");
}

void bolt::kill_monster(monster &mon)
{
    // Preserve name of the source monster if it winds up killing
    // itself.
    if (mon.mid == source_id && source_name.empty())
        source_name = mon.name(DESC_A, true);

    const coord_def where = mon.pos();
    const monster_type species = mons_species(mons_base_type(mon));
    const bool goldify = mons_will_goldify(mon);

    _print_pre_death_message(mon, goldify, origin_spell);

    int kindex = actor_to_death_source(agent());
    // Prevent spore explosions killing plants from being registered
    // as a Fedhas misconduct. Deaths can trigger the ally dying or
    // plant dying conducts, but spore explosions shouldn't count
    // for either of those.
    //
    // FIXME: Should be a better way of doing this. For now, we are
    // just falsifying the death report... -cao
    if (flavour == BEAM_SPORE && god_protects(mon) && fedhas_protects(mon))
    {
        if (mon.attitude == ATT_FRIENDLY)
            mon.attitude = ATT_HOSTILE;
        monster_die(mon, KILL_MON, kindex);
        return;
    }

    killer_type ref_killer = thrower;
    if (!YOU_KILL(thrower) && reflector == MID_PLAYER)
    {
        ref_killer = KILL_YOU_MISSILE;
        kindex = YOU_FAULTLESS;
    }

    if (flavour == BEAM_BLOODRITE)
        mon.props[MAKHLEB_BLOODRITE_KILL_KEY] = true;

    item_def *corpse = monster_die(mon, ref_killer, kindex);

    if (origin_spell != SPELL_GLACIATE && origin_spell != SPELL_GLACIAL_BREATH)
        return;

    if (corpse)
        destroy_item(corpse->index());
    if (monster *pillar = create_monster(
                                         mgen_data(MONS_BLOCK_OF_ICE,
                                                   BEH_HOSTILE,
                                                   where,
                                                   MHITNOT,
                                                   MG_FORCE_PLACE).set_base(species),
                                         false))
    {
        // Make glacial breath ice blocks more sturdy, since they're supposed
        // to make useful obstacles
        if (origin_spell == SPELL_GLACIAL_BREATH)
        {
            pillar->max_hit_points = 15 + div_rand_round(ench_power * 5, 2);
            pillar->hit_points = pillar->max_hit_points;
        }

        const int time_left = random_range(7, 17) * BASELINE_DELAY;
        mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
        pillar->add_ench(temp_en);
    }
}

void bolt::monster_post_hit(monster* mon, int dmg)
{
    // Suppress the message for tremorstones.
    if (YOU_KILL(thrower) && you.see_cell(mon->pos())
        && name != "burst of rock shards")
    {
        print_wounds(*mon);
    }

    // Don't annoy friendlies or good neutrals if the player's beam
    // did no damage. Hostiles will still take umbrage.
    if (dmg > 0 || !mon->wont_attack() || !YOU_KILL(thrower))
    {
        behaviour_event(mon, ME_ANNOY, agent());

        // behaviour_event can make a monster leave the level or vanish.
        if (!mon->alive())
            return;
    }

    if (YOU_KILL(thrower) && !mon->wont_attack() && !mons_is_firewood(*mon))
        you.pet_target = mon->mindex();

    // We check player Sticky Flame by name and other effects by flavour, since
    // the player spell has impact damage that the others do not, and thus is
    // BEAM_FIRE instead.
    //
    // Possibly this should be refactored.
    if (origin_spell == SPELL_STICKY_FLAME)
    {
        const int dur = 3 + random_range(div_rand_round(ench_power, 20),
                                         max(1, div_rand_round(ench_power, 10)));
        sticky_flame_monster(mon, agent(), dur);
    }
    else if (flavour == BEAM_STICKY_FLAME)
    {
        // Current numbers of monster sticky flame versus other monsters are
        // more arbitrary.
        sticky_flame_monster(mon, agent(), 3 + random2(ench_power / 20));
    }

    if (origin_spell == SPELL_QUICKSILVER_BOLT)
        debuff_monster(*mon);

    if (origin_spell == SPELL_NULLIFYING_BREATH)
    {
        debuff_monster(*mon);
        if (mon->antimagic_susceptible())
        {
            const int dur = (5 + random_range(div_rand_round(ench_power, 2),
                                             div_rand_round(ench_power * 3, 4) + 1))
                            * BASELINE_DELAY;
            mon->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0, agent(), dur));
        }
    }

    if (dmg)
        beogh_follower_convert(mon, true);

    knockback_actor(mon, dmg);
    pull_actor(mon, dmg);

    if (origin_spell == SPELL_COMBUSTION_BREATH && !in_explosion_phase)
        _combustion_breath_explode(this, mon->pos());

    if (origin_spell == SPELL_FLASH_FREEZE
             || origin_spell == SPELL_HOARFROST_BULLET
             || name == "blast of ice"
             || origin_spell == SPELL_GLACIATE && !is_explosion
             || origin_spell == SPELL_UNLEASH_DESTRUCTION
                && flavour == BEAM_COLD
                && you.has_mutation(MUT_MAKHLEB_DESTRUCTION_COC))
    {
        if (!mon->has_ench(ENCH_FROZEN))
        {
            simple_monster_message(*mon, " is flash-frozen.");
            mon->add_ench(ENCH_FROZEN);
        }
    }

    if (origin_spell == SPELL_UNLEASH_DESTRUCTION
        && flavour == BEAM_FIRE
        && you.has_mutation(MUT_MAKHLEB_DESTRUCTION_GEH))
    {
        const int dur = 3 + div_rand_round(dmg, 3);
        if (!mon->has_ench(ENCH_FIRE_VULN))
        {
            mprf("%s fire resistance burns away.",
                mon->name(DESC_ITS).c_str());
            mon->add_ench(mon_enchant(ENCH_FIRE_VULN, 1, agent(),
                                      dur * BASELINE_DELAY));
        }
    }

    if (origin_spell == SPELL_THROW_BARBS && dmg > 0
        && !(mon->is_insubstantial() || mons_genus(mon->type) == MONS_JELLY))
    {
        mon->add_ench(mon_enchant(ENCH_BARBS, 1, agent(),
                                  random_range(5, 7) * BASELINE_DELAY));
    }

    if (origin_spell == SPELL_THROW_PIE && dmg > 0)
    {
        const pie_effect effect = _random_pie_effect(*mon);
        if (you.see_cell(mon->pos()))
            mprf("%s!", effect.desc);
        effect.effect(*mon, *this);
    }

    if (flavour == BEAM_STUN_BOLT
        && !mon->has_ench(ENCH_PARALYSIS)
        && mon->res_elec() <= 0
        && !mon->stasis())
    {
        simple_monster_message(*mon, " is paralysed.");
        mon->add_ench(mon_enchant(ENCH_PARALYSIS, 1, agent(), BASELINE_DELAY));
    }

    if (flavour == BEAM_LIGHT
        && mon->can_be_dazzled()
        && !mon->has_ench(ENCH_BLIND))
    {
        const int dur = max(1, div_rand_round(54, mon->get_hit_dice())) * BASELINE_DELAY;
        auto ench = mon_enchant(ENCH_BLIND, 1, agent(),
                                random_range(dur, dur * 2));
        if (mon->add_ench(ench))
            simple_monster_message(*mon, " is blinded.");
    }

    if (origin_spell == SPELL_PRIMAL_WAVE && agent() && agent()->is_player())
        _waterlog_mon(*mon, ench_power);

    if (origin_spell == SPELL_HURL_TORCHLIGHT && agent() && agent()->is_player()
        && mon->friendly() && mon->holiness() & MH_UNDEAD)
    {
        int dur = random_range(2 + you.skill_rdiv(SK_INVOCATIONS, 1, 5),
                               4 + you.skill_rdiv(SK_INVOCATIONS, 1, 3))
                               * BASELINE_DELAY;
        mon->add_ench(mon_enchant(ENCH_MIGHT, 0, &you, dur));
        mon->speed_increment += 10;
        simple_monster_message(*mon, " is empowered.");
    }

    if (origin_spell == SPELL_RIMEBLIGHT)
        maybe_spread_rimeblight(*mon, ench_power);

    if (origin_spell == SPELL_GRAVE_CLAW && !mon->has_ench(ENCH_BOUND))
    {
        simple_monster_message(*mon, " is pinned in place!");
        mon->add_ench(mon_enchant(ENCH_BOUND, 0, nullptr, random_range(2, 4) * BASELINE_DELAY));
    }
}

static int _knockback_dist(spell_type origin, int pow)
{
    switch (origin)
    {
        case SPELL_ISKENDERUNS_MYSTIC_BLAST:
            return 2 + div_rand_round(pow, 50);
        case SPELL_FORCE_LANCE:
            return 4;
        default:
            return 1;
    }
}

static int _collision_damage(spell_type spell, int pow)
{
    switch (spell)
    {
        case SPELL_PRIMAL_WAVE:
        case SPELL_MUD_BREATH:
            return 0;

        default:
            return default_collision_damage(pow, true).roll();
    }
}

void bolt::knockback_actor(actor *act, int dam)
{
    if (!can_knockback(dam)) return;

    const int max_dist = _knockback_dist(origin_spell, ench_power);
    const monster_type montyp = act->is_monster() ? act->type
                                                  : you.mons_species();
    const int weight = max_corpse_chunks(montyp);
    const int roll = origin_spell == SPELL_FORCE_LANCE ? 42
                     : origin_spell == SPELL_ISKENDERUNS_MYSTIC_BLAST ? 17
                     : 25;
    const int dist = binomial(max_dist, roll - weight, roll); // This is silly! -- PF

    act->knockback(*agent(), dist, _collision_damage(origin_spell, ench_power), name);
}

void bolt::pull_actor(actor *act, int dam)
{
    if (!act || !can_pull(*act, dam) || act->resists_dislodge("being pulled"))
        return;

    // How far we'll try to pull the actor to make them adjacent to the source.
    const int distance = (act->pos() - source).rdist() - 1;
    ASSERT(distance > 0);

    const coord_def oldpos = act->pos();
    ASSERT(ray.pos() == oldpos);

    coord_def newpos = oldpos;
    for (int dist_travelled = 0; dist_travelled < distance; ++dist_travelled)
    {
        const ray_def oldray(ray);

        ray.regress();

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
        mprf("%s %s yanked forward by the %s.", act->name(DESC_THE).c_str(),
             act->conj_verb("are").c_str(), name.c_str());
    }

    if (act->pos() != newpos)
        act->collide(newpos, agent(), default_collision_damage(ench_power, true).roll());

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

    const int sh_hit = random2(hit * 130 / 100);
    if (sh_hit >= shield_block || mon->shield_exhausted())
        return false;

    item_def *shield = mon->mslot_item(MSLOT_SHIELD);
    if (is_reflectable(*mon))
    {
        if (mon->observable())
        {
            if (shield && shield_reflects(*shield))
            {
                mprf("%s blocks the %s with %s %s... and reflects it back!",
                     mon->name(DESC_THE).c_str(),
                     name.c_str(),
                     mon->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     shield->name(DESC_PLAIN).c_str());
            }
            else
            {
                mprf("The %s reflects off an invisible shield around %s!",
                     name.c_str(),
                     mon->name(DESC_THE).c_str());
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
        // Briars allow bolts fired from aligned actors to pass but block
        // those from unaligned actors. If this bolt has no agent, we're
        // assuming alignment.
        (mons.type == MONS_BRIAR_PATCH && mons_aligned(&mons, agent())
             // Bushes pass bolts regardless of alignment of the actor.
             || mons_species(mons.type) == MONS_BUSH)
        // We allow hitting the bush-like monster with a bolt when it's the
        // target.
        && target != mons.pos();
}

// Is there a visible monster at this position which will keep the beam from
// continuing onward? (And, if so, is it firewood or something else we'd never
// actually want to bother hitting?)
bool bolt::at_blocking_monster() const
{
    const monster *mon = monster_at(pos());
    if (!mon || !you.can_see(*mon))
        return false;

    if (!pierce
        && !ignores_monster(mon)
        && mons_is_firewood(*mon))
    {
        return true;
    }
    if (have_passive(passive_t::neutral_slimes)
        && god_protects(agent(), *mon, true)
        && flavour != BEAM_VILE_CLUTCH)
    {
        return true;
    }
    return false;
}

void bolt::affect_monster(monster* mon)
{
    // Don't hit dead or fake monsters.
    if (!mon->alive() || mon->type == MONS_GOD_WRATH_AVATAR)
        return;

    hit_count[mon->mid]++;

    // Jiyva absorbs attacks on slimes.
    if (agent()
        && flavour != BEAM_VILE_CLUTCH
        && have_passive(passive_t::neutral_slimes)
        && god_protects(agent(), *mon, true))
    {
        if (!is_tracer && you.see_cell(mon->pos()))
        {
            mprf(MSGCH_GOD, GOD_JIYVA,
                 "%s absorbs the %s as it strikes your slime.",
                 god_speaker(GOD_JIYVA).c_str(), name.c_str());
        }

        finish_beam();
        return;
    }

    if (shoot_through_monster(*this, mon) && !is_tracer)
    {
        if (you.see_cell(mon->pos()))
        {
            if (testbits(mon->flags, MF_DEMONIC_GUARDIAN))
                mpr("Your demonic guardian avoids your attack.");
            else
                god_protects(agent(), *mon, false); // messaging
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
        // Test if this qualifies to trigger Dimensional Bullseye later on.
        if (agent()->is_player() && you.duration[DUR_DIMENSIONAL_BULLSEYE]
            && !can_trigger_bullseye && !special_explosion
            && !mons_is_firewood(*mon) && mon->summoner != MID_PLAYER)
        {
            can_trigger_bullseye = true;
        }

        actor *ag = agent(true);
        // if the immediate agent is now dead, check to see if we can get a
        // usable agent by factoring in reflections.
        // At this point, it is possible that the agent is the dummy monster
        // associated with YOU_FAULTLESS. This case will cause
        // "INVALID YOU_FAULTLESS" to show up in dprfs and mess up the to-hit,
        // but it otherwise works.
        // TODO: is there a good way of handling the to-hit correctly? (And why
        // should the to-hit be affected by reflections at all?)
        // An alternative would be to stop the missile at this point.
        if (!ag)
            ag = agent(false);
        // if that didn't work, blanket fall back on YOU_FAULTLESS. This covers
        // a number of other weird penetration cases.
        if (!ag)
            ag = &env.mons[YOU_FAULTLESS];
        ASSERT(ag);
        ranged_attack attk(ag, mon, launcher,
                           item, use_target_as_pos, agent(), item_mulches);
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
    int preac, postac, final;
    if (!determine_damage(mon, preac, postac, final))
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

    if (nasty_to(mon))
    {
        if (agent() && agent()->is_player()  && final > 0)
            set_attack_conducts(conducts, *mon, you.can_see(*mon));
    }

    if (engulfs && flavour == BEAM_SPORE // XXX: engulfs is redundant?
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
        beam_hit = apply_lighting(beam_hit, *mon);

    // The monster may block the beam.
    if (!engulfs && is_blockable() && attempt_block(mon))
        return;

    defer_rand r;
    const int repel = mon->missile_repulsion() ? REPEL_MISSILES_EV_BONUS : 0;
    int rand_ev = random2(mon->evasion() + repel);

    int hit_margin = _test_beam_hit(beam_hit, rand_ev, r);

    if (you.duration[DUR_BLIND] && agent() && agent()->is_player())
    {
        const int distance = you.pos().distance_from(mon->pos());
        if (x_chance_in_y(player_blind_miss_chance(distance), 100))
            hit_margin = -1;
    }

    // FIXME: We're randomising mon->evasion, which is further
    // randomised inside test_beam_hit. This is so we stay close to the
    // 4.0 to-hit system (which had very little love for monsters).
    if (!engulfs && hit_margin < 0)
    {
        // If the PLAYER cannot see the monster, don't tell them anything!
        if (mon->observable())
        {
            // if it would have hit otherwise...
            if (hit_margin > -repel)
            {
                msg::stream << mon->name(DESC_THE) << " "
                            << "repels the " << name
                            << '!' << endl;
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

    // We'll say ballistomycete spore explosions don't trigger the ally attack
    // conduct for Fedhas worshipers. Mostly because you can accidentally blow
    // up a group of 8 plants and get placed under penance until the end of
    // time otherwise. I'd prefer to do this elsewhere but the beam information
    // goes out of scope.
    if (you_worship(GOD_FEDHAS) && flavour == BEAM_SPORE)
        conducts[0].set();

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

        // If the beam did no damage because of resistances,
        // mons_adjust_flavoured below will print "%s completely resists", so
        // no need to also say "does no damage" here.
        mprf("The %s %s %s%s%s",
             name.c_str(),
             hit_verb.c_str(),
             mon->name(DESC_THE).c_str(),
             postac || damage.num == 0 ? "" : " but does no damage",
             attack_strength_punctuation(final).c_str());

    }
    else if (heard && !hit_noise_msg.empty())
        mprf(MSGCH_SOUND, "%s", hit_noise_msg.c_str());

    // Spell vampirism
    if (agent() && agent()->is_player() && is_player_book_spell(origin_spell))
        majin_bo_vampirism(*mon, min(final, mon->stat_hp()));

    // Apply flavoured specials.
    mons_adjust_flavoured(mon, *this, postac, true);

    // Arc out from the original target AFTER damage done to the main target
    if (origin_spell == SPELL_GALVANIC_BREATH)
        do_galvanic_jolt(*agent(), pos(), damage);

    // mons_adjust_flavoured may kill the monster directly.
    if (mon->alive())
    {
        // If the beam is of the MMISSILE type (Earth magic) we might bleed on
        // the floor.
        if (!engulfs && flavour == BEAM_MMISSILE && !mon->is_summoned())
        {
            // Using raw_damage instead of the flavoured one!
            // assumes DVORP_PIERCING, factor: 0.5
            const int blood = min(postac/2, mon->hit_points);
            bleed_onto_floor(mon->pos(), mon->type, blood, true);
        }
        // Now hurt monster.
        mon->hurt(agent(), final, flavour, KILLED_BY_BEAM, "", "", false);

        // Haemoclasm explosions will always chain-explode if they kill something
        if (!mon->alive() && flavour == BEAM_HAEMOCLASM
            && you.has_mutation(MUT_MAKHLEB_MARK_HAEMOCLASM))
        {
            mon->props[MAKHLEB_HAEMOCLASM_KEY] = true;
        }
    }

    if (mon->alive())
        monster_post_hit(mon, final);
    // The monster (e.g. a spectral weapon) might have self-destructed in its
    // behaviour_event called from mon->hurt() above. If that happened, it
    // will have been cleaned up already (and is therefore invalid now).
    else if (!invalid_monster(mon))
        kill_monster(*mon);

    extra_range_used += range_used_on_hit();
}

bool bolt::ignores_monster(const monster* mon) const
{
    // This is the lava digging tracer. It will stop at anything which cannot
    // survive having lava put beneath it (ie: ignore everything else)
    if (origin_spell == SPELL_HELLFIRE_MORTAR)
        return mon && (mon->airborne() || monster_habitable_grid(mon, DNGN_LAVA));

    // Digging doesn't affect monsters (should it harm earth elementals?).
    if (flavour == BEAM_DIGGING)
        return true;

    // The targeters might call us with nullptr in the event of a remembered
    // monster that is no longer there. Treat it as opaque.
    if (!mon)
        return false;

    // Player shadows (or a dummy for a shadow spell tracer) can shoot through
    // other shadows (ie: where they are currently standing while contemplating
    // being somewhere else).
    if (mon->type == MONS_PLAYER_SHADOW
        && ((agent() && agent()->type == MONS_PLAYER_SHADOW)
            || source_id == MID_PLAYER_SHADOW_DUMMY))
    {
        return true;
    }

    // Shadow spells have no friendly fire, even against monsters.
    // (These should be the only ones that don't avoid that in other ways.)
    if ((origin_spell == SPELL_SHADOW_PRISM || origin_spell == SPELL_SHADOW_BEAM
         || origin_spell == SPELL_SHADOW_TORPOR
         || (origin_spell == SPELL_SHADOW_BALL && in_explosion_phase))
        && mons_atts_aligned(attitude, mons_attitude(*mon)))
    {
        return true;
    }

    // All kinds of beams go past orbs of destruction and friendly
    // battlespheres.
    if (always_shoot_through_monster(agent(), *mon))
        return true;

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

    // Rimeblight explosions won't hurt allies OR the monster that is exploding
    if (origin_spell == SPELL_RIMEBLIGHT && is_explosion)
        return mon->friendly() || mon->good_neutral() || mon->pos() == source;

    if (origin_spell == SPELL_HOARFROST_BULLET)
        return in_explosion_phase && mons_aligned(agent(), mon);

    // Explosions caused by Mark of Carnage don't cause friendly fire, and Mark
    // of the Legion allows firing through allies at all times.
    if ((origin_spell == SPELL_UNLEASH_DESTRUCTION && mon->friendly())
        && (is_explosion || you.has_mutation(MUT_MAKHLEB_MARK_LEGION)))
    {
        return true;
    }

    if ((flavour == BEAM_HAEMOCLASM || flavour == BEAM_BLOODRITE) && mon->friendly())
        return true;

    int summon_type = 0;
    mon->is_summoned(nullptr, &summon_type);
    if (flavour == BEAM_QAZLAL && summon_type == MON_SUMM_AID)
        return true;

    if (origin_spell == SPELL_UPHEAVAL && agent() && agent() == mon)
        return true;

    return false;
}

bool bolt::self_targeted() const
{
    // aimed_at_feet isn't enough to determine this. Examples: wrath uses a
    // fake monster at the player's position, xom's chesspiece fakes smite
    // targeting by setting the monster as the source position.
    const actor* a = agent(true);
    // TODO: are there cases that should count as self-targeted without an
    // agent?
    return aimed_at_feet && a && a->pos() == source
        && (!a->is_monster() || !mons_is_wrath_avatar(*a->as_monster()));
}

bool bolt::has_saving_throw() const
{
    if (self_targeted() || no_saving_throw)
        return false;

    switch (flavour)
    {
    case BEAM_HASTE:
    case BEAM_MIGHT:
    case BEAM_BERSERK:
    case BEAM_HEALING:
    case BEAM_INVISIBILITY:
    case BEAM_DISPEL_UNDEAD:
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
    case BEAM_VILE_CLUTCH:
    case BEAM_ROOTS:
    case BEAM_VAMPIRIC_DRAINING:
    case BEAM_CONCENTRATE_VENOM:
    case BEAM_ENFEEBLE:
    case BEAM_VITRIFYING_GAZE:
    case BEAM_RIMEBLIGHT:
    case BEAM_SHADOW_TORPOR:
        return false;
    case BEAM_VULNERABILITY:
        return !one_chance_in(3);  // Ignores will 1/3 of the time
    default:
        return true;
    }
}

bool ench_flavour_affects_monster(actor *agent, beam_type flavour,
                                  const monster* mon, bool intrinsic_only)
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
    case BEAM_SHADOW_TORPOR:
        rc = !mon->stasis();
        break;

    case BEAM_CONFUSION:
    case BEAM_IRRESISTIBLE_CONFUSION:
        rc = !mon->clarity();
        break;

    case BEAM_POLYMORPH:
        rc = mon->can_polymorph();
        break;

    case BEAM_DISPEL_UNDEAD:
        rc = bool(mon->holiness() & MH_UNDEAD);
        break;

    case BEAM_PAIN:
        rc = mon->res_negative_energy(intrinsic_only) < 3;
        break;

    case BEAM_AGONY:
    case BEAM_CURSE_OF_AGONY:
        rc = !mon->res_torment();
        break;

    case BEAM_HIBERNATION:
        rc = mon->can_hibernate(false, intrinsic_only);
        break;

    case BEAM_SLEEP:
        rc = mon->can_sleep();
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

    case BEAM_VAMPIRIC_DRAINING:
        rc = actor_is_susceptible_to_vampirism(*mon)
                && (mon->res_negative_energy(intrinsic_only) < 3);
        break;

    case BEAM_VIRULENCE:
        rc = (mon->res_poison() < 3);
        break;

    case BEAM_DRAIN_MAGIC:
        rc = mon->antimagic_susceptible();
        break;

    case BEAM_INNER_FLAME:
        rc = !mon->has_ench(ENCH_INNER_FLAME);
        break;

    case BEAM_PETRIFY:
        rc = !mon->res_petrify();
        break;

    case BEAM_INFESTATION:
        rc = (mons_gives_xp(*mon, you) || mon->props.exists(KIKU_WRETCH_KEY))
             && !mon->friendly()
             && !mon->has_ench(ENCH_INFESTATION);
        break;

    case BEAM_VILE_CLUTCH:
    case BEAM_ROOTS:
        ASSERT(agent);
    {
        const auto constr_typ = flavour == BEAM_ROOTS ? CONSTRICT_ROOTS : CONSTRICT_BVC;
        rc = !mons_aligned(agent, mon) && agent->can_constrict(*mon, constr_typ);
    }
        break;

    // These are special allies whose loyalty can't be so easily bent
    case BEAM_CHARM:
        rc = !(god_protects(*mon)
               || testbits(mon->flags, MF_DEMONIC_GUARDIAN))
             && !mons_aligned(agent, mon);
        break;

    case BEAM_MINDBURST:
        rc = mons_intel(*mon) > I_BRAINLESS;
        break;

    case BEAM_ENFEEBLE:
        rc = mons_has_attacks(*mon)
             || mon->antimagic_susceptible()
             || !mons_invuln_will(*mon);
        break;

    case BEAM_RIMEBLIGHT:
        rc = !mon->has_ench(ENCH_RIMEBLIGHT);
        break;

    case BEAM_SOUL_SPLINTER:
        rc = mons_can_be_spectralised(*mon, false, true)
             && !mon->props.exists(SOUL_SPLINTERED_KEY);
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
    if (!ench_flavour_affects_monster(agent(), flavour, mon))
        return MON_UNAFFECTED;

    // Check willpower.
    if (has_saving_throw())
    {
        if (mons_invuln_will(*mon))
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
            if (mon->check_willpower(agent(true), ench_power) > 0)
            {
                // Note only actually used by messages in this case.
                res_margin = mon->willpower() - ench_power_stepdown(ench_power);
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
            const int level = 2 + random2(3);
            did_god_conduct(DID_DELIBERATE_MUTATING, level, god_cares());
        }
        return MON_AFFECTED;

    case BEAM_MALMUTATE:
    case BEAM_UNRAVELLED_MAGIC:
        if (mon->malmutate("")) // exact source doesn't matter
            obvious_effect = true;
        if (YOU_KILL(thrower))
        {
            const int level = 2 + random2(3);
            did_god_conduct(DID_DELIBERATE_MUTATING, level, god_cares());
        }
        return MON_AFFECTED;

    case BEAM_BANISH:
        mon->banish(agent());
        obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_DISPEL_UNDEAD:
    {
        const int dam = damage.roll();
        if (you.see_cell(mon->pos()))
        {
            mprf("%s is dispelled%s",
                 mon->name(DESC_THE).c_str(),
                 attack_strength_punctuation(dam).c_str());
            obvious_effect = true;
        }
        mon->hurt(agent(), dam);
        return MON_AFFECTED;
    }

    case BEAM_VAMPIRIC_DRAINING:
    {
        const int dam = resist_adjust_damage(mon, flavour, damage.roll());
        if (dam && actor_is_susceptible_to_vampirism(*mon))
        {
            _vampiric_draining_effect(*mon, *agent(), dam);
            obvious_effect = true;
            return MON_AFFECTED;
        }
        else
            return MON_UNAFFECTED;
    }

    case BEAM_PAIN:
    {
        const int dam = resist_adjust_damage(mon, flavour, damage.roll());
        if (!dam)
            return MON_UNAFFECTED;
        if (you.see_cell(mon->pos()))
        {
            mprf("%s writhes in agony%s",
                 mon->name(DESC_THE).c_str(),
                 attack_strength_punctuation(dam).c_str());
            obvious_effect = true;
        }
        mon->hurt(agent(), dam, flavour);
        return MON_AFFECTED;
    }

    case BEAM_SOUL_SPLINTER:
        if (you.see_cell(mon->pos()))
            obvious_effect = true;
        if (make_soul_wisp(*agent(), *mon))
            return MON_AFFECTED;
        else
            return MON_UNAFFECTED;

    case BEAM_AGONY:
        torment_cell(mon->pos(), agent(), TORMENT_AGONY);
        obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_CURSE_OF_AGONY:
        // Don't allow stacking number of charges in a opaque fashion
        // by recasting the spell on already-cursed enemies.
        // (But allow refreshing duration, if someone wants)
        if (mon->has_ench(ENCH_CURSE_OF_AGONY))
            mon->del_ench(ENCH_CURSE_OF_AGONY, true, false);

        if (mon->add_ench(mon_enchant(ENCH_CURSE_OF_AGONY, 2, agent(),
                      random_range(6, 8) * BASELINE_DELAY)))
        {
            obvious_effect = true;
            simple_monster_message(*mon, " is cursed with the promise of agony.");

        }
        return MON_AFFECTED;

    case BEAM_MINDBURST:
    {
        const int dam = damage.roll();
        if (you.see_cell(mon->pos()))
        {
            if (mon->type == MONS_GLOWING_ORANGE_BRAIN)
            {
                mprf("%s is blasted smooth%s",
                     mon->name(DESC_THE).c_str(),
                     attack_strength_punctuation(dam).c_str());
            } else {
                const bool plural = mon->heads() > 1;
                mprf("%s mind%s blasted%s",
                     mon->name(DESC_ITS).c_str(),
                     plural ? "s are" : " is",
                     attack_strength_punctuation(dam).c_str());
            }
            obvious_effect = true;
        }
        mon->hurt(agent(), dam, flavour);
        return MON_AFFECTED;
    }

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
    {
        const int dur = 1 + div_rand_round(ench_power, 2)
                          + random2(1 + ench_power);
        obvious_effect = do_slow_monster(*mon, agent(), dur * BASELINE_DELAY);
        return MON_AFFECTED;
    }

    case BEAM_HASTE:
        if (YOU_KILL(thrower))
            did_god_conduct(DID_HASTY, 6, god_cares());

        if (mon->stasis())
            return MON_AFFECTED;

        if (!mon->has_ench(ENCH_HASTE)
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
            && mons_has_attacks(*mon)
            && mon->add_ench(ENCH_MIGHT))
        {
            if (simple_monster_message(*mon, " seems to grow stronger."))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_WEAKNESS:
        mon->weaken(agent(), 8 + random2(4));
        obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_BERSERK:
        if (!mon->berserk_or_frenzied())
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
                if (simple_monster_message(*mon, " wounds heal themselves!", true))
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
    {
        if (mon->clarity())
        {
            if (you.can_see(*mon))
                obvious_effect = true;
            return MON_AFFECTED;
        }

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
        return MON_AFFECTED;
    }

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

    case BEAM_CHARM:
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
            if (simple_monster_message(*mon, " is charmed!"))
                obvious_effect = true;
            mon->add_ench(mon_enchant(good, 0, agent()));
            if (!obvious_effect && could_see && !you.can_see(*mon))
                obvious_effect = true;
            return MON_AFFECTED;
        }

        // Being a puppet on magic strings is a nasty thing.
        // Mindless creatures shouldn't probably mind, but because of complex
        // behaviour of charmed neutrals, let's disallow that for now.
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
            && mon->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, agent())))
        {
            if (simple_monster_message(*mon,
                                       (mon->body_size(PSIZE_BODY) > SIZE_LARGE)
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
        if (!mon->has_ench(ENCH_LOWERED_WL))
        {
            if (mon->strip_willpower(agent(), random_range(20, 30)))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_VITRIFY:
        if (!mon->has_ench(ENCH_VITRIFIED)
            && mon->add_ench(mon_enchant(ENCH_VITRIFIED, 0, agent(),
                                         random_range(8, 18) * BASELINE_DELAY)))
        {
            if (you.can_see(*mon))
            {
                mprf("%s becomes as fragile as glass!",
                     mon->name(DESC_THE).c_str());
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;

    case BEAM_VITRIFYING_GAZE:
    {
        bool had_status = mon->has_ench(ENCH_VITRIFIED);

        if (mon->add_ench(mon_enchant(ENCH_VITRIFIED, 0, agent(),
                                  random_range(4, 8) * BASELINE_DELAY)))
        {
            if (you.can_see(*mon))
            {
                if (had_status)
                {
                    mprf("%s looks even more glass-like.",
                         mon->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s becomes as fragile as glass!",
                         mon->name(DESC_THE).c_str());
                }
                obvious_effect = true;
            }
        }
        return MON_AFFECTED;
    }

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

        if (!dur)
            break;

        mon->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                  agent(), // doesn't matter
                                  dur));
        if (you.can_see(*mon))
        {
            mprf("%s magic leaks into the air.",
                 apostrophise(mon->name(DESC_THE)).c_str());
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
        if (!monster_can_be_unravelled(*mon))
            return MON_UNAFFECTED;

        if (mon->is_summoned())
        {
            mprf("The magic binding %s to this plane unravels!",
                 mon->name(DESC_THE).c_str());
            monster_die(*mon, KILL_DISMISSED, actor_to_death_source(agent()));
        }
        else
            debuff_monster(*mon);
        _unravelling_explode(*this);
        return MON_AFFECTED;

    case BEAM_INFESTATION:
    {
        const int dur = (5 + random2avg(div_rand_round(ench_power,2), 2))
                            * BASELINE_DELAY;
        mon->add_ench(mon_enchant(ENCH_INFESTATION, 0, &you, dur));
        if (simple_monster_message(*mon, " is infested!"))
            obvious_effect = true;
        return MON_AFFECTED;
    }

    case BEAM_VILE_CLUTCH:
    {
        const int dur = 4 + random_range(div_rand_round(ench_power, 35),
                                         div_rand_round(ench_power, 25) + 1);
        if (start_ranged_constriction(*agent(), *mon, dur, CONSTRICT_BVC))
            obvious_effect = true;
        return MON_AFFECTED;
    }

    case BEAM_ROOTS:
    {
        const int dur = 3 + random_range(div_rand_round(ench_power, 22),
                                         div_rand_round(ench_power, 16) + 1);
        if (start_ranged_constriction(*agent(), *mon, dur, CONSTRICT_ROOTS))
            obvious_effect = true;
        return MON_AFFECTED;
    }

    case BEAM_CONCENTRATE_VENOM:
        if (!mon->has_ench(ENCH_CONCENTRATE_VENOM)
            && mon->add_ench(ENCH_CONCENTRATE_VENOM))
        {
            if (simple_monster_message(*mon, " venom grows more potent.", true))
                obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_ENFEEBLE:
        if (enfeeble_monster(*mon, ench_power))
            obvious_effect = true;
        return MON_AFFECTED;

    case BEAM_RIMEBLIGHT:
        if (apply_rimeblight(*mon, ench_power, true))
        {
            mprf("A stygian plague fills %s body.",
                 apostrophise(mon->name(DESC_THE)).c_str());
            obvious_effect = true;
        }
        return MON_AFFECTED;

    case BEAM_SHADOW_TORPOR:
    {
        int dur = max(30, ench_power / 2 + 40 - random2(mon->get_hit_dice() * 4));
        obvious_effect = do_slow_monster(*mon, agent(), dur);
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
    if (is_tracer && source_id == MID_PLAYER && hit < AUTOMATIC_HIT)
        return 0;

    // Non-beams can only affect one thing (player/monster).
    if (!pierce)
        return BEAM_STOP;
    // These beams fully penetrate regardless of anything else.
    if (flavour == BEAM_DAMNATION
        || flavour == BEAM_DIGGING
        || flavour == BEAM_VILE_CLUTCH
        || flavour == BEAM_ROOTS
        || flavour == BEAM_SHADOW_TORPOR)
    {
        return 0;
    }
    // explosions/clouds and enchants that aren't Line Pass stop.
    if (is_enchantment() && name != "line pass"
        || is_explosion
        || is_big_cloud())
    {
        return BEAM_STOP;
    }
    // Lightning that isn't an explosion goes through things.
    return 0;
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
    { SPELL_VIOLENT_UNRAVELLING, {
        "The enchantments explode!",
        "a sharp crackling", // radiation = geiger counter
    } },
    { SPELL_ICEBLAST, {
        "The mass of ice explodes!",
        "an explosion",
    } },
    { SPELL_PERMAFROST_ERUPTION, {
        "Piercing cold boils outward!",
        "an explosion", // maybe?
    } },
    { SPELL_GHOSTLY_SACRIFICE, {
        "The ghostly flame explodes!",
        "the shriek of haunting fire",
    } },
    { SPELL_SERACFALL, {
        "The mass of ice explodes!",
        "an explosion",
    } },
    { SPELL_FLAME_WAVE, {
        "A wave of flame ripples out!",
        "the roar of flame",
    } },
    { SPELL_FASTROOT, {
        "The roots erupt in riotous growth!",
        "creaking and crackling",
    } },
    { SPELL_BLASTMOTE, {
        "The cloud of blastmotes explodes!",
        "a concussive explosion",
    } },
    { SPELL_HURL_TORCHLIGHT, {
        "The gout of umbral fire explodes!",
        "the shriek of umbral fire",
    } },
    { SPELL_WARP_SPACE, {
        "Space twists violently!",
        "shrill echo",
    } },
    { SPELL_COMBUSTION_BREATH, {
        "The embers explode!",
        "a fiery explosion",
    } },
    { SPELL_NULLIFYING_BREATH, {
        "The antimagic surges outward!",
        "a quiet echo",
    } },
    { SPELL_HOARFROST_BULLET, {
        "The shards fragment into shrapnel!",
        "an explosion",
    } },
    { SPELL_SHADOW_BALL, {
        "The flickering shadows explode!",
        "a quiet whistle",
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
    else if (origin_spell == SPELL_NULLIFYING_BREATH)
    {
        colour  = MAGENTA;
        ex_size = 2;
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
 *  @param the size of the explosion (radius, not diameter)
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
    ASSERT(ex_size >= 0);

    // explode() can be called manually without setting real_flavour.
    // FIXME: The entire flavour/real_flavour thing needs some
    // rewriting!
    if (real_flavour == BEAM_CHAOS
        || real_flavour == BEAM_RANDOM)
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

    if (!is_tracer && flavour != BEAM_VISUAL)
    {
        loudness = explosion_noise(r);

        // Not an "explosion", but still a bit noisy at the target location.
        if (origin_spell == SPELL_INFESTATION
            || origin_spell == SPELL_DAZZLING_FLASH
            || origin_spell == SPELL_NULLIFYING_BREATH)
        {
            loudness = spell_effect_noise(origin_spell);
        }
        else if (origin_spell == SPELL_FLAME_WAVE)
        {
            loudness = spell_effect_noise(origin_spell)
                     + (r - 1) * 2; // at radius 1, base noise
        }
        // These explosions are too punishing for the player (and also a bit too
        // Qazlal-like in my opinion) if they make full uncontrollable noise
        // all the time.
        else if (flavour == BEAM_HAEMOCLASM)
            loudness = 5;

        // Make bloated husks quieter, both for balance (they're waking up
        // whole levels!) and for theme (it's not a huge fireball, it's a big
        // gas leak).
        if (name == "blast of putrescent gases")
            loudness = loudness * 2 / 3;
        // Shadow spells are silent
        else if (origin_spell == SPELL_SHADOW_BALL
                 || origin_spell == SPELL_SHADOW_PRISM)
        {
            loudness = 0;
        }

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
    if (!is_tracer && animate)
    {
        for (const auto &line : sweep)
        {
            bool pass_visible = false;
            for (const coord_def &delta : line)
            {
                if (delta.origin() && hole_in_the_middle)
                    continue;

                if (exp_map(delta + centre) < INT_MAX)
                    pass_visible |= explosion_draw_cell(delta + pos());
            }
            // redraw for an entire explosion block, so redraw_per_cell is not
            // relevant
            if (pass_visible && !Options.reduce_animations)
                animation_delay(explode_delay, true);
        }
    }

    // Affect pass.
    int cells_seen = 0;
    for (const auto &line : sweep)
    {
        for (const coord_def &delta : line)
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
    if (!is_tracer && cells_seen > 0 && show_more && animate)
        animation_delay(explode_delay * 3, Options.reduce_animations);

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
            view_add_tile_overlay(p, vary_bolt_tile(tile, dist));
#endif
            const unsigned short c = colour == BLACK ?
                    random_colour(true) : element_colour(colour, false, p);
            view_add_glyph_overlay(p, {dchar_glyph(DCHAR_EXPLOSION), c});

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

    const dungeon_feature_type dngn_feat = env.grid(loc);

    bool at_wall = false;

    // Check to see if we're blocked by a wall or a tree. Can't use
    // feat_is_solid here, since that includes statues which are a separate
    // check, nor feat_is_opaque, since that excludes transparent walls, which
    // we want. -ebering
    // XXX: We could just include trees as wall features, but this currently
    // would have some unintended side-effects. Would be ideal to deal with
    // those and simplify feat_is_wall() to return true for trees. -gammafunk
    if (feat_is_wall(dngn_feat)
        || feat_is_tree(dngn_feat)
           && (!feat_is_flammable(dngn_feat)
               || !can_burn_trees()
               || have_passive(passive_t::shoot_through_plants) // would be nice to message
               || env.markers.property_at(loc, MAT_ANY, "veto_destroy") == "veto")
        || feat_is_closed_door(dngn_feat))
    {
        // Special case: explosion originates from rock/statue
        // (e.g. Lee's Rapid Deconstruction) - in this case, ignore
        // solid cells at the center of the explosion.
        if (stop_at_walls && !(delta.origin() && can_affect_wall(loc)))
            return;
        // But remember that we are at a wall.
        if (flavour != BEAM_DIGGING)
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

        // Is that cell already covered?
        if (m(new_delta + centre) <= count + cadd)
            continue;

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

    // Foul flame.
    if (flavour == BEAM_FOUL_FLAME)
        return mon->res_foul_flame() < 3;

    // The orbs are made of pure disintegration energy. This also has the side
    // effect of not stopping us from firing further orbs when the previous one
    // is still flying.
    if (flavour == BEAM_DESTRUCTION)
        return mon->type != MONS_ORB_OF_DESTRUCTION;

    // Take care of other non-enchantments.
    if (!is_enchantment())
        return true;

    // Positive effects.
    if (nice_to(monster_info(mon)))
        return false;

    switch (flavour)
    {
        case BEAM_DIGGING:
            return false;
        case BEAM_INNER_FLAME:
            // Co-aligned inner flame is fine.
            return !mons_aligned(mon, agent());
        case BEAM_TELEPORT:
        case BEAM_BECKONING:
        case BEAM_INFESTATION:
        case BEAM_VILE_CLUTCH:
        case BEAM_ROOTS:
        case BEAM_SLOW:
        case BEAM_PARALYSIS:
        case BEAM_PETRIFY:
        case BEAM_POLYMORPH:
        case BEAM_DISPEL_UNDEAD:
        case BEAM_PAIN:
        case BEAM_AGONY:
        case BEAM_CURSE_OF_AGONY:
        case BEAM_HIBERNATION:
        case BEAM_MINDBURST:
        case BEAM_VAMPIRIC_DRAINING:
        case BEAM_ENFEEBLE:
        case BEAM_RIMEBLIGHT:
        case BEAM_CHARM:
            return ench_flavour_affects_monster(agent(), flavour, mon);
        case BEAM_TUKIMAS_DANCE:
            return tukima_affects(*mon); // XXX: move to ench_flavour_affects?
        case BEAM_UNRAVELLING:
            return monster_can_be_unravelled(*mon); // XXX: as tukima's
        default:
            break;
    }

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
        || flavour == BEAM_RESISTANCE
        || flavour == BEAM_CONCENTRATE_VENOM)
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
            return &env.mons[YOU_FAULTLESS];
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
        || real_flavour == BEAM_CHAOS)
    {
        return _beam_type_name(real_flavour);
    }

    if (flavour == BEAM_FIRE
        && (origin_spell == SPELL_STICKY_FLAME
            || origin_spell == SPELL_PYRE_ARROW))
    {
        return "sticky fire";
    }

    if (flavour == BEAM_ELECTRICITY && pierce)
        return "lightning";

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
    case BEAM_LIGHT:                 return "light";
    case BEAM_MIASMA:                return "miasma";
    case BEAM_SPORE:                 return "spores";
    case BEAM_POISON_ARROW:          return "poison sting";
    case BEAM_DAMNATION:             return "damnation";
    case BEAM_STICKY_FLAME:          return "sticky fire";
    case BEAM_STEAM:                 return "steam";
    case BEAM_ENERGY:                return "energy";
    case BEAM_HOLY:                  return "cleansing flame";
    case BEAM_FOUL_FLAME:            return "foul flame";
    case BEAM_FRAG:                  return "fragments";
    case BEAM_LAVA:                  return "magma";
    case BEAM_ICE:                   return "ice";
    case BEAM_THUNDER:               return "thunder";
    case BEAM_STUN_BOLT:             return "stunning bolt";
    case BEAM_DESTRUCTION:           return "destruction";
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
    case BEAM_CHARM:                 return "charming";
    case BEAM_BANISH:                return "banishment";
    case BEAM_PAIN:                  return "pain";
    case BEAM_AGONY:                 return "agony";
    case BEAM_CURSE_OF_AGONY:        return "curse of agony";
    case BEAM_DISPEL_UNDEAD:         return "dispel undead";
    case BEAM_MINDBURST:             return "mindburst";
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
    case BEAM_DRAIN_MAGIC:           return "drain magic";
    case BEAM_TUKIMAS_DANCE:         return "tukima's dance";
    case BEAM_DEATH_RATTLE:          return "breath of the dead";
    case BEAM_RESISTANCE:            return "resistance";
    case BEAM_UNRAVELLING:           return "unravelling";
    case BEAM_UNRAVELLED_MAGIC:      return "unravelled magic";
    case BEAM_SHARED_PAIN:           return "shared pain";
    case BEAM_IRRESISTIBLE_CONFUSION:return "confusion";
    case BEAM_INFESTATION:           return "infestation";
    case BEAM_VILE_CLUTCH:           return "vile clutch";
    case BEAM_VAMPIRIC_DRAINING:     return "vampiric draining";
    case BEAM_CONCENTRATE_VENOM:     return "concentrate venom";
    case BEAM_ENFEEBLE:              return "enfeeble";
    case BEAM_SOUL_SPLINTER:         return "soul splinter";
    case BEAM_ROOTS:                 return "roots";
    case BEAM_VITRIFY:               return "vitrification";
    case BEAM_VITRIFYING_GAZE:       return "vitrification";
    case BEAM_WEAKNESS:              return "weakness";
    case BEAM_DEVASTATION:           return "devastation";
    case BEAM_UMBRAL_TORCHLIGHT:     return "umbral torchlight";
    case BEAM_CRYSTALLIZING:         return "crystallizing";
    case BEAM_WARPING:               return "spatial disruption";
    case BEAM_QAZLAL:                return "upheaval targetter";
    case BEAM_RIMEBLIGHT:            return "rimeblight";
    case BEAM_SHADOW_TORPOR:         return "shadow torpor";
    case BEAM_HAEMOCLASM:            return "gore";
    case BEAM_BLOODRITE:             return "blood";

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
 * Can this bolt potentially knock back a creature it hits?
 *
 * @param dam The damage dealt. If non-negative, check that dam > 0 for bolts
 *             like force lance that only push back upon damage.
 * @return True iff the bolt could knock something back.
*/
bool bolt::can_knockback(int dam) const
{
    switch (origin_spell)
    {
    case SPELL_PRIMAL_WAVE:
    case SPELL_MUD_BREATH:
        return true;
    case SPELL_FORCE_LANCE:
    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        return dam != 0;
    default:
        return false;
    }
}

/**
 * Can this bolt pull an actor?
 *
 * If a bolt is capable of pulling actors and the given actor can be pulled,
 * return true. May print messages.
 *
 * @param act The target actor. Check if the actor is non-stationary and not
 *            already adjacent.
 * @param dam The damage dealt. Check that dam > 0.
 * @return True if the bolt could pull the actor, false otherwise.
*/
bool bolt::can_pull(const actor &act, int dam) const
{
    if (act.is_stationary() || adjacent(source, act.pos()))
        return false;

    return origin_spell == SPELL_HARPOON_SHOT && dam;
}

ai_action::goodness bolt::good_to_fire() const
{
    // Use of foeRatio:
    // The higher this number, the more monsters will _avoid_ collateral
    // damage to their friends.
    // Setting this to zero will in fact have all monsters ignore their
    // friends when considering collateral damage.

    // Quick check - did we in fact hit anything?
    if (foe_info.count == 0)
        return ai_action::neutral();

    const int total_pow = foe_info.power + friend_info.power;
    // Only fire if they do acceptably low collateral damage.
    if (!friend_info.power
        || foe_info.power >= div_round_up(foe_ratio * total_pow, 100))
    {
        return ai_action::good();
    }
    return ai_action::bad();
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
    return stepdown(pow, 35);
}

/// Translate a given ench power to a duration, in aut, and randomize it.
int _ench_pow_to_dur(int pow)
{
    // ~15 turns at 25 pow, ~21 turns at 50 pow, ~27 turns at 100 pow
    int base_dur = stepdown(pow * BASELINE_DELAY, 70);

    return div_rand_round(base_dur, 2) + random2(1 + base_dur);
}

// Do all beams skip past a particular monster?
// see also shoot_through_monsters
// can these be consolidated? Some checks there don't need a bolt arg
bool always_shoot_through_monster(const actor *originator, const monster &victim)
{
    return mons_is_projectile(victim)
        || (mons_is_avatar(victim.type)
            && originator && mons_aligned(originator, &victim));
}

// Can a particular beam go through a particular monster?
// Fedhas worshipers can shoot through non-hostile plants,
// and players can shoot through their demonic guardians.
bool shoot_through_monster(const bolt& beam, const monster* victim)
{
    actor *originator = beam.agent();
    if (!victim || !originator)
        return false;
    return god_protects(originator, *victim)
           || (originator->is_player()
               && testbits(victim->flags, MF_DEMONIC_GUARDIAN));
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
    return SH + 20;
}

/// Set up a beam aiming from the given monster to their target.
bolt setup_targeting_beam(const monster &mons)
{
    bolt beem;

    beem.source    = mons.pos();
    beem.target    = mons.target;
    beem.source_id = mons.mid;

    return beem;
}

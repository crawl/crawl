/**
 * @file
 * @brief Spell miscast class.
**/

#include "AppHdr.h"

#include "spl-miscast.h"

#include <sstream>

#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "item-use.h"
#include "item-prop.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mutation.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

// This determines how likely it is that more powerful wild magic
// effects will occur. Set to 100 for the old probabilities (although
// the individual effects have been made much nastier since then).
#define WILD_MAGIC_NASTINESS 150

#define MAX_RECURSE 100

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source,
                             int _source, spell_type _spell,
                             int _pow, int _fail, string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause), spell(_spell),
    school(SPTYP_NONE), pow(_pow), fail(_fail), level(-1),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(is_valid_spell(_spell));
    ASSERT(get_spell_disciplines(_spell) != SPTYP_NONE);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source, int _source,
                             spschool_flag_type _school, int _level,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause),
    spell(SPELL_NO_SPELL), school(_school), pow(-1), fail(-1), level(_level),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);
    ASSERT_RANGE(level, 0, 3 + 1);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source, int _source,
                             spschool_flag_type _school, int _pow, int _fail,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause),
    spell(SPELL_NO_SPELL), school(_school), pow(_pow), fail(_fail), level(-1),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);

    init();
    do_miscast();
}

MiscastEffect::~MiscastEffect()
{
    ASSERT(recursion_depth == 0);
}

void MiscastEffect::init()
{
    ASSERT(spell != SPELL_NO_SPELL && school == SPTYP_NONE
           || spell == SPELL_NO_SPELL && school != SPTYP_NONE);
    ASSERT(pow != -1 && fail != -1 && level == -1
           || pow == -1 && fail == -1 && level >= 0 && level <= 3);

    ASSERT(target != nullptr);
    ASSERT(target->alive());

    ASSERT(lethality_margin == 0 || target->is_player());

    recursion_depth = 0;

    source_known = target_known = false;

    if (target->is_monster())
        target_known = you.can_see(*target);
    else
        target_known = true;

    if (act_source && act_source->is_player())
    {
        kt           = KILL_YOU_MISSILE;
        source_known = true;
    }
    else if (act_source && act_source->is_monster())
    {
        if (act_source->as_monster()->confused_by_you()
            && !act_source->as_monster()->friendly())
        {
            kt = KILL_YOU_CONF;
        }
        else
            kt = KILL_MON_MISSILE;

        source_known = you.can_see(*act_source);

        if (target_known && special_source == MUMMY_MISCAST)
            source_known = true;
    }
    else
    {
        ASSERT(special_source != MELEE_MISCAST);

        kt = KILL_MISCAST;

        if (special_source == ZOT_TRAP_MISCAST)
        {
            source_known = target_known;

            if (target->is_monster()
                && target->as_monster()->confused_by_you())
            {
                kt = KILL_YOU_CONF;
            }
        }
        else
            source_known = true;
    }

    ASSERT(kt != KILL_NONE);

    // source_known = false for MELEE_MISCAST so that melee miscasts
    // won't give a "nothing happens" message.
    if (special_source == MELEE_MISCAST)
        source_known = false;

    if (hand_str.empty())
        target->hand_name(true, &can_plural_hand);

    // Explosion stuff.
    beam.is_explosion = true;

    // [ds] Don't attribute the beam's cause to the actor, because the
    // death message will name the actor anyway.
    if (cause.empty())
        cause = get_default_cause(false);
    beam.aux_source  = cause;
    if (act_source)
        beam.source_id = act_source->mid;
    else
        beam.source_id = MID_NOBODY;
    beam.thrower     = kt;
}

string MiscastEffect::get_default_cause(bool attribute_to_user) const
{
    // This is only for true miscasts, which means both a spell and that
    // the source of the miscast is the same as the target of the miscast.
    ASSERT(special_source == SPELL_MISCAST);
    ASSERT(spell != SPELL_NO_SPELL);
    ASSERT(school == SPTYP_NONE);
    ASSERT(target->is_player());

    return string("miscasting ") + spell_title(spell);
}

bool MiscastEffect::neither_end_silenced()
{
    return !silenced(you.pos()) && !silenced(target->pos());
}

void MiscastEffect::do_miscast()
{
    ASSERT_RANGE(recursion_depth, 0, MAX_RECURSE);

    if (recursion_depth == 0)
        did_msg = false;

    unwind_var<int> unwind_depth(recursion_depth);
    recursion_depth++;

    // Repeated calls to do_miscast() on a single object instance have
    // killed a target which was alive when the object was created.
    if (!target->alive())
    {
        dprf("Miscast target '%s' already dead",
             target->name(DESC_PLAIN, true).c_str());
        return;
    }

    spschool_flag_type sp_type;
    int                severity;

    if (spell != SPELL_NO_SPELL)
    {
        vector<spschool_flag_type> school_list;
        for (const auto bit : spschools_type::range())
            if (spell_typematch(spell, bit))
                school_list.push_back(bit);

        sp_type = school_list[random2(school_list.size())];
    }
    else
    {
        sp_type = school;
        if (sp_type == SPTYP_RANDOM)
            sp_type = spschools_type::exponent(random2(SPTYP_LAST_EXPONENT + 1));
    }

    if (level != -1)
        severity = level;
    else
    {
        severity = (pow * fail * (10 + pow) / 7 * WILD_MAGIC_NASTINESS) / 100;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mprf(MSGCH_DIAGNOSTICS, "'%s' miscast power: %d",
             spell != SPELL_NO_SPELL ? spell_title(spell)
                                     : spelltype_short_name(sp_type),
             severity);
#endif

        if (random2(40) > severity && random2(40) > severity)
        {
            if (target->is_player())
                canned_msg(MSG_NOTHING_HAPPENS);
            return;
        }

        severity /= 100;
        severity = random2(severity);
        if (severity > 3)
            severity = 3;
        else if (severity < 0)
            severity = 0;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mprf(MSGCH_DIAGNOSTICS, "Sptype: %s, severity: %d",
         spelltype_short_name(sp_type), severity);
#endif

    beam.ex_size            = 1;
    beam.name               = "";
    beam.damage             = dice_def(0, 0);
    beam.flavour            = BEAM_NONE;
    beam.msg_generated      = false;
    beam.in_explosion_phase = false;

    // Do this here since multiple miscasts (wizmode testing) might move
    // the target around.
    beam.source = target->pos();
    beam.target = target->pos();
    beam.use_target_as_pos = true;

    all_msg        = you_msg = mon_msg = mon_msg_seen = mon_msg_unseen = "";
    msg_ch         = MSGCH_PLAIN;
    sound_loudness = 0;

    if (special_source == ZOT_TRAP_MISCAST)
    {
        _zot();
        if (target->is_player())
            xom_is_stimulated(150);
        return;
    }

    switch (sp_type)
    {
    case SPTYP_CONJURATION:    _conjuration(severity);    break;
    case SPTYP_HEXES:          _hexes(severity);          break;
    case SPTYP_CHARMS:         _charms(severity);         break;
    case SPTYP_TRANSLOCATION:  _translocation(severity);  break;
    case SPTYP_SUMMONING:      _summoning(severity);      break;
    case SPTYP_NECROMANCY:     _necromancy(severity);     break;
    case SPTYP_TRANSMUTATION:  _transmutation(severity);  break;
    case SPTYP_FIRE:           _fire(severity);           break;
    case SPTYP_ICE:            _ice(severity);            break;
    case SPTYP_EARTH:          _earth(severity);          break;
    case SPTYP_AIR:            _air(severity);            break;
    case SPTYP_POISON:         _poison(severity);         break;

    default:
        die("Invalid miscast spell discipline.");
    }

    if (target->is_player())
        xom_is_stimulated(severity * 50);
}

void MiscastEffect::do_msg(bool suppress_nothing_happens)
{
    ASSERT(!did_msg);

    if (!you.see_cell(target->pos()))
        return;

    did_msg = true;

    string msg;

    if (!all_msg.empty())
        msg = all_msg;
    else if (target->is_player())
        msg = you_msg;
    else if (!mon_msg.empty())
    {
        msg = mon_msg;
        // Monster might be unseen with hands that can't be seen.
        ASSERT(msg.find("@hand") == string::npos);
    }
    else
    {
        if (you.can_see(*target))
            msg = mon_msg_seen;
        else
        {
            msg = mon_msg_unseen;
            // Can't see the hands of invisible monsters.
            ASSERT(msg.find("@hand") == string::npos);
        }
    }

    if (msg.empty())
    {
        if (!suppress_nothing_happens
            && (nothing_happens_when == NH_ALWAYS
                || (nothing_happens_when == NH_DEFAULT && source_known
                    && target_known)))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }

        return;
    }

    bool plural;

    if (hand_str.empty())
    {
        msg = replace_all(msg, "@hand@",  target->hand_name(false, &plural));
        msg = replace_all(msg, "@hands@", target->hand_name(true));
    }
    else
    {
        plural = can_plural_hand;
        msg = replace_all(msg, "@hand@",  hand_str);
        if (can_plural_hand)
            msg = replace_all(msg, "@hands@", pluralise(hand_str));
        else
            msg = replace_all(msg, "@hands@", hand_str);
    }

    if (plural)
        msg = replace_all(msg, "@hand_conj@", "");
    else
        msg = replace_all(msg, "@hand_conj@", "s");

    if (target->is_monster())
    {
        msg = do_mon_str_replacements(msg, *target->as_monster(), S_SILENT);
        if (!mons_has_body(*target->as_monster()))
            msg = replace_all(msg, "'s body", "");
    }

    mprf(msg_ch, "%s", msg.c_str());

    if (msg_ch == MSGCH_SOUND)
    {
        // XXX: can this just be target->mid?
        mid_t src = target->is_player() ? MID_PLAYER : target->mid;
        noisy(sound_loudness, target->pos(), src);
    }
}

bool MiscastEffect::_ouch(int dam, beam_type flavour)
{
    // Delay do_msg() until after avoid_lethal().
    if (target->is_monster())
    {
        monster* mon_target = target->as_monster();

        do_msg(true);

        bolt beem;

        beem.flavour = flavour;
        dam = mons_adjust_flavoured(mon_target, beem, dam, true);
        mon_target->hurt(act_source, dam, BEAM_MISSILE, KILLED_BY_BEAM,
                         "", "", false);

        if (!mon_target->alive())
            monster_die(*mon_target, kt, actor_to_death_source(act_source));
    }
    else
    {
        dam = check_your_resists(dam, flavour, cause);

        if (avoid_lethal(dam))
            return false;

        do_msg(true);

        kill_method_type method;

        if (special_source == SPELL_MISCAST && spell != SPELL_NO_SPELL)
            method = KILLED_BY_WILD_MAGIC;
        else if (special_source == ZOT_TRAP_MISCAST)
            method = KILLED_BY_TRAP;
        else if (special_source >= GOD_MISCAST)
        {
            god_type god = static_cast<god_type>(special_source - GOD_MISCAST);

            if (god == GOD_XOM && !player_under_penance(GOD_XOM))
                method = KILLED_BY_XOM;
            else
                method = KILLED_BY_DIVINE_WRATH;
        }
        else
            method = KILLED_BY_SOMETHING;

        bool see_source = act_source && you.can_see(*act_source);
        ouch(dam, method, act_source ? act_source->mid : MID_NOBODY,
             cause.c_str(), see_source,
             act_source ? act_source->name(DESC_A, true).c_str() : nullptr);
    }

    return true;
}

bool MiscastEffect::_explosion()
{
    ASSERT(!beam.name.empty());
    ASSERT(beam.damage.num != 0);
    ASSERT(beam.damage.size != 0);
    ASSERT(beam.flavour != BEAM_NONE);

    // wild magic card
    if (special_source == DECK_MISCAST)
        beam.thrower = KILL_MISCAST;

    int max_dam = beam.damage.num * beam.damage.size;
    max_dam = check_your_resists(max_dam, beam.flavour, cause);
    if (avoid_lethal(max_dam))
        return false;

    do_msg(true);
    beam.explode();

    return true;
}

bool MiscastEffect::_big_cloud(cloud_type cl_type, int cloud_pow, int size,
                               int spread_rate)
{
    if (avoid_lethal(2 * max_cloud_damage(cl_type, cloud_pow)))
        return false;

    do_msg(true);
    big_cloud(cl_type, act_source, target->pos(), cloud_pow, size, spread_rate);

    return true;
}

bool MiscastEffect::_paralyse(int dur)
{
    if (special_source != HELL_EFFECT_MISCAST)
    {
        target->paralyse(act_source, dur, cause);
        return true;
    }
    else
        return false;
}

bool MiscastEffect::_sleep(int dur)
{
    if (!target->can_sleep() || special_source == HELL_EFFECT_MISCAST)
        return false;

    if (target->is_player())
        you.put_to_sleep(act_source, dur);
    else
        target->put_to_sleep(act_source, dur);
    return true;
}

bool MiscastEffect::_send_to_abyss()
{
    // The Abyss depth check is duplicated here (and the banishment forced if
    // successful), in order to degrade to Malign Gateway in the Abyss instead
    // of doing nothing.
    if ((player_in_branch(BRANCH_ABYSS)
         && x_chance_in_y(you.depth, brdepth[BRANCH_ABYSS]))
        || special_source == HELL_EFFECT_MISCAST)
    {
        return _malign_gateway();
    }
    target->banish(act_source, cause, target->get_experience_level(), true);
    return true;
}

// XXX: Mostly duplicated from cast_malign_gateway.
bool MiscastEffect::_malign_gateway(bool hostile)
{
    coord_def point = find_gateway_location(target);
    bool success = (point != coord_def(0, 0));

    if (success)
    {
        const int malign_gateway_duration = BASELINE_DELAY * (random2(3) + 2);
        env.markers.add(new map_malign_gateway_marker(point,
                                malign_gateway_duration,
                                false,
                                cause,
                                hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                                GOD_NO_GOD,
                                200));
        env.markers.clear_need_activate();
        env.grid(point) = DNGN_MALIGN_GATEWAY;
        set_terrain_changed(point);

        noisy(spell_effect_noise(SPELL_MALIGN_GATEWAY), point);
        all_msg = "The dungeon shakes, a horrible noise fills the air, and a "
                  "portal to some otherworldly place is opened!";
        msg_ch = MSGCH_WARN;
        do_msg();
    }

    return success;
}

bool MiscastEffect::avoid_lethal(int dam)
{
    if (lethality_margin <= 0 || (you.hp - dam) > lethality_margin)
        return false;

    if (recursion_depth == MAX_RECURSE)
    {
        // Any possible miscast would kill you, now that's interesting.
        if (you_worship(GOD_XOM))
            simple_god_message(" watches you with interest.");
        return true;
    }

    if (did_msg)
    {
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mprf(MSGCH_ERROR, "Couldn't avoid lethal miscast: already printed "
                          "message for this miscast.");
#endif
        return false;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mprf(MSGCH_DIAGNOSTICS, "Avoided lethal miscast.");
#endif

    do_miscast();

    return true;
}

bool MiscastEffect::_create_monster(monster_type what, int abj_deg,
                                    bool alert)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (cause.empty())
        cause = get_default_cause(true);
    mgen_data data = mgen_data::hostile_at(what, alert, target->pos());
    data.set_summoned(nullptr, abj_deg, SPELL_NO_SPELL, god);
    data.set_non_actor_summoner(cause);

    if (special_source != HELL_EFFECT_MISCAST)
        data.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    // hostile_at() assumes the monster is hostile to the player,
    // but should be hostile to the target monster unless the miscast
    // is a result of either divine wrath or a Zot trap.
    if (target->is_monster() && !player_under_penance(god)
        && special_source != ZOT_TRAP_MISCAST)
    {
        monster* mon_target = target->as_monster();

        switch (mon_target->temp_attitude())
        {
            case ATT_FRIENDLY:     data.behaviour = BEH_HOSTILE; break;
            case ATT_HOSTILE:      data.behaviour = BEH_FRIENDLY; break;
            case ATT_GOOD_NEUTRAL:
            case ATT_NEUTRAL:
            case ATT_STRICT_NEUTRAL:
                data.behaviour = BEH_NEUTRAL;
            break;
        }

        if (alert)
            data.foe = mon_target->mindex();

        // No permanent allies from miscasts.
        if (data.behaviour == BEH_FRIENDLY && abj_deg == 0)
            data.abjuration_duration = 6;
    }

    // If data.abjuration_duration == 0, then data.summon_type will
    // simply be ignored.
    if (data.abjuration_duration != 0)
    {
        if (what == RANDOM_MOBILE_MONSTER)
            data.summon_type = SPELL_SHADOW_CREATURES;
        else if (player_under_penance(god))
            data.summon_type = MON_SUMM_WRATH;
        else if (special_source == ZOT_TRAP_MISCAST)
            data.summon_type = MON_SUMM_ZOT;
        else
            data.summon_type = MON_SUMM_MISCAST;
    }

    return create_monster(data);
}

// hair or hair-equivalent (like bandages)
static bool _has_hair(actor* target)
{
    // Don't bother for monsters.
    if (target->is_monster())
        return false;

    return !form_changed_physiology()
           && (species_has_hair(you.species) || you.species == SP_MUMMY);
}

static string _hair_str(actor* target, bool &plural)
{
    ASSERT(target->is_player());

    if (you.species == SP_MUMMY)
    {
        plural = true;
        return "bandages";
    }
    else
    {
        plural = false;
        return "hair";
    }
}

/**
 * Malmutate the player.
 *
 * Significantly worse than the spell. (Gives 1-2 muts, never pure-random.)
 */
void MiscastEffect::_malmutate()
{
    you_msg = "Your body is distorted in a weirdly horrible way!";
    // We don't need messages when the mutation fails, because we give our
    // own (which is justified anyway as you take damage).
    mutate(RANDOM_BAD_MUTATION, cause, false, false);
    if (coinflip())
        mutate(RANDOM_BAD_MUTATION, cause, false, false);
}

void MiscastEffect::_conjuration(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Sparks fly from the ground!";
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles "
                           "with energy!";
            break;
        case 2:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Wisps of smoke drift around you.";
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You are momentarily dazzled by a flash of light!";
            mon_msg_seen = "@The_monster@ emits a flash of light!";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@ glows momentarily.";
            // A small glow isn't going to make it past invisibility.
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (you.can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Smoke billows around you!";
            you_msg        = "Smoke pours from your @hands@!";
            mon_msg_seen   = "Smoke pours from @the_monster@'s @hands@!";
            mon_msg_unseen = "Smoke appears from out of nowhere!";

            _big_cloud(CLOUD_GREY_SMOKE, 20, 7 + random2(7));
            break;
        case 1:
            you_msg      = "A wave of violent energy washes through your body!";
            mon_msg_seen = "@The_monster@ lurches violently!";
            _ouch(6 + random2avg(7, 2));
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "Energy rips through your body!";
            mon_msg_seen = "@The_monster@ jerks violently!";
            _ouch(9 + random2avg(17, 2));
            break;
        case 1:
            you_msg        = "You are caught in a violent explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a violent explosion!";
            mon_msg_unseen = "A violent explosion happens from out of thin "
                             "air!";

            beam.flavour = BEAM_MISSILE;
            beam.damage  = dice_def(3, 12);
            beam.name    = "explosion";
            beam.colour  = random_colour();

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "You are blasted with magical energy!";
            mon_msg_seen = "@The_monster@ is blasted with magical energy!";
            // No message for invis monsters?
            _ouch(12 + random2avg(29, 2));
            break;
        case 1:
            all_msg = "There is a sudden explosion of magical energy!";

            beam.flavour = BEAM_MISSILE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "explosion";
            beam.colour  = random_colour();
            beam.ex_size = random_range(1, 2);

            _explosion();
            break;
        }
    }
}

static void _your_hands_glow(actor* target, string& you_msg,
                             string& mon_msg_seen, bool pluralise)
{
    you_msg      = "Your @hands@ ";
    mon_msg_seen = "@The_monster@'s @hands@ ";
    // No message for invisible monsters.

    if (pluralise)
    {
        you_msg      += "glow";
        mon_msg_seen += "glow";
    }
    else
    {
        you_msg      += "glows";
        mon_msg_seen += "glows";
    }

    you_msg      += " momentarily.";
    mon_msg_seen += " momentarily.";
}

void MiscastEffect::_hexes(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg      = "You feel off-balance for a moment.";
            mon_msg_seen = "@The_monster@ looks off-balance for a moment.";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg      = "You feel a bit tired.";
            mon_msg_seen = "@The_monster@ looks sleepy for a second.";
            break;
        case 4:
            you_msg        = "The light around you dims momentarily.";
            mon_msg_seen   = "The light around @the_monster@ dims momentarily.";
            mon_msg_unseen = "A patch of light dims momentarily.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            // Mental noises ignore silence.
            all_msg = "You hear an indistinct dissonance whispering inside your mind.";
            break;
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(target->is_player() ? 2 : 1))
        {
        case 0:
            if (target->is_player())
                you.backlight();
            else
                target->as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                           act_source));
            break;
        case 1:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            if (target->is_player())
            {
                mpr("You feel dizzy.");
                you.increase_duration(DUR_VERTIGO, 10 + random2(11), 50);
                break;
            }
            // Intentional fall-through for monsters.
        case 3:
        case 4:
        case 5:
            target->slow_down(act_source, 10);
            break;
        case 6:
            // XXX: Monster silence shrinks awkwardly (i.e. very
            //  suddenly) due to its usual offensive use. Maybe adjust?
            if (target->is_player())
                you.increase_duration(DUR_SILENCE, 5 + random2(11), 50);
            else if (target->is_monster())
            {
                 target->as_monster()->add_ench(mon_enchant(ENCH_SILENCE,
                 0, act_source, 5 + random2(11) * BASELINE_DELAY));
            }
            you_msg        = "An unnatural silence engulfs you.";
            mon_msg_seen   = "An unnatural silence engulfs @The_monster@.";
            do_msg();
            invalidate_agrid(true);
            break;
        }
        break;

    case 3:         // potentially lethal
        // Avoid the last case for monsters.
        bool reroll = true;
        while (reroll)
        {
            switch (random2(target->is_player() ? 4 : 3))
            {
            case 0:
                // FIXME: Spellbinder should get the same trick as
                // sleeping needles to actually see this effect.
                reroll = !_sleep(3 + random2(7));
                break;
            case 1:
                target->confuse(act_source, 5 + random2(3));
                reroll = false;
                break;
            case 2:
                you_msg        = "You feel susceptible to magic.";
                mon_msg_seen   = "@The_monster@ looks more susceptible to magic.";
                if (target->is_player())
                    you.increase_duration(DUR_LOWERED_MR, 20 + random2(11), 50);
                else if (target->is_monster())
                {
                     target->as_monster()->add_ench(mon_enchant(ENCH_LOWERED_MR,
                     0, act_source, 20 + random2(11) * BASELINE_DELAY));
                }
                do_msg();
                reroll = false;
                break;
            case 3:
                contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
                reroll = false;
                break;
            }
        }
        break;
    }
}

void MiscastEffect::_charms(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with"
                           " energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Waves of light ripple over your body.";
            mon_msg_seen = "Waves of light ripple over @the_monster@'s body.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear something strange.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
                return;
            }
            else if (target->is_player())
            {
                if (you.species == SP_OCTOPODE)
                    you_msg = "Your beak vibrates slightly."; // the only hard part
                else
                    you_msg = "Your skull vibrates slightly.";
            }
            break;
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(target->is_player() ? 2 : 1))
        {
        case 0:
            if (target->is_player())
                you.backlight();
            else
                target->as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                           act_source));
            break;
        case 1:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            you_msg      = "You feel enfeebled.";
            mon_msg_seen = "@The_monster@ is weakened.";
            do_msg();
            if (target->is_player())
                you.increase_duration(DUR_WEAK, 10 + random2(6), 50);
            else if (target->is_monster())
            {
                 target->as_monster()->add_ench(mon_enchant(ENCH_WEAK,
                 0, act_source, 10 + random2(6) * BASELINE_DELAY));
            }
            break;
        case 3:
        case 4:
        case 5:
            target->slow_down(act_source, 10);
            break;
        case 6:
            if (target->can_go_berserk())
                target->go_berserk(false);
            else
                target->slow_down(act_source, 10);
            break;
        }
        break;

    case 3:         // potentially lethal
        // Avoid the last case for monsters.
        bool reroll = true;
        while (reroll)
        {
            switch (random2(target->is_player() ? 4 : 2))
            {
            case 0:
                reroll = !_paralyse(2 + random2(6));
                break;
            case 1:
                target->confuse(act_source, 5 + random2(3));
                reroll = false;
                break;
            case 2:
                if (special_source == HELL_EFFECT_MISCAST)
                    all_msg = "Magic is drained from your body!";
                you_msg        = "Magic surges out from your body!";
                mon_msg_seen   = "Magic surges out from @the_monster@!";
                mon_msg_unseen = "Magic surges out from thin air!";
                if (target->is_player())
                {
                    debuff_player();
                    if (you.magic_points > 0)
                        dec_mp(4 + random2(3));
                }
                else if (target->is_monster())
                {
                    debuff_monster(*target->as_monster());
                    enchant_actor_with_flavour(target->as_monster(), nullptr,
                                               BEAM_DRAIN_MAGIC, 50 + random2avg(51, 2));
                }
                do_msg();
                reroll = false;
                break;
            case 3:
                contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
                reroll = false;
                break;
            }
        }
        break;
    }
}

void MiscastEffect::_translocation(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            all_msg = "You catch a glimpse of the back of your own head.";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with "
                           "energy!";
            mon_msg_unseen = "The air around something crackles with energy!";
            break;
        case 2:
            you_msg      = "You feel a wrenching sensation.";
            mon_msg_seen = "@The_monster@ jerks violently for a moment.";
            break;
        case 3:
            all_msg = "The floor and the ceiling briefly switch places.";
            break;
        case 4:
            you_msg      = "You spin around.";
            mon_msg_seen = "@The_monster@ spins around.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            mon_msg_unseen = "A spot of thin air glows "
                             + weird_glowing_colour() + " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "The world appears momentarily distorted!";
            mon_msg_seen = "@The_monster@ appears momentarily distorted!";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg      = "You feel uncomfortable.";
            mon_msg_seen = "@The_monster@ scowls.";
            break;
        }
        do_msg();
        break;

    case 1:         // mostly harmless
        switch (random2(6))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a localised field of spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised field of "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            _ouch(4 + random2avg(9, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space bends around you!";
            mon_msg_seen   = "Space bends around @the_monster@!";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            if (_ouch(4 + random2avg(7, 2)) && target->alive() && !target->no_tele())
                target->blink();
            break;
        case 5:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }
        break;

    case 2:         // less harmless
    reroll_2:
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a strong localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a strong localised "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            _ouch(9 + random2avg(23, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space warps around you!";
            mon_msg_seen   = "Space warps around @the_monster@!";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            _ouch(5 + random2avg(9, 2));
            if (target->alive())
            {
                // Same message as a harmless miscast, thus no permit_id.
                if (!target->no_tele(true, false))
                {
                    if (one_chance_in(3))
                        target->teleport(true);
                    else
                        target->blink();
                }
                if (target->alive())
                    target->confuse(act_source, 5 + random2(3));
            }
            break;
        case 5:
        {
            bool success = false;
            for (int i = 1 + random2(3); i > 0; --i)
                success |= _create_monster(MONS_SPATIAL_VORTEX, 3);

            if (success)
                all_msg = "Space twists in upon itself!";
            break;
        }
        case 6:
            if (!_send_to_abyss())
                goto reroll_2;
            break;
        }
        break;

    case 3:         // much less harmless
    reroll_3:
        // Don't use the last case for monsters.
        switch (random2(target->is_player() ? 4 : 3))
        {
        case 0:
            {
            you_msg        = "You are crushed and pinned by an extremely strong "
                             "localised spatial distortion!";
            mon_msg_seen   = "@The_monster@ is crushed and pinned by an extremely "
                             "strong localised spatial distortion!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";

            _ouch(15 + random2avg(29, 2));
            if (!target->alive())
                break;

            if (target->is_player())
                you.increase_duration(DUR_DIMENSION_ANCHOR, 20 + random2(11), 50);
            else if (target->is_monster())
                     target->as_monster()->add_ench(mon_enchant(ENCH_DIMENSION_ANCHOR,
                     0, act_source, 20 + random2(11) * BASELINE_DELAY));
            }
            break;
        case 1:
            you_msg        = "Space warps crazily around you!";
            mon_msg_seen   = "Space warps crazily around @the_monster@!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
            if (_ouch(9 + random2avg(17, 2)) && target->alive())
            {
                if (!target->no_tele())
                    target->teleport(true);
                if (target->alive())
                    target->confuse(act_source, 5 + random2(3));
            }
            break;
        case 2:
            if (!_send_to_abyss())
                goto reroll_3;
            break;
        case 3:
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            break;
        }
        break;
    }
}

void MiscastEffect::_summoning(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            you_msg      = "Shadowy shapes form in the air around you, "
                           "then vanish.";
            mon_msg_seen = "Shadowy shapes form in the air around "
                           "@the_monster@, then vanish.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg        = "You hear strange voices.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel momentarily dizzy.";
            break;
        case 2:
            you_msg = "Your head hurts.";
            // Monster messages needed.
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg = "Your brain hurts!";
            // Monster messages needed.
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "The world appears momentarily distorted.";
            mon_msg_seen = "@The_monster@ appears momentarily distorted.";
            break;
        case 7:
            all_msg = "A fistful of eyes flickers past your view.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                you_msg        = "Distant voices call out to you!";
                mon_msg_seen   = "Distant voices call out to @the_monster@!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel watched.";
            break;
        }
        do_msg();
        break;

    case 1:         // a little bad
        switch (random2(6))
        {
        case 0:             // identical to translocation
        case 1:
        case 2:
            you_msg        = "You are caught in a localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised spatial "
                             "distortion.";
            mon_msg_unseen = "An empty piece of space distorts and twists.";
            _ouch(5 + random2avg(9, 2));
            break;
        case 3:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        case 4:
        case 5:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_LESSER), 5, true))
                all_msg = "Something appears in a flash of light!";
            do_msg();
            break;
        }
        break;

    case 2:         // more bad
        switch (random2(6))
        {
        case 0:
        {
            bool success = false;
            for (int i = 1 + random2(3); i > 0; --i)
                success |= _create_monster(MONS_SPATIAL_VORTEX, 3);

            if (success)
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }

        case 1:
        case 2:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 5, true))
                all_msg = "Something forms from out of thin air!";
            do_msg();
            break;

        case 3:
        case 4:
        case 5:
        {
            bool success = false;
            for (int i = 2 + random2(2); i > 0; --i)
                success |= _create_monster(MONS_ABOMINATION_SMALL, 5, true);

            if (success && neither_end_silenced())
            {
                you_msg        = "A chorus of moans calls out to you!";
                mon_msg        = "A chorus of moans calls out!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
            }
            do_msg();
            break;
        }
        }
        break;

    case 3:         // more bad
        bool reroll = true;

        while (reroll)
        {
            switch (random2(5))
            {
            case 0:
            {
                bool success = false;
                for (int i = 1 + random2(3); i > 0; --i)
                    success |= _create_monster(MONS_WORLDBINDER, 5, true);

                if (success)
                    all_msg = "Desperate hands claw out from thin air!";
                do_msg();
                reroll = false;
                break;
            }
            case 1:
                if (_create_monster(summon_any_demon(RANDOM_DEMON_GREATER), 0, true))
                    all_msg = "You sense a hostile presence.";
                do_msg();
                reroll = false;
                break;

            case 2:
            {
                bool success = false;
                for (int i = 1 + random2(2); i > 0; --i)
                    success |= _create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 3, true);

                if (success)
                {
                    you_msg = "Something turns its malign attention towards "
                              "you...";
                    mon_msg = "You sense a malign presence.";
                }
                do_msg();
                reroll = false;
                break;
            }

            case 3:
                reroll = !_send_to_abyss();
                break;

            case 4:
                reroll = !_malign_gateway(target->is_player());
                break;
            }
        }

        break;
    }
}

void MiscastEffect::_necromancy(int severity)
{
    if (target->is_player()
        && have_passive(passive_t::miscast_protection_necromancy))
    {
        if (spell != SPELL_NO_SPELL)
        {
            // An actual necromancy miscast.
            if (x_chance_in_y(you.piety, piety_breakpoint(5)))
            {
                simple_god_message(" protects you from your miscast "
                                   "necromantic spell!");
                return;
            }
        }
        else if (special_source == MUMMY_MISCAST)
        {
            if (coinflip())
            {
                simple_god_message(" averts the curse.");
                return;
            }
            else
            {
                simple_god_message(" partially averts the curse.");
                severity = max(severity - 1, 0);
            }
        }
    }

    switch (severity)
    {
    case 0:
        switch (random2(10))
        {
        case 0:
            if (you.can_smell())
                all_msg = "You smell decay.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg        = "You hear strange and distant voices.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
            }
            else if (target->is_player())
                you_msg = "You feel homesick.";
            break;
        case 2:
            you_msg = "Pain shoots through your body.";
            mon_msg_seen = "@The_monster@ twitches violently.";
            break;
        case 3:
            if (you.species == SP_OCTOPODE)
                you_msg = "You feel numb.";
            else
                you_msg = "Your bones ache.";
            mon_msg_seen = "@The_monster@ pauses, visibly distraught.";
            break;
        case 4:
            you_msg      = "The world around you seems to dim momentarily.";
            mon_msg_seen = "@The_monster@ seems to dim momentarily.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 7:
            you_msg        = "You sense a malignant aura.";
            mon_msg_seen   = "@The_monster@ is briefly tinged with black.";
            mon_msg_unseen = "The air has a black tinge for a moment.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg      = "You feel very uncomfortable.";
            mon_msg_seen = "@The_monster@ scowls horribly.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit nasty
        switch (random2(3))
        {
        case 0:
            if (target->res_torment())
            {
                you_msg      = "You feel weird for a moment.";
                mon_msg_seen = "@The_monster@ has a weird expression for a "
                               "moment.";
            }
            else
            {
                you_msg      = "Pain shoots through your body!";
                mon_msg_seen = "@The_monster@ convulses with pain!";
                _ouch(5 + random2avg(15, 2));
            }
            break;
        case 1:
            you_msg      = "You feel horribly lethargic.";
            mon_msg_seen = "@The_monster@ looks incredibly listless.";
            target->slow_down(act_source, 15);
            break;
        case 2:
            if (!target->res_rotting())
            {
                you_msg      = "Your flesh rots away!";
                mon_msg_seen = "@The_monster@ rots away!";

                // Must produce the message before rotting, because that
                // might kill a target monster, and do_msg does not like
                // dead monsters. FIXME: We should avoid_lethal here!
                if (!did_msg)
                    do_msg();
                target->rot(act_source, 1, true);
            }
            else if (you.species == SP_MUMMY)
            {
                // Monster messages needed.
                you_msg = "Your bandages flutter.";
            }
            break;
        }
        if (!did_msg)
            do_msg();
        break;

    case 2:         // much nastier
        switch (random2(3))
        {
        case 0:
        {
            bool success = false;
            for (int i = 0; i < 2; ++i)
                success |= _create_monster(MONS_SHADOW, 2, true);

            if (success)
            {
                you_msg        = "Flickering shadows surround you.";
                mon_msg_seen   = "Flickering shadows surround @the_monster@.";
                mon_msg_unseen = "Shadows flicker in the thin air.";
            }
            do_msg();
            break;
        }

        case 1:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            if (one_chance_in(3))
            {
                do_msg();
                target->drain_exp(act_source, false, 100);
                break;
            }

            // If we didn't do anything, just flow through...
            if (did_msg)
                break;

        case 2:
            if (target->res_torment())
            {
                you_msg      = "You feel weird for a moment.";
                mon_msg_seen = "@The_monster@ has a weird expression for a "
                               "moment.";
            }
            else
            {
                you_msg      = "You convulse helplessly as pain tears through "
                               "your body!";
                mon_msg_seen = "@The_monster@ convulses helplessly with pain!";
                _ouch(15 + random2avg(23, 2));
            }
            if (!did_msg)
                do_msg();
            break;
        }
        break;

    case 3:         // even nastier
        // Don't use last case for monsters.
        switch (random2(target->is_player() ? 6 : 5))
        {
        case 0:
            if (target->holiness() & MH_UNDEAD)
            {
                you_msg      = "Something just walked over your grave. No, "
                               "really!";
                mon_msg_seen = "@The_monster@ seems frightened for a moment.";
                do_msg();
            }
            else
                torment_cell(target->pos(), nullptr, TORMENT_MISCAST);
            break;

        case 1:
            target->rot(act_source, 2 + random2(2));
            break;

        case 2:
            if (_create_monster(MONS_SOUL_EATER, 4, true))
            {
                you_msg        = "Something reaches out for you...";
                mon_msg_seen   = "Something reaches out for @the_monster@...";
                mon_msg_unseen = "Something reaches out from thin air...";
            }
            do_msg();
            break;

        case 3:
            if (_create_monster(MONS_REAPER, 4, true))
            {
                you_msg        = "Death has come for you...";
                mon_msg_seen   = "Death has come for @the_monster@...";
                mon_msg_unseen = "Death appears from thin air...";
            }
            do_msg();
            break;

        case 4:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            do_msg();
            target->drain_exp(act_source, false, 100);
            break;

            // If we didn't do anything, just flow through if it's the player.
            if (target->is_monster() || did_msg)
                break;

        case 5:
            lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_transmutation(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg        = "The air around you crackles with energy!";
            mon_msg_seen   = "The air around @the_monster@ crackles with "
                             "energy!";
            mon_msg_unseen = "The thin air crackles with energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            mon_msg = "There is a strange surge of energy around @the_monster@.";
            break;
        case 4:
            you_msg        = "Waves of light ripple over your body.";
            mon_msg_seen   = "Waves of light ripple over @the_monster@'s body.";
            mon_msg_unseen = "Waves of light ripple in the air.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (you.can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s momentarily turn%s into snakes!",
                                   hair.c_str(), plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(target->is_player() ? 10 : 6))
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            you_msg      = "Your body is twisted painfully.";
            mon_msg_seen = "@The_monster@'s body twists unnaturally.";
            _ouch(1 + random2avg(11, 2));
            break;
        case 5:
        {
            // Should be actual monsters and not durable summons,
            // but there's a bunch of problems there (corpses,
            // piety, perma-allies-on-hostiles-somehow, etc)
            bool success = false;
            for (int i = 2 + random2(3); i > 0; --i)
                success |= _create_monster(MONS_GIANT_COCKROACH, 0, true);

            if (success)
            {
                you_msg        = "Shape-changing energy floods out "
                                 "from your body and enlargens the floor bugs!";
                mon_msg_seen   = "Shape-changing energy floods out "
                                 "from @the_monster@ and enlargens the floor bugs!";
                mon_msg_unseen = "Shape-changing energy pours out "
                                 "from thin air and enlargens the floor bugs!";
            }
            do_msg();
            break;
        }
        case 6:
        case 7:
        case 8:
        case 9:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        // Last case for players only.
        switch (random2(target->is_player() ? 5 : 4))
        {
        case 0:
            if (target->polymorph(0)) // short duration
                break;
        case 1:
        case 2:
            you_msg      = "Your body is twisted very painfully!";
            mon_msg_seen = "@The_monster@'s body twists and writhes.";
            _ouch(3 + random2avg(23, 2));
            break;
        case 3:
            you_msg        = "Your limbs ache and wobble like jelly!";
            mon_msg_seen   = "@The_monster@ looks weaker.";
            do_msg();

            if (target->is_player())
                you.increase_duration(DUR_WEAK, 10 + random2(6), 50);
            else if (target->is_monster())
            {
                 target->as_monster()->add_ench(mon_enchant(ENCH_WEAK,
                 0, act_source, 10 + random2(6) * BASELINE_DELAY));
            }
            break;
        case 4:
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            break;
        }
        break;

    case 3:         // even nastier
        if (coinflip() && target->polymorph(200))
            break;

        switch (random2(3))
        {
        case 0:
            you_msg = "Your body is flooded with distortional energies!";
            mon_msg = "@The_monster@'s body is flooded with distortional "
                      "energies!";
            if (_ouch(3 + random2avg(18, 2)) && target->alive())
            {
                if (target->is_player())
                {
                    contaminate_player(random2avg(34000, 3),
                                       spell != SPELL_NO_SPELL, false);
                }
                else
                    target->polymorph(0);
            }
            break;

        case 1:
            // HACK: Avoid lethality before deleting mutation, since
            // afterwards a message would already have been given.
            if (lethality_margin > 0
                && (you.hp - lethality_margin) <= 27
                && avoid_lethal(you.hp))
            {
                return;
            }

            if (target->is_player())
            {
                you_msg = "You feel very strange.";
                delete_mutation(RANDOM_MUTATION, cause, true, false, false, false);
            }
            else
                target->polymorph(0);
            _ouch(5 + random2avg(23, 2));
            break;

        case 2:
            if (target->is_player())
            {
                // HACK: Avoid lethality before giving mutation, since
                // afterwards a message would already have been given.
                if (lethality_margin > 0
                    && (you.hp - lethality_margin) <= 27
                    && avoid_lethal(you.hp))
                {
                    return;
                }

                _malmutate();
            }
            else
                target->malmutate(cause);
            _ouch(5 + random2avg(23, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_fire(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Sparks fly from the ground!";
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you burns with energy!";
            mon_msg_seen = "The air around @the_monster@ burns with energy!";
            break;
        case 2:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Wisps of smoke drift around you.";
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s @hands@.";
            break;
        case 3:
            all_msg = "You have a brief vision of globes of primordial fire.";
            break;
        case 4:
            if (you.can_smell())
                all_msg = "You smell smoke.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 5:
            you_msg = "Heat runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably hot.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Lukewarm flames ripple over your body.";
            mon_msg_seen = "Dim flames ripple over @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a sizzling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel like you have heartburn.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
        {
            bool success = false;
            for (int i = 1 + random2(2); i > 0; --i)
                success |= _create_monster(MONS_FIRE_VORTEX, 2, true);

            if (success)
            {
                if (special_source == HELL_EFFECT_MISCAST)
                    all_msg = "Fire whirls out of nowhere!";
                you_msg        = "Fire whirls out from your @hands@!";
                mon_msg_seen   = "Fire whirls out @the_monster@'s @hands@!";
                mon_msg_unseen = "Fire whirls out of nowhere!";
                // FIXME: do_msg(); freaks out with the secondary effect here:
                // _big_cloud(random_smoke_type(), 20, 7 + random2(7));
                // If the crash can be figured out, re-add it and adjust messages.
            }
            do_msg();
            break;
        }
        case 1:
            you_msg      = "Flames sear your flesh.";
            mon_msg_seen = "Flames sear @the_monster@.";
            if (target->res_fire() < 0)
            {
                if (!_ouch(2 + random2avg(13, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_FIRE, 3);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are blasted with fire.";
            mon_msg_seen   = "@The_monster@ is blasted with fire.";
            mon_msg_unseen = "A flame briefly burns in thin air.";
            if (_ouch(5 + random2avg(29, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 5);
            break;

        case 1:
            you_msg        = "You are caught in a fiery explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a fiery explosion!";
            mon_msg_unseen = "Fire explodes from out of thin air!";

            beam.flavour = BEAM_FIRE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 14);
            beam.name    = "explosion";
            beam.colour  = RED;

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(3))
        {
        case 0:
            you_msg        = "You are blasted with searing flames! "
                             "The searing flames burn away your fire resistance!";
            mon_msg_seen   = "@The_monster@ is blasted with searing flames! "
                             "The searing flames burn away @possessive@ fire resistance!";
            mon_msg_unseen = "A large flame burns hotly for a moment in the "
                             "thin air.";

            if (_ouch(9 + random2avg(33, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 10);
            if (!target->alive())
                break;

            if (target->is_player())
                you.increase_duration(DUR_FIRE_VULN, 15 + random2(11), 50);
            else if (target->is_monster())
                     target->as_monster()->add_ench(mon_enchant(ENCH_FIRE_VULN,
                     0, act_source, 15 + random2(11) * BASELINE_DELAY));
            break;
        case 1:
            all_msg = "There is a sudden and violent explosion of flames!";

            beam.flavour = BEAM_FIRE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "fireball";
            beam.colour  = RED;
            beam.ex_size = random_range(1, 2);

            _explosion();
            break;
        case 2:
        {
            you_msg      = "You are covered in liquid flames!";
            mon_msg_seen = "@The_monster@ is covered in liquid flames!";
            do_msg();

            if (target->is_player())
                napalm_player(random2avg(7,3) + 1, cause);
            else
            {
                monster* mon_target = target->as_monster();
                mon_target->add_ench(mon_enchant(ENCH_STICKY_FLAME,
                    min(4, 1 + random2(mon_target->get_hit_dice()) / 2),
                    act_source));
            }
            break;
        }
        }
        break;
    }
}

void MiscastEffect::_ice(int severity)
{
    const dungeon_feature_type feat = grd(target->pos());

    const bool frostable_feat =
        (feat == DNGN_FLOOR || feat_altar_god(feat) != GOD_NO_GOD
         || feat_is_staircase(feat) || feat_is_water(feat));

    const string feat_name = (feat == DNGN_FLOOR ? "the " : "") +
        feature_description_at(target->pos(), false, DESC_THE);

    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (frostable_feat ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 1:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 2:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Wisps of condensation drift around you.";
            you_msg        = "Wisps of condensation drift from your @hands@.";
            mon_msg_seen   = "Wisps of condensation drift from @the_monster@'s "
                             "@hands@.";
            mon_msg_unseen = "Wisps of condensation drift in the air.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Your @hands@ feel@hand_conj@ numb with cold.";
            // Monster messages needed.
            break;
        case 5:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably cold.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Frost covers your body.";
            mon_msg_seen = "Frost covers @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a crackling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "A snowflake lands on your nose.";
            break;
        case 10:
            if (feat_is_water(feat))
                all_msg  = "A thin layer of ice forms on " + feat_name;
            else
                all_msg  = "Frost spreads across " + feat_name;
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg = "You feel extremely cold.";
            mon_msg_seen = "@The_monster@ shivers.";
            break;
        case 1:
            you_msg      = "You are covered in a thin layer of ice.";
            mon_msg_seen = "@The_monster@ is covered in a thin layer of ice.";
            if (target->res_cold() < 0)
            {
                if (!_ouch(4 + random2avg(5, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_COLD, 2, false);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg = "Heat is drained from your body!";
            mon_msg = "Heat is drained from @the_monster@.";
            if (_ouch(5 + random2(6) + random2(7), BEAM_COLD) && target->alive())
                target->expose_to_element(BEAM_COLD, 4);
            if (target->is_player() && !you_foodless())
                you.increase_duration(DUR_NO_POTIONS, 10 + random2(11), 50);
            break;

        case 1:
            you_msg        = "You are caught in an explosion of ice and frost!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "ice and frost!";
            mon_msg_unseen = "Ice and frost explode from out of thin air!";

            beam.flavour = BEAM_COLD;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 11);
            beam.name    = "explosion";
            beam.colour  = WHITE;

            _explosion();
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are encased in ice!";
            mon_msg_seen   = "@The_monster@ is encased in ice!";
            mon_msg_unseen = "An unseen figure is encased in ice!";

            if (_ouch(9 + random2avg(23, 2), BEAM_ICE) && target->alive())
                target->expose_to_element(BEAM_COLD, 9);
            if (!target->alive())
                break;

            if (target->is_player())
                you.increase_duration(DUR_FROZEN, 6 + random2(11), 50);
            else if (target->is_monster())
            {
                 target->as_monster()->add_ench(mon_enchant(ENCH_FROZEN,
                 0, act_source, 6 + random2(16) * BASELINE_DELAY));
            }
            break;

            break;
        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Freezing gases billow around you!";
            you_msg        = "Freezing gases pour from your @hands@!";
            mon_msg_seen   = "Freezing gases pour from @the_monster@'s "
                             "@hands@!";

            _big_cloud(CLOUD_COLD, 20, 8 + random2(4));
            break;
        }
        break;
    }
}

static bool _on_floor(actor* target)
{
    return target->ground_level() && grd(target->pos()) == DNGN_FLOOR;
}

void MiscastEffect::_earth(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
    case 1:
        num = 11 + (_on_floor(target) ? 2 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg = "You feel earthy.";
            // Monster messages needed.
            break;
        case 1:
            you_msg        = "You are showered with tiny particles of grit.";
            mon_msg_seen   = "@The_monster@ is showered with tiny particles "
                             "of grit.";
            mon_msg_unseen = "Tiny particles of grit hang in the air for a "
                             "moment.";
            break;
        case 2:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Sand pours from out of thin air.";
            you_msg        = "Sand pours from your @hands@.";
            mon_msg_seen   = "Sand pours from @the_monster@'s @hands@.";
            mon_msg_unseen = "Sand pours from out of thin air.";
            break;
        case 3:
            you_msg = "You feel a surge of energy from the ground.";
            // Monster messages needed.
            break;
        case 4:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a distant rumble.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You sympathise with the stones.";
            break;
        case 5:
            you_msg = "You feel gritty.";
            // Monster messages needed.
            break;
        case 6:
            you_msg      = "You feel momentarily lethargic.";
            mon_msg_seen = "@The_monster@ looks momentarily listless.";
            break;
        case 7:
            you_msg        = "Motes of dust swirl before your eyes.";
            mon_msg_seen   = "Motes of dust swirl around @the_monster@.";
            mon_msg_unseen = "Motes of dust swirl around in the air.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
        {
            bool          pluralised = true;
            string        feet       = you.foot_name(true, &pluralised);
            ostringstream str;

            str << "Your " << feet << (pluralised ? " feel" : " feels")
                << " warm.";

            you_msg = str.str();
            // Monster messages needed.
            break;
        }
        case 10:
            if (target->cannot_move())
            {
                you_msg      = "You briefly vibrate.";
                mon_msg_seen = "@The_monster@ briefly vibrates.";
            }
            else
            {
                you_msg      = "You momentarily stiffen.";
                mon_msg_seen = "@The_monster@ momentarily stiffens.";
            }
            break;
        case 11:
            all_msg = "The floor vibrates.";
            break;
        case 12:
            all_msg = "The floor shifts beneath you alarmingly!";
            break;
        }
        do_msg();
        break;

    case 2:         // slightly less harmless stuff
        switch (random2(1))
        {
        case 0:
            switch (random2(3))
            {
            case 0:
                you_msg        = "You are hit by flying rocks!";
                mon_msg_seen   = "@The_monster@ is hit by flying rocks!";
                mon_msg_unseen = "Flying rocks appear out of thin air!";
                break;
            case 1:
                you_msg        = "You are blasted with sand!";
                mon_msg_seen   = "@The_monster@ is blasted with sand!";
                mon_msg_unseen = "A miniature sandstorm briefly appears!";
                break;
            case 2:
                you_msg        = "Rocks fall onto you out of nowhere!";
                mon_msg_seen   = "Rocks fall onto @the_monster@ out of "
                                 "nowhere!";
                mon_msg_unseen = "Rocks fall out of nowhere!";
                break;
            }
            _ouch(target->apply_ac(random2avg(13, 2) + 10));
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(5))
        {
        case 0:
        case 1:
        case 2:
        case 3:
            you_msg        = "You are caught in an explosion of flying "
                             "shrapnel!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "flying shrapnel!";
            mon_msg_unseen = "Flying shrapnel explodes from thin air!";

            beam.flavour = BEAM_FRAG;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 15);
            beam.name    = "explosion";
            beam.colour  = CYAN;

            if (one_chance_in(5))
                beam.colour = BROWN;
            if (one_chance_in(5))
                beam.colour = LIGHTCYAN;

            _explosion();
            break;

        case 4:
            target->petrify(act_source);
        }
        break;
    }
}

void MiscastEffect::_air(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 9;
        if (target->is_player())
            num += 3 + _has_hair(target);
        switch (random2(num))
        {
        case 0:
            you_msg      = "You feel momentarily weightless.";
            mon_msg_seen = "@The_monster@ bobs in the air for a moment.";
            break;
        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Wisps of vapour drift around you.";
            you_msg      = "Wisps of vapour drift from your @hands@.";
            mon_msg_seen = "Wisps of vapour drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 2:
        {
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Sparks of electricity dance around you.";

            bool pluralised = true;
            if (!hand_str.empty())
                pluralised = can_plural_hand;
            else
                target->hand_name(true, &pluralised);

            if (pluralised)
            {
                you_msg      = "Sparks of electricity dance between your "
                               "@hands@.";
                mon_msg_seen = "Sparks of electricity dance between "
                               "@the_monster@'s @hands@.";
            }
            else
            {
                you_msg      = "Sparks of electricity dance over your "
                               "@hand@.";
                mon_msg_seen = "Sparks of electricity dance over "
                               "@the_monster@'s @hand@.";
            }
            break;
        }
        case 3:
            you_msg      = "You are blasted with air!";
            mon_msg_seen = "@The_monster@ is blasted with air!";
            break;
        case 4:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a whooshing sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (you.can_smell())
                all_msg = "You smell ozone.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 5:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 6:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a crackling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (you.can_smell())
                all_msg = "You smell something musty.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 7:
            you_msg      = "There is a short, sharp shower of sparks.";
            mon_msg_seen = "@The_monster@ is briefly showered in sparks.";
            break;
        case 8:
            if (silenced(you.pos()))
            {
                you_msg        = "The wind whips around you!";
                mon_msg_seen   = "The wind whips around @the_monster@!";
                mon_msg_unseen = "The wind whips!";
            }
            else
            {
                you_msg        = "The wind howls around you!";
                mon_msg_seen   = "The wind howls around @the_monster@!";
                mon_msg_unseen = "The wind howls!";
            }
            break;
        case 9:
            you_msg = "Ouch! You feel a sudden electric shock.";
            // Monster messages needed.
            break;
        case 10:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 11:
            you_msg = "You feel electric!";
            // Monster messages needed.
            break;
        case 12:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
            break;
        }
        }
        do_msg();
        break;

    case 1:         // rather less harmless stuff
        switch (random2(3))
        {
        case 0:
            you_msg        = "Electricity courses through your body.";
            mon_msg_seen   = "@The_monster@ is jolted!";
            mon_msg_unseen = "Something invisible sparkles with electricity.";
            _ouch(4 + random2avg(9, 2), BEAM_ELECTRICITY);
            break;
        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Noxious gases billow around you.";
            you_msg        = "Noxious gases pour from your @hands@!";
            mon_msg_seen   = "Noxious gases pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gases appear from out of thin air!";

            _big_cloud(CLOUD_MEPHITIC, 20, 9 + random2(4));
            break;
        case 2:
            you_msg        = "You are under the weather.";
            mon_msg_seen   = "@The_monster@ looks under the weather.";
            mon_msg_unseen = "Inclement weather forms around a spot in thin air.";

            _big_cloud(CLOUD_RAIN, 20, 20 + random2(20));
            break;
        }
        break;

    case 2:         // much less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are caught in an explosion of electrical "
                             "discharges!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "electrical discharges!";
            mon_msg_unseen = "Electrical discharges explode from out of "
                             "thin air!";

            beam.flavour = BEAM_ELECTRICITY;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 8);
            beam.name    = "explosion";
            beam.colour  = LIGHTBLUE;
            beam.ex_size = one_chance_in(4) ? 1 : 2;

            _explosion();
            break;
        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Venomous gases billow around you!";
            you_msg        = "Venomous gases pour from your @hands@!";
            mon_msg_seen   = "Venomous gases pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gases pour forth from the thin air!";

            _big_cloud(CLOUD_POISON, 20, 8 + random2(5));
            break;
        }
        break;

    case 3:         // even less harmless stuff
        switch (random2(5))
        {
        case 0:
        case 1:
            if (_create_monster(MONS_BALL_LIGHTNING, 3))
                all_msg = "A ball of electricity appears!";
            do_msg();
            break;
        case 2:
        case 3:
            you_msg        = "The air twists around and strikes you!";
            mon_msg_seen   = "@The_monster@ is struck by twisting air!";
            mon_msg_unseen = "The air madly twists around a spot.";
            _ouch(12 + random2avg(29, 2), BEAM_AIR);
            break;
        case 4:
            if (_create_monster(MONS_TWISTER, 1))
                all_msg = "A huge vortex of air appears!";
            do_msg();
            break;
        }
        break;
    }
}

void MiscastEffect::_poison(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            you_msg      = "You feel mildly nauseous.";
            mon_msg_seen = "@The_monster@ briefly looks nauseous.";
            break;
        case 1:
            you_msg      = "You feel slightly ill.";
            mon_msg_seen = "@The_monster@ briefly looks sick.";
            break;
        case 2:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Wisps of poison gas drift around you.";
            you_msg      = "Wisps of poison gas drift from your @hands@.";
            mon_msg_seen = "Wisps of poison gas drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You feel faint for a moment.";
            mon_msg_seen = "@The_monster@ looks faint for a moment.";
            break;
        case 5:
            you_msg      = "You feel sick.";
            mon_msg_seen = "@The_monster@ looks sick.";
            break;
        case 6:
            you_msg      = "You feel odd.";
            mon_msg_seen = "@The_monster@ has an odd expression for a "
                           "moment.";
            break;
        case 7:
            you_msg      = "You feel weak for a moment.";
            mon_msg_seen = "@The_monster@ looks weak for a moment.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg        = "Your vision is briefly tinged with green.";
            mon_msg_seen   = "@The_monster@ is briefly tinged with green.";
            mon_msg_unseen = "The air has a green tinge for a moment.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel sick.";
                mon_msg_seen = "@The_monster@ looks sick.";
                _do_poison(7 + random2(9));
            }
            do_msg();
            break;

        case 1:
            if (cell_is_solid(target->pos()))
                break;
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Noxious gases billow around you!";
            you_msg        = "Noxious gases pour from your @hands@!";
            mon_msg_seen   = "Noxious gases pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gases pour forth from the thin air!";
            place_cloud(CLOUD_MEPHITIC, target->pos(), 2 + random2(4),
                        act_source);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(3))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel very sick.";
                mon_msg_seen = "@The_monster@ looks very sick.";
                _do_poison(14 + random2avg(17, 2));
            }
            do_msg();
            break;

        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Noxious gases billow around you!";
            you_msg        = "Noxious gases pour from your @hands@!";
            mon_msg_seen   = "Noxious gases pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gases pour forth from the thin air!";

            _big_cloud(CLOUD_MEPHITIC, 20, 8 + random2(5));
            break;

        case 2:
            if (target->res_poison() >= 3)
            {
                you_msg        = "You feel rather nauseous for a moment.";
                mon_msg_seen   = "@The_monster@ looks rather nauseous for a moment.";
                do_msg();
                break;
            }
            else
            {
                you_msg        = "You feel more vulnerable to poison.";
                mon_msg_seen   = "@The_monster@ grows more vulnerable to poison.";
                do_msg();
                if (target->is_player())
                    you.increase_duration(DUR_POISON_VULN, 20 + random2(11), 50);
                else if (target->is_monster())
                {
                     target->as_monster()->add_ench(mon_enchant(ENCH_POISON_VULN,
                     0, act_source, 20 + random2(11) * BASELINE_DELAY));
                }
                break;
            }
        }
        break;

    case 3:         // less harmless stuff
        // Don't use last case for monsters.
        switch (random2(target->is_player() ? 3 : 2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel incredibly sick.";
                mon_msg_seen = "@The_monster@ looks incredibly sick.";
                _do_poison(20 + random2avg(35, 2));
            }
            do_msg();
            break;
        case 1:
            if (special_source == HELL_EFFECT_MISCAST)
                all_msg = "Venomous gases billow around you!";
            you_msg        = "Venomous gases pour from your @hands@!";
            mon_msg_seen   = "Venomous gases pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gases pour forth from the thin air!";

            _big_cloud(CLOUD_POISON, 20, 7 + random2(7));
            break;
        case 2:
            if (player_res_poison() > 0)
                canned_msg(MSG_NOTHING_HAPPENS);
            else
                lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_do_poison(int amount)
{
    if (target->is_player())
        poison_player(amount, cause, "residual poison");
    else
        target->poison(act_source, amount);
}

void MiscastEffect::_zot()
{
    bool success = false;
    int roll;
    switch (random2(4))
    {
    case 0:    // mainly explosions
        beam.name = "explosion";
        beam.damage = dice_def(3, 20);
        beam.ex_size = random_range(1, 2);
        beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        switch (random2(7))
        {
        case 0:
        case 1:
            all_msg = "There is a sudden explosion of flames!";
            beam.flavour = BEAM_FIRE;
            beam.colour  = RED;
            _explosion();
            break;
        case 2:
        case 3:
            all_msg = "There is a sudden explosion of frost!";
            beam.flavour = BEAM_COLD;
            beam.colour  = WHITE;
            _explosion();
            break;
        case 4:
            all_msg = "There is a sudden blast of acid!";
            beam.name    = "acid blast";
            beam.flavour = BEAM_ACID;
            beam.colour  = YELLOW;
            _explosion();
            break;
        case 5:
            if (_create_monster(MONS_BALL_LIGHTNING, 3))
                all_msg = "A ball of electricity appears!";
            do_msg();
            break;
        case 6:
            if (_create_monster(MONS_TWISTER, 1))
                all_msg = "A huge vortex of air appears!";
            do_msg();
            break;
        }
        break;
    case 1:    // summons
    reroll_1:
        switch (random2(9))
        {
        case 0:
            if (_create_monster(MONS_SOUL_EATER, 4, true))
            {
                you_msg        = "Something reaches out for you...";
                mon_msg_seen   = "Something reaches out for @the_monster@...";
                mon_msg_unseen = "Something reaches out from thin air...";
            }
            do_msg();
            break;
        case 1:
            if (_create_monster(MONS_REAPER, 4, true))
            {
                you_msg        = "Death has come for you...";
                mon_msg_seen   = "Death has come for @the_monster@...";
                mon_msg_unseen = "Death appears from thin air...";
            }
            do_msg();
            break;
        case 2:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_GREATER), 0, true))
                all_msg = "You sense a hostile presence.";
            do_msg();
            break;
        case 3:
        case 4:
            for (int i = 1 + random2(2); i > 0; --i)
                success |= _create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 0, true);

            if (success)
            {
                you_msg = "Something turns its malign attention towards "
                          "you...";
                mon_msg = "You sense a malign presence.";
                do_msg();
            }
            break;
        case 5:
            for (int i = 2 + random2(2); i > 0; --i)
                success |= _create_monster(MONS_ABOMINATION_SMALL, 5, true);

            if (success && neither_end_silenced())
            {
                you_msg        = "A chorus of moans calls out to you!";
                mon_msg        = "A chorus of moans calls out!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
                do_msg();
            }
            break;
        case 6:
        case 7:
            for (int i = 2 + random2(4); i > 0; --i)
                success |= _create_monster(RANDOM_MOBILE_MONSTER, 4, true);

            if (success)
            {
                all_msg = "Wisps of shadow whirl around...";
                do_msg();
            }
            break;
        case 8:
            if (!_malign_gateway(true))
                goto reroll_1;
            break;
        }
        break;
    case 2:
    case 3:    // other misc stuff
    reroll_2:
        // Cases at the end are for players only.
        switch (random2(target->is_player() ? 13 : 9))
        {
        case 0:
            target->paralyse(act_source, 2 + random2(4), cause);
            break;
        case 1:
            target->petrify(act_source);
            break;
        case 2:
            target->rot(act_source, 3 + random2(3));
            break;
        case 3:
            if (!_send_to_abyss())
                goto reroll_2;
            break;
        case 4:
            if (!target->is_player())
                target->polymorph(0);
            else if (coinflip())
            {
                you_msg = "You feel very strange.";
                delete_mutation(RANDOM_MUTATION, cause, true, false, false, false);
                do_msg();
            }
            else
            {
                _malmutate();
                do_msg();
            }
            break;
        case 5:
        case 6:
            roll = random2(3); // Give 2 of 3 effects.
            if (roll != 0)
                target->confuse(act_source, 5 + random2(3));
            if (roll != 1)
                target->slow_down(act_source, 5 + random2(3));
            if (roll != 2)
            {
                you_msg        = "Space warps around you!";
                mon_msg_seen   = "Space warps around @the_monster@!";
                mon_msg_unseen = "A piece of empty space twists and writhes.";
                _ouch(5 + random2avg(9, 2));
                if (target->alive())
                {
                    if (!target->no_tele())
                    {
                        if (one_chance_in(3))
                            target->teleport(true);
                        else
                            target->blink();
                    }
                }
            }
            break;
        case 7:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";
            do_msg();
            target->drain_exp(act_source, false, 100);
            break;
        case 8:
            if (target->is_player())
                contaminate_player(2000 + random2avg(13000, 2), false);
            else
                target->polymorph(0);
            break;
        case 9:
            if (you.magic_points > 0)
            {
                dec_mp(10 + random2(21));
                canned_msg(MSG_MAGIC_DRAIN);
            }
            break;
        case 10:
            lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
            break;
        case 11:
            mpr("An unnatural silence engulfs you.");
            you.increase_duration(DUR_SILENCE, 10 + random2(21), 30);
            invalidate_agrid(true);
            break;
        case 12:
            if (!mons_word_of_recall(nullptr, 2 + random2(3)))
                canned_msg(MSG_NOTHING_HAPPENS);
            break;
        }
        break;
    }
}

#include "AppHdr.h"

#include "status.h"

#include "mutation.h"
#include "player.h"
#include "skills2.h"
#include "transform.h"

// Status defaults for durations that are handled straight-forwardly.
struct duration_def
{
    duration_type dur;
    bool expire;              // whether to do automat expiring transforms
    int         light_colour; // status light base colour
    std::string light_text;   // for the status lights
    std::string short_text;   // for @: line
    std::string long_text ;   // for @ message
};

static duration_def duration_data[] =
{
    { DUR_AGILITY, false,
      0, "", "agile", "You are agile." },
    { DUR_BARGAIN, true,
      BLUE, "Brgn", "charismatic", "You get a bargain in shops." },
    { DUR_BERSERKER, false,
      0, "", "berserking", "You are possessed by a berserker rage." },
    { DUR_BREATH_WEAPON, false,
      YELLOW, "BWpn", "short of breath", "You are short of breath." },
    { DUR_BRILLIANCE, false,
      0, "", "brilliant", "You are brilliant." },
    { DUR_CONF, false,
      RED, "Conf", "confused", "You are confused." },
    { DUR_CONFUSING_TOUCH, true,
      BLUE, "Touch", "confusing touch", "" },
    { DUR_CONTROL_TELEPORT, true,
      MAGENTA, "cTele", "", "You can control teleporation." },
    { DUR_DEATH_CHANNEL, true,
      MAGENTA, "DChan", "death channel", "You are channeling the dead." },
    { DUR_DEFLECT_MISSILES, true,
      MAGENTA, "DMsl", "deflect missiles", "You deflect missiles." },
    { DUR_DIVINE_STAMINA, false,
      0, "", "divinely fortified", "You are divinely fortified." },
    { DUR_DIVINE_VIGOUR, false,
      0, "", "divinely vigorous", "You are divinely vigorous." },
    { DUR_EXHAUSTED, false,
      YELLOW, "Fatig", "exhausted", "You are exhausted." },
    { DUR_FIRE_SHIELD, true,
      BLUE, "RoF", "immune to fire clouds", "" },
    { DUR_ICY_ARMOUR, true,
      0, "", "icy armour", "You get protected by an icy armour." },
    { DUR_LIQUID_FLAMES, false,
      RED, "Fire", "liquid flames", "You are covered in liquid flames." },
    { DUR_LOWERED_MR, false,
      RED, "-MR", "", "" },
    { DUR_MAGIC_SHIELD, false,
      0, "", "shielded", "" },
    { DUR_MIGHT, false,
      0, "", "mighty", "You are mighty." },
    { DUR_MISLED, true,
      LIGHTMAGENTA, "Misled", "", "" },
    { DUR_PARALYSIS, false,
      0, "", "paralysed", "You are paralysed." },
    { DUR_PETRIFIED, false,
      0, "", "petrified", "You are petrified." },
    { DUR_PRAYER, false,
      WHITE, "Pray", "praying", "You are praying." },
    { DUR_REPEL_MISSILES, true,
      BLUE, "RMsl", "repel missiles", "You are protected from missiles." },
    { DUR_SAGE, true,
      BLUE, "Sage", "", "" },
    { DUR_SEE_INVISIBLE, false,
      0, "", "", "You can see invisible." },
    { DUR_SLAYING, false,
      0, "", "deadly", "" },
    { DUR_SLIMIFY, true,
      GREEN, "Slime", "slimy", "" },
    { DUR_SLEEP, false,
      0, "", "sleeping", "You are sleeping." },
    { DUR_STONEMAIL, true,
      0, "", "stone mail", "You are covered in scales of stone."},
    { DUR_STONESKIN, false,
      0, "", "stone skin", "Your skin is tough as stone." },
    { DUR_SWIFTNESS, true,
      BLUE, "Swift", "swift", "You can move swiftly." },
    { DUR_TELEPATHY, false,
      LIGHTBLUE, "Emp", "", "" },
    { DUR_TELEPORT, false,
      LIGHTBLUE, "Tele", "about to teleport", "You are about to teleport." },
    { DUR_DEATHS_DOOR, true,
      LIGHTGREY, "DDoor", "death's door", "" },
    { DUR_INSULATION, true,
      BLUE, "Ins", "", "You are insulated." },
    { DUR_PHASE_SHIFT, true,
      0, "", "phasing", "You are out of phase with the material plane." },
    { DUR_QUAD_DAMAGE, true,
      BLUE, "Quad", "", "" },
    { DUR_SILENCE, true,
      BLUE, "Sil", "silence", "You radiate silence." }
};

static int duration_index[NUM_DURATIONS];

void init_duration_index()
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
        duration_index[i] = -1;

    for (unsigned i = 0; i < ARRAYSZ(duration_data); ++i)
    {
        duration_type dur = duration_data[i].dur;
        ASSERT(dur >= 0 && dur < NUM_DURATIONS);
        ASSERT(duration_index[dur] == -1);
        duration_index[dur] = i;
    }
}

static const duration_def* _lookup_duration(duration_type dur)
{
    ASSERT(dur >= 0 && dur < NUM_DURATIONS);
    if (duration_index[dur] == -1)
        return (NULL);
    else
        return (&duration_data[duration_index[dur]]);
}

static void _reset_status_info(status_info* inf)
{
    inf->light_colour = 0;
    inf->light_text = "";
    inf->short_text = "";
    inf->long_text = "";
};

static int _bad_ench_colour( int lvl, int orange, int red )
{
    if (lvl > red)
        return (RED);
    else if (lvl > orange)
        return (LIGHTRED);

    return (YELLOW);
}

int dur_colour(int exp_colour, bool expiring)
{
    if (expiring)
    {
        return (exp_colour);
    }
    else
    {
        switch (exp_colour)
        {
        case GREEN:
            return (LIGHTGREEN);
        case BLUE:
            return (LIGHTBLUE);
        case MAGENTA:
            return (LIGHTMAGENTA);
        case LIGHTGREY:
            return (WHITE);
        default:
            return (exp_colour);
        }
    }
}

static void _mark_expiring(status_info* inf, bool expiring)
{
    if (expiring)
    {
        if (!inf->short_text.empty())
            inf->short_text += " (expires)";
        if (!inf->long_text.empty())
            inf->long_text = "Expiring: " + inf->long_text;
    }
}

static void _describe_airborne(status_info* inf);
static void _describe_burden(status_info* inf);
static void _describe_glow(status_info* inf);
static void _describe_hunger(status_info* inf);
static void _describe_regen(status_info* inf);
static void _describe_rotting(status_info* inf);
static void _describe_sickness(status_info* inf);
static void _describe_speed(status_info* inf);
static void _describe_poison(status_info* inf);
static void _describe_transform(status_info* inf);

void fill_status_info(int status, status_info* inf)
{
    _reset_status_info(inf);

    bool found = false;

    // Sort out inactive durations, and fill in data from duration_data
    // for the simple durations.
    if (status >= 0 && status < NUM_DURATIONS)
    {
        duration_type dur = static_cast<duration_type>(status);
        if (!you.duration[dur])
            return;

        const duration_def* ddef = _lookup_duration(dur);
        if (ddef)
        {
            found = true;
            inf->light_colour = ddef->light_colour;
            inf->light_text   = ddef->light_text;
            inf->short_text   = ddef->short_text;
            inf->long_text    = ddef->long_text;
            if (ddef->expire)
            {
                inf->light_colour = dur_colour(inf->light_colour,
                                                 dur_expiring(dur));
                _mark_expiring(inf, dur_expiring(dur));
            }
        }
    }

    // Now treat special status types and durations, possibly
    // completing or overriding the defaults set above.
    switch (status)
    {
    case STATUS_AIRBORNE:
        _describe_airborne(inf);
        break;

    case STATUS_BEHELD:
        if (you.beheld())
        {
            inf->light_colour = RED;
            inf->light_text   = "Mesm";
            inf->short_text   = "mesmerised";
            inf->long_text    = "You are mesmerised.";
        }
        break;

    case STATUS_BURDEN:
        _describe_burden(inf);
        break;

    case STATUS_GLOW:
        // includes corona
        _describe_glow(inf);
        break;

    case STATUS_NET:
        if (you.attribute[ATTR_HELD])
        {
            inf->light_colour = RED;
            inf->light_text   = "Held";
            inf->short_text   = "held";
            inf->long_text    = "You are held in a net.";
        }
        break;

    case STATUS_HUNGER:
        _describe_hunger(inf);
        break;

    case STATUS_REGENERATION:
        // DUR_REGENERATION + some vampire and non-healing stuff
        _describe_regen(inf);
        break;

    case STATUS_ROT:
        _describe_rotting(inf);
        break;

    case STATUS_SICK:
        _describe_sickness(inf);
        break;

    case STATUS_SPEED:
        _describe_speed(inf);
        break;

    case DUR_CONFUSING_TOUCH:
    {
        const int dur = you.duration[DUR_CONFUSING_TOUCH];
        const int high = 40 * BASELINE_DELAY;
        const int low  = 20 * BASELINE_DELAY;
        inf->long_text = "Your hands are glowing ";
        if (dur > high)
            inf->long_text += "an extremely bright";
        else if (dur > low)
            inf->long_text += "bright";
        else
           inf->long_text += "a soft";
        inf->long_text += "red.";
    }

    case DUR_FIRE_SHIELD:
    {
        // Might be better to handle this with an extra virtual status.
        const bool exp = dur_expiring(DUR_FIRE_SHIELD);
        if (exp)
            inf->long_text += "Expiring: ";
        inf->long_text += "You are surrounded by a ring of flames.\n";
        if (exp)
            inf->long_text += "Expiring: ";
        inf->long_text += "You are immune to clouds of flame.";
        break;
    }

    case DUR_INVIS:
        inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_INVIS));
        inf->light_text   = "Invis";
        inf->short_text   = "invisible";
        if (you.backlit())
        {
            inf->light_colour = DARKGREY;
            inf->short_text += " (but backlit and visible)";
        }
        inf->long_text = "You are " + inf->short_text + ".";
        _mark_expiring(inf, dur_expiring(DUR_INVIS));
        break;

    case DUR_POISONING:
        _describe_poison(inf);
        break;

    case DUR_POWERED_BY_DEATH:
        if (handle_pbd_corpses(false) > 0)
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = "PbD";
        }
        break;

    case DUR_REPEL_MISSILES:
        // no status light or short text when also deflecting
        if (you.duration[DUR_DEFLECT_MISSILES])
        {
            inf->light_colour = 0;
            inf->light_text   = "";
            inf->short_text   = "";
        }
        break;

    case DUR_SAGE:
    {
        std::string sk = skill_name(you.sage_bonus_skill);
        inf->short_text = "studying " + sk;
        inf->long_text = "You are " + inf->short_text + ".";
        _mark_expiring(inf, dur_expiring(DUR_SAGE));
        break;
    }

    case DUR_SURE_BLADE:
    {
        inf->light_colour = BLUE;
        inf->light_text   = "Blade";
        inf->short_text   = "bonded with blade";
        std::string desc;
        if (you.duration[DUR_SURE_BLADE] > 15 * BASELINE_DELAY)
            desc = "strong ";
        else if (you.duration[DUR_SURE_BLADE] >  5 * BASELINE_DELAY)
            desc = "";
        else
            desc = "weak";
        inf->long_text = "You have a " + desc + "bond with your blade.";
        break;
    }

    case DUR_TRANSFORMATION:
        _describe_transform(inf);
        break;

    default:
        if (!found)
        {
            inf->light_colour = RED;
            inf->light_text   = "Missing";
            inf->short_text   = "missing status";
            inf->long_text    = "Missing status description.";
        }
        break;
    }
}

static void _describe_hunger(status_info* inf)
{
    const bool vamp = (you.species == SP_VAMPIRE);

    switch (you.hunger_state)
    {
    case HS_ENGORGED:
        inf->light_colour = LIGHTGREEN;
        inf->light_text   = (vamp ? "Alive" : "Engorged");
        break;
    case HS_VERY_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = "Very Full";
        break;
    case HS_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = "Full";
        break;
    case HS_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Thirsty" : "Hungry");
        break;
    case HS_VERY_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Very Thirsty" : "Very Hungry");
        break;
    case HS_NEAR_STARVING:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Near Bloodless" : "Near Starving");
        break;
    case HS_STARVING:
        inf->light_colour = RED;
        inf->light_text   = (vamp ? "Bloodless" : "Starving");
        break;
    case HS_SATIATED: // no status light
    default:
        break;
    }
}

static void _describe_glow(status_info* inf)
{
    const int cont = get_contamination_level();
    if (cont > 0 || you.backlit(false))
    {
        inf->light_colour = DARKGREY;
        if (you.backlit(false))
            inf->light_colour = LIGHTBLUE;
        if (cont > 1)
            inf->light_colour = _bad_ench_colour(cont, 2, 3);
        inf->light_text = "Glow";
    }

    if (cont > 0)
    {
        inf->short_text =
                 (cont == 1) ? "very slightly " :
                 (cont == 2) ? "slightly " :
                 (cont == 3) ? "" :
                 (cont == 4) ? "moderately " :
                 (cont == 5) ? "heavily "
                             : "really heavily ";
        inf->short_text += "glowing";
        inf->long_text = describe_contamination(cont);
    }
}

static void _describe_regen(status_info* inf)
{
    const bool regen = (you.duration[DUR_REGENERATION] > 0);
    const bool no_heal =
            (you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING)
            || (player_mutation_level(MUT_SLOW_HEALING) == 3);
    // Does vampire hunger level affect regeneration rate significantly?
    const bool vampmod = !no_heal && !regen && you.species == SP_VAMPIRE
                         && you.hunger_state != HS_SATIATED;

    if (regen)
    {
        inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_REGENERATION));
        inf->light_text   = "Regen";
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            inf->light_text += " MR";
        else if (no_heal)
            inf->light_colour = DARKGREY;
    }

    if ((you.disease && !regen) || no_heal)
    {
       inf->short_text = "non-regenerating";
    }
    else if (regen)
    {
        if (you.disease)
        {
            inf->short_text = "recuperating";
            inf->long_text  = "You are recuperating from your illness.";
        }
        else
        {
            inf->short_text = "regenerating";
            inf->long_text  = "You are regenerating.";
        }
        _mark_expiring(inf, dur_expiring(DUR_REGENERATION));
    }
    else if (vampmod)
    {
        if (you.disease)
            inf->short_text = "recuperating";
        else
            inf->short_text = "regenerating";

        if (you.hunger_state < HS_SATIATED)
            inf->short_text += " slowly";
        else if (you.hunger_state < HS_ENGORGED)
            inf->short_text += " quickly";
        else
            inf->short_text += " very quickly";
    }
}

static void _describe_poison(status_info* inf)
{
    int pois = you.duration[DUR_POISONING];
    inf->light_colour = _bad_ench_colour(pois, 5, 10);
    inf->light_text   = "Pois";
    const std::string adj =
         (pois > 10) ? "extremely" :
         (pois > 5)  ? "very" :
         (pois > 3)  ? "quite"
                     : "mildly";
    inf->short_text   = adj + " poisoned";
    inf->long_text    = "You are " + inf->short_text + ".";
}

static void _describe_speed(status_info* inf)
{
    if (you.duration[DUR_SLOW] && you.duration[DUR_HASTE])
    {
        inf->long_text = "You are under both slowing and hasting effects.";
    }
    else if (you.duration[DUR_SLOW])
    {
        inf->light_colour = RED;
        inf->light_text   = "Slow";
        inf->short_text   = "slowed";
        inf->long_text    = "You are slowed.";
    }
    else if (you.duration[DUR_HASTE])
    {
        inf->light_colour = dur_colour(BLUE, dur_expiring(DUR_HASTE));
        inf->light_text   = "Fast";
        inf->short_text = "hasted";
        inf->long_text = "Your actions are hasted.";
        _mark_expiring(inf, dur_expiring(DUR_HASTE));
    }
}

static void _describe_airborne(status_info* inf)
{
    if (!you.airborne())
        return;

    const bool perm     = you.permanent_flight();
    const bool expiring = (!perm && dur_expiring(DUR_LEVITATION));
    const bool uncancel = you.attribute[ATTR_LEV_UNCANCELLABLE];

    if (wearing_amulet(AMU_CONTROLLED_FLIGHT))
    {
        inf->light_colour = you.light_flight() ? BLUE : MAGENTA;
        inf->light_text   = "Fly";
        inf->short_text   = "flying";
        inf->long_text    = "You are flying.";
    }
    else
    {
        inf->light_colour = uncancel ? GREEN : BLUE;
        inf->light_text   = "Lev";
        inf->short_text   = "levitating";
        inf->long_text    = "You are hovering above the floor.";
    }
    inf->light_colour = dur_colour(inf->light_colour, expiring);
    _mark_expiring(inf, expiring);
}

static void _describe_rotting(status_info* inf)
{
    if (you.rotting)
    {
        inf->light_colour = _bad_ench_colour(you.rotting, 4, 8);
        inf->light_text   = "Rot";
    }

    if (you.rotting || you.species == SP_GHOUL)
    {
        inf->short_text = "rotting";
        inf->long_text = "Your flesh is rotting";
        if (you.rotting > 15)
            inf->long_text += " before your eyes";
        else if (you.rotting > 8)
            inf->long_text += " away quickly";
        else if (you.rotting > 4)
            inf->long_text += " badly";
        else if (you.species == SP_GHOUL)
            inf->long_text += " faster than usual";
        inf->long_text += ".";
    }
}

static void _describe_sickness(status_info* inf)
{
    if (you.disease)
    {
        const int high = 120 * BASELINE_DELAY;
        const int low  =  40 * BASELINE_DELAY;

        inf->light_colour   = _bad_ench_colour(you.disease, low, high);
        inf->light_text     = "Sick";

        std::string mod = (you.disease > high) ? "badly "  :
                          (you.disease >  low) ? ""        :
                                                 "mildly ";

        inf->short_text = mod + "diseased";
        inf->long_text  = "You are " + mod + "diseased.";
    }
}

static void _describe_burden(status_info* inf)
{
    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        inf->light_colour = RED;
        inf->light_text   = "Overloaded";
        inf->short_text   = "overloaded";
        inf->long_text    = "You are overloaded with stuff.";
        break;
    case BS_ENCUMBERED:
        inf->light_colour = LIGHTRED;
        inf->light_text   = "Burdened";
        inf->short_text   = "burdened";
        inf->long_text    = "You are burdened.";
        break;
    case BS_UNENCUMBERED:
        if (you.species == SP_KENKU && you.flight_mode() == FL_FLY)
        {
            if (you.travelling_light())
                inf->long_text = "Your small burden allows quick flight.";
            else
                inf->long_text = "Your heavy burden is slowing your flight.";
        }
        break;
    }
}

static void _describe_transform(status_info* inf)
{
    const bool vampbat = (you.species == SP_VAMPIRE && player_in_bat_form());
    const bool expire  = dur_expiring(DUR_TRANSFORMATION) && !vampbat;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_BAT:
        inf->light_text     = "Bat";
        inf->short_text     = "bat-form";
        inf->long_text      = "You are in ";
        if (vampbat)
            inf->long_text += "vampire ";
        inf->long_text     += "bat-form.";
        break;
    case TRAN_BLADE_HANDS:
        inf->light_text = "Blades";
        inf->short_text = "blade hands";
        inf->long_text  = "You have blades for hands.";
        break;
    case TRAN_DRAGON:
        inf->light_text = "Dragon";
        inf->short_text = "dragon-form";
        inf->long_text  = "You are in dragon-form.";
        break;
    case TRAN_ICE_BEAST:
        inf->light_text = "Ice";
        inf->short_text = "ice-form";
        inf->long_text  = "You are an ice creature.";
        break;
    case TRAN_LICH:
        inf->light_text = "Lich";
        inf->short_text = "lich-form";
        inf->long_text  = "You are in lich-form.";
        break;
    case TRAN_PIG:
        inf->light_text = "Pig";
        inf->short_text = "pig-form";
        inf->long_text  = "You are a filthy swine.";
        break;
    case TRAN_SPIDER:
        inf->light_text = "Spider";
        inf->short_text = "spider-form";
        inf->long_text  = "You are in spider-form.";
        break;
    case TRAN_STATUE:
        inf->light_text = "Statue";
        inf->short_text = "statue-form";
        inf->long_text  = "You are a statue.";
        break;
    }

    inf->light_colour = dur_colour(GREEN, expire);
    _mark_expiring(inf, expire);
}

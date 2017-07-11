/**
 * Status defaults for durations.
 */

#include "god-passive.h"

static void _end_weapon_brand()
{
    you.duration[DUR_EXCRUCIATING_WOUNDS] = 1;
    ASSERT(you.weapon());
    end_weapon_brand(*you.weapon(), true);
}

static void _end_invis()
{
    if (you.invisible())
        mprf(MSGCH_DURATION, "You feel more conspicuous.");
    else
        mprf(MSGCH_DURATION, "You flicker back into view.");
    you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
}

static void _end_corrosion()
{
    you.props["corrosion_amount"] = 0;
    you.redraw_armour_class = true;
    you.wield_change = true;
}

static void _redraw_armour()
{
    you.redraw_armour_class = true;
}

// properties of the duration.
enum duration_flags : uint32_t
{
    D_NO_FLAGS    = 0,

    // Whether to do automatic expiration colouring.
    D_EXPIRES     = 1<< 0,

    // Whether !cancellation (and the like) end the duration.
    D_DISPELLABLE = 1<< 1,
};

/// A description of the behaviour when a duration begins 'expiring'.
struct midpoint_msg
{
    /// What message should be printed when the duration begins expiring?
    const char* msg;
    int max_offset; ///< What's the maximum value that the duration may be
                    ///< reduced by after the message prints?

    /// Randomly, much should the duration actually be reduced by?
    int offset() const
    {
        return max_offset ? random2(max_offset+1) : 0;
    }
};

/// What happens when a duration ends?
struct end_effect
{
    /// A message for when the duration ends. Implies the duration ticks down.
    const char* msg;
    /// An effect that triggers when the duration ends.
    void (*on_end)();
};

/// Does the duration decrease simply over time? If so, what happens?
struct decrement_rules
{
    /// What happens when the duration ends?
    end_effect end;
    /// What happens when a duration hits 50% remaining.
    midpoint_msg mid_msg;
    /// Should the message be MSGCH_RECOVERY instead of MSGCH_DURATION?
    bool recovery;
};

struct duration_def
{
    duration_type dur;
    int    light_colour; ///< Base colour for status light.
    const char *light_text; ///< Text for the status light.
    const char *short_text; ///< Text for @ line on the % screen and morgues.
                            ///< Usually an adjective.
    const char *name_text;  ///< Text used in wizmode &^D. If empty, use the
                            ///< short_text.
    const char *long_text;  ///< Text for the @ message.

    uint32_t flags;       ///< A bitfield for various flags a duration can have.

    /// Does the duration decrease simply over time? If so, what happens?
    decrement_rules decr;
    /// The number of turns below which the duration is considered 'expiring'.
    int expire_threshold;

    /// Return the name of the duration (name_text or short_text). */
    const char *name() const
    {
        return name_text[0] ? name_text : short_text;
    }

    const bool duration_has_flag(uint64_t flag_wanted) const
    {
        return flags & flag_wanted;
    }

};

/**
 * "" for an entry means N/A, either because it is not shown in the
 * relevant place, or because it is shown in a more complicated way.
 * A "" for name_text indicates that the name is the same as the
 * short text.
 *
 * Examples:
 *
 * DUR_FORESTED has "" for the short_text entry, and therefore has no
 * entry on the "@:" line.
 *
 * DUR_QAZLAL_AC has "" for the light_text entry, and therefore has no
 * status light (instead showing the AC bonus by coloring the player's
 * AC blue).
 */

static const duration_def duration_data[] =
{
    { DUR_AGILITY,
      LIGHTBLUE, "Agi",
      "agile", "agility",
      "You are agile.", D_DISPELLABLE,
      {{ "You feel a little less agile now.", []() {
          notify_stat_change(STAT_DEX, -5, true);
      }}}},
    { DUR_BERSERK,
      BLUE, "Berserk",
      "berserking", "berserker",
      "You are possessed by a berserker rage.", D_EXPIRES,
      {{ "You are no longer berserk.", player_end_berserk }}, 6},
    { DUR_BERSERK_COOLDOWN,
      YELLOW, "-Berserk",
      "berserk cooldown", "",
      "", D_NO_FLAGS},
    { DUR_BREATH_WEAPON,
      YELLOW, "Breath",
      "short of breath", "breath weapon",
      "You are short of breath.", D_NO_FLAGS,
      { { "You have got your breath back." }, {}, true }},
    { DUR_BRILLIANCE,
      LIGHTBLUE, "Brill",
      "brilliant", "brilliance",
      "You are brilliant.", D_DISPELLABLE,
      {{ "You feel a little less clever now.", []() {
          notify_stat_change(STAT_INT, -5, true);
      }}}},
    { DUR_CONF,
      RED, "Conf",
      "confused", "conf",
      "You are confused.", D_DISPELLABLE,
      {{ "You feel less confused." }}},
    { DUR_CONFUSING_TOUCH,
      LIGHTBLUE, "Touch",
      "confusing touch", "",
      "" , D_DISPELLABLE,
      {{ "", []() {
          mprf(MSGCH_DURATION, "%s",
               you.hands_act("stop", "glowing.").c_str());
      }}}, 20},
    { DUR_CORONA,
      YELLOW, "Corona",
      "", "corona",
      "", D_DISPELLABLE,
      {{ "", []() {
          if (!you.backlit())
              mprf(MSGCH_DURATION, "You are no longer glowing.");
      }}}},
    { DUR_DEATH_CHANNEL,
      MAGENTA, "DChan",
      "death channel", "",
      "You are channeling the dead.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your unholy channel expires.", []() {
          you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
      }}, { "Your unholy channel is weakening.", 1 }}, 6},
    { DUR_DIVINE_STAMINA,
      WHITE, "Vit",
      "vitalised", "divine stamina",
      "You are divinely vitalised.", D_EXPIRES,
      {{ "", zin_remove_divine_stamina }}},
    { DUR_DIVINE_VIGOUR,
      0, "",
      "divinely vigorous", "divine vigour",
      "You are imbued with divine vigour.", D_NO_FLAGS,
      {{ "", elyvilon_remove_divine_vigour }}},
    { DUR_EXHAUSTED,
      YELLOW, "Exh",
      "exhausted", "",
      "You are exhausted.", D_NO_FLAGS,
      {{ "You feel less exhausted." }}},
    { DUR_FIRE_SHIELD,
      BLUE, "RoF",
      "immune to fire clouds", "fire shield",
      "", D_DISPELLABLE | D_EXPIRES,
      {{ "Your ring of flames gutters out." },
       { "Your ring of flames is guttering out.", 2}}, 5},
    { DUR_ICY_ARMOUR,
      0, "",
      "icy armour", "",
      "You are protected by a layer of icy armour.", D_DISPELLABLE | D_EXPIRES,
      {}, 6},
    { DUR_LIQUID_FLAMES,
      RED, "Fire",
      "liquid flames", "",
      "You are covered in liquid flames.", D_NO_FLAGS},
    { DUR_LOWERED_MR,
      RED, "MR/2",
      "vulnerable", "lowered mr",
      "", D_DISPELLABLE,
      {{ "You feel less vulnerable to hostile enchantments." }}},
    { DUR_MIGHT,
      LIGHTBLUE, "Might",
      "mighty", "might",
      "You are mighty.", D_DISPELLABLE,
      {{ "You feel a little less mighty now.", []() {
          notify_stat_change(STAT_STR, -5, true);
      }}}},
    { DUR_PARALYSIS,
      RED, "Para",
      "paralysed", "paralysis",
      "You are paralysed.", D_DISPELLABLE},
    { DUR_PETRIFIED,
      RED, "Stone",
      "petrified", "",
      "You are petrified.", D_DISPELLABLE},
    { DUR_PETRIFYING,
      LIGHTRED, "Petr",
      "petrifying", "",
      "You are turning to stone.", D_DISPELLABLE /*but special-cased*/ | D_EXPIRES,
        {}, 1},
    { DUR_RESISTANCE,
      BLUE, "Resist",
      "resistant", "resistance",
      "You resist elements.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your resistance to elements expires." },
          { "You start to feel less resistant.", 1}}, 6},
    { DUR_SLIMIFY,
      GREEN, "Slime",
      "slimy", "slimify",
      "", D_EXPIRES,
      {{ "You feel less slimy."},
        { "Your slime is starting to congeal.", 1 }}, 10},
    { DUR_SLEEP,
      0, "",
      "sleeping", "sleep",
      "You are sleeping.", D_DISPELLABLE},
    { DUR_SWIFTNESS,
      BLUE, "Swift",
      "swift", "swiftness",
      "You can move swiftly.", D_DISPELLABLE | D_EXPIRES, {}, 6},
    { DUR_TELEPORT,
      LIGHTBLUE, "Tele",
      "about to teleport", "teleport",
      "You are about to teleport.", D_DISPELLABLE /*but special-cased*/,
      {{ "", []() {
          you_teleport_now();
          untag_followers();
      }}}},
    { DUR_DEATHS_DOOR,
      LIGHTGREY, "DDoor",
      "death's door", "deaths door",
      "You are standing in death's doorway.", D_EXPIRES,
      {{ "Your life is in your own hands again!", []() {
            you.duration[DUR_DEATHS_DOOR_COOLDOWN] = random_range(10, 30);
      }}, { "Your time is quickly running out!", 5 }}, 10},
    { DUR_DEATHS_DOOR_COOLDOWN,
      YELLOW, "-DDoor",
      "death's door cooldown", "",
      "", D_NO_FLAGS,
      {{ "You step away from death's doorway." }}},
    { DUR_QUAD_DAMAGE,
      BLUE, "Quad",
      "quad damage", "",
      "", D_EXPIRES,
      {{ "", []() { invalidate_agrid(true); }},
        { "Quad Damage is wearing off."}}, 3 }, // per client.qc
    { DUR_SILENCE,
      0, "",
      "silence", "",
      "You radiate silence.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your hearing returns.", []() { invalidate_agrid(true); }}}, 5 },
    { DUR_STEALTH,
      BLUE, "Stealth",
      "especially stealthy", "stealth",
      "", D_NO_FLAGS,
      {{ "You feel less stealthy." }}},
    { DUR_AFRAID,
      RED, "Fear",
      "afraid", "",
      "You are terrified.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your fear fades away.", []() { you.clear_fearmongers(); }},
        {}, true }},
    { DUR_MIRROR_DAMAGE,
      WHITE, "Mirror",
      "injury mirror", "mirror damage",
      "You mirror injuries.", D_NO_FLAGS,
      {{ "Your dark mirror aura disappears." }}},
    { DUR_SCRYING,
      WHITE, "Scry",
      "scrying", "",
      "Your astral vision lets you see through walls.", D_NO_FLAGS,
      {{ "Your astral sight fades away.", []() {
          you.xray_vision = false;
      }}}},
    { DUR_TORNADO,
      LIGHTGREY, "Tornado",
      "tornado", "",
      "You are in the eye of a mighty hurricane.", D_EXPIRES},
    { DUR_LIQUEFYING,
      LIGHTBLUE, "Liquid",
      "liquefying", "",
      "The ground has become liquefied beneath your feet.", D_DISPELLABLE,
      {{ "The ground is no longer liquid beneath you.", []() {
          invalidate_agrid(false);
      }}}},
    { DUR_HEROISM,
      LIGHTBLUE, "Hero",
      "heroism", "",
      "You possess the skills of a mighty hero.", D_NO_FLAGS,
      {{ "You feel like a meek peon again.", []() {
          you.redraw_evasion      = true;
          you.redraw_armour_class = true;
      }}}},
    { DUR_FINESSE,
      LIGHTBLUE, "Finesse",
      "finesse", "",
      "Your blows are lightning fast.", D_NO_FLAGS,
      {{ "", []() {
          mprf(MSGCH_DURATION, "%s", you.hands_act("slow", "down.").c_str());
      }}}},
    { DUR_LIFESAVING,
      LIGHTGREY, "Prot",
      "protection", "lifesaving",
      "You are calling for your life to be saved.", D_EXPIRES,
      {{ "Your divine protection fades away." }}},
    { DUR_DARKNESS,
      BLUE, "Dark",
      "darkness", "",
      "You emit darkness.", D_DISPELLABLE | D_EXPIRES,
      {{ "The ambient light returns to normal.", update_vision_range },
         { "The darkness around you begins to abate.", 1 }}, 6},
    { DUR_SHROUD_OF_GOLUBRIA,
      BLUE, "Shroud",
      "shrouded", "",
      "You are protected by a distorting shroud.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your shroud unravels." },
        { "Your shroud begins to fray at the edges." }}, 6},
    { DUR_TORNADO_COOLDOWN,
      YELLOW, "-Tornado",
      "", "tornado cooldown",
      "", D_NO_FLAGS,
      {{ "The winds around you calm down.", []() {
          remove_tornado_clouds(MID_PLAYER);
      }}}},
    { DUR_DISJUNCTION,
      BLUE, "Disjoin",
      "disjoining", "disjunction",
      "You are disjoining your surroundings.", D_DISPELLABLE | D_EXPIRES,
      {{ "The translocation energy dissipates.", []() {
            invalidate_agrid(true);
      }}}},
    { DUR_SENTINEL_MARK,
      LIGHTRED, "Mark",
      "marked", "sentinel's mark",
      "A sentinel's mark is revealing your location to enemies.", D_DISPELLABLE | D_EXPIRES,
      {{ "The sentinel's mark upon you fades away." }}},
    { DUR_INFUSION,
      BLUE, "Infus",
      "infused", "",
      "Your attacks are magically infused.", D_DISPELLABLE | D_EXPIRES,
      {{ "You are no longer magically infusing your attacks." },
        { "Your magical infusion is running out." }}, 6},
    { DUR_SONG_OF_SLAYING,
      BLUE, "Slay",
      "singing", "song of slaying",
      "Your melee attacks are strengthened by your song.", D_DISPELLABLE | D_EXPIRES,
      {{ "Your song has ended." },
        { "Your song is almost over." }}, 6},
    { DUR_FLAYED,
      RED, "Flay",
      "flayed", "",
      "You are covered in terrible wounds.", D_DISPELLABLE /* but special-cased */ | D_EXPIRES},
    { DUR_WEAK,
      RED, "Weak",
      "weakened", "weak",
      "Your attacks are enfeebled.", D_DISPELLABLE,
      {{ "Your attacks no longer feel as feeble." }}},
    { DUR_DIMENSION_ANCHOR,
      RED, "-Tele",
      "cannot translocate", "dimension anchor",
      "You are firmly anchored to this plane.", D_DISPELLABLE,
      {{ "You are no longer firmly anchored in space." }}},
    { DUR_TOXIC_RADIANCE,
      MAGENTA, "Toxic",
      "radiating poison", "toxic radiance",
      "You are radiating toxic energy.", D_DISPELLABLE,
      {{ "Your toxic aura wanes." }}},
    { DUR_RECITE,
      WHITE, "Recite",
      "reciting", "",
      "You are reciting Zin's Axioms of Law.", D_NO_FLAGS},
    { DUR_RECITE_COOLDOWN,
      YELLOW, "-Recite",
      "", "recite cooldown",
      "", D_NO_FLAGS,
      {{ "You are ready to recite again." }}},
    { DUR_GRASPING_ROOTS,
      BROWN, "Roots",
      "grasped by roots", "grasping roots",
      "Your movement is impeded by grasping roots.", D_NO_FLAGS},
    { DUR_FIRE_VULN,
      RED, "rF-",
      "fire vulnerable", "fire vulnerability",
      "You are more vulnerable to fire.", D_DISPELLABLE,
      {{ "You feel less vulnerable to fire." }}},
    { DUR_BARBS,
      RED, "Barbs",
      "barbed spikes", "",
      "Barbed spikes are embedded in your body.", D_NO_FLAGS},
    { DUR_POISON_VULN,
      RED, "rP-",
      "poison vulnerable", "poison vulnerability",
      "You are more vulnerable to poison.", D_DISPELLABLE,
      {{ "You feel less vulnerable to poison." }}},
    { DUR_FROZEN,
      RED, "Frozen",
      "frozen", "",
      "You are partly encased in ice.", D_NO_FLAGS,
      {{ "The ice encasing you melts away." }, {}, true }},
    { DUR_SAP_MAGIC,
      RED, "Sap",
      "sap magic", "",
      "Casting spells hinders your spell success.", D_DISPELLABLE,
      {{ "Your magic seems less tainted.", []() {
          you.props.erase(SAP_MAGIC_KEY);
      }}}},
    { DUR_PORTAL_PROJECTILE,
      LIGHTBLUE, "PProj",
      "portal projectile", "",
      "You are teleporting projectiles to their destination.", D_DISPELLABLE,
      {{ "You are no longer teleporting projectiles to their destination.",
         []() { you.attribute[ATTR_PORTAL_PROJECTILE] = 0; }}}},
    { DUR_FORESTED,
      GREEN, "Forest",
      "", "forested",
      "", D_NO_FLAGS,
      {{ "Space becomes stable." }}},
    { DUR_DRAGON_CALL,
      WHITE, "Dragoncall",
      "dragon's call", "dragon call",
      "You are beckoning forth a horde of dragons.", D_NO_FLAGS,
      {{ "The roar of the dragon horde subsides.", []() {
          you.duration[DUR_DRAGON_CALL_COOLDOWN] = random_range(160, 260);
      }}}},
    { DUR_DRAGON_CALL_COOLDOWN,
      YELLOW, "-Dragoncall",
      "", "dragon call cooldown",
      "", D_NO_FLAGS,
      {{ "You can once more reach out to the dragon horde." }}},
    { DUR_ABJURATION_AURA,
      BLUE, "Abj",
      "aura of abjuration", "",
      "You are abjuring all hostile summons around you.", D_DISPELLABLE,
      {{ "Your aura of abjuration expires." }}},
    { DUR_NO_POTIONS,
      RED, "-Potion",
      "no potions", "",
      "You cannot drink potions.", D_NO_FLAGS,
      {{ "", []() {
          if (!you_foodless())
              mprf(MSGCH_RECOVERY, "You can drink potions again.");
      }}}},
    { DUR_QAZLAL_FIRE_RES,
      LIGHTBLUE, "rF+",
      "protected from fire", "qazlal fire resistance",
      "Qazlal is protecting you from fire.", D_NO_FLAGS,
      {{ "You feel less protected from fire." },
        { "Your protection from fire is fading.", 1}}, 6},
    { DUR_QAZLAL_COLD_RES,
      LIGHTBLUE, "rC+",
      "protected from cold", "qazlal cold resistance",
      "Qazlal is protecting you from cold.", D_NO_FLAGS,
      {{ "You feel less protected from cold." },
        { "Your protection from cold is fading.", 1}}, 6},
    { DUR_QAZLAL_ELEC_RES,
      LIGHTBLUE, "rElec+",
      "protected from electricity", "qazlal elec resistance",
      "Qazlal is protecting you from electricity.", D_NO_FLAGS,
      {{ "You feel less protected from electricity." },
        { "Your protection from electricity is fading.", 1}}, 6},
    { DUR_QAZLAL_AC,
      LIGHTBLUE, "",
      "protected from physical damage", "qazlal ac",
      "Qazlal is protecting you from physical damage.", D_NO_FLAGS,
      {{ "You feel less protected from physical attacks.",  _redraw_armour },
         { "Your protection from physical attacks is fading." , 1 }}, 6},
    { DUR_CORROSION,
      RED, "Corr",
      "corroded", "corrosion",
      "You are corroded.", D_DISPELLABLE,
      {{ "You are no longer corroded.", _end_corrosion }}},
    { DUR_HORROR,
      RED, "Horr",
      "horrified", "",
      "You are horrified, weakening your attacks and spells.", D_NO_FLAGS},
    { DUR_NO_SCROLLS,
      RED, "-Scroll",
      "no scrolls", "",
      "You cannot read scrolls.", D_NO_FLAGS,
      {{ "You can read scrolls again." }, {}, true }},
    { DUR_DIVINE_SHIELD,
      0, "",
      "divine shield", "",
      "You are shielded by the power of the Shining One.", D_NO_FLAGS,
      {{ "", tso_remove_divine_shield }}},
    { DUR_CLEAVE,
      LIGHTBLUE, "Cleave",
      "cleaving", "cleave",
      "You are cleaving through your foes.", D_NO_FLAGS,
      {{ "Your cleaving frenzy subsides." }}},
    { DUR_AMBROSIA, GREEN, "Ambros", "", "ambrosia",
      "You are regenerating under the effects of ambrosia.", D_DISPELLABLE },
    { DUR_CHANNEL_ENERGY, LIGHTBLUE, "Channel", "", "channel",
      "You are rapidly regenerating magical energy.", D_DISPELLABLE },
    { DUR_DEVICE_SURGE, WHITE, "Surge", "device surging", "device surge",
      "You have readied a device surge.", D_EXPIRES,
      {{ "Your device surge dissipates." },
        { "Your device surge is dissipating.", 1 }}, 10},
    { DUR_DOOM_HOWL,
      RED, "Howl",
      "doom-hounded", "howl",
      "A terrible howling echoes in your mind.", D_DISPELLABLE,
      {{ "The infernal howling subsides.", []() {
          you.props.erase(NEXT_DOOM_HOUND_KEY);
      }}}},
    { DUR_VERTIGO, YELLOW, "Vertigo", "", "vertigo",
      "Vertigo is making it harder to attack, cast, and dodge.", D_DISPELLABLE,
      {{ "The world stops spinning.", []() {
          you.redraw_evasion = true;
      }}}},
    { DUR_SANGUINE_ARMOUR, LIGHTBLUE, "Blood",
      "sanguine armour", "",
      "Your shed blood clings to and protects you.", D_NO_FLAGS,
        {{ "Your blood armour dries and flakes away.", _redraw_armour }}},
    { DUR_SPWPN_PROTECTION, 0, "", "protection aura", "",
      "Your weapon is exuding a protective aura.", D_NO_FLAGS,
      {{ "", _redraw_armour }}},
    { DUR_NO_HOP, YELLOW, "-Hop",
      "can't hop", "",
      "", D_NO_FLAGS,
      {{ "You are ready to hop once more." }}},

    // The following are visible in wizmode only, or are handled
    // specially in the status lights and/or the % or @ screens.

    { DUR_INVIS, 0, "", "", "invis", "", D_DISPELLABLE,
        {{ "", _end_invis }, { "You flicker for a moment.", 1}}, 6},
    { DUR_SLOW, 0, "", "", "slow", "", D_DISPELLABLE},
    { DUR_MESMERISED, 0, "", "", "mesmerised", "", D_DISPELLABLE,
      {{ "You break out of your daze.", []() { you.clear_beholders(); }},
         {}, true }},
    { DUR_MESMERISE_IMMUNE, 0, "", "", "mesmerisation immunity", "", D_NO_FLAGS, {{""}} },
    { DUR_HASTE, 0, "", "", "haste", "", D_DISPELLABLE, {}, 6},
    { DUR_FLIGHT, 0, "", "", "flight", "", D_DISPELLABLE /*but special-cased*/, {}, 10},
    { DUR_POISONING, 0, "", "", "poisoning", "", D_NO_FLAGS},
    { DUR_PIETY_POOL, 0, "", "", "piety pool", "", D_NO_FLAGS},
    { DUR_REGENERATION, 0, "", "", "regeneration", "", D_DISPELLABLE,
      {{ "Your skin stops crawling." },
          { "Your skin is crawling a little less now.", 1}}, 6},
    { DUR_TRANSFORMATION, 0, "", "", "transformation", "", D_DISPELLABLE /*but special-cased*/, {}, 10},
    { DUR_EXCRUCIATING_WOUNDS, 0, "", "", "excruciating wounds", "", D_DISPELLABLE,
      {{ "", _end_weapon_brand }}},
    { DUR_DEMONIC_GUARDIAN, 0, "", "", "demonic guardian", "", D_NO_FLAGS, {{""}}},
    { DUR_POWERED_BY_DEATH, 0, "", "", "pbd", "", D_NO_FLAGS},
    { DUR_GOURMAND, 0, "", "", "gourmand", "", D_NO_FLAGS},
        { DUR_REPEL_STAIRS_MOVE, 0, "", "", "repel stairs move", "", D_NO_FLAGS, {{""}}},
    { DUR_REPEL_STAIRS_CLIMB, 0, "", "", "repel stairs climb", "", D_NO_FLAGS, {{""}}},
    { DUR_CLOUD_TRAIL, 0, "", "", "cloud trail", "", D_NO_FLAGS},
    { DUR_TIME_STEP, 0, "", "", "time step", "", D_NO_FLAGS},
    { DUR_ICEMAIL_DEPLETED, 0, "", "", "icemail depleted", "", D_NO_FLAGS,
      {{ "Your icy envelope is restored.", _redraw_armour }}},
    { DUR_PARALYSIS_IMMUNITY, 0, "", "", "paralysis immunity", "", D_NO_FLAGS},
    { DUR_VEHUMET_GIFT, 0, "", "", "vehumet gift", "", D_NO_FLAGS, {{""}}},
    { DUR_SICKENING, 0, "", "", "sickening", "", D_DISPELLABLE, {{""}}},
    { DUR_WATER_HOLD, 0, "", "", "drowning", "", D_NO_FLAGS},
    { DUR_WATER_HOLD_IMMUNITY, 0, "", "", "drowning immunity", "", D_NO_FLAGS, {{""}}},
    { DUR_SLEEP_IMMUNITY, 0, "", "", "sleep immunity", "", D_NO_FLAGS, {{""}}},
    { DUR_ELIXIR_HEALTH, 0, "", "", "elixir health", "", D_NO_FLAGS},
    { DUR_ELIXIR_MAGIC, 0, "", "", "elixir magic", "", D_NO_FLAGS},
    { DUR_TROGS_HAND, 0, "", "", "trogs hand", "", D_NO_FLAGS,
        {{"", trog_remove_trogs_hand},
          {"You feel the effects of Trog's Hand fading.", 1}}, 6},
    { DUR_GOZAG_GOLD_AURA, 0, "", "gold aura", "", "", D_NO_FLAGS,
        {{ "", []() { you.props[GOZAG_GOLD_AURA_KEY] = 0; you.redraw_title = true;}}}},
    { DUR_COLLAPSE, 0, "", "", "collapse", "", D_NO_FLAGS },
    { DUR_BRAINLESS, 0, "", "", "brainless", "", D_NO_FLAGS },
    { DUR_CLUMSY, 0, "", "", "clumsy", "", D_NO_FLAGS },
    { DUR_ANCESTOR_DELAY, 0, "", "", "ancestor delay", "", D_NO_FLAGS, {{""}}},
    { DUR_NO_CAST, 0, "", "", "no cast", "", D_NO_FLAGS,
      {{ "You regain access to your magic." }, {}, true }},
    { DUR_HEAVENLY_STORM, 0, "", "", "", "", D_NO_FLAGS,
      {{ "",  wu_jian_heaven_tick }}},

#if TAG_MAJOR_VERSION == 34
    // And removed ones
    { DUR_MAGIC_SAPPED, 0, "", "", "old magic sapped", "", D_NO_FLAGS},
    { DUR_REPEL_MISSILES, 0, "", "", "old repel missiles", "", D_NO_FLAGS},
    { DUR_DEFLECT_MISSILES, 0, "", "", "old deflect missiles", "", D_NO_FLAGS},
    { DUR_JELLY_PRAYER, 0, "", "", "old jelly prayer", "", D_NO_FLAGS},
    { DUR_CONTROLLED_FLIGHT, 0, "", "", "old controlled flight", "", D_NO_FLAGS},
    { DUR_SEE_INVISIBLE, 0, "", "", "old see invisible", "", D_NO_FLAGS},
    { DUR_INSULATION, 0, "", "", "old insulation", "", D_NO_FLAGS},
    { DUR_BARGAIN, 0, "", "", "old bargain", "", D_NO_FLAGS},
    { DUR_SLAYING, 0, "", "", "old slaying", "", D_NO_FLAGS},
    { DUR_MISLED, 0, "", "", "old misled", "", D_NO_FLAGS},
    { DUR_NAUSEA, 0, "", "", "old nausea", "", D_NO_FLAGS},
    { DUR_TEMP_MUTATIONS, 0, "", "", "old temporary mutations", "", D_NO_FLAGS},
    { DUR_BATTLESPHERE, 0, "", "", "old battlesphere", "", D_NO_FLAGS},
    { DUR_RETCHING, 0, "", "", "old retching", "", D_NO_FLAGS},
    { DUR_SPIRIT_HOWL, 0, "", "", "old spirit howl", "", D_NO_FLAGS},
    { DUR_SONG_OF_SHIELDING, 0, "", "", "old song of shielding", "", D_NO_FLAGS},
    { DUR_ANTENNAE_EXTEND, 0, "", "", "old antennae extend", "", D_NO_FLAGS},
    { DUR_BUILDING_RAGE, 0, "", "", "old building rage", "", D_NO_FLAGS},
    { DUR_NEGATIVE_VULN, 0, "", "", "old negative vuln", "", D_NO_FLAGS},
    { DUR_SURE_BLADE, 0, "", "", "old sure blade", "", D_NO_FLAGS},
    { DUR_CONTROL_TELEPORT, 0, "", "", "old control teleport", "", D_NO_FLAGS},
    { DUR_DOOM_HOWL_IMMUNITY, 0, "", "", "old howl immunity", "", D_NO_FLAGS, {{""}}},
    { DUR_CONDENSATION_SHIELD, 0, "", "", "old condensation shield", "", D_NO_FLAGS},
    { DUR_PHASE_SHIFT, 0, "", "", "old phase shift", "", D_NO_FLAGS},
    { DUR_ANTIMAGIC,
        RED, "-Mag",
        "antimagic", "",
        "You have trouble accessing your magic.", D_DISPELLABLE | D_EXPIRES,
        {{ "You regain control over your magic." }}, 27},
    { DUR_TELEPATHY, 0, "", "", "old telepathy", "", D_NO_FLAGS},
    { DUR_MAGIC_ARMOUR, 0, "", "", "old magic armour", "", D_NO_FLAGS},
    { DUR_MAGIC_SHIELD, 0, "", "", "old magic shield", "", D_NO_FLAGS},
    { DUR_FORTITUDE, 0, "", "", "old fortitude", "", D_NO_FLAGS},
#endif
};

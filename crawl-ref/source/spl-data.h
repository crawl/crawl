/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

/*
   XXX: Some of these notes are obsolete now!

   In case anyone ever wants to add new spells, or just understand my reasons
   for putting a particular spell into a particular type, read on:

   Guidelines for typing spells

   Conjuration
   This type has a near monopoly on effective and relatively risk-free combat
   spells. All other types of combat spells are either indirect (enchantments),
   risky/detrimental/not versatile (necromancy) or just plain crappy (burn and
   freeze), although smiting is not too bad.
   Conjuration spells all involve the magical creation of matter and/or energy
   (which are the same thing anyway, right?). They are distinguished from
   summoning spells in that they do not involve the summoning of an entire
   creature from another place.

   Enchantment
   These spells mostly cause some kind of durational effect, which lasts only
   until the magic wears off. Enchantments are distinguished from
   transmutations in that the latter cause a permanent alteration in something
   which persists even after the magic has faded, while the effects of the
   former last only so long as the magic does. Sometimes enchantments may take
   advantage of the more powerful aspects of transmutation to induce some
   kind of radical change (eg polymorph).
   Some enchantments would also fall under the description of 'meta-magic'
   spells, like Selective Amnesia and Remove Curse (and if I ever implement
   Dispel Magic, it will be an enchantment).
   It is possible that some divinations could be retyped as
   divination/enchantment, as they appear to be primarily concerned with
   detecting enchantments. Detect Curse and Identify are what I'm thinking
   of here.

   Fire and Ice
   These are quite obvious. I'm trying to keep these two balanced with each
   other, but it can be difficult. I have to weigh up some useful fire spells,
   like Sticky Flame, Fireball, Ring of Flames and Firestorm, and the fact that
   Fire wizards have an advantage when summoning fire elementals by either
   spell or device, with the also quite useful Refrigeration, Ice Armour and
   Freezing Cloud. Ice wizards don't have a corresponding advantage with
   water elementals, because water and ice are two different things (ice is not
   necessarily water ice, for example).
   Generally, Fire spells tend towards chaos, disorder and entropy, while
   Ice spells tend towards order and stasis. But these trends are rather
   underdeveloped at the moment.
   Note that just about the only reason one would ever choose an ice or fire
   wizard over a conjurer would be the resistance gained at level 12.
   Especially because having a fire specialisation basically removes any chance
   of ever using ice spells effectively, and vice versa.

   Transmutation
   See enchantments.

   Necromancy
   This is the fun stuff. Necromancy is a mixed bag of many and various
   different kinds of spells, with a few common themes:
   -Differentiation of living, dead and undead. Some necromancy affects only
   the living (pain, vampiric draining etc), some affects only the dead
   (animate dead, twisted resurrection etc), and some affects only undead
   (dispel and control undead).
   -Actual or potential harm: eg risk in Death's Door, hp loss with Pain,
   disease with summon greater undead, etc. Also loss of potential experience
   gain with bolt of draining and degeneration.
   -Material components are central to many of the spells.
   -Some spells duplicate effects of other types, but do so in a different
   (neither superior or inferior) way. Eg bone shards is a very powerful spell
   for only 3 magic points, but requires preparation. Also, necromantic
   healing spells are different and more idiosyncratic than holy healing.
   Although regeneration is usually less useful than lesser healing and is
   level 3 instead of 2, it can be cast before combat (when 1 turn spent
   casting is less important), and is affected by extension.
   -Generally unholy theme of spells (I mean, Twisted Resurrection?).

   Holy
   The Holy type is also fairly various, but is rather less interesting than
   necromancy (after all, priests are better at fighting than necromancers).
   Holy spells do things like driving off undead and healing. Note that I
   consider item stickycursing to be more of an issue for enchantments rather
   than holy magic, which is why remove curse is enchantment.

   Summoning
   These spells involve bringing a creature from somewhere else (possibly on
   another plane of existence) to this world to do battle for the caster. Some
   future summonings could potentially be combination conjuration/summoning
   spells, eg the ball lightning spell I keep planning to implement.
   Also, potential exists for some risky high-level spells, maybe demon
   summoning?

   Divination
   These spells provide information to the caster. A diviner class would be
   possible (and having detect curse and identify would be very handy), but
   would be extremely difficult to play - there is no potential in this type
   for combat spells.

   Translocation
   Translocation spells deal with teleportation etc, also interplanar travel
   (eg Banishment, and the planned Gate spell).
   It is possible that I may give summoners some special access to trans-
   locations due to the obvious similarities.

   Poison
   These spells all involve poison. Most are also conjurations.
   I don't plan to implement a 'Poisoner' class, as it would become unplayable
   deep in the dungeon where most monsters are poison resistant.

   Your skill for a spell is effectively the average of all schools used in it.
*/

/*
 * When adding enchantments, must add them to extension as well!
 *
 * spells to do:
 *   Contingency?
 *   Trigger contingency
 *   Preserve Corpses
 *   Permanency
 *   Explosive rune?
 *   Fennel wands
 *   More summonings!
 */

#ifndef SPLDATA_H
#define SPLDATA_H


{
    SPELL_IDENTIFY, "Identify",
     SPTYP_DIVINATION,
     SPFLAG_NONE,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_TELEPORT_SELF, "Teleport Self",
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CAUSE_FEAR, "Cause Fear",
     SPTYP_ENCHANTMENT,
     SPFLAG_AREA,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_REMOVE_CURSE, "Remove Curse",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MAGIC_DART, "Magic Dart",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FIREBALL, "Fireball",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_APPORTATION, "Apportation",
     SPTYP_TRANSLOCATION,
     SPFLAG_GRID | SPFLAG_NOT_SELF,
     1,
     1000,
     LOS_RADIUS, LOS_RADIUS,
     0,
     "Pull items from where?",
     false,
     false
},

{
    SPELL_DELAYED_FIREBALL, "Delayed Fireball",
     SPTYP_FIRE | SPTYP_CONJURATION,
     SPFLAG_NONE,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_STRIKING, "Striking",
     0,
     SPFLAG_DIR_OR_TARGET | SPFLAG_BATTLE, // rod of striking
     1,
     25,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONJURE_FLAME, "Conjure Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_GRID | SPFLAG_NOT_SELF,
     3,
     100,
     4, 4,
     0,
     "Where would you like to place the cloud?",
     false,
     false
},

{
    SPELL_DIG, "Dig",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_NEUTRAL,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BOLT_OF_FIRE, "Bolt of Fire",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_COLD, "Bolt of Cold",
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_LIGHTNING_BOLT, "Lightning Bolt",
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     5, 12,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_MAGMA, "Bolt of Magma",
     SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_POLYMORPH_OTHER, "Polymorph Other",
     SPTYP_TRANSMUTATION,  // removed enchantment, wasn't needed -- bwr
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SLOW, "Slow",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_HASTE, "Haste",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL,
     6,  // lowered to 6 from 8, since it's easily available from various items
         // and Swiftness is level 2 (and gives a similar effect).  It's also
         // not that much better than Invisibility.  -- bwr
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PARALYSE, "Paralyse",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_PETRIFY, "Petrify",
     SPTYP_ENCHANTMENT | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONFUSE, "Confuse",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET,
     3,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INVISIBILITY, "Invisibility",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET | SPFLAG_HELPFUL,
     6,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_THROW_FLAME, "Throw Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     2,
     50,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_THROW_FROST, "Throw Frost",
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     2,
     50,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CONTROLLED_BLINK, "Controlled Blink",
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZING_CLOUD, "Freezing Cloud",
     SPTYP_CONJURATION | SPTYP_ICE | SPTYP_AIR,
     SPFLAG_GRID | SPFLAG_AREA,
     7,
     200,
     6, 6,
     0,
     "Where do you want to put it?",
     true,
     false
},

{
    SPELL_MEPHITIC_CLOUD, "Mephitic Cloud",
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA,
     3,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_RING_OF_FLAMES, "Ring of Flames",
     SPTYP_ENCHANTMENT | SPTYP_FIRE,
     SPFLAG_AREA,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_VENOM_BOLT, "Venom Bolt",
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     5,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, "Olgreb's Toxic Radiance",
     SPTYP_POISON,
     SPFLAG_AREA | SPFLAG_BATTLE,
     4,
     0,
     -1, -1,
     0,
     NULL,
     true,
     false
},

{
    SPELL_TELEPORT_OTHER, "Teleport Other",
     SPTYP_TRANSLOCATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_ESCAPE,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MINOR_HEALING, "Minor Healing",
     SPTYP_HOLY,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL | SPFLAG_MONSTER,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MAJOR_HEALING, "Major Healing",
     SPTYP_HOLY,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL | SPFLAG_MONSTER,
     6,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DEATHS_DOOR, "Death's Door",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SELECTIVE_AMNESIA, "Selective Amnesia",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_MASS_CONFUSION, "Mass Confusion",
     SPTYP_ENCHANTMENT,
     SPFLAG_AREA,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SMITING, "Smiting",
     SPTYP_NONE,
     SPFLAG_TARGET | SPFLAG_NOT_SELF, // divine ability, rod, monsters
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     4, // SPTYP_NONE spells have no default noise level
     "Smite whom?",
     false,
     false
},

{
    SPELL_DETECT_CURSE, "Detect Curse",
     SPTYP_DIVINATION,
     SPFLAG_NONE,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SUMMON_SMALL_MAMMALS, "Summon Small Mammals",
     SPTYP_SUMMONING ,
     SPFLAG_BATTLE,
     1,
     80,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ABJURATION, "Abjuration",
     SPTYP_SUMMONING,
     SPFLAG_AREA | SPFLAG_NEUTRAL | SPFLAG_ESCAPE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_SCORPIONS, "Summon Scorpions",
     SPTYP_SUMMONING | SPTYP_POISON,
     SPFLAG_BATTLE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_LEVITATION, "Levitation",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_NONE,
     2,
     150,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BOLT_OF_DRAINING, "Bolt of Draining",
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, "Lehudib's Crystal Spear",
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     8,
     200,
     4, 4,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BOLT_OF_INACCURACY, "Bolt of Inaccuracy",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET, // rod/tome of destruction
     3,
     1000,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_POISONOUS_CLOUD, "Poisonous Cloud",
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     SPFLAG_GRID | SPFLAG_AREA,
     6,
     200,
     6, 6,
     0,
     "Where do you want to put it?",
     true,
     false
},

{
    SPELL_FIRE_STORM, "Fire Storm",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_GRID | SPFLAG_AREA,
     9,
     200,
     6, 6,
     0,
     "Where?",
     true,
     false
},

{
    SPELL_DETECT_TRAPS, "Detect Traps",
     SPTYP_DIVINATION,
     SPFLAG_MAPPING,
     2,
     50,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BLINK, "Blink",
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

// The following name was found in the hack.exe file of an early version
// of PCHACK - credit goes to its creator (whoever that may be):
{
    SPELL_ISKENDERUNS_MYSTIC_BLAST, "Iskenderun's Mystic Blast",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_SWARM, "Summon Swarm",
     SPTYP_SUMMONING,
     SPFLAG_BATTLE, // rod of summoning, and Xom may cast this.
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_HORRIBLE_THINGS, "Summon Horrible Things",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ENSLAVEMENT, "Enslavement",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MAGIC_MAPPING, "Magic Mapping",
     SPTYP_DIVINATION | SPTYP_EARTH,
     SPFLAG_MAPPING,
     4,
     45,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
   SPELL_ANIMATE_DEAD, "Animate Dead",
     SPTYP_NECROMANCY,
     SPFLAG_AREA | SPFLAG_NEUTRAL,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PAIN, "Pain",
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_BATTLE,
     1,
     25,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_EXTENSION, "Extension",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CONTROL_UNDEAD, "Control Undead",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     true
},

{
    SPELL_ANIMATE_SKELETON, "Animate Skeleton",
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_VAMPIRIC_DRAINING, "Vampiric Draining",
     SPTYP_NECROMANCY,
     SPFLAG_DIR | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     3,
     200,
     1, 1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_WRAITHS, "Summon Wraiths",
     SPTYP_NECROMANCY | SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DETECT_ITEMS, "Detect Items",
     SPTYP_DIVINATION,
     SPFLAG_MAPPING,
     2,
     50,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BORGNJORS_REVIVIFICATION, "Borgnjor's Revivification",
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZE, "Freeze",
     SPTYP_ICE,
     SPFLAG_DIR | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     1,
     25,
     1, 1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_ELEMENTAL, "Summon Elemental",
     SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_OZOCUBUS_REFRIGERATION, "Ozocubu's Refrigeration",
     SPTYP_ICE,
     SPFLAG_AREA,
     5,
     200,
     -1, -1,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STICKY_FLAME, "Sticky Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_ICE_BEAST, "Summon Ice Beast",
     SPTYP_ICE | SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_OZOCUBUS_ARMOUR, "Ozocubu's Armour",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CALL_IMP, "Call Imp",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_REPEL_MISSILES, "Repel Missiles",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_NONE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BERSERKER_RAGE, "Berserker Rage",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISPEL_UNDEAD, "Dispel Undead",
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FULSOME_DISTILLATION, "Fulsome Distillation",
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     1,
     50,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_POISON_ARROW, "Poison Arrow",
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_TWISTED_RESURRECTION, "Twisted Resurrection",
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_REGENERATION, "Regeneration",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BONE_SHARDS, "Bone Shards",
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET,
     3,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BANISHMENT, "Banishment",
     SPTYP_TRANSLOCATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNHOLY,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_CIGOTUVIS_DEGENERATION, "Cigotuvi's Degeneration",
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STING, "Sting",
     SPTYP_CONJURATION | SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUBLIMATION_OF_BLOOD, "Sublimation of Blood",
     SPTYP_NECROMANCY,
     SPFLAG_NONE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_TUKIMAS_DANCE, "Tukima's Dance",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_HELLFIRE, "Hellfire",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNHOLY | SPFLAG_MONSTER,
        // plus DS ability, staff of Dispater & Sceptre of Asmodeus
     9,
     200,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_DEMON, "Summon Demon",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DEMONIC_HORDE, "Demonic Horde",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_GREATER_DEMON, "Summon Greater Demon",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_BATTLE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CORPSE_ROT, "Corpse Rot",
     SPTYP_NECROMANCY,
     SPFLAG_AREA | SPFLAG_NEUTRAL,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_TUKIMAS_VORPAL_BLADE, "Tukima's Vorpal Blade",
     SPTYP_ENCHANTMENT,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FIRE_BRAND, "Fire Brand",
     SPTYP_ENCHANTMENT | SPTYP_FIRE,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FREEZING_AURA, "Freezing Aura",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_LETHAL_INFUSION, "Lethal Infusion",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_BOLT_OF_IRON, "Bolt of Iron",
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     6,
     200,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STONE_ARROW, "Stone Arrow",
     SPTYP_CONJURATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET,
     3,
     50,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STONEMAIL, "Stonemail",
     SPTYP_ENCHANTMENT | SPTYP_EARTH,
     SPFLAG_NONE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SHOCK, "Shock",
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_DIR_OR_TARGET,
     1,
     25,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SWIFTNESS, "Swiftness",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_NONE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FLY, "Flight",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_NONE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_INSULATION, "Insulation",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_NONE,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DETECT_CREATURES, "Detect Creatures",
     SPTYP_DIVINATION,
     SPFLAG_MAPPING,
     2,
     60,                        // not 50, note the fuzz
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CURE_POISON, "Cure Poison",
     SPTYP_POISON,
     SPFLAG_RECOVERY | SPFLAG_HELPFUL,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CONTROL_TELEPORT, "Control Teleport",
     SPTYP_ENCHANTMENT | SPTYP_TRANSLOCATION,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_POISON_AMMUNITION, "Poison Ammunition",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_POISON_WEAPON, "Poison Weapon",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_RESIST_POISON, "Resist Poison",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PROJECTED_NOISE, "Projected Noise",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ALTER_SELF, "Alter Self",
     SPTYP_TRANSMUTATION,
     SPFLAG_NONE,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DEBUGGING_RAY, "Debugging Ray",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_TESTING,
     7,
     100,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_RECALL, "Recall",
     SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
     SPFLAG_NONE,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_PORTAL, "Portal",
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_AGONY, "Agony",
     SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SPIDER_FORM, "Spider Form",
     SPTYP_TRANSMUTATION | SPTYP_POISON,
     SPFLAG_HELPFUL,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DISRUPT, "Disrupt",
     SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_BATTLE | SPFLAG_DEVEL,
     1,
     25,
     7, 7,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISINTEGRATE, "Disintegrate",
     SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_CARD, // also: wand
     6,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_BLADE_HANDS, "Blade Hands",
     SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     5,  // only removes weapon, so I raised this from 4 -- bwr
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_STATUE_FORM, "Statue Form",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_HELPFUL,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_ICE_FORM, "Ice Form",
     SPTYP_ICE | SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL,
     4, // doesn't allow for equipment, so I lowered this from 5 -- bwr
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DRAGON_FORM, "Dragon Form",
     SPTYP_FIRE | SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_NECROMUTATION, "Necromutation",
     SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_DEATH_CHANNEL, "Death Channel",
     SPTYP_NECROMANCY,
     SPFLAG_HELPFUL,
     9,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SYMBOL_OF_TORMENT, "Symbol of Torment",
     SPTYP_NECROMANCY,
     SPFLAG_AREA,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DEFLECT_MISSILES, "Deflect Missiles",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_HELPFUL,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_ICE_BOLT, "Ice Bolt",
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET,
     4,
     100,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_ICE_STORM, "Ice Storm",
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA,
     9,
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_AIRSTRIKE, "Airstrike",
     SPTYP_AIR,
     SPFLAG_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     4,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SHADOW_CREATURES, "Shadow Creatures",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CONFUSING_TOUCH, "Confusing Touch",
     SPTYP_ENCHANTMENT,
     SPFLAG_NONE,
     1,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SURE_BLADE, "Sure Blade",
     SPTYP_ENCHANTMENT,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FLAME_TONGUE, "Flame Tongue",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     1,
     40,                           // cap for range; damage cap is at 25
     1, 4,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PASSWALL, "Passwall",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_ESCAPE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_IGNITE_POISON, "Ignite Poison",
     SPTYP_FIRE | SPTYP_TRANSMUTATION,
     SPFLAG_AREA | SPFLAG_BATTLE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STICKS_TO_SNAKES, "Sticks to Snakes",
     SPTYP_TRANSMUTATION | SPTYP_SUMMONING,
     SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CALL_CANINE_FAMILIAR, "Call Canine Familiar",
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     3,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_DRAGON, "Summon Dragon",
     SPTYP_FIRE | SPTYP_SUMMONING,
     SPFLAG_NONE, // Xom may cast this spell.
     9,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_TAME_BEASTS, "Tame Beasts",
     SPTYP_ENCHANTMENT,
     SPFLAG_AREA,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SLEEP, "Ensorcelled Hibernation",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     2,
     56,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MASS_SLEEP, "Metabolic Englaciation",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     SPFLAG_AREA,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DETECT_SECRET_DOORS, "Detect Secret Doors",
     SPTYP_DIVINATION,
     SPFLAG_NONE,
     1,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SEE_INVISIBLE, "See Invisible",
     SPTYP_ENCHANTMENT | SPTYP_DIVINATION,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_FORESCRY, "Forescry",
     SPTYP_DIVINATION,
     SPFLAG_HELPFUL,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SUMMON_BUTTERFLIES, "Summon Butterflies",
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     1,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_WARP_BRAND, "Warp Weapon",
     SPTYP_ENCHANTMENT | SPTYP_TRANSLOCATION,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     7,     // this is high for a reason - Warp brands are very powerful.
     0,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SILENCE, "Silence",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     SPFLAG_AREA,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SHATTER, "Shatter",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_AREA,
     9,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISPERSAL, "Dispersal",
     SPTYP_TRANSLOCATION,
     SPFLAG_AREA | SPFLAG_ESCAPE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_DISCHARGE, "Static Discharge",
     SPTYP_CONJURATION | SPTYP_AIR,
     SPFLAG_AREA,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BACKLIGHT, "Corona",
     SPTYP_ENCHANTMENT,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     1,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_INTOXICATE, "Alistair's Intoxication",
     SPTYP_TRANSMUTATION | SPTYP_POISON,
     SPFLAG_NONE,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_EVAPORATE, "Evaporate",
     SPTYP_FIRE | SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_AREA,
     2,   // XXX: level 2 or 3, what should it be now? -- bwr
     200,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FRAGMENTATION, "Lee's Rapid Deconstruction",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_GRID,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     "Fragment what (e.g. a wall or monster)?",
     false,
     false
},

{
    SPELL_SANDBLAST, "Sandblast",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF | SPFLAG_BATTLE,
     1,
     50,
     1, 1,                      // Special-cased!
     0,
     NULL,
     true,
     false
},

{
    SPELL_MAXWELLS_SILVER_HAMMER, "Maxwell's Silver Hammer",
     SPTYP_TRANSMUTATION | SPTYP_EARTH,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_CONDENSATION_SHIELD, "Condensation Shield",
     SPTYP_ICE | SPTYP_TRANSMUTATION,
     SPFLAG_HELPFUL,
     4,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_STONESKIN, "Stoneskin",
     SPTYP_EARTH | SPTYP_TRANSMUTATION, // was ench -- bwr
     SPFLAG_HELPFUL,
     2,
     200,
     -1, -1,
     0,
     NULL,
     false,
     true
},

{
    SPELL_SIMULACRUM, "Simulacrum",
     SPTYP_ICE | SPTYP_NECROMANCY,
     SPFLAG_NONE,
     6,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CONJURE_BALL_LIGHTNING, "Conjure Ball Lightning",
     SPTYP_AIR | SPTYP_CONJURATION,
     SPFLAG_NONE,
     7,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_CHAIN_LIGHTNING, "Chain Lightning",
     SPTYP_AIR | SPTYP_CONJURATION,
     SPFLAG_AREA,
     8,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_EXCRUCIATING_WOUNDS, "Excruciating Wounds",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     SPFLAG_HELPFUL | SPFLAG_BATTLE,
     5,     // fairly high level - potentially one of the best brands
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PORTAL_PROJECTILE, "Portal Projectile",
     SPTYP_TRANSLOCATION,
     SPFLAG_TARGET | SPFLAG_BATTLE,
     2,
     50,
     4, 8,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_UGLY_THING, "Summon Ugly Thing",
     SPTYP_SUMMONING,
     SPFLAG_NONE,
     5,
     200,
     -1, -1,
     0,
     NULL,
     false,
     false
},

// From here on, all spells are monster-only spells.

{
    SPELL_HELLFIRE_BURST, "Hellfire Burst",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_UNHOLY | SPFLAG_MONSTER,
     9,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_VAMPIRE_SUMMON, "Vampire Summon",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BRAIN_FEED, "Brain Feed",
     SPTYP_NECROMANCY,
     SPFLAG_UNHOLY | SPFLAG_TARGET | SPFLAG_MONSTER,
     3,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     false,
     false
},

{
    SPELL_FAKE_RAKSHASA_SUMMON, "Rakshasa Summon",
     SPTYP_SUMMONING | SPTYP_NECROMANCY,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     3,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_STEAM_BALL, "Steam Ball",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     0,
     7, 7,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_UFETUBUS, "Summon Ufetubus",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_SUMMON_BEAST, "Summon Beast",
     SPTYP_SUMMONING,
     SPFLAG_UNHOLY | SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ENERGY_BOLT, "Energy Bolt",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     4,
     0,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_POISON_SPLASH, "Poison Splash",
     SPTYP_POISON,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     2,
     0,
     7, 7,
     2,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_UNDEAD, "Summon Undead",
     SPTYP_SUMMONING | SPTYP_NECROMANCY,
     SPFLAG_MONSTER,
     7,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false,
},

{
    SPELL_CANTRIP, "Cantrip",
     SPTYP_NONE,
     SPFLAG_MONSTER,
     1,
     0,
     -1, -1,
     1, // SPTYP_NONE spells have no default noise level
     NULL,
     false,
     false
},

{
    SPELL_QUICKSILVER_BOLT, "Quicksilver Bolt",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     5,
     0,
     8, 8,
     0,
     NULL,
     true,
     false
},

{
    SPELL_METAL_SPLINTERS, "Metal Splinters",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     5,
     0,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_MIASMA, "Miasma",
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     6,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_DRAKES, "Summon Drakes",
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     6,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_BLINK_OTHER, "Blink Other",
     SPTYP_TRANSLOCATION,
     SPFLAG_ESCAPE | SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER,
     2,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_SUMMON_MUSHROOMS, "Summon Mushrooms",
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     4,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_ACID_SPLASH, "Acid Splash",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_STICKY_FLAME_SPLASH, "Sticky Flame Splash",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     4,
     100,
     5, 5,
     0,
     NULL,
     true,
     false
},

{
    SPELL_FIRE_BREATH, "Fire Breath",
     SPTYP_CONJURATION | SPTYP_FIRE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_COLD_BREATH, "Cold Breath",
     SPTYP_CONJURATION | SPTYP_ICE,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     5,
     0,
     6, 6,
     0,
     NULL,
     true,
     false
},

{
    SPELL_DRACONIAN_BREATH, "Draconian Breath",
     SPTYP_CONJURATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_MONSTER | SPFLAG_INNATE | SPFLAG_NOISY,
     8,
     0,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_WATER_ELEMENTALS, "Summon Water Elementals",
     SPTYP_SUMMONING,
     SPFLAG_MONSTER,
     5,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

{
    SPELL_PORKALATOR, "Porkalator",
     SPTYP_ENCHANTMENT | SPTYP_TRANSMUTATION,
     SPFLAG_DIR_OR_TARGET | SPFLAG_NOT_SELF,
     5,
     200,
     LOS_RADIUS, LOS_RADIUS,
     0,
     NULL,
     true,
     false
},

{
    SPELL_NO_SPELL, "nonexistent spell",
     0,
     SPFLAG_TESTING,
     1,
     0,
     -1, -1,
     0,
     NULL,
     false,
     false
},

#endif

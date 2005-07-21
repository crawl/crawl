/*
   In case anyone ever wants to add new spells, or just understand my reasons
   for putting a particular spell into a particular type, read on:

   Guidelines for typing spells

   Conjuration
   This type has a near monopoly on effective and relatively risk-free combat
   spells. All other types of combat spells are either indirect (enchantments),
   risky/detrimental/not versatile (necromancy) or just plain crappy (burn and
   freeze), although smiting (holy) is not too bad.
   Conjuration spells all involve the magical creation of matter and/or energy
   (which are the same thing anyway, right?). They are distinguished from
   summoning spells in that they do not involve the summoning of an entire
   creature from another place.

   Enchantment
   These spells mostly cause some kind of durational effect, which lasts only
   until the magic wears off. Enchantments are distinguished from trans-
   migrations in that the latter cause a permanent alteration in something
   which persists even after the magic has faded, while the effects of the
   former last only so long as the magic does. Sometimes enchantments may take
   advantage of the more powerful aspects of transmigration to induce some
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

   Transmigration
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

   Many spells use magic from two types. These spells are equally
   available to either type; a conjurer is no worse at a fire/conjuration than
   at a pure conjuration. I guess a spell could be of three types, but they
   would have to be types with short names (limited space in the spell
   windows).
   - Note : this is no longer true, with the implementation of magic skills.
   Your skill for a spell is effectively the average of all types used in it.
   Poison has no skills, but still has a staff


*/

/*
 * When adding enchantments, must add them to extension as well!
 *
 * spells to do:
 *   Contingency?
 *   Trigger contingency
 *   Preserve Corpses
 *   Permanency
 *   Ball Lightning
 *   Explosive rune?
 *   Fennel wands
 *   More summonings!
 */

#ifndef SPLDATA_H
#define SPLDATA_H


{
    SPELL_IDENTIFY, "Identify",
     SPTYP_DIVINATION,
     6
},

{
    SPELL_TELEPORT_SELF, "Teleport Self",
     SPTYP_TRANSLOCATION,
     5
},

{
    SPELL_CAUSE_FEAR, "Cause Fear",
     SPTYP_ENCHANTMENT,
     5
},

{
    SPELL_CREATE_NOISE, "Create Noise",
     SPTYP_ENCHANTMENT,
     1
},

{
    SPELL_REMOVE_CURSE, "Remove Curse",
     SPTYP_ENCHANTMENT,
     5
},

{
    SPELL_MAGIC_DART, "Magic Dart",
     SPTYP_CONJURATION,
     1
},

{
    SPELL_FIREBALL, "Fireball",
     SPTYP_CONJURATION | SPTYP_FIRE,
     6
},

{
  SPELL_SWAP, "Swap",
    SPTYP_TRANSLOCATION,
    3
},

{
  SPELL_APPORTATION, "Apportation",
    SPTYP_TRANSLOCATION,
    1
},

{
  SPELL_TWIST, "Twist",
    SPTYP_TRANSLOCATION,
    1
},

{
    SPELL_CONJURE_FLAME, "Conjure Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     3
},

{
    SPELL_DIG, "Dig",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     4
},

{
    SPELL_BOLT_OF_FIRE, "Bolt of Fire",
     SPTYP_CONJURATION | SPTYP_FIRE,
     5
},

{
    SPELL_BOLT_OF_COLD, "Bolt of Cold",
     SPTYP_CONJURATION | SPTYP_ICE,
     5
},

{
    SPELL_LIGHTNING_BOLT, "Lightning Bolt",
     SPTYP_CONJURATION | SPTYP_AIR,
     6
},

{
    SPELL_BOLT_OF_MAGMA, "Bolt of Magma",
     SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_EARTH,
     5
},

{
    SPELL_POLYMORPH_OTHER, "Polymorph Other",
     SPTYP_TRANSMIGRATION,  // removed enchantment, wasn't needed -- bwr
     5
},

{
    SPELL_SLOW, "Slow",
     SPTYP_ENCHANTMENT,
     3
},

{
    SPELL_HASTE, "Haste",
     SPTYP_ENCHANTMENT,
     6  // lowered to 6 from 8, since its easily available from various items
        // and Swiftness is level 2 (and gives a similar effect).  Its also
        // not that much better than Invisibility.  -- bwr
},

{
    SPELL_PARALYZE, "Paralyze",
     SPTYP_ENCHANTMENT,
     4
},

{
    SPELL_CONFUSE, "Confuse",
     SPTYP_ENCHANTMENT,
     3
},

{
    SPELL_INVISIBILITY, "Invisibility",
     SPTYP_ENCHANTMENT,
     6
},

{
    SPELL_THROW_FLAME, "Throw Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     2
},

{
    SPELL_THROW_FROST, "Throw Frost",
     SPTYP_CONJURATION | SPTYP_ICE,
     2
},

{
    SPELL_CONTROLLED_BLINK, "Controlled Blink",
     SPTYP_TRANSLOCATION,
     4
},

{
    SPELL_FREEZING_CLOUD, "Freezing Cloud",
     SPTYP_CONJURATION | SPTYP_ICE | SPTYP_AIR,
     7
},

{
    SPELL_MEPHITIC_CLOUD, "Mephitic Cloud",
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     3
},

{
    SPELL_RING_OF_FLAMES, "Ring of Flames",
     SPTYP_ENCHANTMENT | SPTYP_FIRE,
     8
},

{
    SPELL_RESTORE_STRENGTH, "Restore Strength",
     SPTYP_HOLY,
     2
},

{
    SPELL_RESTORE_INTELLIGENCE, "Restore Intelligence",
     SPTYP_HOLY,
     2
},

{
    SPELL_RESTORE_DEXTERITY, "Restore Dexterity",
     SPTYP_HOLY,
     2
},

{
    SPELL_VENOM_BOLT, "Venom Bolt",
     SPTYP_CONJURATION | SPTYP_POISON,
     5
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, "Olgreb's Toxic Radiance",
     SPTYP_POISON,
     4
},

{
    SPELL_TELEPORT_OTHER, "Teleport Other",
     SPTYP_TRANSLOCATION,
     4
},

{
    SPELL_LESSER_HEALING, "Lesser Healing",
     SPTYP_HOLY,
     2
},

{
    SPELL_GREATER_HEALING, "Greater Healing",
     SPTYP_HOLY,
     6
},

{
    SPELL_CURE_POISON_I, "Cure Poison",
     SPTYP_HOLY,
     3
},

{
    SPELL_PURIFICATION, "Purification",
     SPTYP_HOLY,
     5
},

{
    SPELL_DEATHS_DOOR, "Death's Door",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     8
},

{
    SPELL_SELECTIVE_AMNESIA, "Selective Amnesia",
     SPTYP_ENCHANTMENT,
     3
},

{
    SPELL_MASS_CONFUSION, "Mass Confusion",
     SPTYP_ENCHANTMENT,
     6
},

{
    SPELL_SMITING, "Smiting",
     SPTYP_HOLY,
     4
},

{
    SPELL_REPEL_UNDEAD, "Repel Undead",
     SPTYP_HOLY,
     3
},

{
    SPELL_HOLY_WORD, "Holy Word",
     SPTYP_HOLY,
     7
},

{
    SPELL_DETECT_CURSE, "Detect Curse",
     SPTYP_DIVINATION,
     3
},

{
    SPELL_SUMMON_SMALL_MAMMAL, "Summon Small Mammals",
     SPTYP_SUMMONING,
     1
},

{
    SPELL_ABJURATION_I, "Abjuration",
     SPTYP_SUMMONING,
     3
},

{
    SPELL_SUMMON_SCORPIONS, "Summon Scorpions",
     SPTYP_SUMMONING | SPTYP_POISON,
     4
},

{
    SPELL_LEVITATION, "Levitation",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     2
},

{
    SPELL_BOLT_OF_DRAINING, "Bolt of Draining",
     SPTYP_CONJURATION | SPTYP_NECROMANCY,
     6
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, "Lehudib's Crystal Spear",
     SPTYP_CONJURATION | SPTYP_EARTH,
     8
},

{
    SPELL_BOLT_OF_INACCURACY, "Bolt of Inaccuracy",
     SPTYP_CONJURATION,
     2
},

{
    SPELL_POISONOUS_CLOUD, "Poisonous Cloud",
     SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
     6
}
,

{
    SPELL_FIRE_STORM, "Fire Storm",
     SPTYP_CONJURATION | SPTYP_FIRE,
     9
},

{
    SPELL_DETECT_TRAPS, "Detect Traps",
     SPTYP_DIVINATION,
     2
},

{
    SPELL_BLINK, "Blink",
     SPTYP_TRANSLOCATION,
     2
},


// The following name was found in the hack.exe file of an early version
// of PCHACK - credit goes to its creator (whoever that may be):
{
    SPELL_ISKENDERUNS_MYSTIC_BLAST, "Iskenderun's Mystic Blast",
     SPTYP_CONJURATION,
     4
},

{
    SPELL_SWARM, "Summon Swarm",
     SPTYP_SUMMONING,
     6
},

{
    SPELL_SUMMON_HORRIBLE_THINGS, "Summon Horrible Things",
     SPTYP_SUMMONING,
     8
},

{
    SPELL_ENSLAVEMENT, "Enslavement",
     SPTYP_ENCHANTMENT,
     4
},

{
    SPELL_MAGIC_MAPPING, "Magic Mapping",
     SPTYP_DIVINATION | SPTYP_EARTH,
     4
},

{
    SPELL_HEAL_OTHER, "Heal Other",
     SPTYP_HOLY,
     3
},

{
    SPELL_ANIMATE_DEAD, "Animate Dead",
     SPTYP_NECROMANCY,
     4
},

{
    SPELL_PAIN, "Pain",
     SPTYP_NECROMANCY,
     1
},

{
    SPELL_EXTENSION, "Extension",
     SPTYP_ENCHANTMENT,
     5
},

{
    SPELL_CONTROL_UNDEAD, "Control Undead",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     6
},

{
    SPELL_ANIMATE_SKELETON, "Animate Skeleton",
     SPTYP_NECROMANCY,
     1
},

{
    SPELL_VAMPIRIC_DRAINING, "Vampiric Draining",
     SPTYP_NECROMANCY,
     3
},

{
    SPELL_SUMMON_WRAITHS, "Summon Wraiths",
     SPTYP_NECROMANCY | SPTYP_SUMMONING,
     7
},

{
    SPELL_DETECT_ITEMS, "Detect Items",
     SPTYP_DIVINATION,
     2
},

{
    SPELL_BORGNJORS_REVIVIFICATION, "Borgnjor's Revivification",
     SPTYP_NECROMANCY,
     6
},

{
    SPELL_BURN, "Burn", // used by wanderers
     SPTYP_FIRE,
     1
},

{
    SPELL_FREEZE, "Freeze",
     SPTYP_ICE,
     1
},

{
    SPELL_SUMMON_ELEMENTAL, "Summon Elemental",
     SPTYP_SUMMONING,
     4
},

{
    SPELL_OZOCUBUS_REFRIGERATION, "Ozocubu's Refrigeration",
     SPTYP_ICE,
     5
},

{
    SPELL_STICKY_FLAME, "Sticky Flame",
     SPTYP_CONJURATION | SPTYP_FIRE,
     4
},

{
    SPELL_SUMMON_ICE_BEAST, "Summon Ice Beast",
     SPTYP_ICE | SPTYP_SUMMONING,
     5
},

{
    SPELL_OZOCUBUS_ARMOUR, "Ozocubu's Armour",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     3
},

{
    SPELL_CALL_IMP, "Call Imp",
     SPTYP_SUMMONING,
     3
},

{
    SPELL_REPEL_MISSILES, "Repel Missiles",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     2
},

{
    SPELL_BERSERKER_RAGE, "Berserker Rage",
     SPTYP_ENCHANTMENT,
     3
},

{
    SPELL_DISPEL_UNDEAD, "Dispel Undead",
     SPTYP_NECROMANCY,
     4
},

{
    SPELL_GUARDIAN, "Guardian",
     SPTYP_HOLY,
     7
},

{
    SPELL_PESTILENCE, "Pestilence",
     SPTYP_HOLY,
     4
},

{
    SPELL_THUNDERBOLT, "Thunderbolt",
     SPTYP_HOLY | SPTYP_AIR,
     6         // why is this the only holy spell with a secondary? {dlb}
}
,

{
    SPELL_FLAME_OF_CLEANSING, "Flame of Cleansing",
     SPTYP_HOLY,
     8
},

{
    SPELL_SHINING_LIGHT, "Shining Light",
     SPTYP_HOLY,
     7
},

{
    SPELL_SUMMON_DAEVA, "Summon Daeva",
     SPTYP_HOLY,
     8
},

{
    SPELL_ABJURATION_II, "Abjuration",
     SPTYP_HOLY,
     4
},

{
    SPELL_TWISTED_RESURRECTION, "Twisted Resurrection",
     SPTYP_NECROMANCY,
     5
},

{
    SPELL_REGENERATION, "Regeneration",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     3
},

{
    SPELL_BONE_SHARDS, "Bone Shards",
     SPTYP_NECROMANCY,
     3
},

{
    SPELL_BANISHMENT, "Banishment",
     SPTYP_TRANSLOCATION,
     5
},

{
    SPELL_CIGOTUVIS_DEGENERATION, "Cigotuvi's Degeneration",
     SPTYP_TRANSMIGRATION | SPTYP_NECROMANCY,
     5
},

{
    SPELL_STING, "Sting",
     SPTYP_CONJURATION | SPTYP_POISON,
     1
},

{
    SPELL_SUBLIMATION_OF_BLOOD, "Sublimation of Blood",
     SPTYP_NECROMANCY,
     2
},

{
    SPELL_TUKIMAS_DANCE, "Tukima's Dance",
     SPTYP_ENCHANTMENT,
     3
},

{
    SPELL_HELLFIRE, "Hellfire",
     SPTYP_CONJURATION | SPTYP_FIRE,
     9
},

{
    SPELL_SUMMON_DEMON, "Summon Demon",
     SPTYP_SUMMONING,
     5
},

{
    SPELL_DEMONIC_HORDE, "Demonic Horde",
     SPTYP_SUMMONING,
     6
},

{
    SPELL_SUMMON_GREATER_DEMON, "Summon Greater Demon",
     SPTYP_SUMMONING,
     7
},

{
    SPELL_CORPSE_ROT, "Corpse Rot",
     SPTYP_NECROMANCY,
     2
},

{
    SPELL_TUKIMAS_VORPAL_BLADE, "Tukima's Vorpal Blade",
     SPTYP_ENCHANTMENT,
     2
},

{
    SPELL_FIRE_BRAND, "Fire Brand",
     SPTYP_ENCHANTMENT | SPTYP_FIRE,
     2
},

{
    SPELL_FREEZING_AURA, "Freezing Aura",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     2
},

{
    SPELL_LETHAL_INFUSION, "Lethal Infusion",
     SPTYP_ENCHANTMENT | SPTYP_NECROMANCY,
     2
},

{
    SPELL_CRUSH, "Crush", // used by wanderers
     SPTYP_EARTH,
     1
},

{
    SPELL_BOLT_OF_IRON, "Bolt of Iron",
     SPTYP_CONJURATION | SPTYP_EARTH,
     6
},

{
    SPELL_STONE_ARROW, "Stone Arrow",
     SPTYP_CONJURATION | SPTYP_EARTH,
     3
},

{
    SPELL_TOMB_OF_DOROKLOHE, "Tomb of Doroklohe",
     SPTYP_CONJURATION | SPTYP_EARTH, // conj makes more sense than tmig -- bwr
     7 
}
,

{
    SPELL_STONEMAIL, "Stonemail",
     SPTYP_ENCHANTMENT | SPTYP_EARTH,
     6
},

{
    SPELL_SHOCK, "Shock",
     SPTYP_CONJURATION | SPTYP_AIR,
     1
},

{
    SPELL_SWIFTNESS, "Swiftness",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     2
},

{
    SPELL_FLY, "Fly",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     4
},

{
    SPELL_INSULATION, "Insulation",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     4
},

{
    SPELL_ORB_OF_ELECTROCUTION, "Orb of Electrocution",
     SPTYP_CONJURATION | SPTYP_AIR,
     7
},

{
    SPELL_DETECT_CREATURES, "Detect Creatures",
     SPTYP_DIVINATION,
     2
},

{
    SPELL_CURE_POISON_II, "Cure Poison",
     SPTYP_POISON,
     2
}
,

{
    SPELL_CONTROL_TELEPORT, "Control Teleport",
     SPTYP_ENCHANTMENT | SPTYP_TRANSLOCATION,
     6
},

{
    SPELL_POISON_AMMUNITION, "Poison Ammunition",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     4 // jmf: SPTYP_TRANSMIGRATION vs SPTYP_ENCHANTMENT?
}
,

{
    SPELL_POISON_WEAPON, "Poison Weapon",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     4
},

{
    SPELL_RESIST_POISON, "Resist Poison",
     SPTYP_ENCHANTMENT | SPTYP_POISON,
     4
},

{
    SPELL_PROJECTED_NOISE, "Projected Noise",
     SPTYP_ENCHANTMENT,
     2
},

{
    SPELL_ALTER_SELF, "Alter Self",
     SPTYP_TRANSMIGRATION,
     7
},

{
    SPELL_DEBUGGING_RAY, "Debugging Ray",
     SPTYP_CONJURATION,
     7
},

{
    SPELL_RECALL, "Recall",
     SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
     3
},

{
    SPELL_PORTAL, "Portal",
     SPTYP_TRANSLOCATION,
     8
},

{
    SPELL_AGONY, "Agony",
     SPTYP_NECROMANCY,
     5
},

{
    SPELL_SPIDER_FORM, "Spider Form",
     SPTYP_TRANSMIGRATION | SPTYP_POISON,
     3
},

{
    SPELL_DISRUPT, "Disrupt",
     SPTYP_TRANSMIGRATION,
     1
},

{
    SPELL_DISINTEGRATE, "Disintegrate",
     SPTYP_TRANSMIGRATION,
     6
},

{
    SPELL_BLADE_HANDS, "Blade Hands",
     SPTYP_TRANSMIGRATION,
     5  // only removes weapon, so I raised this from 4 -- bwr
},

{
    SPELL_STATUE_FORM, "Statue Form",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     6
},

{
    SPELL_ICE_FORM, "Ice Form",
     SPTYP_ICE | SPTYP_TRANSMIGRATION,
     4 // doesn't allow for equipment, so I lowered this from 5 -- bwr
},

{
    SPELL_DRAGON_FORM, "Dragon Form",
     SPTYP_FIRE | SPTYP_TRANSMIGRATION,
     8
},

{
    SPELL_NECROMUTATION, "Necromutation",
     SPTYP_TRANSMIGRATION | SPTYP_NECROMANCY,
     8
},

{
    SPELL_DEATH_CHANNEL, "Death Channel",
     SPTYP_NECROMANCY,
     9
},

{
    SPELL_SYMBOL_OF_TORMENT, "Symbol of Torment",
     SPTYP_NECROMANCY,
     6
},

{
    SPELL_DEFLECT_MISSILES, "Deflect Missiles",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     6
},

{
    SPELL_ORB_OF_FRAGMENTATION, "Orb of Fragmentation",
     SPTYP_CONJURATION | SPTYP_EARTH,
     7
},

{
    SPELL_ICE_BOLT, "Ice Bolt",
     SPTYP_CONJURATION | SPTYP_ICE,
     4
},

{
    SPELL_ICE_STORM, "Ice Storm",
     SPTYP_CONJURATION | SPTYP_ICE,
     9
},

{
    SPELL_ARC, "Arc",   // used by wanderers
     SPTYP_AIR,
     1
},

{
    SPELL_AIRSTRIKE, "Airstrike",
     SPTYP_AIR,
     4
},

{
    SPELL_SHADOW_CREATURES, "Shadow Creatures",
     SPTYP_SUMMONING,  // jmf: or SPTYP_SUMMONING | SPTYP_CONJURATION
     5
}
,

{
    SPELL_CONFUSING_TOUCH, "Confusing Touch",
     SPTYP_ENCHANTMENT,
     1
},

{
    SPELL_SURE_BLADE, "Sure Blade",
     SPTYP_ENCHANTMENT,
     2
},



  //jmf: new spells


{
    SPELL_FLAME_TONGUE, "Flame Tongue",
     SPTYP_CONJURATION | SPTYP_FIRE,
     1
},

{
    SPELL_PASSWALL, "Passwall",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     3
},

{
    SPELL_IGNITE_POISON, "Ignite Poison",
     SPTYP_FIRE | SPTYP_TRANSMIGRATION,
     7
},

{
    SPELL_STICKS_TO_SNAKES, "Sticks to Snakes",
     SPTYP_TRANSMIGRATION | SPTYP_SUMMONING,
     2
},

{
    SPELL_SUMMON_LARGE_MAMMAL, "Call Canine Familiar",
     SPTYP_SUMMONING,
     3
},

{
    SPELL_SUMMON_DRAGON, "Summon Dragon",
     SPTYP_FIRE | SPTYP_SUMMONING,
     9
},

{
    SPELL_TAME_BEASTS, "Tame Beasts",
     SPTYP_ENCHANTMENT,
     5
},

{
    SPELL_SLEEP, "Ensorcelled Hibernation",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     2
},

{
    SPELL_MASS_SLEEP, "Metabolic Englaciation",
     SPTYP_ENCHANTMENT | SPTYP_ICE,
     7
},

{
    SPELL_DETECT_MAGIC, "Detect Magic",
     SPTYP_DIVINATION,
     1
},

{
    SPELL_DETECT_SECRET_DOORS, "Detect Secret Doors",
     SPTYP_DIVINATION,
     1
},

{
    SPELL_SEE_INVISIBLE, "See Invisible",
     SPTYP_ENCHANTMENT | SPTYP_DIVINATION,
     4
},

{
    SPELL_FORESCRY, "Forescry",
     SPTYP_DIVINATION,
     5
},

{
    SPELL_SUMMON_BUTTERFLIES, "Summon Butterflies",
     SPTYP_SUMMONING,
     1
},

{
    SPELL_WARP_BRAND, "Warp Weapon",
     SPTYP_ENCHANTMENT | SPTYP_TRANSLOCATION,
     7     // this is high for a reason - Warp brands are very powerful.
},

{
    SPELL_SILENCE, "Silence",
     SPTYP_ENCHANTMENT | SPTYP_AIR,
     3
},

{
    SPELL_SHATTER, "Shatter",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     9
},

{
    SPELL_DISPERSAL, "Dispersal",
     SPTYP_TRANSLOCATION,
     7
},

{
    SPELL_DISCHARGE, "Static Discharge",
     SPTYP_CONJURATION | SPTYP_AIR,
     4
},

{
    SPELL_BEND, "Bend",
     SPTYP_TRANSLOCATION,
     1
},

{
    SPELL_BACKLIGHT, "Corona",
     SPTYP_ENCHANTMENT,
     1
},

{
    SPELL_INTOXICATE, "Alistair's Intoxication",
     SPTYP_TRANSMIGRATION | SPTYP_POISON,
     4
},

{
    SPELL_GLAMOUR, "Glamour",
     SPTYP_ENCHANTMENT,
     5
},

{
    SPELL_EVAPORATE, "Evaporate",
     SPTYP_FIRE | SPTYP_TRANSMIGRATION,
     2   // XXX: level 2 or 3, what should it be now? -- bwr
},

{
    SPELL_ERINGYAS_SURPRISING_BOUQUET, "Eringya's Surprising Bouquet",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     4
},

{
    SPELL_FRAGMENTATION, "Lee's Rapid Deconstruction",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     5
},

{
    SPELL_AIR_WALK, "Air Walk",
     SPTYP_TRANSMIGRATION | SPTYP_AIR,
     9
},

{
    SPELL_SANDBLAST, "Sandblast",
     SPTYP_TRANSMIGRATION | SPTYP_EARTH,
     1
},

{
    SPELL_ROTTING, "Rotting",
     SPTYP_TRANSMIGRATION | SPTYP_NECROMANCY,
     5
},

{
    SPELL_SHUGGOTH_SEED, "Shuggoth Seed",
     SPTYP_NECROMANCY | SPTYP_SUMMONING,
     7
},

{
    SPELL_MAXWELLS_SILVER_HAMMER, "Maxwell's Silver Hammer",
     SPTYP_ENCHANTMENT | SPTYP_EARTH,
     2
},

{
    SPELL_CONDENSATION_SHIELD, "Condensation Shield",
     SPTYP_ICE | SPTYP_TRANSMIGRATION,
     4
},

{
    SPELL_SEMI_CONTROLLED_BLINK, "Semi-Controlled Blink",
     SPTYP_TRANSLOCATION,
     3
},

{
  SPELL_STONESKIN, "Stoneskin",
    SPTYP_EARTH | SPTYP_TRANSMIGRATION, // was ench -- bwr
    2
},

{
  SPELL_SIMULACRUM, "Simulacrum",
    SPTYP_ICE | SPTYP_NECROMANCY,
    7
},

{
  SPELL_CONJURE_BALL_LIGHTNING, "Conjure Ball Lightning",
    SPTYP_AIR | SPTYP_CONJURATION,
    8
},

{
  SPELL_FAR_STRIKE, "Far Strike",
    SPTYP_TRANSLOCATION,
    3
},

{
  SPELL_DELAYED_FIREBALL, "Delayed Fireball",
    SPTYP_FIRE | SPTYP_CONJURATION,
    7 
},

{
  SPELL_FULSOME_DISTILLATION, "Fulsome Distillation",
    SPTYP_TRANSMIGRATION | SPTYP_NECROMANCY,
    1 
},

{
  SPELL_POISON_ARROW, "Poison Arrow",
    SPTYP_CONJURATION | SPTYP_POISON,
    6 
},

{
  SPELL_STRIKING, "Striking",
    0,
    1 
},

{
    SPELL_NO_SPELL, "nonexistent spell",
     0,
     0,
},


#endif

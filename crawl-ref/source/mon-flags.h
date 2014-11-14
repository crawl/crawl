#ifndef MON_CLASS_FLAGS_H
#define MON_CLASS_FLAGS_H

#define BIT(x) ((uint64_t)1<<(x))

// properties of the monster class (other than resists/vulnerabilities).
// NOT using enum, since C++ doesn't guarantee support for 64-bit enum
// constants.
const uint64_t M_NO_FLAGS = 0;

                                   //1<< 0;

                                   //1<< 1;

                                   //1<< 2;

// monster is skilled fighter
const uint64_t M_FIGHTER           = 1<< 3;

// do not give (unique) a wand
const uint64_t M_NO_WAND           = 1<< 4;

// do not give a high tier wand
const uint64_t M_NO_HT_WAND        = 1<< 5;

// is created invis
const uint64_t M_INVIS             = 1<< 6;

// can see invis
const uint64_t M_SEE_INVIS         = 1<< 7;

// can't be blinded
const uint64_t M_UNBLINDABLE       = 1<< 8;

// uses talking code
const uint64_t M_SPEAKS            = 1<< 9;

// monster is perma-confused;
const uint64_t M_CONFUSED          = 1<<10;

// monster is batty
const uint64_t M_BATTY             = 1<<11;

// monster can split
const uint64_t M_SPLITS            = 1<<12;

// monster glows with light
const uint64_t M_GLOWS_LIGHT       = 1<<13;

// monster is stationary
const uint64_t M_STATIONARY        = 1<<14;

// monster can smell blood
const uint64_t M_BLOOD_SCENT       = 1<<15;

// susceptible to cold; drainable by vampires
const uint64_t M_COLD_BLOOD        = 1<<16;

// drainable by vampires, no other effect currently
const uint64_t M_WARM_BLOOD        = 1<<17;

// monster glows with radiation
const uint64_t M_GLOWS_RADIATION   = 1<<18;

// monster digs through rock
const uint64_t M_BURROWS           = 1<<19;

// monster can submerge
const uint64_t M_SUBMERGES         = 1<<20;

// monster is a unique
const uint64_t M_UNIQUE            = 1<<21;

// Passive acid splash when hit.
const uint64_t M_ACID_SPLASH       = 1<<22;

// gets various archery boosts
const uint64_t M_ARCHER            = 1<<23;

// is insubstantial
const uint64_t M_INSUBSTANTIAL     = 1<<24;

// wields two weapons at once
const uint64_t M_TWO_WEAPONS       = 1<<25;

// has extra-fast regeneration
const uint64_t M_FAST_REGEN        = 1<<26;

// cannot regenerate
const uint64_t M_NO_REGEN          = 1<<27;

                                   //1<<28;

                                   //1<<29;

// boneless corpses
const uint64_t M_NO_SKELETON       = 1<<30;

// worth 0 xp
const uint64_t M_NO_EXP_GAIN       = (uint64_t)1<<31;

                                   //(uint64_t)1<<32;

// phase shift (EV bonus not included)
const uint64_t M_PHASE_SHIFT       = (uint64_t)1<<33;

// not a valid polymorph target (but can be polymorphed)
const uint64_t M_NO_POLY_TO        = (uint64_t)1<<34;

                                   //(uint64_t)1<<35;

// always leaves a corpse
const uint64_t M_ALWAYS_CORPSE     = (uint64_t)1<<36;

// mostly doesn't try to melee
const uint64_t M_DONT_MELEE        = (uint64_t)1<<37;

// is an artificial being
const uint64_t M_ARTIFICIAL        = (uint64_t)1<<38;

// can survive without breathing; immune to asphyxiation and Mephitic Cloud
const uint64_t M_UNBREATHING       = (uint64_t)1<<39;

// not fully coded; causes a warning
const uint64_t M_UNFINISHED        = (uint64_t)1<<40;

const uint64_t M_HERD              = (uint64_t)1<<41;

// can flee at low hp
const uint64_t M_FLEES             = (uint64_t)1<<42;

// can sense vibrations in web traps
const uint64_t M_WEB_SENSE         = (uint64_t)1<<43;

// tries to maintain LOS/2 range from its target
const uint64_t M_MAINTAIN_RANGE    = (uint64_t)1<<44;

// flesh is not usable for making zombies
const uint64_t M_NO_ZOMBIE         = (uint64_t)1<<45;

// cannot be placed by any means, even in the arena, etc.
const uint64_t M_CANT_SPAWN        = (uint64_t)1<<46;

// derived undead can't be randomly generated
const uint64_t M_NO_GEN_DERIVED    = (uint64_t)1<<47;

                                   //(uint64_t)1<<48;

// hybridized monster composed of other monster parts
const uint64_t M_HYBRID            = (uint64_t)1<<49;

                                   //(uint64_t)1<<50;

                                   //(uint64_t)1<<51;

                                   //(uint64_t)1<<52;

// monster is shadowy and cannot be backlit (was M_GLOWS_LIGHT)
const uint64_t M_SHADOW            = (uint64_t)1<<53;

// monster is a proxy for a charm/conjuration spell (IOOD, etc.)
const uint64_t M_SPELL_PROXY       = (uint64_t)1<<54;

// Same for flags for actual monsters.
typedef uint64_t monster_flag_type;
const uint64_t MF_NO_REWARD          = BIT(0);  // no benefit from killing
const uint64_t MF_JUST_SUMMONED      = BIT(1);  // monster skips next available action
const uint64_t MF_TAKING_STAIRS      = BIT(2);  // is following player through stairs
const uint64_t MF_INTERESTING        = BIT(3);  // Player finds monster interesting

const uint64_t MF_SEEN               = BIT(4);  // Player has already seen monster
const uint64_t MF_KNOWN_SHIFTER      = BIT(5);  // A known shapeshifter.
const uint64_t MF_BANISHED           = BIT(6);  // Monster that has been banished.

const uint64_t MF_HARD_RESET         = BIT(7);  // Summoned, should not drop gear on reset
const uint64_t MF_WAS_NEUTRAL        = BIT(8);  // mirror to CREATED_FRIENDLY for neutrals
const uint64_t MF_ATT_CHANGE_ATTEMPT = BIT(9);  // Saw player and attitude changed (or
                                     // not); currently used for holy beings
                                     // (good god worshippers -> neutral)
                                     // orcs (Beogh worshippers -> friendly),
                                     // and slimes (Jiyva worshippers -> neutral)
const uint64_t MF_WAS_IN_VIEW        = BIT(10); // Was in view during previous turn.

const uint64_t MF_BAND_MEMBER        = BIT(11); // Created as a member of a band
const uint64_t MF_GOT_HALF_XP        = BIT(12); // Player already got half xp value earlier
const uint64_t MF_FAKE_UNDEAD        = BIT(13); // Consider this monster to have MH_UNDEAD
                                     // holiness, regardless of its actual type
const uint64_t MF_ENSLAVED_SOUL      = BIT(14); // An undead monster soul enslaved by
                                     // Yredelemnul's power

const uint64_t MF_NAME_SUFFIX        = BIT(15); // mname is a suffix.
const uint64_t MF_NAME_ADJECTIVE     = BIT(16); // mname is an adjective.
                                     // between it and the monster type name.
const uint64_t MF_NAME_REPLACE       = MF_NAME_SUFFIX|MF_NAME_ADJECTIVE;
                                     // mname entirely replaces normal monster
                                     // name.
const uint64_t MF_NAME_MASK          = MF_NAME_REPLACE;
const uint64_t MF_GOD_GIFT           = BIT(17); // Is a god gift.
const uint64_t MF_FLEEING_FROM_SANCTUARY = BIT(18); // Is running away from player sanctuary
const uint64_t MF_EXPLODE_KILL       = BIT(19); // Is being killed with disintegration

    // These are based on the flags in monster class, but can be set for
    // monsters that are not normally fighters (in vaults).
const uint64_t MF_FIGHTER            = BIT(20); // Monster is skilled fighter.
const uint64_t MF_TWO_WEAPONS        = BIT(21); // Monster wields two weapons.
const uint64_t MF_ARCHER             = BIT(22); // Monster gets various archery boosts.
const uint64_t MF_MELEE_MASK         = MF_FIGHTER|MF_TWO_WEAPONS|MF_ARCHER;

                                     //BIT(23);
                                     //BIT(24);
                                     //BIT(25);

const uint64_t MF_NO_REGEN           = BIT(26); // This monster cannot regenerate.

const uint64_t MF_NAME_DESCRIPTOR    = BIT(27); // mname should be treated with normal
                                     // grammar, ie, prevent "You hit red rat"
                                     // and other such constructs.
const uint64_t MF_NAME_DEFINITE      = BIT(28); // give this monster the definite "the"
                                     // article, instead of the indefinite "a"
                                     // article.
const uint64_t MF_SPECTRALISED       = BIT(29); // living monster revived by a lost soul
const uint64_t MF_DEMONIC_GUARDIAN   = BIT(30); // is a demonic_guardian
const uint64_t MF_NAME_SPECIES       = BIT(31); // mname should be used for corpses as well,
const uint64_t MF_NAME_ZOMBIE        = BIT(32); // mname replaces zombies/skeletons, use
                                     // only for already zombified monsters
const uint64_t MF_SENSED             = BIT(33); // Player has been warned
                                     // about this monster being nearby.
const uint64_t MF_NAME_NOCORPSE      = BIT(34); // mname should not be used for corpses
const uint64_t MF_SEEN_RANGED        = BIT(35); // known to have a ranged attack

const uint64_t MF_POLYMORPHED        = BIT(36); // this monster has been polymorphed.
const uint64_t MF_JUST_SLEPT         = BIT(37); // just got hibernated/slept
const uint64_t MF_TSO_SEEN           = BIT(38); // possibly got piety with TSO
#endif

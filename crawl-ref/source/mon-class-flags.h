#ifndef MON_CLASS_FLAGS_H
#define MON_CLASS_FLAGS_H

// properties of the monster class (other than resists/vulnerabilities).
// NOT using enum, since C++ doesn't guarantee support for 64-bit enum
// constants.
const uint64_t M_NO_FLAGS = 0;

// any non-physical-attack powers,
const uint64_t M_SPELLCASTER       = 1<< 0;

// monster is a wizard (hated by Trog; affected by silence)
const uint64_t M_ACTUAL_SPELLS     = 1<< 1;

// monster is a priest
const uint64_t M_PRIEST            = 1<< 2;

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

// can sense invisible things
const uint64_t M_SENSE_INVIS       = 1<< 8;

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

// XXX: eventually make these spells?
const uint64_t M_SPECIAL_ABILITY   = 1<<26;

// cannot regenerate
const uint64_t M_NO_REGEN          = 1<<27;

// cannot cast spells when silenced; even though it's not a priest or
// wizard
const uint64_t M_SPELL_NO_SILENT   = 1<<28;

// can cast spells when silenced; but casting makes noise when not
// silenced
const uint64_t M_NOISY_SPELLS      = 1<<29;

// boneless corpses
const uint64_t M_NO_SKELETON       = 1<<30;

// worth 0 xp
const uint64_t M_NO_EXP_GAIN       = (uint64_t)1<<31;

// has a deflection effect
const uint64_t M_DEFLECT_MISSILES  = (uint64_t)1<<32;

// phase shift (EV bonus not included)
const uint64_t M_PHASE_SHIFT       = (uint64_t)1<<33;

// not a valid polymorph target (but can be polymorphed)
const uint64_t M_NO_POLY_TO        = (uint64_t)1<<34;

// has special abilities coded as spells which are entirely non-magical
const uint64_t M_FAKE_SPELLS       = (uint64_t)1<<35;

// always leaves a corpse
const uint64_t M_ALWAYS_CORPSE     = (uint64_t)1<<36;

// is constantly "fleeing"
const uint64_t M_FLEEING           = (uint64_t)1<<37;

// is an artificial being
const uint64_t M_ARTIFICIAL        = (uint64_t)1<<38;
#endif

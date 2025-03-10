#pragma once

#include <vector>

#include "mon-info.h"
#include "tag-version.h"

using std::vector;

struct monster_info_flag_name
{
    monster_info_flags flag;
    string short_singular; // HUD display (empty to not show)
    string long_singular; // Targeter display
    string plural; // HUD display in groups (empty to not show)
};

// This vector is in display order. Roughly:
// - Attitudes and summon status
// - Bad things for the player (most buffs)
// - Vulnerabilities
// - Debuffs
// - Ally-only buffs
static const vector<monster_info_flag_name> monster_info_flag_names = {
    // Attitudes
    { MB_CHARMED, "charmed", "charmed", "charmed"},
    { MB_HEXED, "hexed", "control wrested from you", "hexed"},
    { MB_ABJURABLE, "summoned", "summoned", "summoned"},
    { MB_MINION, "minion", "minion", "minion"},
    { MB_UNREWARDING, "", "unrewarding", ""},
    { MB_WANDERING, "wandering", "wandering", "wandering"},
    // Bad things for the player
    { MB_MESMERIZING, "mesmerising", "mesmerising", "mesmerising"},
    { MB_BERSERK, "berserk", "berserk", "berserk"},
    { MB_HASTED, "fast", "fast", "fast"},
    { MB_ROLLING, "rolling", "rolling", "rolling"},
    { MB_INNER_FLAME, "inner flame", "inner flame", "inner flame"},
    { MB_MIRROR_DAMAGE, "reflects damage", "reflecting injuries", "reflect damage"},
    { MB_BOUND_SOUL, "soul bound", "soul bound", "souls bound"},
    { MB_STRONG, "strong", "strong", "strong"},
    { MB_EMPOWERED_SPELLS, "empowered", "spells empowered", "empowered"},
    { MB_FULLY_CHARGED, "charged", "fully charged", "charged"},
    { MB_PARTIALLY_CHARGED, "charging", "partially charged", "charging"},
    { MB_AGILE, "agile", "unusually agile", "agile"},
    { MB_SWIFT, "swift", "covering ground quickly", "swift"},
#if TAG_MAJOR_VERSION == 34
    { MB_PURSUING, "pursuing", "pursuing quickly", "pursuing"},
#endif
    { MB_STILL_WINDS, "stilling wind", "stilling the winds", "stilling wind"},
    { MB_SILENCING, "silencing", "radiating silence", "silencing"},
    { MB_READY_TO_HOWL, "can howl", "ready to howl", "can howl"},
    { MB_VORTEX, "vortex", "surrounded by a freezing vortex", "vortices"},
    { MB_VORTEX_COOLDOWN, "gusty", "surrounded by restless winds", "gusty"},
    { MB_FRENZIED, "frenzied", "frenzied and wild", "frenzied"},
    { MB_FEAR_INSPIRING, "scary", "inspiring fear", "scary"},
    { MB_WORD_OF_RECALL, "chanting recall", "chanting recall", "chanting recall"},
    { MB_REPEL_MSL, "repels missiles", "repelling missiles", "repel missiles"},
    { MB_TOXIC_RADIANCE, "toxic aura", "radiating toxic energy", "toxic auras"},
    { MB_CONCENTRATE_VENOM, "curare", "concentrated venom", "curare" },
    { MB_SIGN_OF_RUIN, "sign of ruin", "marked with the sign of ruin", "signs of ruin"},
    { MB_RESISTANCE, "resistant", "unusually resistant", "resistant"},
    { MB_INVISIBLE, "invisible", "slightly transparent", "invisible"},
    { MB_REGENERATION, "regenerating", "regenerating", "regenerating"},
    { MB_STRONG_WILLED, "strong-willed", "strong-willed", "strong-willed"},
    { MB_INJURY_BOND, "sheltered", "sheltered from injuries", "sheltered"},
    { MB_GOZAG_INCITED, "incited", "incited by Gozag", "incited"},
    { MB_CLOUD_RING_THUNDER, "clouds", "surrounded by thunder", "clouds" },
    { MB_CLOUD_RING_FLAMES, "clouds", "surrounded by flames", "clouds" },
    { MB_CLOUD_RING_CHAOS, "clouds", "surrounded by chaotic energy", "clouds" },
    { MB_CLOUD_RING_MUTATION, "clouds", "surrounded by mutagenic energy", "clouds" },
    { MB_CLOUD_RING_FOG, "clouds", "surrounded by fog", "clouds" },
    { MB_CLOUD_RING_ICE, "clouds", "surrounded by freezing clouds", "clouds" },
    { MB_CLOUD_RING_MISERY, "clouds", "surrounded by negative energy", "clouds" },
    { MB_CLOUD_RING_ACID, "clouds", "surrounded by acidic fog", "clouds" },
    { MB_CLOUD_RING_MIASMA, "clouds", "surrounded by foul miasma", "clouds" },
    { MB_FIRE_CHAMPION, "flame-wreathed", "flame-wreathed", "flame-wreathed"},
    { MB_SILENCE_IMMUNE, "unsilenced", "unaffected by silence", "unsilenced" },
    { MB_DOUBLED_HEALTH, "doubled health", "doubled in health", "doubled health"},
    // Vulnerabilities
    { MB_POSSESSABLE, "soul-gripped", "soul-gripped", "soul-gripped"},
    { MB_CAUGHT, "caught", "entangled in a net", "caught"},
    { MB_WEBBED, "webbed", "entangled in a web", "webbed"},
    { MB_PARALYSED, "paralysed", "paralysed", "paralysed"},
    { MB_PETRIFIED, "petrified", "petrified", "petrified"},
    { MB_CONFUSED, "confused", "confused", "confused"},
    { MB_DORMANT, "dormant", "dormant", "dormant"},
    { MB_SLEEPING, "asleep", "asleep", "asleep"},
    { MB_UNAWARE, "unaware", "unaware", "unaware"},
    { MB_BLIND, "blind", "blind", "blind"},
    { MB_DISTRACTED, "distracted", "not watching you", "distracted"},
    { MB_CANT_SEE_YOU, "unable to see you", "unable to see you", "unable to see you"},
    { MB_INFESTATION, "infested", "infested", "infested"},
    // Debuffs
    { MB_DUMB, "stupefied", "stupefied", "stupefied"},
#if TAG_MAJOR_VERSION == 34
    { MB_PINNED, "pinned", "pinned", "pinned"},
#endif
    { MB_WITHERING, "withering", "withering away", "withering"},
    { MB_CRUMBLING, "crumbling", "crumbling away", "crumbling"},
    { MB_PETRIFYING, "petrifying", "petrifying slowly", "petrifying"},
    { MB_MAD, "mad", "lost in madness", "mad"},
    { MB_FLEEING, "fleeing", "fleeing", "fleeing"},
    { MB_DAZED, "dazed", "dazed", "dazed"},
    { MB_MUTE, "mute", "mute", "mute"},
    { MB_FROZEN, "frozen", "encased in ice", "frozen"},
    { MB_WATER_HOLD, "engulfed", "engulfed", "engulfed"},
    { MB_WATER_HOLD_DROWN, "asphyxiating", "asphyxiating", "asphyxiating"},
    { MB_BURNING, "burning", "covered in liquid flames", "burning"},
    { MB_POISONED, "poisoned", "poisoned", "poisoned"},
    { MB_MORE_POISONED, "very poisoned", "very poisoned", "very poisoned"},
    { MB_MAX_POISONED, "extremely poisoned", "extremely poisoned", "extremely poisoned"},
    { MB_CONTAM_LIGHT, "contam", "contaminated", "contam"},
    { MB_CONTAM_HEAVY, "heavy contam", "heavily contaminated", "heavy contam"},
    { MB_SLOWED, "slow", "slow", "slow"},
    { MB_BREATH_WEAPON, "catching breath", "catching @possessive@ breath", "catching breath"},
    { MB_LOWERED_WL, "weak-willed", "weak-willed", "weak-willed"},
    { MB_FIRE_VULN, "combustible", "more vulnerable to fire", "combustible"},
    { MB_POISON_VULN, "easily poisoned", "more vulnerable to poison", "easily poisoned"},
    { MB_WRETCHED, "misshapen", "misshapen and mutated", "misshapen"},
    { MB_CORROSION, "corroded", "corroded", "corroded"},
    { MB_FLAYED, "flayed", "covered in terrible wounds", "flayed"},
    { MB_GRASPING_ROOTS, "rooted", "constricted by roots", "rooted"},
    { MB_VILE_CLUTCH, "clutched", "constricted by zombie hands", "clutched"},
    { MB_BARBS, "skewered", "skewered by barbs", "skewered"},
    { MB_SICK, "sick", "sick", "sick"},
    { MB_WEAK, "weak", "weak", "weak"},
    { MB_LIGHTLY_DRAINED, "drained", "lightly drained", "drained"},
    { MB_HEAVILY_DRAINED, "very drained", "heavily drained", "very drained"},
    { MB_SAP_MAGIC, "magic-sapped", "magic-sapped", "magic-sapped"},
    { MB_GLOWING, "corona", "softly glowing", "coronas"},
    { MB_WATERLOGGED, "waterlogged", "waterlogged", "waterlogged"},
    { MB_DIMENSION_ANCHOR, "anchored", "unable to translocate", "anchored"},
    { MB_SLOW_MOVEMENT, "struggling", "covering ground slowly", "struggling"},
    { MB_PAIN_BOND, "pain bonded", "sharing @possessive@ pain", "pain bonded"},
    { MB_IDEALISED, "idealised", "idealised", "idealised"},
    { MB_ALLY_TARGET, "ally target", "ally target", "ally target"},
    { MB_ANTIMAGIC, "magic disrupted", "magic disrupted", "magic disrupted"},
    { MB_ANGUISH, "anguished", "anguished", "anguished"},
    { MB_REFLECTING, "reflecting", "reflecting blocked projectiles", "reflecting"},
    { MB_TELEPORTING, "teleporting", "about to teleport", "teleporting"},
    { MB_BOUND, "bound", "bound in place", "bound" },
    { MB_BULLSEYE_TARGET, "bullseye target", "targeted by your dimensional bullseye", "bullseye target" },
    { MB_VITRIFIED, "vitrified", "fragile as glass", "vitrified" },
    { MB_CURSE_OF_AGONY, "agonized", "cursed with the promise of agony", "cursed with agony" },
    { MB_RETREATING, "retreating", "retreating", "retreating"},
    { MB_TOUCH_OF_BEOGH, "divinely empowered", "empowered by the touch of Beogh", "divinely empowered"},
    { MB_AWAITING_RECRUITMENT, "anointable", "ready to become your apostle", "anointable"},
    { MB_VENGEANCE_TARGET, "target of vengeance", "target of orcish vengeance", "targets of vengeance"},
    { MB_MAGNETISED, "magnetised", "covered in magnetic dust", "magnetised"},
    { MB_RIMEBLIGHT, "rimeblight", "afflicted by rimeblight", "rimeblight"},
    { MB_SHADOWLESS, "shadowless", "missing a shadow", "shadowless"},
    { MB_FROZEN_IN_TERROR, "paralysed with fear", "paralysed with fear", "paralysed with fear"},
    { MB_SOUL_SPLINTERED, "soul-splintered", "soul-splintered", "soul-splintered"},
    { MB_ENGULFING_PLAYER, "engulfing you", "engulfing you", "engulfing you"},
    { MB_KINETIC_GRAPNEL, "grapneled", "grapneled", "grapneled"},
    { MB_TEMPERED, "tempered", "tempered", "tempered"},
    { MB_BLINKITIS, "untethered", "untethered in space", "untethered"},
    { MB_CHAOS_LACE, "chaos-laced", "interlaced with chaos", "chaos-laced"},
    { MB_VEXED, "vexed", "lashing out in frustration", "vexed"},
    { MB_PYRRHIC_RECOLLECTION, "ablaze", "ablaze with memories", "ablaze"},
};

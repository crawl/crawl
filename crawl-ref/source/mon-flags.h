#ifndef MON_CLASS_FLAGS_H
#define MON_CLASS_FLAGS_H

#define BIT(x) ((uint64_t)1<<(x))

/// Properties of the monster class (other than resists/vulnerabilities).
enum monclass_flag_type : uint64_t
{
    M_NO_FLAGS = 0,

    /// monster eats doors
    M_EAT_DOORS         = BIT(0),

    /// monster crashes through doors rather than opening
    M_CRASH_DOORS       = BIT(1),

    /// monster flies all the time
    M_FLIES             = BIT(2),

    /// monster is skilled fighter
    M_FIGHTER           = BIT(3),

    /// do not give (unique) a wand
    M_NO_WAND           = BIT(4),

    /// do not give a high tier wand
    M_NO_HT_WAND        = BIT(5),

    /// is created invis
    M_INVIS             = BIT(6),

    /// can see invis
    M_SEE_INVIS         = BIT(7),

    /// can't be blinded
    M_UNBLINDABLE       = BIT(8),

    /// uses talking code
    M_SPEAKS            = BIT(9),

    /// monster is perma-confused,
    M_CONFUSED          = BIT(10),

    /// monster is batty
    M_BATTY             = BIT(11),

    /// monster can split
    M_SPLITS            = BIT(12),

                        //BIT(13), // was M_GLOWS

    /// monster is stationary
    M_STATIONARY        = BIT(14),

    /// monster can smell blood
    M_BLOOD_SCENT       = BIT(15),

    /// susceptible to cold; drainable by vampires, splashes blood when hit
    M_COLD_BLOOD        = BIT(16),

    /// drainable by vampires, splashes blood when hit
    M_WARM_BLOOD        = BIT(17),

    /// uses the 'ghost demon' struct to track stats varying per individual monster
    M_GHOST_DEMON       = BIT(18),

    /// monster digs through rock
    M_BURROWS           = BIT(19),

    /// monster can submerge
    M_SUBMERGES         = BIT(20),

    /// monster is a unique
    M_UNIQUE            = BIT(21),

    /// Passive acid splash when hit.
    M_ACID_SPLASH       = BIT(22),

    /// gets various archery boosts
    M_ARCHER            = BIT(23),

    /// is insubstantial
    M_INSUBSTANTIAL     = BIT(24),

    /// wields two weapons at once
    M_TWO_WEAPONS       = BIT(25),

    /// has extra-fast regeneration
    M_FAST_REGEN        = BIT(26),

    /// cannot regenerate
    M_NO_REGEN          = BIT(27),

    /// uses male pronouns
    M_MALE              = BIT(28),

    /// uses female pronouns
    M_FEMALE            = BIT(29),

    /// boneless corpses
    M_NO_SKELETON       = BIT(30),

    /// worth 0 xp
    M_NO_EXP_GAIN       = BIT(31),

    /// can do damage when attacked in melee
    M_SPINY             = BIT(32),

                        //BIT(33),

    /// not a valid polymorph target (but can be polymorphed)
    M_NO_POLY_TO        = BIT(34),

    /// An ancestor granted by Hepliaklqana
    M_ANCESTOR          = BIT(35),

    /// always leaves a corpse
    M_ALWAYS_CORPSE     = BIT(36),

    /// mostly doesn't try to melee
    M_DONT_MELEE        = BIT(37),

                        //BIT(38), // was M_ARTIFICIAL

    /// can survive without breathing; immune to asphyxiation and Mephitic Cloud
    M_UNBREATHING       = BIT(39),

    /// not fully coded; causes a warning
    M_UNFINISHED        = BIT(40),

    M_HERD              = BIT(41),

    // has a double-sized tile
    M_TALL_TILE         = BIT(42),

    /// can sense vibrations in web traps
    M_WEB_SENSE         = BIT(43),

    /// tries to maintain LOS/2 range from its target
    M_MAINTAIN_RANGE    = BIT(44),

    /// flesh is not usable for making zombies
    M_NO_ZOMBIE         = BIT(45),

    /// cannot be placed by any means, even in the arena, etc.
    M_CANT_SPAWN        = BIT(46),

    /// derived undead can't be randomly generated
    M_NO_GEN_DERIVED    = BIT(47),

                        //BIT(48),

    /// hybridized monster composed of other monster parts
    M_HYBRID            = BIT(49),

                        //BIT(50),

    /// monster is a projectile (just OOD right now)
    M_PROJECTILE        = BIT(51),

    /// monster is an "avatar" (no independent attacks, only support)
    M_AVATAR            = BIT(52),

                        //BIT(53),

    /// monster is a proxy for a charm/conjuration spell (ball lightning, etc.)
    M_CONJURED          = BIT(54),

    /// monster will never harm the player
    M_NO_THREAT         = BIT(55),

    /// wields four weapons at once
    M_FOUR_WEAPONS      = BIT(56),
};
DEF_BITFIELD(monclass_flags_t, monclass_flag_type);

/// Properties of specific monsters.
enum monster_flag_type : uint64_t
{
    MF_NO_FLAGS           = 0,
    /// no benefit from killing
    MF_NO_REWARD          = BIT(0),
    /// monster skips next available action
    MF_JUST_SUMMONED      = BIT(1),
    /// is following player through stairs
    MF_TAKING_STAIRS      = BIT(2),

    //                      BIT(3),

    /// Player has already seen monster
    MF_SEEN               = BIT(4),
    /// A known shapeshifter.
    MF_KNOWN_SHIFTER      = BIT(5),
    /// Monster that has been banished.
    MF_BANISHED           = BIT(6),

    /// Summoned, should not drop gear on reset
    MF_HARD_RESET         = BIT(7),
    /// mirror to CREATED_FRIENDLY for neutrals
    MF_WAS_NEUTRAL        = BIT(8),
    /// Saw player and attitude changed (or not); currently used for holy
    /// beings (good god worshippers -> neutral) orcs (Beogh worshippers
    /// -> friendly), and slimes (Jiyva worshippers -> neutral)
    MF_ATT_CHANGE_ATTEMPT = BIT(9),
    /// Was in view during previous turn.
    MF_WAS_IN_VIEW        = BIT(10),

    /// Created as a member of a band
    MF_BAND_MEMBER        = BIT(11),
    /// Monter has been pacified
    MF_PACIFIED           = BIT(12),
    /// Consider this monster to have MH_UNDEAD holiness, regardless
    /// of its actual type
    MF_FAKE_UNDEAD        = BIT(13),
    /// An undead monster soul enslaved by
    /// Yredelemnul's power
    MF_ENSLAVED_SOUL      = BIT(14),

    /// mname is a suffix.
    MF_NAME_SUFFIX        = BIT(15),
    /// mname is an adjective (prefix).
    MF_NAME_ADJECTIVE     = BIT(16),
    /// mname entirely replaces normal monster name.
    MF_NAME_REPLACE       = MF_NAME_SUFFIX|MF_NAME_ADJECTIVE,
    /// Is a god gift.
    MF_GOD_GIFT           = BIT(17),
    /// Is running away from player sanctuary
    MF_FLEEING_FROM_SANCTUARY = BIT(18),
    /// Is being killed with disintegration
    MF_EXPLODE_KILL       = BIT(19),

    // These are based on the flags in monster class, but can be set for
    // monsters that are not normally fighters (in vaults).

    /// Monster is skilled fighter.
    MF_FIGHTER            = BIT(20),
    /// Monster wields two weapons.
    MF_TWO_WEAPONS        = BIT(21),
    /// Monster gets various archery boosts.
    MF_ARCHER             = BIT(22),

                          //BIT(23),
                          //BIT(24),
                          //BIT(25),

    /// This monster cannot regenerate.
    MF_NO_REGEN           = BIT(26),

    /// mname should be treated with normal grammar, ie, prevent
    /// "You hit red rat" and other such constructs.
    MF_NAME_DESCRIPTOR    = BIT(27),
    /// give this monster the definite "the" article, instead of the
    /// indefinite "a" article.
    MF_NAME_DEFINITE      = BIT(28),
    /// living monster revived by a lost soul
    MF_SPECTRALISED       = BIT(29),
    /// is a demonic_guardian
    MF_DEMONIC_GUARDIAN   = BIT(30),
    /// mname should be used for corpses as well
    MF_NAME_SPECIES       = BIT(31),
    /// mname replaces "zombie" etc.; use only for already zombified monsters
    MF_NAME_ZOMBIE        = BIT(32),
    /// Player has been warned about this monster being nearby.
    MF_SENSED             = BIT(33),
    /// mname should not be used for corpses
    MF_NAME_NOCORPSE      = BIT(34),
    /// known to have a ranged attack
    MF_SEEN_RANGED        = BIT(35),

    /// this monster has been polymorphed.
    MF_POLYMORPHED        = BIT(36),
    /// just got hibernated/slept
    MF_JUST_SLEPT         = BIT(37),
    /// possibly got piety with TSO
    MF_TSO_SEEN           = BIT(38),
};
DEF_BITFIELD(monster_flags_t, monster_flag_type);

constexpr monster_flags_t MF_NAME_MASK = MF_NAME_REPLACE;
constexpr monster_flags_t MF_MELEE_MASK = MF_FIGHTER | MF_TWO_WEAPONS
                                        | MF_ARCHER;
#endif

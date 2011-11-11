------------------------------------------------------------------------------
-- tags.lua:
-- Tag and version compatibility functions and info.
------------------------------------------------------------------------------
tags = {}

------------------------------------------------------------------------------
-- simpler access to tag_major_version
------------------------------------------------------------------------------

function tags.major_version ()
  return file.major_version()
end

tags.TAG_MINOR_INVALID = -1
tags.TAG_MINOR_RESET = 0
tags.TAG_MINOR_DETECTED_MONSTER = 1
tags.TAG_MINOR_FIRING_POS = 2
tags.TAG_MINOR_FOE_MEMORY = 3
tags.TAG_MINOR_SHOPS = 4
tags.TAG_MINOR_MON_TIER_STATS = 5
tags.TAG_MINOR_MFLAGS64 = 6
tags.TAG_MINOR_ENCH_MID = 7
tags.TAG_MINOR_CLOUD_BUG = 8
tags.TAG_MINOR_MINFO_PROP = 9
tags.TAG_MINOR_MON_INV_ORDER = 10
tags.TAG_MINOR_ASH_PENANCE = 11
tags.TAG_MINOR_BOOK_BEASTS = 12
tags.TAG_MINOR_CHR_COMPAT = 13
tags.TAG_MINOR_ZOT_POINTS = 14
tags.TAG_MINOR_TRAP_KNOWLEDGE = 15
tags.TAG_MINOR_NEW_HP = 16
tags.TAG_MINOR_HP_MP_CALC = 17
tags.TAG_MINOR_64_MB = 18
tags.TAG_MINOR_SEEN_MISC = 19
tags.TAG_MINOR_SPECIES_HP_NO_MUT = 20
tags.TAG_MINOR_GOLDIFIED_RUNES = 21
tags.TAG_MINOR_ZIG_COUNT = 22
tags.TAG_MINOR_ZIG_FIX = 23
tags.TAG_MINOR_XP_POOL_FIX = 24
tags.TAG_MINOR_DECK_RARITY = 25
tags.TAG_MINOR_PIETY_MAX = 26
tags.TAG_MINOR_CHR_NAMES = 27
tags.TAG_MINOR_SKILL_TRAINING = 28
tags.TAG_MINOR_FOCUS_SKILL = 29
tags.TAG_MINOR_MANUAL = 30
tags.TAG_MINOR_MONSTER_TILES = 31
tags.TAG_MINOR_SKILL_MENU_STATES = 32
tags.TAG_MINOR_MONS_THREAT_LEVEL = 33
tags.TAG_MINOR_SPELL_USAGE = 34
tags.TAG_MINOR_UNIQUE_NOTES = 35
tags.TAG_MINOR_NEW_MIMICS = 36
tags.TAG_MINOR_POLEARMS_REACH = 37
tags.TAG_MINOR_ABYSS_STATE = 38
tags.TAG_MINOR_ABYSS_SPEED = 39
tags.TAG_MINOR_UNPONDERIFY = 40
tags.TAG_MINOR_SKILL_RESTRICTIONS = 41
tags.TAG_MINOR_NUM_LEVEL_CONN = 42
tags.TAG_MINOR_FOOD_MUTATIONS = 43
tags.TAG_MINOR_DISABLED_SKILLS = 44
tags.TAG_MINOR_CHERUB_ATTACKS = 45
tags.TAG_MINOR_FOOD_MUTATIONS_BACK = 46
tags.TAG_MINOR_TEMPORARY_CLOUDS = 47
tags.TAG_MINOR_PHOENIX_ATTITUDE = 48
tags.NUM_TAG_MINORS = 49
tags.TAG_MINOR_VERSION = 48


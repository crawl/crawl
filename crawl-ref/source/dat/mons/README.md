This directory contains the files from which mon-data.h is generated; that is,
the home of core, shared monster definitions. Other logic (specific to given
monsters) is scattered throughout the codebase.

Below is a list of all accepted fields. Required fields are marked with a ✨.
Relevant enums are noted with a (filename ➡ enum).

- ✨ac✨ (int): The monster's AC (armour class).
- ✨attacks✨ (list of `attack` entries): The monster's melee attacks. See
  the description of 'attack entries', below.
- corpse_tile (string): The monster's corpse's sprite. If the monster should
  have a corpse - `has_corpse` is `true`, `species` == `enum`, "cant_spawn" not
  in `flags` - then this defaults to `tile` (if set to anything but
  "program_bug"), or to `enum` otherwise.
  The enum to which this refers is generated from `rltiles/dc-corpse.txt`,
  and, as other enum-based fields, the `TILE_CORPSE_` prefix should be omitted.
- energy (associative array of string to int mappings): The energy the monster
  takes for various types of actions. Lower is faster.
  "move" is a shorthand which sets both `EUT_WALK` and `EUT_SWIM`.
  (energy-use-type.h ➡ energy_use_type).
- enum (string): The monster's enum. Defaults to `name`.
  (monster-type.h ➡ monster-type).
- ✨ev✨ (int): The monster's EV (evasion).
- flags (list of strings): An unordered list of special properties.
  (mon-flags.h ➡ monclass_flag_type)
- genus (string): The genus to which this monster's species belongs. Mainly
  used for some corner cases of monster behavior. Defaults to `species`.
  (monster-type.h ➡ monster-type).
- ✨glyph✨ (`glyph` entry): See below.
- god (string): The god the monster worships. Defaults to "no god".
  (god-type.h ➡ god_type)
- habitat (string): The monster's native terrain type. Defaults to "land".
  (mon-enum.h ➡ habitat_type)
- ✨has_corpse✨ (bool): Whether the monster leaves a corpse behind on death.
  Defaults to false.
- ✨hd✨ (int): The monster's 'hit dice', or level. Affects many small things.
- holiness (string): The grand category to which the monster belongs.
  Defaults to "natural".
  (mon-holy-type.h ➡ mon_holy_type_flags).
- ✨hp_10x✨ (int): Ten times the monster's average hit points. (That is, a
  monster with this set to 100 will have 10 HP on average.) Individual monsters
  vary by up to +-33% (usually much less), to discourage players from trying to
  guess exactly how much HP they have left.
- ✨intelligence✨ (string): Affects a variety of small monster AI quirks.
  (mon-enum.h ➡ mon_intel_type)
- ✨name✨ (string): The monster's name.
- resists (associative array of string to int mappings): The monster's
  elemental and other resistances. Note that monsters also often gain further
  resistances from their holiness.
- ✨shape✨ (string): The form of the monster's physical body, or lack thereof.
  (mon-enum.h ➡ mon_body_shape)
- shout (string): The way in which this species shouts when alerted.
  Defaults to "silent".
  (mon-enum.h ➡ shout_type).
- ✨size✨ (string): How big or small the monster is, relative to a human.
  (size-type.h ➡ size_type).
- species (string): The species to which this monster belongs. Mainly used for
  corpses. Defaults to `enum`.
  (monster-type.h ➡ monster-type).
- speed (int): The monster's speed for all action types. Higher is faster.
  Defaults to 10.
- spells (string): The enum for a spellbook in mon-mst.h. Note that the enum is
  generated from the set of values defined in these monster files - just list
  an enum in one or more monster files and add an entry in mon-mst.h, and it'll
  all work.
- tile (string): The monster's base sprite. Defaults to `enum`.
  If the value of `tile` does *not* begin with "tile", then "`tilep_mons_`" is
  prepended to it.
  The enums to which this refers are generated from `rltiles/` (`dc-mon.txt`,
  etc).
- tile_variance (string): How the monster's sprite should vary - with time, in
  water, etc. Defaults to no variance.
  (mon-util.h ➡ mon_type_tile_variation)
- uses (string): The monster's ability to manipulate items and terrain.
  Defaults to "nothing".
  (mon-enum.h ➡ mon_itemuse_type)
- will (int): The monster's Willpower, or "invuln".
  Exactly one of this and `will_per_hd` must be set.
- will_per_hd (int): A multiplier for the monster's `hd` to get its Willpower.
  Exactly one of this and `will` must be set.
- xp_mult (int): A multiplier for the monster's XP value, in addition to the
  base XP derived from the monster's HP, speed, etc. Defaults to 10.

`attack` entries are lists of associative arrays, each of which may have the
following fields:

- ✨type✨ (string): The description of the attack. Largely cosmetic, but note
  that only `hit` and `weap_only`-type attacks can use weapons.
  (mon-enum.h ➡ attack_type)
- ✨damage✨ (int): The maximum damage that the attack can do, not including
  weapons, special effects, etc.
- flavour: (string): Special effects associated with the attack.
  Defaults to `none`.
  (mon-enum.h ➡ attack_flavour)

`glyph` entries are associative arrays with the following fields:

- ✨char✨ (char): The ASCII character which represents this monster in console
  versions of the game.
- ✨colour✨ (string): The colour for this monster in console versions of the
  game. Should be either one of the standard 16 console colours or one of our
  elemental colours.
  (defines.h ➡ COLOURS, or colour.h ➡ element_type)

Monsters may have at most four attacks.

Note that any fields which refer to enums should be lower-case, and should not
include the prefix for the enum. For example, one would specify
`enum:will_o_the_wisp`, not `enum:MONS_WILL_O_THE_WISP`.

For information on creating monsters, please see [monster_creation.txt].

[monster_creation.txt]: ../../../docs/develop/monster_creation.txt

# Guidelines for specific types of vaults

1. [Guidelines for D:1 arrival vaults](#guidelines-for-d1-arrival-vaults)
2. [Guidelines for faded altar and overflow temples](guidelines-for-faded-altar and-overflow-temples)
3. [Guidelines for creating serial vaults](#guidelines-for-creating-serial-vaults)
4. [Guidelines for creating ghost vaults](#guidelines-for-creating-ghost-vaults)
5. [Guidelines for no_tele_into](#guidelines-for-no_tele_into)

## Guidelines for D:1 arrival vaults

Arrival vaults should provide atmosphere and a nice starting point. The idea
is neither to get a grip on most of D:1 nor to hand out starting gear.
Playing Crawl a bit will show you a number of arrival vaults, which live in
[crawl-ref/source/dat/des/arrival/](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/arrival)

### We need more arrival vaults!

We love new arrival vaults, and it is easy to make new ones. Coding abilities
are not required. If you are completely new to making maps, have a look at
simple.des. After that, glance through
[crawl-ref/docs/develop/levels/introduction.txt](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/develop/levels/introduction.txt),
which will allow you to make
your own maps already! Once you are familiar with this, you may look at more
complicated vaults and read the more
[advanced documentation](https://github.com/crawl/crawl/tree/master/crawl-ref/docs/develop/levels).
Crawl also employs vaults in many other places; check the various `.des` files
in
[crawl-ref/source/dat/des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des)
for what is going on. New maps are always welcome.

### Some guidelines for arrival vault creators

* Try to come up with small maps.
* Avoid placing guaranteed items.
* The same holds for any features which might trigger start-scumming, like
  altars.
* Also avoid ASCII art. It probably won't work in Tiles anyway.
* Also avoid monsters-behind-glass-wall vaults. We already have enough of these.
* Note that at the start of the game, the dungeon builder removes all
  monsters in view of the player. Zero experience monsters like plants
  are exempt.
* Arrival vaults should have multiple entry points, escape hatches, or enough
  space to permit tactics.

### Naming conventions

Entry vaults should be NAMEd as follows:

``` NAME:  nick_arrival_name ```

where "nick" is the name/nick of the vault designer and "name" is a somewhat
descriptive label for the vault. Avoid numbers, please.

The actual maps can be found in the following files:

| File        |  Contents |
|-------------|--------------------------------|
| simple.des  | Particularly simple maps: no more than five header lines! |
| small.des   | Small maps with neither lua nor complicated syntax. |
| twisted.des | Maps which either use a lot of lua or heavy `SUBST`ing etc. |
| large.des   | Sizes of about 30x30 and up. |

If a map is both big and complex, file under twisted.des.
If a map is both plain and small, file under simple.des.

## Guidelines for faded altar and overflow temples

A faded altar vault is placed with very high probability on D:1-3.
Overflow temples are specific vaults used by the dungeon builder to guarantee
altars to temple gods that did not get an altar in the Ecumenical Temple. In
general these should avoid placing significant challenges and leave the altar
accessible without need for flight or blinks, as the purpose is to provide the
player with a guaranteed altar. Both classes of vaults are seen often in the
game and we need a lot of them! These maps are good candidates for a starting
vaultmaker who wants to try something other than an arrival vault.

For overflow temples, see the comments at the start of
[crawl-ref/source/dat/des/altar/overflow.des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/altar/overflow.des) for the specific tagging scheme to use for overflow temples, either generic or devoted to particular gods.

Ecumenical altars are faded, so the player can't tell which god the altar is
devoted to until they worship. Good lore ideas revolve around ruins and decay,
but there are plenty of other creative possibilities. The vaults are in
[crawl-ref/source/dat/des/altar/ecumenical.des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/altar/ecumenical.des)

### General style guidelines for faded altar and overflow temples

* Try to come up with small maps.
* Avoid placing guaranteed items.
* Don't use out-of-depth monsters or a large number of in-depth monsters.
* Avoid ASCII art. It probably won't work in Tiles anyway.

### General style guidelines for overflow temples

* If your vault places a single specific altar and is a plausible mini vault
  tag it `uniq_altar_GODNAME` as well as the overflow temple tags
* Whenever possible, don't add a depth specification to such a vault.
* If a specific monster is necessary for theme, then constrain the vault to
  that monster's depth.
* If your vault is decor (even if it does not have the decor tag, if there
  are no depth-scaling monsters to fight, and no serious loot),
  include `: interest_check(_G)` to ensure the vault is tagged appropriately.

## Guidelines for creating serial vaults

Sometimes, we want flavour (or also other) vaults to be placed several times,
so as to make the level feel more coherent; this is called a **serial vault**.
The lua calls for such serial vaults are in the files in the folder
[crawl-ref/source/dat/des/serial/](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/serial).
Each serial vault has its own file.

The vaults eventually could be just copies of one map, or (much better) come
from a list of thematically linked maps. This is mostly for flavour, although
it may be interesting to created serial vaults around threats.

In general, we don't want more than one serial vault on a level. Use the

``` TAGS: luniq_serial ```

in the serial vault's header map for this.

Often, it will be fine to allow the individual maps comprising a serial vault
to be chosen by the level builder as usual minivaults. If you want to do that,
there are two ways to proceed:

* A default-depth line before all maps.
* Or non-empty `DEPTH` statements in each map definition. (You can still do
* this even if there is a default-depth. It allows you tweak the depths for
  specific maps.)

If you want to disable all or some the maps for from normal minivault
placement, you can:

* Give them no `DEPTH` line when you have no preceding default-depth.
* Explicitly remove them with an empty depth line like this:

``` DEPTH: ```

## Guidelines for creating ghost vaults

FIXME: Most of this whole file will be needed to be updated for Necropolis and
various attempts to establish certain house-styles of branch identity or
header order. This is a quick, skeletal write-up for now to avoid outright
wrong advice while the changeover is being done.

Ghost vaults are subvaults for Necropolis that place a player ghost, which is an
undead monster with properties derived from a player who died. These vaults
should place some amount of additional loot to offer some enticement for players
where the XP from killing the ghost isn't sufficient reward. Often these vaults
place monsters in addition to the ghost. Ghost vaults usually place one ghost,
but may less often place multiple ghosts.

### Organization and setup

All ghost vaults should go in
[crawl-ref/source/dat/des/portals/necropolis.des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/portals/necropolis.des)
and call the lua ghost setup function the following way:

``` : ghost_setup(_G) ```

This well set the common tags we want for all ghost vaults.

### Basic guidelines

Ghost vaults should always be sealed and ideally not diggable by monsters that
have a dig wand or the dig spell. This means the vault should have non-rock
outer walls and use either runed doors or transporters for entry. The vaults
should have transparent stone walls so that it's always possible for the player
to see the ghost before entering, even if the ghost is awake and wandering.
Ghost vaults should only be 19x19 or 20x20, preferrably the latter.

It's strongly recommended to place at least some loot in early levels, and
strongly recommended for later depths where ghost XP provides very little
incentive on its own. It's fine to place monsters in addition to the ghost
monsters; just keep in mind that player ghosts are relatively difficult to kill
compared to a large majority of crawl monsters. See the aforementioned
[necropolis.des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/portals/necropolis.des)
for examples and inspiration.

### Distribution

The current range of levels that ghost subvault tags correspond to:

```
"pre_temple_d":  D:4-7
"pre_lair_d":    D:8-11
"post_lair_d":   D:12-15
"lair":          Lair:1-5
"orc":           Orc:1-2
"snake":         Snake:1-3
"swamp":         Swamp:1-3
"shoals":        Shoals:1-3
"spider":        Spider:1-3
"vaults_elf":    Vaults:1-4, Elf:1-2
"depths_crypt":  Depths:1-4, Crypt:1-3
```

To make a ghost vault relatively more or less commonly chosen among the set of
ghost vaults, use a WEIGHT statement to set a weight other than 10, the
default. A vault can use WEIGHT to set different weights for different
branches/levels. See examples in
[necropolis.des](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/portals/necropolis.des)
and
[syntax.txt](https://github.com/crawl/crawl/tree/master/crawl-ref/docs/develop/levels/syntax.txt).

### Custom Tags

If you use `allow_dup` in your vault, also use `luniq` or
`luniq_necropolis_ghost_minimal` tags to avoid multiple vault placements on the
same Necropolis.

## Guidelines for no_tele_into

The `no_tele_into` KPROP prevents teleports landing you on the tagged locations.

Example:

```
NAME:  example_vault
KPROP: - = no_tele_into
SUBST: - = .
MAP
xxxxxx
+.m--x
xxxxxx
ENDMAP
```

Teleports will never land the player behind the glass wall.

Don't overuse this property. It's a hidden mechanic not exposed to the player.

Good places to use `no_tele_into`:

* Vaults which need the player to enter in a controlled manner to understand/enjoy. For example `gammafunk_steamed_eel`.
* Teleport closets: areas the player cannot escape without a scroll of teleportation (or similar). For example `lemuel_altar_in_water`.
* Egregiously dangerous/unfair situations. For example `chequers_guarded_unrand_ignorance` (four orange crystal statues).

Bad places to use `no_tele_into`:

* Any old runed door / transporter vault. It's fine for players to teleport into tough or scary situations.
* Islands: areas the player can also reach with flight or similar tools. `no_tele_into` would be an incomplete solution. It's better to place a hatch/shaft, which solves all cases.

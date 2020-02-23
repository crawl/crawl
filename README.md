# Coffee Crawl fork

Crawl is won by lair. Therefor, coffee crawl is the first ten floors of crawl, then one floor of Zot.

The goal of this game is to make a 15-30 minute crawl.

## Upcoming changes

Core changes 

- [x] remove lair, orc, vaults, depths
- [x] shorten dungeon to 10 levels
- [x] temple always appears on level 2
- [x] remove portal branches besides bazaar
- [x] actually remove depths
- [x] remove drop restrictions for D1 to D4 to avoid too many 'boring' floors
- [x] remove things like adders from D:10 since the game is almost over by D:10
- [x] map zot enemies to dungeon enemies in random gen (for now)
- [x] remove abyss access via banish
- [x] remove abyss access via distortion weapons
- [x] remove abyss access via starting role
- [x] remove abyss access via god abilty
- [x] remove abyss access via distortion unwielding, replace with maligned_gateway
- [x] remove abyss access via Zot trap, replace with maligned_gateway
- [x] remove abyss access via miscast, replace with maligned_gateway
- [ ] revert mapping zot enemies to dungeon enemies
- [ ] rebalance zot enemies for a 10 level crawl
- [ ] to make orb run easier, nerf or remove panlords
- [ ] to make orb run easier, nerf orb run enemies to easier ones
- [ ] end the game when the player exits the realm of zot with the orb. This is to avoid players spending 10 minutes predigging tunnels in a now 25 minute game.

## Extended changes

Changes I would like to try, ala hellcrawl.

- [ ] remove dexterity
- [ ] remove bows
- [ ] remove upstairs
- [ ] remove food
- [ ] remove strength and intelligence
- [ ] remove id minigame

## Bad ideas

Changes I would like to try which are probably bad ideas.

- [ ] give every enemy in the game the briar patch ability thorn hunters have to prevent luring
- [ ] remove all spells above level 6 probably. bad idea because i don't know they aren't still interesting yet
- [ ] rebalance skillcap. bad idea because then i have to rebalance everything
- [ ] either flat increase piety gain or decrease piety cap. bad idea because i don't know if there are balance issues yet

## Monster changes

To make Zot easier, I am going to change the following monsters to be weaker.

| Done | Monster               | Monster Enum               | Change |
| ---- | --------------------- | -------------------------- | ------ |
|      | MOTH_OF_WRATH         | MONS_MOTH_OF_WRATH         |        |
|      | GHOST_MOTH            | MONS_GHOST_MOTH            |        |
|      | BLACK_DRACONIAN       | MONS_BLACK_DRACONIAN       |        |
|      | YELLOW_DRACONIAN      | MONS_YELLOW_DRACONIAN      |        |
|      | GREEN_DRACONIAN       | MONS_GREEN_DRACONIAN       |        |
|      | PURPLE_DRACONIAN      | MONS_PURPLE_DRACONIAN      |        |
|      | RED_DRACONIAN         | MONS_RED_DRACONIAN         |        |
|      | WHITE_DRACONIAN       | MONS_WHITE_DRACONIAN       |        |
|      | DRACONIAN_STORMCALLER | MONS_DRACONIAN_STORMCALLER |        |
|      | DRACONIAN_MONK        | MONS_DRACONIAN_MONK        |        |
|      | DRACONIAN_SHIFTER     | MONS_DRACONIAN_SHIFTER     |        |
|      | DRACONIAN_ANNIHILATOR | MONS_DRACONIAN_ANNIHILATOR |        |
|      | DRACONIAN_KNIGHT      | MONS_DRACONIAN_KNIGHT      |        |
|      | DRACONIAN_SCORCHER    | MONS_DRACONIAN_SCORCHER    |        |
|      | QUICKSILVER_DRAGON    | MONS_QUICKSILVER_DRAGON    |        |
|      | SHADOW_DRAGON         | MONS_SHADOW_DRAGON         |        |
|      | STORM_DRAGON          | MONS_STORM_DRAGON          |        |
|      | GOLDEN_DRAGON         | MONS_GOLDEN_DRAGON         |        |
|      | KILLER_KLOWN          | MONS_KILLER_KLOWN          |        |
|      | DEATH_COB             | MONS_DEATH_COB             |        |
|      | CURSE_TOE             | MONS_CURSE_TOE             |        |
|      | TENTACLED_MONSTROSITY | MONS_TENTACLED_MONSTROSITY |        |
|      | ELECTRIC_GOLEM        | MONS_ELECTRIC_GOLEM        |        |
|      | ORB_OF_FIRE           | MONS_ORB_OF_FIRE           |        |

Good monsters to compare the above to?

## References

[DCSS README](https://github.com/crawl/crawl)

# Guidelines for specific types of vaults

1. [Guidelines for D:1 arrival vaults](#guidelines-for-d1-arrival-vaults)
2. [Guidelines for creating serial vaults](#guidelines-for-creating-serial-vaults)

## Guidelines for D:1 arrival vaults

Arrival vaults should provide atmosphere and a nice starting point. The idea
is neither to get a grip on most of D:1 nor to hand out starting gear.
Playing Crawl a bit will show you a number of arrival vaults, which live in
[crawl-ref/source/dat/des/arrival/](https://github.com/crawl/crawl/tree/master/crawl-ref/source/dat/des/arrival)

**We need more arrival vaults!**

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

**Some guidelines for arrival vault creators:**

* Try to come up with small maps.
* Avoid placing guaranteed items.
* The same holds for any features which might trigger start-scumming, like
  altars.
* Also avoid ASCII art. It probably won't work in Tiles anyway.
* Also avoid monsters-behind-glass-wall vaults. We already have enough of these.
* Note that at the start of the game, the dungeon builder removes all
  monsters in view of the player. Zero experience monsters like plants
  are exempt.

**Naming conventions**

Entry vaults should be NAMEd as follows:

    NAME:  nick_arrival_name

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

    TAGS: luniq_serial

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

      DEPTH:

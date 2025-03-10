Crawl Quick-Start Guide
=======================

Copyright 1999 Linley Henzell


So, you want to start playing Crawl straight away without bothering with the
manual? Read this, the guide to starting Crawl with a minimum of preparation.

I suggest printing it out and following its instructions while playing your
first few games (you can also press `?` twice while playing for a list of
commands). You should also try the tutorial mode; it only takes a few
minutes.

When you get some more time, you can read `crawl_manual.txt` in the docs/
directory for more detailed information.

INTRODUCTION TO CRAWL
---------------------

Crawl is a large and very random game of subterranean exploration in a fantasy
world of magic and frequent violence. Your quest is to travel into the depths
of the Dungeon (which is different each time you play) and retrieve the Orb of
Zot.

Crawl is an RPG of the `rogue-like` type, one of the descendants of Rogue. Its
graphics are simple but highly informative, designed to be understood at a
glance, and control is exercised largely through one-keystroke commands.

STARTING OUT
------------

After starting the program you will be greeted with a message asking for your
name. Don't spend too much time over this, as your first character will **not**
last very long (sorry, but it's true).

Next you are given menus of species and backgrounds from which to choose.
A mountain dwarf, minotaur or troll Fighter is a good bet. Elves are quite
fragile, humans are pretty average at everything, and the weirder species are
mostly too tricky for beginning players. Finally, you may be given a choice of
weapons. I suggest an axe (axes are fun).

Now you are in the game. The game screen has three parts:

 - the Map takes up the upper left part of the screen. In its very centre is
   the `@` sign (or player doll tile) which represents You. The bright parts of
   the Map are the parts you can see, while places which you have visited 
   before but cannot currently see are darkened.
 - the Message box is the large part of the screen below the map. It describes
   events as they happen and asks you questions from time to time.
 - the Stats area (to the right of the Map) contains various indicators of
   your health and abilities.

EXPLORING
---------

Try walking around, using either the numeric keypad (try numlock off and on) or
the Rogue (`hjklyubn`) keys. Pressing `o` will activate auto-explore, which will
move until an enemy is seen, or some other notable event (e.g. finding a set of
stairs) occurs.

If you want to know what a certain character on the screen represents, you can
use the `x` (examine) command to get a short description, and then `v` to get
more information. You use the `O`, `C` commands to open or close doors, and the
`<` (up) and `>` (down) commands to climb staircases.

The Dungeon gets more dangerous (but more interesting!) as you go down. If you
get lost you can access a map of the whole level you are on with the `X`
command, which uses the whole screen.

ITEMS
-----

After walking around for a while, you will no doubt come across some items
lying around (you may come across some monsters as well; for help in dealing
with them skip to the Monsters section). You can pick up items with the `g`
(get) or `,` commands and drop them again with `d` (drop), and the `i`
(inventory) command shows you what you're carrying. While examining your
inventory, press any item's key to see more information about it, and to list
available commands.

There are several different types of items:

 - **Weapons**, represented by the `)` sign. Wield them with the `w` (wield)
   command.
 - **Armour** (`[`). Wear it, or take it off, with the `W` (Wear) command.
   Heavier armours give more protection, but may hamper your ability to dodge,
   cast spells, or fire ranged weapons.
 - **Ammunition** (which has the `(` sign). Throw it with `f` and `F` (fire).
 - **Wands** (`/`), **Scrolls** (`?`) and **Potions** (`!`) can be very
   valuable, but have limited uses (scrolls and potions can only be used once
   each, wands contain only a certain number of charges). Wands are e`V`oked,
   scrolls are `r`ead and potions are `q`uaffed. Unfortunately, you won't at
   first know what a scroll or potion does; it will only be described by its
   physical appearance. But once you have used, for example, a potion of curing,
   you will in the future recognise all potions of curing.
 - **Rings** (`=`) and **Amulets** (`"`) provide various mostly-helpful effects.
   They are put on with `P` (put on) and can be removed with `R`.
 - **Money** (`$`) can be used to buy stuff in shops.

There are a few other types of items, but you will discover these as you play.

MONSTERS
--------

You will also run into monsters (most of which are represented by letters of
the alphabet). You can attack a monster by trying to move into the square it
is occupying. Pressing Tab will automatically move toward & attack nearby
monsters.

When you are wounded you lose hit points (displayed near the top of the stats
list); these return gradually over time through the natural process of
healing. If you lose all of your hp you die.

To survive, you will need to develop a few basic tactics:

 - Rest between encounters. The `s`, `.`, delete or keypad-5 commands make you
   rest for one turn, while pressing `5` or Shift-and-keypad-5 make you rest
   for a longer time (you will stop resting when fully healed).
 - Never fight more than one monster if you can help it. Always back into a
   corridor so that they must fight you one-on-one.
 - Consumable items like scrolls and potions won't do you any good when you're
   dead, and most are more effective when used at the start of a tough fight
   than when you're near death. Try identifying some ahead of time by 'wasting'
   one from a stack while in a safe spot.
 - Remember to use projectiles before engaging monsters in close combat.
 - Learn when to run away from things you can't handle - this is important!
   It is often wise to skip a dangerous level. But don't overdo this.

DEATH
-----

Before long, you'll probably end up dead.

Death in Crawl is permanent; you cannot just reload a saved game and start
again where you left off. The `S` (save) command exists only to let you leave
a game part-way through and come back to it later. Quitting (`Ctrl-Q`) lets
you abandon a character if you can't even be bothered to help them escape alive.

Well, that's it for the quick-start guide. This should help you through your
first few games, but Crawl is extremely (some would say excessively) complex
and cannot be adequately described in so short a document. So when you feel
ready to start playing with magic, skills, and religions, browse the manual.

Happy Crawling!

Dungeon Crawl Stone Soup
========================

Dungeon Crawl Stone Soup is a game of dungeon exploration, combat and magic, 
involving characters of diverse skills, worshipping deities of great power and 
caprice. To win, you'll need to be a master of tactics and strategy, and 
prevail against overwhelming odds.

Players of versions 0.3.4 and older beware: please read the file
   034_changes.txt
in the docs/ directory for a list of the interface changes, and how 
you could possibly retrieve the 0.3.4 standards.

Contents:
1. How to get started? (Information for new players)
2. The file system of this version.
3. Contact and reporting bugs.
4. License and history information.
5. How you can help!

1. Getting started
------------------
If you'd like to dive in immediately, your best bets are to
* start up a game and choose a tutorial (press T when asked for race), or
* read quickstart.txt (it is in docs/), or
* for studious readers, browse the manual (see below for all doc files).

Additionally, you may want to print out the file keys.pdf from the docs/
folder. Besides a full list of command keys (don't bother with it), it contains 
two pages of help for new players.
Note that you can read quickstart.txt and the manual when playing; pressing ? 
brings up a menu for that.


2. File system
--------------
The following files in the Crawl's main folder are essential:

* crawl           These start the game. (The actual name depends on your
* crawl.exe       operating system.)

* init.txt        These contain the options for the game. The defaults
* .crawlrc        play well, so don't bother with this in the beginning.
                   Permanent death is not an option, but a feature!

* macro.txt       Playing Crawl can be made even more convenient by
                   redefining keys and assigning macros. Ignore early on.

The docs/ folder contains the following helpful texts (all of which can be 
read in-game by bringing up the help menu with '?'):

* the_manual.txt  The complete manual; describing all aspects in the
                   detail. Contains appendices on species, classes, etc.
* options.txt     Describes all options in detail. The structure of
                   init.txt follows this text.
* macros.txt      A how-to on using macros and keymappings, with examples.
* aptitudes.txt   Some numbers defining certain aspects of the races.
                   Helpful, but not needed for winning.
* quickstart.txt  A short introduction for new players.
* ssh_guide.txt   An elaborate introduction on how to get internet play
                   to work. For Windows only.
* irc_guide.txt   An elaborate introduction on how to access the IRC
                   channel ##crawl.
* keys.pdf        A printable document, listing all commands and it also
                   contains a very short guide for new players.


3. Contact and reporting bugs
-----------------------------
The official webpage is
      http://crawl-ref.sourceforge.net/
and there you can find both trackers to add bug reports, feature requests, or 
upload patches as well as sources and binaries. This is the best way to report 
bugs or mention new ideas.

There is a Usenet newsgroup dealing with roguelikes, including Crawl:
      rec.games.roguelike.misc
It is polite to flag your post with -crawl- as other games are discussed over 
there as well. This is a good place to ask general questions, both from new 
players as well as for spoilers, or to announce spectacular wins.

You can play Crawl online, together with many others. The main server has its 
homepage at
      http://crawl.akrasiac.org/
where you can also read how to connect. That page also has links to spoiler 
sites etc.

If you want to chime in with development, you can read the mailing list
      crawl-ref-discuss@lists.sourceforge.net
which can get pretty busy on the occasion.


4. License and history information
----------------------------------
What you have downloaded is a successor to Linley's Dungeon Crawl. Development 
of the main branch stalled at version 4.0.26, with a final alpha of 4.1 being 
released by Brent Ross in 2005. Since 2006, the Dungeon Crawl Stone Soup 
team continues the development.

Dungeon Crawl Stone Soup is an open source, freeware roguelike. It is supported 
on Linux, Windows, OS/X  and, to a lesser extent, on DOS. The source should 
compile and run on any reasonably modern Unix.
Stone Soup features both ASCII and Tiles display.

Crawl gladly and gratiously uses the following open-source packages:
* The Lua script language, see /docs/lualicense.txt.
* The PCRE library for regular expressions, see /docs/pcre-license.txt.
* The Mersenne Twister for random number generation, /docs/mt19937.txt.
* The SQLite library as database enging; it is properly public domain.
* The ReST light markup language for the documentation.

5. How you can help
-------------------
If you like the game and you want to help making it better, there are a number 
of ways to do so:

* Playtesting.
At any time, there will be bugs -- playing and reporting these is a great help. 
There is a beta server around hosting the most recent version of the current 
code; the akrasiac page links to it. Besides finding bugs, ideas on how to 
improve interface or gameplay are welcome as well.

* Vault making.
Crawl uses many hand-drawn (but often randomised) maps. Making them is fun and 
easy. It's best to start with simple entry vaults (glance through 
dat/entry.des for a first impression). Later, you may want to read 
docs/level-design.txt for the full power. If you're ambitious, new maps for 
branch ends are possible, as well.
If you've made some maps, you can test them on your system (no compiling 
needed) and then just mail them to the mailing list.

* Speech.
Monster talking provides a lot of flavour. Just like vaults, speech depends 
upon a large set of entries. Since most of the speech has been outsourced, you 
can add new prose. The syntax is a slightly strange, so you may want to read 
docs/monster_speech.txt.
Again, changing or adding speech is possible on your local game. If you 
have added something, send the files to the list.

* Monster descriptions.
You can look up that current descriptions in-game with ?/ or just read them in 
dat/descript/monsters.txt. The following conventions should be more or less 
obeyed: descriptions ought to contain flavour text, ideally pointing out major 
weaknesses/strenghts. No numbers, please. Citations are okay, but try to stay 
away from the most generic ones.

* Tiles.
Since version 0.4, tiles are integrated within Crawl. Having variants of 
often-used glyphs is always good. If you want to give this a shot, please 
contact us via the mailing list.

* Patches.
If you like to, you can download the source code and apply patches. Both 
patches for bug fixes as well as implementation of new features is welcome. 
Please be sure to read docs/coding_conventions.txt first.

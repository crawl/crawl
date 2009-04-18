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

There is a list of frequently asked questions which you can access pressing
?Q in the game.

1. Getting started
------------------
If you'd like to dive in immediately, your best bets are to
* start up a game and choose a tutorial (press 'T' when asked for race), or
* read quickstart.txt (in the docs/ directory), or
* for studious readers, browse the manual (see below for all doc files).

Internet play:
You can play Crawl online, both competing with other players and watching
them. Check the homepage at http://crawl.akrasiac.org/ for details, including
information about additional servers. You just need a ssh or telnet console;
on Windows, the PuTTY program works very well. Read docs/ssh_guide.txt
for a step by step guide on how to set this up.

Tiles:
Crawl features an alternative to the classical ASCII display; Tile-based
Crawl is often a lot more accessible to new players. Tiles are available for
Linux, Windows and OS X.
Unfortunately, it is not yet possible to combine tiles and Internet play.


2. File system
--------------
The following files in the Crawl's main folder are essential:

* crawl              These start the game. (The actual name depends on your
* crawl.exe          operating system.)

The docs/ folder contains the following helpful texts (all of which can be
read in-game by bringing up the help menu with '?'):

* crawl_manual.txt   The complete manual; describing all aspects in detail.
                     Contains appendices on species, classes, etc.
* options_guide.txt  Describes all options in detail. The structure of
                     init.txt follows this text.
* macros_guide.txt   A how-to on using macros and keymappings, with examples.
* tiles_help.txt     An explanation of the Tiles interface.

The settings/ folder contains, among others, the following files:

* init.txt           These contain the options for the game. The defaults
* .crawlrc           play well, so don't bother with this in the beginning.
                     Permanent death is not an option, but a feature!
* macro.txt          Playing Crawl can be made even more convenient by
                     redefining keys and assigning macros. Ignore early on.


3. Contact and reporting bugs
-----------------------------
The official webpage is at
      http://crawl-ref.sourceforge.net/
and there you can find both trackers to add bug reports, feature requests, or
upload patches, as well as sources and binaries. This is the best way to report
bugs or mention new ideas.

There is a Usenet newsgroup dealing with roguelikes, including Crawl:
      rec.games.roguelike.misc
It is polite to flag your post with -crawl- as other games are discussed over
there as well. This is a good place to ask general questions, both from new
players as well as for spoilers, or to announce spectacular wins.

If you want to chime in with development, you can read the mailing list
      crawl-ref-discuss@lists.sourceforge.net
which can get pretty busy on the occasion.


4. License and history information
----------------------------------
This is a descendant of Linley's Dungeon Crawl. Development of the main branch
stalled at version 4.0.0b26, with a final alpha of 4.1 being released by Brent
Ross in 2005. Since 2006, the Dungeon Crawl Stone Soup team has been continuing
the development. See the CREDITS in the main folder for a myriad of
contributors, past and present; license.txt contains the legal blurb.

Dungeon Crawl Stone Soup is an open source, freeware roguelike. It is supported
on Linux, Windows, OS X, and, to a lesser extent, on DOS. The source should
compile and run on any reasonably modern Unix.
Stone Soup features both ASCII and graphical (Tiles) display.

Crawl gladly and gratuitously uses the following open source packages:
* The Lua script language, see docs/lualicense.txt.
* The PCRE library for regular expressions, see docs/pcre_license.txt.
* The Mersenne Twister for random number generation, docs/mt19937.txt.
* The SQLite library as database engine; it is properly public domain.
* The ReST light markup language for the documentation.


5. How you can help
-------------------
If you like the game and you want to help making it better, there are a number
of ways to do so:

* Playtesting.
At any time, there will be bugs -- playing and reporting these is a great help.
There is a beta server around hosting the most recent version of the current
code; http://crawl.akrasiac.org/ links to it. Besides finding bugs, ideas on
how to improve interface or gameplay are welcome as well.

* Vault making.
Crawl uses many hand-drawn (but often randomised) maps. Making them is fun and
easy. It's best to start with simple entry vaults (glance through
dat/entry.des for a first impression). Later, you may want to read
docs/level_design.txt for the full power. If you're ambitious, new maps for
branch ends are possible, as well.
If you've made some maps, you can test them on your system (no compiling
needed) and then just mail them to the mailing list.

* Speech.
Monster talking provides a lot of flavour. Just like vaults, speech depends
upon a large set of entries. Since most of the speech has been outsourced, you
can add new prose. The syntax is effective, but slightly strange, so you may
want to read docs/monster_speech.txt.
Again, changing or adding speech is possible on your local game. If you
have added something, send the files to the list.

* Monster descriptions.
You can look up the current monster descriptions in-game with '?/' or just read
them in dat/descript/monsters.txt. The following conventions should be more or
less obeyed: Descriptions ought to contain flavour text, ideally pointing out
major weaknesses/strengths. No numbers, please. Citations are okay, but try to
stay away from the most generic ones.
If you like, you can similarly modify the descriptions for features, items or
branches.

* Tiles.
Since version 0.4, tiles are integrated within Crawl. Having variants of
often-used glyphs is always good. If you want to give this a shot, please
contact us via the mailing list. In case you drew some tiles of your own,
tell us via the list.

* Patches.
If you like to, you can download the source code and apply patches. Both
patches for bug fixes as well as implementation of new features are very much
welcome. If you want to code a cool feature that is likely to be accepted but
unlikely to be coded by the devteam, search the Feature Requests tracker on
the Sourceforge site for Groups "Patches Welcome".
Please be sure to read docs/coding_conventions.txt first.

Thank you, and have fun crawling!

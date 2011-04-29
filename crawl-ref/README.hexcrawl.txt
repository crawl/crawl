Hexcrawl-0.3
==============
A version of Lindley's Dungeon Crawl with a hexagonal grid, based on Crawl
Stone Soup 0.3 (see README.stone_soup.txt).

This is an updated version of what began as a 7-day roguelike patch; much of
the code bears the legacy of its hacky birth, but it seems fairly stable now.
See docs/changes.hexcrawl for what's new since the 7drl version.

Discussions, victory posts (I'll be impressed!) and so on are on topic on the
newsgroup rec.games.roguelike.misc; bug reports and other comments should be
addressed to me at mbays@sdf.lonestar.org.

Happy crawling!



Options
-------
Hexcrawl specific options (for init.txt / .crawlrc)
    hex_interpolate_walls:
	Fill in the gaps between horizontally adjacent walls - showing '#####'
	    rather than '# # #', and so on.
	Possibly confusing, but pretty.
	Default: true

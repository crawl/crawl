# Creating a DCSS artefact

Most of the documentation needed for designing your own artefact can be found in
the comments that are at the beginning of art-data.txt, so there's no need to
duplicate that here. This will be a simple overview of some files that you may
need to edit.

## art-data.txt

This plaintext file is where you put the definition of your artefact. The format
is straightforward, and similar to the YAML format described in
https://github.com/crawl/crawl/edit/master/crawl-ref/docs/develop/species_creation.md
Again, reading the comments in this file will tell you most of what you need to
know in order to implement your artefact.

## art-func.h

If you need to write code to handle the unique properties of your artefact, put
that code here. For instance, if you're designing an artefact that poisons
enemies who poison the player, there's no existing function that you can call,
so you must write your own in this file.

## dat/descript/unrand.txt

This is the file in which you put the item's description.

## artefact.cc

This file builds a complete artefact out of all the component parts that are in
the other files. You probably won't need to mess with it.

## Further reading

You might need to edit many other files if your item interacts with other parts
of the game. The Staff of Battle is a fairly complex item that serves as a good
example of artefact creation, and also happens to have handling written into
many other source files due to the fact that it is "wizardly" and hated by Trog.
See it here:
https://github.com/crawl/crawl/commit/2f2aade273a4c368969f5d5b25d6ba56c9539575

### rltiles/item

As you can see in the Staff of Battle example, it is acceptable to not add a
tile for your new item; if it is merged into the game, an extant pixel artist is
likely to whip up a nice tile for it, though feel free to cook up your own.
Instructions for doing that are available in art-data.txt.

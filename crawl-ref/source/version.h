/*
 *  File: version.h
 *  Summary: Contains version information
 *  Written by: ??
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

/* Crawl versioning:
 * Crawl uses three numbers to determine the version:
 * Version, which changes when the dev team makes enormous overhauls
 *    to the game (which may cause savefiles from previous versions to
 *    temporarily stop working, for example)
 * Release, which changes when siginficant new features have been
 *    added to the game.
 * Mod, which changes with every publicly released version that
 *    contains nothing more than bug fixes, cosmetic changes,
 *    internal cleanup, etc.
 *
 * Further, any source or binary uploaded anywhere that is _not_ of
 * release quality should be labelled as such:
 * alpha for potentially unstable dev versions, or
 * beta for feature-complete and mostly balanced versions
 *
 * several alphas or betas in a row should be labelled incrementally;
 * alpha1 -> alpha2 -> alpha3 -> beta1 -> beta2 -> ...
 */


#ifndef VERSION_H
#define VERSION_H

#define CRAWL "Dungeon Crawl Stone Soup"

#define VER_NUM  "0.5"
#define VER_QUAL "-svn"

// Undefine for official releases.
#define DISPLAY_BUILD_REVISION

// last updated 07august2001 {mv}
/* ***********************************************************************
 * called from: chardump - command - newgame
 * *********************************************************************** */
#define VERSION  VER_NUM VER_QUAL " (crawl-ref)"

// last updated 20feb2001 {GDL}
/* ***********************************************************************
 * called from: command
 * *********************************************************************** */
#define VERSION_DETAIL __DATE__

// Returns the largest SVN revision number that a source file has been updated
// to.  This is not perfectly accurate, but should be good enough for save
// files, as breaking a save almost always involves changing a source file.
int svn_revision();

class check_revision
{
public:
    check_revision(const char *rev_string);
    static int max_rev;
};

// This macro is meant to be used once per source file.
// It can't be put in header files, as there's no way to generate a unique
// object name across includes.  Blame the lack of cross-platform __COUNTER__.
#define REVISION(rev) static check_revision check_this_source_file_revision(rev)

#endif

/*
 *  File:       version.cc
 *  Summary:    Version (and revision) functionality.
 *  Written by: Enne Walker
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

int check_revision::max_rev = 0;

check_revision::check_revision(const char *rev_str)
{
    ASSERT(!strncmp("$Rev:", rev_str, 4));

    int rev = atoi(&rev_str[5]);
    max_rev = std::max(rev, max_rev);
}

int svn_revision()
{
#if BUILD_REVISION
    return BUILD_REVISION;
#else
    return check_revision::max_rev;
#endif
}

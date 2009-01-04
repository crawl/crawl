/*
 *  File:       version.cc
 *  Summary:    Version (and revision) functionality.
 *  Written by: Enne Walker
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
#include "version.h"

REVISION("$Rev$")

int check_revision::max_rev = 0;

check_revision::check_revision(int rev)
{
    max_rev = std::max(rev, max_rev);
}

int svn_revision()
{
    return check_revision::max_rev;
}

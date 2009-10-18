/*
 *  File:       version.cc
 *  Summary:    Version (and revision) functionality.
 *  Written by: Steven Noonan
 */

#include "AppHdr.h"

#include "build.h"

namespace Version
{
    std::string Short()
    {
        return std::string(CRAWL_VERSION_TAG);
    }

    std::string Long()
    {
        return std::string(CRAWL_VERSION_LONG);
    }

    int Major()
    {
        return CRAWL_VERSION_MAJOR;
    }

    int Minor()
    {
        return CRAWL_VERSION_MINOR;
    }

    int Revision()
    {
        return CRAWL_VERSION_REVISION;
    }

    int Build()
    {
        return CRAWL_VERSION_BUILD;
    }

    Class ReleaseType()
    {
        return CRAWL_VERSION_PREREL_TYPE;
    }

    int ReleaseID()
    {
        return CRAWL_VERSION_PREREL_NUM;
    }
}


#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_VERSION  31

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_RESET        = 0, // Minor tags were reset
    TAG_MINOR_DIAG_COUNTERS= 1, // Counters for diag/ortho moves.
    TAG_MINOR_FISHTAIL     = 2, // Merfolk's tail state.
    TAG_MINOR_DENSITY      = 3, // Count of level's explorable area.
    TAG_MINOR_MALIGN       = 4, // Keep malign gateway markers around for longer.
    TAG_MINOR_GOD_GIFTS    = 5, // Track current as well as total god gifts.
    TAG_MINOR_VERSION      = 5, // Current version.  (Keep equal to max.)
};

#endif

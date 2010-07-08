#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_START     5
#define TAG_MAJOR_VERSION  27

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_RESET        = 0, // Minor tags were reset
    TAG_MINOR_DACTIONS     = 1, // Delayed level functions.
    TAG_MINOR_DA_MSTATS    = 2, // Counts of monsters affectable by dactions.
    TAG_MINOR_MAPDESC      = 3, // Save individual map descriptions.
    TAG_MINOR_SAVEVER      = 4, // Save full game version into .chr file.
    TAG_MINOR_CAT_LIVES    = 5, // Let cats accumulate lives.
    TAG_MINOR_NO_ROD_DISCO = 6, // Remove rods of discovery.
    TAG_MINOR_VERSION      = 6, // Current version.  (Keep equal to max.)
};

#endif

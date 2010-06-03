#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_START     5
#define TAG_MAJOR_VERSION  24

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_RESET     = 0, // Minor tags were reset
    TAG_MINOR_TILE_CHANGES = 1,
    TAG_MINOR_MON_REFCOUNT = 2,  // Monster reference counting.
    TAG_MINOR_MON_REFCOUNT_REV = 3,  // Monster reference counting reverted.
    TAG_MINOR_CORRUPTION_RAD = 4,  // Corruption map markers' radii removed.
    TAG_MINOR_VERSION   = 4  // Current version.  (Keep equal to max.)
};

#endif


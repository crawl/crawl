#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_START     5
#define TAG_MAJOR_VERSION  25

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_RESET        = 0, // Minor tags were reset
    TAG_MINOR_OLD_POS      = 1, // Save old player position
    TAG_MINOR_LEVEL_VAULTS = 2, // Level vaults saved in level save.
    TAG_MINOR_VERSION      = 2, // Current version.  (Keep equal to max.)
};

#endif

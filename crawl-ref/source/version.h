/**
 * @file
 * @brief Contains version information
**/

#ifndef VERSION_H
#define VERSION_H

#define CRAWL "Dungeon Crawl Stone Soup"

namespace Version
{
    //! The short version string.
    /*!
     * This version will generally match the last version tag. For instance,
     * if the last tag of Crawl before this build was '0.1.2', you'd see
     * '0.1.2'. This version number does not include some rather important
     * extra information useful for getting the exact revision (the Git commit
     * hash and the number of revisions since the tag). For that extra information,
     * use Version::Long() instead.
     */
    std::string Short();

    //! The long version string.
    /*!
     * This string contains detailed version information about the CrissCross
     * build in use. The string will always start with the Git tag that this
     * build descended from. If this build is not an exact match for a given
     * tag, this string will also include the number of commits since the tag
     * and the Git commit id (the SHA-1 hash).
     */
    std::string Long();

    //! The release type.
    /*!
     * Indicates whether it's a devel or a stable version.
     */
    bool ReleaseType();

    //! The compiler used.
    /*!
     * Names the compiler used to genrate the executable.
     */
    std::string Compiler();

    //! Build architecture.
    /*!
     * Host triplet of the architecture used for build.
     */
    std::string BuildArch();

    //! Target architecture.
    /*!
     * Host triplet of the architecture Crawl was compiled for.
     */
    std::string Arch();

    //! The CFLAGS.
    /*!
     * Returns the CFLAGS the executable was compiled with.
     */
    std::string CFLAGS();

    //! The LDFLAGS.
    /*!
     * Returns the flags the executable was linked with.
     */
    std::string LDFLAGS();
}

std::string compilation_info();

#endif

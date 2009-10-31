/*
 *  File: version.h
 *  Summary: Contains version information
 */

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
     *
     * For extracting individual components of the version, you should use the
     * Major(), Minor(), Revision() and Build() functions.
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

    //! The major version number.
    /*!
     * This is the first number to appear in a version tag. For instance,
     * if the tag is '0.1.2.3', this function would return '0'.
     */
    int Major();

    //! The minor version number.
    /*!
     * This is the second number to appear in a version tag. For instance,
     * if the tag is '0.1.2.3', this function would return '1'.
     */
    int Minor();

    //! The revision number.
    /*!
     * This is the third number to appear in a version tag. For instance,
     * if the tag is '0.1.2.3', this function would return '2'.
     */
    int Revision();

    //! The build number.
    /*!
     * This is the fourth number to appear in a version tag. For instance,
     * if the tag is '0.1.2.3', this function would return '3'.
     */
    int Build();

    typedef enum {
        DEV,    /*!< In-development version (does not exactly match a tag). i.e. '0.1.2-3-g3af4131'. */
        ALPHA,  /*!< An alpha release. i.e. '0.1.2-a3' */
        BETA,   /*!< A beta release. i.e. '0.1.2-b3' */
        RC,     /*!< A release candidate. i.e. '0.1.2-rc3' */
        FINAL   /*!< A final release. i.e. '0.1.2' */
    } Class;

    //! The release class.
    /*!
     * Indicates the type of release. For instance, if you have a tag such
     * as '0.1.2-b1', the class is 'BETA'. Valid suffixes are '-a', '-b',
     * '-rc'. If the version string does not precisely match a tag, then it
     * is considered an in-development version.
     */
    Class ReleaseType();

    //! The release ID.
    /*!
     * If this is a special type of release (alpha, beta, rc), then this
     * will return the alpha/beta/rc number. Otherwise, this returns 0.
     */
    int ReleaseID();

    //! The compiler used.
    /*!
     * Names the compiler used to genrate the executable.
     */
    std::string Compiler();

    //! The compiling operating system.
    /*!
     * Names the operating system that the executable was compiled on.
     */
    std::string BuildOS();

    //! The compiling operating system's version.
    /*!
     * Specifies the version of the OS that the executable was compiled on.
     */
    std::string BuildOSVersion();

    //! The machine type.
    /*!
     * Names the machine type (e.g., "i686") the executable was compiled on.
     */
    std::string BuildMachine();

    //! The processor type.
    /*!
     * Names the processor type the executable was compiled on.
     */
    std::string BuildProcessor();

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

/* ***********************************************************************
 * called from: initfile debug
 * *********************************************************************** */
std::string compilation_info();

#endif

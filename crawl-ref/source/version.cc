/*
 *  File:       version.cc
 *  Summary:    Version (and revision) functionality.
 *  Written by: Steven Noonan
 */

#include "AppHdr.h"

#include "build.h"
#include "compflag.h"

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

    std::string Compiler()
    {
#if defined(__GNUC__) && defined(__VERSION__)
        return make_stringf("GCC %s", __VERSION__);
#elif defined(__GNUC__)
        return ("GCC (unknown version)");
#elif defined(TARGET_COMPILER_MINGW)
        return ("MINGW");
#elif defined(TARGET_COMPILER_CYGWIN)
        return ("CYGWIN");
#elif defined(TARGET_COMPILER_VC)
        return ("Visual C++");
#elif defined(TARGET_COMPILER_ICC)
        return ("Intel C++");
#else
        return ("Unknown compiler");
#endif
    }

    std::string BuildOS()
    {
        return CRAWL_BUILD_OS;
    }

    std::string BuildOSVersion()
    {
        return CRAWL_BUILD_OS_VER;
    }

    std::string BuildMachine()
    {
        return CRAWL_BUILD_MACHINE;
    }

    std::string BuildProcessor()
    {
        return CRAWL_BUILD_PROCESSOR;
    }

    std::string CFLAGS()
    {
        return CRAWL_CFLAGS;
    }

    std::string LDFLAGS()
    {
        return CRAWL_LDFLAGS;
    }
}

std::string compilation_info()
{
    std::string out = "";

    out += make_stringf("Compiled with %s on %s at %s" EOL,
                        Version::Compiler().c_str(), __DATE__, __TIME__);
    out += make_stringf("Compiled on OS: %s %s" EOL,
                        Version::BuildOS().c_str(),
                        Version::BuildOSVersion().c_str());
    out += make_stringf("Compiled on machine type: %s" EOL,
                        Version::BuildMachine().c_str());
    out += make_stringf("Compiled on processor type: %s" EOL,
                        Version::BuildProcessor().c_str());

    out += make_stringf("CLFAGS: %s" EOL, Version::CFLAGS().c_str());
    out += make_stringf("LDFLAGS: %s" EOL, Version::LDFLAGS().c_str());

    return (out);
}

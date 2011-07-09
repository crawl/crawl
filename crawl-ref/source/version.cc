/**
 * @file
 * @brief Version (and revision) functionality.
**/

#include "AppHdr.h"

#include "version.h"
#include "build.h"
#include "compflag.h"
#include "libutil.h"

namespace Version
{
    std::string Short()
    {
        return std::string(CRAWL_VERSION_SHORT);
    }

    std::string Long()
    {
        return std::string(CRAWL_VERSION_LONG);
    }

    rel_type ReleaseType()
    {
        return CRAWL_VERSION_RELEASE;
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

    std::string BuildArch()
    {
        return CRAWL_HOST;
    }
    std::string Arch()
    {
        return CRAWL_ARCH;
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

    out += make_stringf("Compiled with %s on %s at %s\n",
                        Version::Compiler().c_str(), __DATE__, __TIME__);
    out += make_stringf("Build platform: %s\n", Version::BuildArch().c_str());
    out += make_stringf("Platform: %s\n", Version::Arch().c_str());

    out += make_stringf("CLFAGS: %s\n", Version::CFLAGS().c_str());
    out += make_stringf("LDFLAGS: %s\n", Version::LDFLAGS().c_str());

    return (out);
}

#include "AppHdr.h"

#ifdef USE_TILE
#include "tilefont.h"

#ifdef USE_FT
#include "fontwrapper-ft.h"
#endif

FontWrapper* FontWrapper::create()
{
#ifdef USE_FT
    return (FontWrapper *) new FTFontWrapper();
#endif
}

#endif // USE_TILE

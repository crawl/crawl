#include "AppHdr.h"

#include "actor.h"
#include "player.h"
#include "state.h"

bool actor::observable() const
{
    return (crawl_state.arena || this == &you || you.can_see(this));
}


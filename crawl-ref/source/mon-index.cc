#include "AppHdr.h"

#include "mon-index.h"

#include "env.h"
#include "mon-util.h"
#include "monster.h"

void auto_mindex::retain()
{
    if(!invalid_monster_index(index))
        menv[index].retain();
}

void auto_mindex::release()
{
    if(!invalid_monster_index(index))
        menv[index].release();
}

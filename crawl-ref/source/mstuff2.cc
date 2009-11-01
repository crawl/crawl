/*
 *  File:       mstuff2.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "mstuff2.h"

#include <string>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "arena.h"
#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "spells1.h"
#include "spells3.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"



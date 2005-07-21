#ifndef INSULT_H
#define INSULT_H

#include "externs.h"

void imp_taunt( struct monsters *mons );
void demon_taunt( struct monsters *mons );
const char * generic_insult(void);
const char * racial_insult(void);

#endif

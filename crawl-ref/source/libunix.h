#pragma once

#ifndef O_BINARY
#define O_BINARY 0
#endif

void fakecursorxy(int x, int y);

#ifdef DGAMELAUNCH
class suppress_dgl_clrscr
{
public:
    suppress_dgl_clrscr();
    ~suppress_dgl_clrscr();
private:
    bool prev;
};
#endif

bool in_headless_mode();
void enter_headless_mode();

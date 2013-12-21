#ifdef USE_TILE_LOCAL
#ifndef TILEREG_POPUP_H
#define TILEREG_POPUP_H

#include "tilereg-menu.h"

class PopupRegion : public MenuRegion
{
public:
    PopupRegion(ImageManager *im, FontWrapper *entry);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual void run();
    virtual void place_entries();

    // get the value returned by the popup
    int get_retval();

protected:
    int m_retval;
};
#endif
#endif

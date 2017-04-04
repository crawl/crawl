#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-menu.h"

class PopupRegion : public MenuRegion
{
public:
    PopupRegion(ImageManager *im, FontWrapper *entry);

    virtual int handle_mouse(MouseEvent &event) override;
    virtual void render() override;
    virtual void run() override;
    virtual void place_entries() override;

    // get the value returned by the popup
    int get_retval();

protected:
    int m_retval;
};
#endif

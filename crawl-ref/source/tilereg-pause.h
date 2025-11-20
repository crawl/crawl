#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg.h"

class PauseButtonRegion : public TileRegion
{
public:
    PauseButtonRegion(const TileRegionInit &init);
    virtual void render() override;
    virtual void clear() override;
    virtual int handle_mouse(wm_mouse_event &event) override;
    virtual bool update_tip_text(string &tip) override;

protected:
    virtual void on_resize() override;

private:
    bool m_hover;
};

#endif
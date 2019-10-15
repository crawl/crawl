#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-text.h"

class MessageRegion : public TextRegion
{
public:
    MessageRegion(FontWrapper *font_arg);

    void set_overlay(bool is_overlay);

    virtual int handle_mouse(wm_mouse_event &event) override;
    virtual void render() override;
    virtual bool update_tip_text(string &tip) override;

    string &alt_text() { return m_alt_text; }
protected:
    string m_alt_text;
    bool m_overlay;
};

#endif

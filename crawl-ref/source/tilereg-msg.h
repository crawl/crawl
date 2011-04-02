#ifdef USE_TILE
#ifndef TILEREG_MSG_H
#define TILEREG_MSG_H

#include "tilereg-text.h"

class MessageRegion : public TextRegion
{
public:
    MessageRegion(FontWrapper *font);

    void set_overlay(bool is_overlay);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual bool update_tip_text(std::string &tip);

    std::string &alt_text() { return m_alt_text; }
protected:
    std::string m_alt_text;
    bool m_overlay;
};

#endif
#endif

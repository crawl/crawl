#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-stat.h"

#include "libutil.h"
#include "macro.h"

StatRegion::StatRegion(FontWrapper *font) : TextRegion(font)
{
}

int StatRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
        return 0;

    // Resting
    return command_to_key(CMD_REST);
}

bool StatRegion::update_tip_text(std::string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    tip = "[L-Click] Rest / Search for a while";
    return (true);
}

#endif

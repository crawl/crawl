/**
 * @file
 * @brief UI widget for the main game view
**/

#include "AppHdr.h"

#include "main-game.h"
#include "output.h"
#include "ui.h"

#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
#endif

UIMainGame* UIMainGame::main_game = nullptr;

UIMainGame::UIMainGame()
{
    expand_h = expand_v = true;

    ASSERT(!main_game);
    main_game = this;
}

UIMainGame::~UIMainGame()
{
    main_game = nullptr;
}

int UIMainGame::get_key()
{
    if (UIMainGame::main_game)
        return UIMainGame::main_game->getch();
    return 0;
}

int UIMainGame::getch()
{
    while (m_keys.empty() && !crawl_state.seen_hups)
        ui::pump_events();
    if (m_keys.empty())
        return 0;
    auto key = m_keys.front();
    m_keys.pop_front();
    return key;
}

void UIMainGame::_render()
{
#ifdef USE_TILE_LOCAL
    tiles.render_current_regions();
    glmanager->reset_transform();
#else
    redraw_screen(true);
#endif
}

void UIMainGame::_allocate_region()
{
#ifdef USE_TILE_LOCAL
    tiles.calculate_default_options();
    tiles.do_layout();
#endif
}

bool UIMainGame::on_event(const ui::Event& ev)
{
#ifdef USE_TILE_LOCAL
    int key;
    if ((key = tiles.handle_event(ev)))
        m_keys.push_back(key);
#else
    if (ev.type() == ui::Event::KeyDown)
        m_keys.push_back(static_cast<const ui::KeyEvent&>(ev).key());
#endif
    _expose();
    return true;
}

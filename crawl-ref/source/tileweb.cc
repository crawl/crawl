#include "AppHdr.h"

#ifdef USE_TILE_WEB

#include "tileweb.h"

#include <cerrno>
#include <cstdarg>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
#include <unistd.h>
#endif

#include "artefact.h"
#include "branch.h"
#include "command.h"
#include "coord.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h" // is_weapon()
#include "json.h"
#include "json-wrapper.h"
#include "lang-fake.h"
#include "libutil.h"
#include "macro.h"
#include "map-knowledge.h"
#include "menu.h"
#include "outer-menu.h"
#include "message.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "player-equip.h"
#include "religion.h"
#include "scroller.h"
#include "showsymb.h"
#include "skills.h"
#include "state.h"
#include "stringutil.h"
#include "throw.h"
#include "tile-flags.h"
#include "tile-player-flag-cut.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-gui.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "rltiles/tiledef-player.h"
#include "tilepick.h"
#include "tilepick-p.h"
#include "tileview.h"
#include "transform.h"
#include "travel.h"
#include "ui.h"
#include "unicode.h"
#include "unwind.h"
#include "version.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "view.h"
#include "xom.h"

//#define DEBUG_WEBSOCKETS

static unsigned int get_milliseconds()
{
    // This is Unix-only, but so is Webtiles at the moment.
    timeval tv;
    gettimeofday(&tv, nullptr);

    return ((unsigned int) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

TilesFramework tiles;

TilesFramework::TilesFramework() :
      m_controlled_from_web(false),
      _send_lock(false),
      m_last_ui_state(UI_INIT),
      m_view_loaded(false),
      m_current_view(coord_def(GXM, GYM)),
      m_next_view(coord_def(GXM, GYM)),
      m_next_view_tl(0, 0),
      m_next_view_br(-1, -1),
      m_need_full_map(true),
      m_text_menu("menu_txt"),
      m_print_fg(15)
{
    screen_cell_t default_cell;
    default_cell.tile.bg = TILE_FLAG_UNSEEN;
    m_current_view.fill(default_cell);
    m_next_view.fill(default_cell);
}

TilesFramework::~TilesFramework()
{
}

void TilesFramework::shutdown()
{
    if (m_sock_name.empty())
        return;

    close(m_sock);
    remove(m_sock_name.c_str());
}

void TilesFramework::draw_doll_edit()
{
}

bool TilesFramework::initialise()
{
    m_cursor[CURSOR_MOUSE] = NO_CURSOR;
    m_cursor[CURSOR_TUTORIAL] = NO_CURSOR;
    m_cursor[CURSOR_MAP] = NO_CURSOR;

    // Initially, switch to CRT.
    cgotoxy(1, 1, GOTO_CRT);

    if (m_sock_name.empty())
        return true;

    // Init socket
    m_sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (m_sock < 0)
        die("Can't open the webtiles socket!");
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, m_sock_name.c_str());
    if (::bind(m_sock, (sockaddr*) &addr, sizeof(sockaddr_un)))
        die("Can't bind the webtiles socket!");

    int bufsize = 64 * 1024;
    if (setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)))
        die("Can't set buffer size!");
    // Need small maximum message size to avoid crashes in OS X
    m_max_msg_size = 2048;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        die("Can't set send timeout!");

    if (m_await_connection)
        _await_connection();

    _send_version();
    send_exit_reason("unknown");
    send_options(); // n.b. full rc read hasn't happened yet
    _send_layout();

    return true;
}

string TilesFramework::get_message()
{
    return m_msg_buf;
}

void TilesFramework::write_message(const char *format, ...)
{
    char buf[2048];
    int len;

    va_list argp;
    va_start(argp, format);
    if ((len = vsnprintf(buf, sizeof(buf), format, argp)) < 0)
        die("Webtiles message format error! (%s)", format);
    else if (len >= (int)sizeof(buf))
        die("Webtiles message too long! (%d)", len);
    va_end(argp);

    m_msg_buf.append(buf);
}

void TilesFramework::finish_message()
{
    if (m_msg_buf.size() == 0)
        return;
#ifdef DEBUG_WEBSOCKETS
    const int initial_buf_size = m_msg_buf.size();
    fprintf(stderr, "websocket: About to send %d bytes.\n", initial_buf_size);
#endif

    if (m_sock_name.empty())
    {
        m_msg_buf.clear();
        return;
    }

    m_msg_buf.append("\n");
    const char* fragment_start = m_msg_buf.data();
    const char* data_end = m_msg_buf.data() + m_msg_buf.size();
#ifdef DEBUG_WEBSOCKETS
    int fragments = 0;
#endif
    while (fragment_start < data_end)
    {
        int fragment_size = data_end - fragment_start;
        if (fragment_size > m_max_msg_size)
            fragment_size = m_max_msg_size;
#ifdef DEBUG_WEBSOCKETS
        fragments++;
#endif

        for (unsigned int i = 0; i < m_dest_addrs.size(); ++i)
        {
            int retries = 30;
            ssize_t sent = 0;
            while (sent < fragment_size)
            {
                ssize_t retval = sendto(m_sock, fragment_start + sent,
                    fragment_size - sent, 0, (sockaddr*) &m_dest_addrs[i],
                    sizeof(sockaddr_un));
#ifdef DEBUG_WEBSOCKETS
                fprintf(stderr,
                            "    trying to send fragment to client %d...", i);
#endif
                if (retval <= 0)
                {
                    const char *errmsg = retval == 0 ? "No bytes sent"
                                                     : strerror(errno);
                    if (--retries <= 0)
                        die("Socket write error: %s", errmsg);

                    if (retval == 0 || errno == ENOBUFS || errno == EWOULDBLOCK
                        || errno == EINTR || errno == EAGAIN)
                    {
                        // Wait for half a second at first (up to five), then
                        // try again.
                        const int sleep_time = retries > 25 ? 2 * 1000
                                             : retries > 10 ? 500 * 1000
                                             : 5000 * 1000;
#ifdef DEBUG_WEBSOCKETS
                        fprintf(stderr, "failed (%s), sleeping for %dms.\n",
                                                    errmsg, sleep_time / 1000);
#endif
                        usleep(sleep_time);
                    }
                    else if (errno == ECONNREFUSED || errno == ENOENT)
                    {
                        // the other side is dead
#ifdef DEBUG_WEBSOCKETS
                        fprintf(stderr,
                            "failed (%s), breaking.\n", errmsg);
#endif
                        m_dest_addrs.erase(m_dest_addrs.begin() + i);
                        i--;
                        break;
                    }
                    else
                        die("Socket write error: %s", errmsg);
                }
                else
                {
#ifdef DEBUG_WEBSOCKETS
                    fprintf(stderr, "fragment size %d sent.\n", fragment_size);
#endif
                    sent += retval;
                }
            }
        }

        fragment_start += fragment_size;
    }
    m_msg_buf.clear();
    m_need_flush = true;
#ifdef DEBUG_WEBSOCKETS
    // should the game actually crash in this case?
    if (m_controlled_from_web && m_dest_addrs.size() == 0)
        fprintf(stderr, "No open websockets after finish_message!!\n");

    fprintf(stderr, "websocket: Sent %d bytes in %d fragments.\n",
                                                initial_buf_size, fragments);
#endif
}

void TilesFramework::send_message(const char *format, ...)
{
    char buf[2048];
    int len;

    va_list argp;
    va_start(argp, format);
    if ((len = vsnprintf(buf, sizeof(buf), format, argp)) >= (int)sizeof(buf)
        || len == -1)
    {
        if (len == -1)
            die("Webtiles message format error! (%s)", format);
        else
            die("Webtiles message too long! (%d)", len);
    }
    va_end(argp);

    m_msg_buf.append(buf);

    finish_message();
}

void TilesFramework::flush_messages()
{
    if (_send_lock)
        return;
    unwind_bool no_rentry(_send_lock, true);

    if (m_need_flush)
    {
        send_message("*{\"msg\":\"flush_messages\"}");
        m_need_flush = false;
    }
}

void TilesFramework::_await_connection()
{
    if (m_sock_name.empty())
        return;

    while (m_dest_addrs.size() == 0)
        _receive_control_message();
}

wint_t TilesFramework::_receive_control_message()
{
    if (m_sock_name.empty())
        return 0;

    char buf[4096]; // Should be enough for client->server messages
    sockaddr_un srcaddr;
    socklen_t srcaddr_len;
    memset(&srcaddr, 0, sizeof(struct sockaddr_un));

    srcaddr_len = sizeof(srcaddr);

    // XX currently this is not interrupted by HUP
    int len = recvfrom(m_sock, buf, sizeof(buf),
                       0,
                       (sockaddr *) &srcaddr, &srcaddr_len);

    if (len == -1)
        die("Socket read error: %s", strerror(errno));

    string data(buf, len);
    try
    {
        return _handle_control_message(srcaddr, data);
    }
    catch (JsonWrapper::MalformedException&)
    {
        dprf("Malformed control message!");
        return 0;
    }
}

static int _handle_cell_target(const coord_def &gc)
{
    // describe
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH)
    {
        if (targeting_mouse_move(gc))
            return CK_REDRAW;
    }
    return 0;
}

static int _handle_cell_click(const coord_def &gc, int button, bool force)
{
    // describe
    if ((mouse_control::current_mode() == MOUSE_MODE_COMMAND
        || mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
        || tiles.get_ui_state() == UI_VIEW_MAP)
        && button == 3)
    {
        full_describe_square(gc, false);
        return CK_MOUSE_CMD;
    }

    // click travel
    if (mouse_control::current_mode() == MOUSE_MODE_COMMAND && button == 1)
    {
        int c = click_travel(gc, force);
        if (c != CK_MOUSE_CMD)
        {
            clear_messages();
            process_command((command_type) c);
        }
        return CK_MOUSE_CMD;
    }

    // fire
    if ((mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH)
        && button == 1)
    {
        if (targeting_mouse_select(gc))
            return CK_MOUSE_CMD;
        // if the targeter is still live, the player did something like click
        // out of range, and we want to redraw immediately
        return CK_REDRAW;
    }

    // generic click: doesn't do a lot, but will interrupt travel, repeats,
    // etc
    return CK_MOUSE_CLICK;
}

wint_t TilesFramework::_handle_control_message(sockaddr_un addr, string data)
{
    JsonWrapper obj = json_decode(data.c_str());
    obj.check(JSON_OBJECT);

    JsonWrapper msg = json_find_member(obj.node, "msg");
    msg.check(JSON_STRING);
    string msgtype(msg->string_);
#ifdef DEBUG_WEBSOCKETS
    fprintf(stderr, "websocket: Received control message '%s' in %d byte.\n", msgtype.c_str(), (int) data.size());
#endif

    int c = 0;

    if (msgtype == "attach")
    {
        JsonWrapper primary = json_find_member(obj.node, "primary");
        primary.check(JSON_BOOL);

        m_dest_addrs.push_back(addr);
        m_controlled_from_web = primary->bool_;
    }
    else if (msgtype == "key")
    {
        JsonWrapper keycode = json_find_member(obj.node, "keycode");
        keycode.check(JSON_NUMBER);

        // TODO: remove this fixup call
        c = (int) keycode->number_;
    }
    else if (msgtype == "spectator_joined")
    {
        flush_messages();
        _send_everything();
        flush_messages();
    }
    else if (msgtype == "menu_hover")
    {
        JsonWrapper hover = json_find_member(obj.node, "hover");
        hover.check(JSON_NUMBER);
        JsonWrapper mouse = json_find_member(obj.node, "mouse");
        mouse.check(JSON_BOOL);

        if (!m_menu_stack.empty()
            && m_menu_stack.back().type == UIStackFrame::MENU)
        {
            m_menu_stack.back().menu->set_hovered(
                (int) hover->number_,
                mouse->bool_);
        }

    }
    else if (msgtype == "menu_scroll")
    {
        JsonWrapper first = json_find_member(obj.node, "first");
        first.check(JSON_NUMBER);
        JsonWrapper hover = json_find_member(obj.node, "hover");
        hover.check(JSON_NUMBER);
        // last visible item is sent too, but currently unused

        if (!m_menu_stack.empty() && m_menu_stack.back().type == UIStackFrame::MENU)
            m_menu_stack.back().menu->webtiles_scroll((int) first->number_, (int) hover->number_);
    }
    else if (msgtype == "*request_menu_range")
    {
        JsonWrapper start = json_find_member(obj.node, "start");
        start.check(JSON_NUMBER);
        JsonWrapper end = json_find_member(obj.node, "end");
        end.check(JSON_NUMBER);

        if (!m_menu_stack.empty() && m_menu_stack.back().type == UIStackFrame::MENU)
        {
            m_menu_stack.back().menu->webtiles_handle_item_request((int) start->number_,
                                                                   (int) end->number_);
        }
    }
    else if (msgtype == "note")
    {
        JsonWrapper content = json_find_member(obj.node, "content");
        content.check(JSON_STRING);

        if (Options.note_chat_messages)
            take_note(Note(NOTE_MESSAGE, MSGCH_PLAIN, 0, content->string_));
    }
    else if (msgtype == "server_announcement")
    {
        JsonWrapper content = json_find_member(obj.node, "content");
        content.check(JSON_STRING);
        string m = "<red>Serverwide announcement:</red> ";
        m += content->string_;

        mprf(MSGCH_DGL_MESSAGE, "%s", m.c_str());
        // The following two lines are a magic incantation to get this mprf
        // to actually render without waiting on player input
        flush_prev_message();
        c = CK_REDRAW;
    }
    else if (msgtype == "click_cell")
    {
        JsonWrapper x = json_find_member(obj.node, "x");
        JsonWrapper y = json_find_member(obj.node, "y");
        JsonWrapper button = json_find_member(obj.node, "button");
        x.check(JSON_NUMBER);
        y.check(JSON_NUMBER);
        button.check(JSON_NUMBER);
        // XX force is currently unused
        JsonWrapper force = json_find_member(obj.node, "force");
        coord_def gc = coord_def((int) x->number_, (int) y->number_) + m_origin;

        c = _handle_cell_click(gc, button->number_,
                        force.node && force->tag == JSON_BOOL && force->bool_);
    }
    else if (msgtype == "target_cursor")
    {
        JsonWrapper x = json_find_member(obj.node, "x");
        JsonWrapper y = json_find_member(obj.node, "y");
        x.check(JSON_NUMBER);
        y.check(JSON_NUMBER);
        coord_def gc = coord_def((int) x->number_, (int) y->number_) + m_origin;
        _handle_cell_target(gc);
        c = CK_REDRAW;
    }
    else if (msgtype == "formatted_scroller_scroll")
    {
        JsonWrapper scroll = json_find_member(obj.node, "scroll");
        scroll.check(JSON_NUMBER);
        recv_formatted_scroller_scroll((int)scroll->number_);
    }
    else if (msgtype == "outer_menu_focus")
    {
        JsonWrapper menu_id = json_find_member(obj.node, "menu_id");
        JsonWrapper hotkey = json_find_member(obj.node, "hotkey");
        menu_id.check(JSON_STRING);
        hotkey.check(JSON_NUMBER);
        OuterMenu::recv_outer_menu_focus(menu_id->string_, (int)hotkey->number_);
    }
    else if (msgtype == "ui_state_sync")
        ui::recv_ui_state_change(obj.node);
    else if (msgtype == "inv_item_describe"
        && mouse_control::current_mode() == MOUSE_MODE_COMMAND)
    {
        JsonWrapper slot = json_find_member(obj.node, "slot");
        slot.check(JSON_NUMBER);
        int inv_slot = (int) slot->number_;
        if (inv_slot >=0 && inv_slot < ENDOFPACK)
        {
            describe_item(you.inv[inv_slot]);
            c = CK_MOUSE_CMD;
        }
    }
    else if (msgtype == "inv_item_action"
        && mouse_control::current_mode() == MOUSE_MODE_COMMAND)
    {
        JsonWrapper slot = json_find_member(obj.node, "slot");
        slot.check(JSON_NUMBER);
        int inv_slot = (int) slot->number_;
        if (inv_slot >=0 && inv_slot < ENDOFPACK)
        {
            quiver::action_cycler tmp;
            tmp.set_from_slot(inv_slot);
            if (!tmp.is_empty())
            {
                if (tmp.get()->is_targeted())
                    tmp.target();
                else
                    tmp.get()->trigger();
            }
            c = CK_MOUSE_CMD;
        }
    }
    else if (msgtype == "set_option")
    {
        // this is an extremely brute force approach...
        JsonWrapper opt_line = json_find_member(obj.node, "line");
        opt_line.check(JSON_STRING);
        Options.read_option_line(opt_line->string_, true);
        // XX only set this flag if a relevant option has actually changed
        Options.prefs_dirty = true;
    }
    else if (msgtype == "main_menu_action"
             && mouse_control::current_mode() == MOUSE_MODE_COMMAND)
    {
        // TODO: add a control message type that can send arbitrary commands
        // (possibly just as a string, like the lua API for this)
        process_command(CMD_GAME_MENU);
    }

    return c;
}

bool TilesFramework::await_input(wint_t& c, bool block)
{
    int result;
    fd_set fds;
    int maxfd = m_sock_name.empty() ? STDIN_FILENO : m_sock;

    while (true)
    {
        do
        {
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            if (!m_sock_name.empty())
                FD_SET(m_sock, &fds);

            if (block)
            {
                tiles.flush_messages();
                result = select(maxfd + 1, &fds, nullptr, nullptr, nullptr);
            }
            else
            {
                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;

                result = select(maxfd + 1, &fds, nullptr, nullptr, &timeout);
            }
        }
        while (result == -1 && errno == EINTR);

        if (result == 0)
            return false;
        else if (result > 0)
        {
            if (!m_sock_name.empty() && FD_ISSET(m_sock, &fds))
            {
                c = _receive_control_message();

                if (c != 0)
                    return true;
            }

            if (FD_ISSET(STDIN_FILENO, &fds))
            {
                c = 0;
                return true;
            }
        }
        else if (errno == EBADF)
        {
            // This probably means that stdin got closed because of a
            // SIGHUP. We'll just return.
            c = 0;
            return false;
        }
        else
            die("select error: %s", strerror(errno));
    }
}

void TilesFramework::dump()
{
    fprintf(stderr, "Webtiles message buffer: %s\n", m_msg_buf.c_str());
    fprintf(stderr, "Webtiles JSON stack:\n");
    for (const JsonFrame &frame : m_json_stack)
    {
        fprintf(stderr, "start: %d end: %d type: %c\n",
                frame.start, frame.prefix_end, frame.type);
    }
}

void TilesFramework::send_exit_reason(const string& type, const string& message)
{
    write_message("*");
    write_message("{\"msg\":\"exit_reason\",\"type\":\"");
    write_message_escaped(type);
    if (!message.empty())
    {
        write_message("\",\"message\":\"");
        write_message_escaped(message);
    }
    write_message("\"}");
    finish_message();
}

void TilesFramework::send_dump_info(const string& type, const string& filename)
{
    write_message("*");
    write_message("{\"msg\":\"dump\",\"type\":\"");
    write_message_escaped(type);
    write_message("\",\"filename\":\"");
    write_message_escaped(strip_filename_unsafe_chars(filename));
    write_message("\"}");
    finish_message();
}

void TilesFramework::_send_version()
{
#ifdef WEB_DIR_PATH
    // The star signals a message to the server
    send_message("*{\"msg\":\"client_path\",\"path\":\"%s\",\"version\":\"%s\"}", WEB_DIR_PATH, Version::Long);
#endif

    string title = CRAWL " " + string(Version::Long);
    send_message("{\"msg\":\"version\",\"text\":\"%s\"}", title.c_str());
}

void TilesFramework::send_milestone(const xlog_fields &xl)
{
    JsonWrapper j = xl.xlog_json();
    json_append_member(j.node, "msg", json_mkstring("milestone"));
    write_message("*");
    write_message("%s", j.to_string().c_str());
    finish_message();
}

void TilesFramework::send_options()
{
    json_open_object();
    json_write_string("msg", "options");
    Options.write_webtiles_options("options");
    json_close_object();
    finish_message();
}

#define ZOOM_INC 0.1

static void _set_option_fixedp(string name, fixedp<int,100> value)
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "set_option");
    tiles.json_write_string("name", name);
    tiles.json_write_int("value", value.to_scaled());
    tiles.json_close_object();
    tiles.finish_message();
}

void TilesFramework::zoom_dungeon(bool in)
{
    if (m_ui_state == UI_VIEW_MAP)
    {
        Options.tile_map_scale = min(3.0, max(0.2,
                    Options.tile_map_scale + (in ? ZOOM_INC : -ZOOM_INC)));
        _set_option_fixedp("tile_map_scale", Options.tile_map_scale);
        dprf("Zooming map to %g", (float) Options.tile_map_scale);
    }
    else
    {
        Options.tile_viewport_scale = min(300, max(20,
                    Options.tile_viewport_scale + (in ? ZOOM_INC : -ZOOM_INC)));
        _set_option_fixedp("tile_viewport_scale", Options.tile_viewport_scale);
        dprf("Zooming to %g", (float) Options.tile_viewport_scale);
    }
    // calling redraw explicitly is not needed here: it triggers from a
    // listener on the webtiles side.
    // TODO: how to implement dynamic max zoom that reacts to the webtiles side?
}

void TilesFramework::_send_layout()
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "layout");
    tiles.json_open_object("message_pane");
    tiles.json_write_int("height",
                        max(Options.msg_webtiles_height, crawl_view.msgsz.y));
    tiles.json_write_bool("small_more", Options.small_more);
    tiles.json_close_object();
    tiles.json_close_object();
    tiles.finish_message();
}

void TilesFramework::push_menu(Menu* m)
{
    UIStackFrame frame;
    frame.type = UIStackFrame::MENU;
    frame.menu = m;
    frame.centred = !crawl_state.need_save;
    m_menu_stack.push_back(frame);
    m->webtiles_write_menu();
    tiles.finish_message();
}

void TilesFramework::push_crt_menu(string tag)
{
    UIStackFrame frame;
    frame.type = UIStackFrame::CRT;
    frame.crt_tag = tag;
    frame.centred = !crawl_state.need_save;
    m_menu_stack.push_back(frame);

    json_open_object();
    json_write_string("msg", "menu");
    json_write_string("type", "crt");
    json_write_string("tag", tag);
    json_write_bool("ui-centred", frame.centred);
    json_close_object();
    finish_message();
}

bool TilesFramework::is_in_crt_menu()
{
    return !m_menu_stack.empty() && m_menu_stack.back().type == UIStackFrame::CRT;
}

bool TilesFramework::is_in_menu(Menu* m)
{
    return !m_menu_stack.empty() && m_menu_stack.back().type == UIStackFrame::MENU
        && m_menu_stack.back().menu == m;
}

void TilesFramework::pop_menu()
{
    if (m_menu_stack.empty())
        return;
    m_menu_stack.pop_back();
    send_message("{\"msg\":\"close_menu\"}");
}

void TilesFramework::pop_all_ui_layouts()
{
    for (auto it = m_menu_stack.crbegin(); it != m_menu_stack.crend(); it++)
    {
        if (it->type == UIStackFrame::UI)
            send_message("{\"msg\":\"ui-pop\"}");
        else
            send_message("{\"msg\":\"close_menu\"}");
    }
    m_menu_stack.clear();

    // This is a bit of a hack, in case the client-side menu stack ever gets
    // out of sync with m_menu_stack. (This can maybe happen for reasons that I
    // don't fully understand, on spectator join.)
    send_message("{\"msg\":\"close_all_menus\"}");
}

void TilesFramework::push_ui_layout(const string& type, unsigned num_state_slots)
{
    ASSERT(m_json_stack.size() == 1);
    ASSERT(m_json_stack.back().type == '}'); // enums, schmenums
    tiles.json_write_string("msg", "ui-push");
    tiles.json_write_string("type", type);
    tiles.json_write_bool("ui-centred", !crawl_state.need_save);
    tiles.json_write_int("generation_id", ui::layout_generation_id());
    tiles.json_close_object();
    UIStackFrame frame;
    frame.type = UIStackFrame::UI;
    frame.ui_json.resize(num_state_slots+1);
    frame.ui_json[0] = m_msg_buf;
    frame.centred = !crawl_state.need_save;
    m_menu_stack.push_back(frame);
    tiles.finish_message();
}

void TilesFramework::pop_ui_layout()
{
    if (m_menu_stack.empty())
        return;
    m_menu_stack.pop_back();
    send_message("{\"msg\":\"ui-pop\"}");
}

void TilesFramework::ui_state_change(const string& type, unsigned state_slot)
{
    ASSERT(!m_menu_stack.empty());
    UIStackFrame &top = m_menu_stack.back();
    ASSERT(top.type == UIStackFrame::UI);
    ASSERT(m_json_stack.size() == 1);
    ASSERT(m_json_stack.back().type == '}');
    tiles.json_write_string("msg", "ui-state");
    tiles.json_write_string("type", type);
    tiles.json_close_object();
    ASSERT(state_slot + 1 < top.ui_json.size());
    top.ui_json[state_slot+1] = m_msg_buf;
    tiles.finish_message();
}

void TilesFramework::push_ui_cutoff()
{
    int cutoff = static_cast<int>(m_menu_stack.size());
    m_ui_cutoff_stack.push_back(cutoff);
    send_message("{\"msg\":\"ui_cutoff\",\"cutoff\":%d}", cutoff);
}

void TilesFramework::pop_ui_cutoff()
{
    m_ui_cutoff_stack.pop_back();
    int cutoff = m_ui_cutoff_stack.empty() ? -1 : m_ui_cutoff_stack.back();
    send_message("{\"msg\":\"ui_cutoff\",\"cutoff\":%d}", cutoff);
}

static void _send_text_cursor(bool enabled)
{
    tiles.send_message("{\"msg\":\"text_cursor\",\"enabled\":%s}",
                       enabled ? "true" : "false");
}

void TilesFramework::set_text_cursor(bool enabled)
{
    if (m_text_cursor == enabled)
        return;

    m_text_cursor = enabled;
}

static void _send_ui_state(WebtilesUIState state)
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "ui_state");
    tiles.json_write_int("state", state);
    tiles.json_close_object();
    tiles.finish_message();
}

void TilesFramework::set_ui_state(WebtilesUIState state)
{
    if (m_ui_state == state)
        return;

    m_ui_state = state;
}

void TilesFramework::update_input_mode(mouse_mode mode, bool force)
{
    auto prev_mode = mouse_control::current_mode();
    if (prev_mode == mode && !force)
        return;

    // we skip redrawing in this case because it happens on every key input,
    // and is very heavy on held down keys
    if (force
        || !(prev_mode == MOUSE_MODE_COMMAND && mode == MOUSE_MODE_NORMAL
             || prev_mode == MOUSE_MODE_NORMAL && mode == MOUSE_MODE_COMMAND))
    {
        redraw();
    }

    json_open_object();
    json_write_string("msg", "input_mode");
    json_write_int("mode", mode);
    json_close_object();
    finish_message();
}

static bool _update_string(bool force, string& current,
                           const string& next,
                           const string& name,
                           bool update = true)
{
    if (force || current != next)
    {
        tiles.json_write_string(name, next);
        if (update)
            current = next;
        return true;
    }
    else
        return false;
}

template<class T> static bool _update_int(bool force, T& current, T next,
                                          const string& name,
                                          bool update = true)
{
    if (force || current != next)
    {
        tiles.json_write_int(name, next);
        if (update)
            current = next;
        return true;
    }
    else
        return false;
}

static bool _update_statuses(player_info& c)
{
    bool changed = false;
    unsigned int counter = 0;
    status_info inf;
    for (unsigned int status = 0; status <= STATUS_LAST_STATUS; ++status)
    {
        if (status == DUR_ICEMAIL_DEPLETED)
        {
            inf = status_info();
            if (you.duration[status] <= ICEMAIL_TIME / ICEMAIL_MAX)
                continue;
            inf.short_text = "icemail depleted";
        }
        else if (status == DUR_ACROBAT)
        {
            inf = status_info();
            if (!acrobat_boost_active())
                continue;
            inf.short_text = "acrobatic";
        }
        else if (!fill_status_info(status, inf)) // this will reset inf itself
            continue;

        if (!inf.light_text.empty() || !inf.short_text.empty())
        {
            if (!changed)
            {
                // up until now, c.status has not changed. Does this dur differ
                // from the counter-th element in c.status?
                if (counter >= c.status.size()
                    || inf.light_text != c.status[counter].light_text
                    || inf.light_colour != c.status[counter].light_colour
                    || inf.short_text != c.status[counter].short_text)
                {
                    changed = true;
                }
            }

            if (changed)
            {
                // c.status has changed at some point before counter, so all
                // bets are off for any future statuses.
                c.status.resize(counter + 1);
                c.status[counter] = inf;
            }

            counter++;
        }
    }
    if (c.status.size() != counter)
    {
        // the only thing that has happened is that some durations are removed
        ASSERT(!changed);
        changed = true;
        c.status.resize(counter);
    }

    return changed;
}

player_info::player_info()
{
    _state_ever_synced = false;
    position = coord_def(-1, -1);
}

/**
 * Send the player properties to the webserver. Any player properties that
 * must be available to the WebTiles client must be sent here through an
 * _update_* function call of the correct data type.
 * @param force_full  If true, all properties will be updated in the json
 *                    regardless whether their values are the same as the
 *                    current info in m_current_player_info.
 *
 * Warning: `force_full` is only ever set to true when sending player info
 * for spectators, and some details below make use of this semantics.
 */
void TilesFramework::_send_player(bool force_full)
{
    player_info& c = m_current_player_info;
    const bool spectator = force_full;
    if (!c._state_ever_synced)
    {
        // force the initial sync to be full: otherwise the _update_blah
        // functions will incorrectly detect initial values to be ones that
        // have previously been sent to the client, when they will not have
        // been. (This is made ever worse by the fact that player_info does
        // not initialize most of its values...)
        c._state_ever_synced = true;
        force_full = true;
    }

    json_open_object();
    json_write_string("msg", "player");
    json_treat_as_empty();

    _update_string(force_full, c.name, you.your_name, "name");
    _update_string(force_full, c.job_title, filtered_lang(player_title()),
                   "title");
    _update_int(force_full, c.wizard, you.wizard, "wizard");
    _update_int(force_full, c.explore, you.explore, "explore");
    _update_string(force_full, c.species, player_species_name(),
                   "species");
    string god = "";
    if (you_worship(GOD_JIYVA))
        god = god_name_jiyva(true);
    else if (!you_worship(GOD_NO_GOD))
        god = god_name(you.religion);
    _update_string(force_full, c.god, god, "god");
    _update_int(force_full, c.under_penance, (bool) player_under_penance(), "penance");
    int prank = 0;
    if (you_worship(GOD_XOM))
        prank = xom_favour_rank() - 1;
    else if (!you_worship(GOD_NO_GOD))
        prank = max(0, piety_rank());
    else if (you.char_class == JOB_MONK && !you.has_mutation(MUT_FORLORN)
             && !had_gods())
    {
        prank = 2;
    }
    _update_int(force_full, c.piety_rank, prank, "piety_rank");

    _update_int(force_full, c.form, (uint8_t) you.form, "form");

    _update_int(force_full, c.hp, you.hp, "hp");
    _update_int(force_full, c.hp_max, you.hp_max, "hp_max");
    int max_max_hp = get_real_hp(true, false);

    _update_int(force_full, c.real_hp_max, max_max_hp, "real_hp_max");
    _update_int(force_full, c.mp, you.magic_points, "mp");
    _update_int(force_full, c.mp_max, you.max_magic_points, "mp_max");
#if TAG_MAJOR_VERSION == 34
    _update_int(force_full, c.dd_real_mp_max,
                you.species == SP_DEEP_DWARF ? get_real_mp(false) : 0,
                "dd_real_mp_max");
#else
    // TODO: clean up the JS that uses this
    _update_int(force_full, c.dd_real_mp_max, 0, "dd_real_mp_max");
#endif

    _update_int(force_full, c.poison_survival, max(0, poison_survival()),
                "poison_survival");

    _update_int(force_full, c.armour_class, you.armour_class_scaled(1), "ac");
    _update_int(force_full, c.evasion, you.evasion_scaled(1), "ev");
    _update_int(force_full, c.shield_class, player_displayed_shield_class(),
                "sh");

    _update_int(force_full, c.strength, (int8_t) you.strength(false), "str");
    _update_int(force_full, c.intel, (int8_t) you.intel(false), "int");
    _update_int(force_full, c.dex, (int8_t) you.dex(false), "dex");

    if (you.has_mutation(MUT_MULTILIVED))
    {
        _update_int(force_full, c.lives, you.lives, "lives");
        _update_int(force_full, c.deaths, you.deaths, "deaths");
    }

    _update_int(force_full, c.experience_level, you.experience_level, "xl");
    _update_int(force_full, c.exp_progress, (int8_t) get_exp_progress(), "progress");
    _update_int(force_full, c.gold, you.gold, "gold");
    _update_int(force_full, c.noise,
                (you.wizard ? you.get_noise_perception(false) : -1), "noise");
    _update_int(force_full, c.adjusted_noise, you.get_noise_perception(true), "adjusted_noise");

    if (you.running == 0) // Don't update during running/resting
    {
        _update_int(force_full, c.elapsed_time, you.elapsed_time, "time");

        // only send this for spectators; the javascript version of the time
        // indicator works somewhat differently than the local version
        // XX reconcile?
        if (spectator)
            tiles.json_write_int("time_last_input", you.elapsed_time_at_last_input);

        _update_int(force_full, c.num_turns, you.num_turns, "turn");
    }

    const PlaceInfo& place = you.get_place_info();
    string short_name = branches[place.branch].shortname;

    if (brdepth[place.branch] == 1)
    {
        // Definite articles
        if (place.branch == BRANCH_ABYSS)
            short_name.insert(0, "The ");
        // Indefinite articles
        else if (place.branch != BRANCH_PANDEMONIUM &&
                 !is_connected_branch(place.branch))
        {
            short_name = article_a(short_name);
        }
    }
    _update_string(force_full, c.place, short_name, "place");
    _update_int(force_full, c.depth, brdepth[place.branch] > 1 ? you.depth : 0, "depth");

    if (m_origin.equals(-1, -1))
        m_origin = you.position;
    coord_def pos = you.position - m_origin;
    if (force_full || c.position != pos)
    {
        json_open_object("pos");
        json_write_int("x", pos.x);
        json_write_int("y", pos.y);
        json_close_object();
        c.position = pos;
    }

    if (force_full || _update_statuses(c))
    {
        json_open_array("status");
        for (const status_info &status : c.status)
        {
            json_open_object();
            if (!status.light_text.empty())
            {
                json_write_string("light", status.light_text);
                // split off any extra info, e.g. counts for things like Zot
                // and Flay. (Status db descriptions never have spaces.)
                string dbname = split_string(" ", status.light_text, true, true, 1)[0];
                // Don't claim Zot is impending when it's not near.
                if (dbname == "Zot" && status.light_colour == WHITE)
                    dbname = "Zot count";
                const string dbdesc = getLongDescription(dbname + " status");
                json_write_string("desc", dbdesc.size() ? dbdesc : "No description found");
            }
            if (!status.short_text.empty())
                json_write_string("text", status.short_text);
            if (status.light_colour)
                json_write_int("col", macro_colour(status.light_colour));
            json_close_object(true);
        }
        json_close_array();
    }

    json_open_object("inv");
    for (unsigned int i = 0; i < ENDOFPACK; ++i)
    {
        json_open_object(to_string(i));
        item_def item = you.inv[i];
        if (you.corrosion_amount() && is_weapon(item)
            && you.equipment.find_equipped_slot(item) != SLOT_UNUSED)
        {
            item.plus -= 1 * you.corrosion_amount();
        }
        _send_item(c.inv[i], item, c.inv_uselessness[i], force_full);
        json_close_object(true);
    }
    json_close_object(true);

    _update_int(force_full, c.quiver_item,
                (int8_t) you.quiver_action.get()->get_item(), "quiver_item");

    _update_string(force_full, c.quiver_desc,
                you.quiver_action.get()->quiver_description().to_colour_string(LIGHTGRAY),
                "quiver_desc");

    item_def* weapon = you.weapon();
    item_def* offhand = you.offhand_weapon();
    _update_int(force_full, c.weapon_index, (int8_t) (weapon ? weapon->link : -1), "weapon_index");
    _update_int(force_full, c.offhand_index, (int8_t) (offhand ? offhand->link : -1), "offhand_index");
    _update_int(force_full, c.offhand_weapon, (bool) offhand, "offhand_weapon");

    _update_string(force_full, c.unarmed_attack,
                   you.unarmed_attack_name(), "unarmed_attack");
    _update_int(force_full, c.unarmed_attack_colour,
                (uint8_t) get_form()->uc_colour, "unarmed_attack_colour");
    _update_int(force_full, c.quiver_available,
                    you.quiver_action.get()->is_valid()
                                && you.quiver_action.get()->is_enabled(),
                "quiver_available");

    json_close_object(true);

    finish_message();
}

// Checks if an item should be displayed on the action panel
static int _useful_consumable_order(const item_def &item, const string &name)
{
    const vector<object_class_type> &base_types = Options.action_panel;
    const auto order = std::find(base_types.begin(), base_types.end(),
                                                            item.base_type);

    if (item.quantity < 1
        || order == base_types.end() // covers the empty case
        || (!Options.action_panel_show_unidentified && !item.is_identified()))
    {
        return -1;
    }

    for (const text_pattern &p : Options.action_panel_filter)
        if (p.matches(name))
            return -1;

    return order - base_types.begin();
}

// Returns the name of an item_def field to display on the action panel
static string _qty_field_name(const item_def &item)
{
    if (item.base_type == OBJ_MISCELLANY && item.sub_type != MISC_ZIGGURAT
        || item.base_type == OBJ_WANDS)
    {
        return "plus";
    }
    else
        return "quantity";
}

void TilesFramework::_send_item(item_def& current, const item_def& next,
                                bool& current_uselessness,
                                bool force_full)
{
    bool changed = false;
    bool xp_evoker_changed = false;
    bool defined = next.defined();

    if (force_full || current.base_type != next.base_type)
    {
        changed = true;
        json_write_int("base_type", next.base_type);
    }

    changed |= _update_int(force_full, current.quantity, next.quantity,
                           "quantity", false);

    if (!defined)
    {
        current = next;
        return; // For undefined items, only send base_type and quantity
    }
    else if (!current.defined())
        force_full = true; // if the item was undefined before, send everything

    changed |= _update_int(force_full, current.sub_type, next.sub_type,
                           "sub_type", false);
    if (Options.action_panel_glyphs)
    {
        string cur_glyph = force_full ? "" : stringize_glyph(get_item_glyph(current).ch);
        string next_glyph = !defined ? "" : stringize_glyph(get_item_glyph(next).ch);
        changed |= _update_string(force_full, cur_glyph, next_glyph, "g", false);
    }
    if (is_xp_evoker(next))
    {
        short int next_charges = evoker_charges(next.sub_type);
        xp_evoker_changed = _update_int(force_full, current.plus,
                                        next_charges,
                                        PLUS_KEY, false);
        changed |= xp_evoker_changed;
    }
    else
    {
        changed |= _update_int(force_full, current.plus, next.plus,
                               PLUS_KEY, false);
    }
    changed |= _update_int(force_full, current.plus2, next.plus2,
                           "plus2", false);
    changed |= _update_int(force_full, current.flags, next.flags,
                           "flags", false);
    changed |= _update_string(force_full, current.inscription,
                              next.inscription, "inscription", false);

    // TODO: props?

    changed |= (current.special != next.special);

    // Derived stuff
    changed |= _update_int(force_full, current_uselessness,
                           is_useless_item(next, true), "useless");

    if (changed && defined)
    {
        string name = next.name(DESC_A, true, false, true);
        if (force_full || current.name(DESC_A, true, false, true) != name
            || xp_evoker_changed)
        {
            json_write_string("name", name);
        }

        // -1 in this field means don't show. *note*: showing in the action
        // panel has undefined behavior for item types that don't have a
        // quiver::action implementation...
        json_write_int("action_panel_order", _useful_consumable_order(next, name));
        json_write_string("qty_field", _qty_field_name(next));

        const string prefix = item_prefix(next);
        const int prefcol = menu_colour(next.name(DESC_INVENTORY), prefix, "inventory", false);
        if (force_full)
            json_write_int("col", macro_colour(prefcol));
        else
        {
            const string current_prefix = item_prefix(current);
            const int current_prefcol = menu_colour(
                current.name(DESC_INVENTORY), current_prefix,
                "inventory", false);

            if (current_prefcol != prefcol)
                json_write_int("col", macro_colour(prefcol));
        }

        // We have to check identification flags directly for exactly the case
        // of gaining item type knowledge of items the player already had.
        // Because item_def::is_identified() includes item knowledge, the old
        // copy of the item will appear to already be identified, but its tile
        // should be updated regardless.
        const bool id_state_changed = (current.flags & ISFLAG_IDENTIFIED)
                                        != (next.flags & ISFLAG_IDENTIFIED);
        tileidx_t tile = tileidx_item(next);
        if (force_full || tileidx_item(current) != tile || xp_evoker_changed
            || id_state_changed)
        {
            json_open_array("tile");
            tileidx_t base_tile = tileidx_known_base_item(tile);
            if (base_tile)
                json_write_int(base_tile);
            json_write_int(tile);
            json_close_array();
        }

        current = next;
        if (is_xp_evoker(current))
            current.plus = evoker_charges(current.sub_type);
        if (in_inventory(current))
        {
            auto action = quiver::slot_to_action(current.link, false);
            // TODO: does this stay in sync? Do anything with enabledness?
            if (action && action->is_valid())
                json_write_string("action_verb", action->quiver_verb());
        }
    }
}

void TilesFramework::send_doll(const dolls_data &doll, bool submerged, bool ghost)
{
    static int p_order[TILEP_PART_MAX];
    static int flags[TILEP_PART_MAX];

    tilep_fill_order_and_flags(doll, p_order, flags);

    tiles.json_open_array("doll");

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        const int ymax = flags[p] == TILEP_FLAG_CUT_BOTTOM ? 18 : TILE_Y;
        tiles.json_write_comma();
        tiles.write_message("[%u,%d]", (unsigned int) doll.parts[p], ymax);
    }
    tiles.json_close_array();
}

void TilesFramework::send_mcache(mcache_entry *entry, bool submerged, bool send)
{
    bool trans = entry->transparent();
    if (trans && send)
        tiles.json_write_int("trans", 1);

    const dolls_data *doll = entry->doll();
    if (send)
    {
        if (doll)
            send_doll(*doll, submerged, trans);
        else
        {
            tiles.json_write_comma();
            tiles.write_message("\"doll\":[]");
        }
    }

    tiles.json_open_array("mcache");

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        tiles.json_write_comma();
        tiles.write_message("[%u,%d,%d]", (unsigned int) dinfo[i].idx,
                            dinfo[i].ofs_x, dinfo[i].ofs_y);
    }

    tiles.json_close_array();
}

static bool _in_water(const packed_cell &cell)
{
    return (cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING);
}

static bool _needs_flavour(const packed_cell &cell)
{
    tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_FIRST_TRANSPARENT)
        return true; // Needs flv.floor
    if (cell.is_liquefied || cell.is_bloody)
        return true; // Needs flv.special
    return false;

}

// XX code duplicateion
static inline unsigned _get_highlight(int col)
{
    return ((col & COLFLAG_UNUSUAL_MASK) == COLFLAG_UNUSUAL_MASK) ?
                                              Options.unusual_highlight :
           (col & COLFLAG_FRIENDLY_MONSTER) ? Options.friend_highlight :
           (col & COLFLAG_NEUTRAL_MONSTER)  ? Options.neutral_highlight :
           (col & COLFLAG_ITEM_HEAP)        ? Options.heap_highlight :
           (col & COLFLAG_WILLSTAB)         ? Options.stab_highlight :
           (col & COLFLAG_MAYSTAB)          ? Options.may_stab_highlight :
           (col & COLFLAG_FEATURE_ITEM)     ? Options.feature_item_highlight :
           (col & COLFLAG_TRAP_ITEM)        ? Options.trap_item_highlight :
           (col & COLFLAG_REVERSE)          ? unsigned{CHATTR_REVERSE}
                                            : unsigned{CHATTR_NORMAL};
}

void TilesFramework::write_tileidx(tileidx_t t)
{
    // JS can only handle signed ints
    const int lo = t & 0xFFFFFFFF;
    const int hi = t >> 32;
    if (hi == 0)
        tiles.write_message("%d", lo);
    else
        tiles.write_message("[%d,%d]", lo, hi);
}

void TilesFramework::_send_cell(const coord_def &gc,
                                const screen_cell_t &current_sc, const screen_cell_t &next_sc,
                                const map_cell &current_mc, const map_cell &next_mc,
                                map<uint32_t, coord_def>& new_monster_locs,
                                bool force_full)
{
    if (current_mc.feat() != next_mc.feat())
        json_write_int("f", next_mc.feat());

    if (next_mc.monsterinfo())
        _send_monster(gc, next_mc.monsterinfo(), new_monster_locs, force_full);
    else if (current_mc.monsterinfo())
        json_write_null("mon");

    map_feature mf = get_cell_map_feature(gc);
    if (get_cell_map_feature(current_mc) != mf)
        json_write_int("mf", mf);

    // Glyph and colour
    char32_t glyph = next_sc.glyph;
    if (current_sc.glyph != glyph)
    {
        char buf[5];
        buf[wctoutf8(buf, glyph)] = 0;
        json_write_string("g", buf);
    }
    if ((current_sc.colour != next_sc.colour
         || current_sc.glyph == ' ') && glyph != ' ')
    {
        int col = next_sc.colour;
        col = (_get_highlight(col) << 4) | macro_colour(col & 0xF);
        json_write_int("col", col);
    }
    if (current_sc.flash_colour != next_sc.flash_colour)
        json_write_int("flc", next_sc.flash_colour);
    if (current_sc.flash_alpha != next_sc.flash_alpha)
        json_write_int("fla", next_sc.flash_alpha);

    json_open_object("t");
    {
        // Tile data
        const packed_cell &next_pc = next_sc.tile;
        const packed_cell &current_pc = current_sc.tile;

        const tileidx_t fg_idx = next_pc.fg & TILE_FLAG_MASK;

        const bool in_water = _in_water(next_pc);
        bool fg_changed = false;

        if (next_pc.fg != current_pc.fg)
        {
            fg_changed = true;

            json_write_name("fg");
            write_tileidx(next_pc.fg);
            if (get_tile_texture(fg_idx) == TEX_DEFAULT)
                json_write_int("base", (int) tileidx_known_base_item(fg_idx));
        }

        if (next_pc.bg != current_pc.bg)
        {
            json_write_name("bg");
            write_tileidx(next_pc.bg);
        }

        if (next_pc.cloud != current_pc.cloud)
        {
            json_write_name("cloud");
            write_tileidx(next_pc.cloud);
        }

        if (next_pc.icons != current_pc.icons)
            json_write_icons(next_pc.icons);

        if (Options.show_blood) {
            if (next_pc.is_bloody != current_pc.is_bloody)
                json_write_bool("bloody", next_pc.is_bloody);

            if (next_pc.old_blood != current_pc.old_blood)
                json_write_bool("old_blood", next_pc.old_blood);
        }

        if (next_pc.is_silenced != current_pc.is_silenced)
            json_write_bool("silenced", next_pc.is_silenced);

        if (next_pc.halo != current_pc.halo)
            json_write_int("halo", next_pc.halo);

        if (next_pc.is_highlighted_summoner
            != current_pc.is_highlighted_summoner)
        {
            json_write_bool("highlighted_summoner",
                            next_pc.is_highlighted_summoner);
        }

        if (next_pc.is_sanctuary != current_pc.is_sanctuary)
            json_write_bool("sanctuary", next_pc.is_sanctuary);
        if (next_pc.is_blasphemy != current_pc.is_blasphemy)
            json_write_bool("blasphemy", next_pc.is_blasphemy);

        if (next_pc.has_bfb_corpse != current_pc.has_bfb_corpse)
            json_write_bool("has_bfb_corpse", next_pc.has_bfb_corpse);

        if (next_pc.is_liquefied != current_pc.is_liquefied)
            json_write_bool("liquefied", next_pc.is_liquefied);

        if (next_pc.orb_glow != current_pc.orb_glow)
            json_write_int("orb_glow", next_pc.orb_glow);

        if (next_pc.quad_glow != current_pc.quad_glow)
            json_write_bool("quad_glow", next_pc.quad_glow);

        if (next_pc.disjunct != current_pc.disjunct)
            json_write_bool("disjunct", next_pc.disjunct);

        if (next_pc.mangrove_water != current_pc.mangrove_water)
            json_write_bool("mangrove_water", next_pc.mangrove_water);

        if (next_pc.awakened_forest != current_pc.awakened_forest)
            json_write_bool("awakened_forest", next_pc.awakened_forest);

        if (next_pc.blood_rotation != current_pc.blood_rotation)
            json_write_int("blood_rotation", next_pc.blood_rotation);

        if (next_pc.travel_trail != current_pc.travel_trail)
            json_write_int("travel_trail", next_pc.travel_trail);

        if (_needs_flavour(next_pc) &&
            (next_pc.flv.floor != current_pc.flv.floor
             || next_pc.flv.special != current_pc.flv.special
             || !_needs_flavour(current_pc)
             || force_full))
        {
            json_open_object("flv");
            json_write_int("f", next_pc.flv.floor);
            if (next_pc.flv.special)
                json_write_int("s", next_pc.flv.special);
            json_close_object();
        }

        if (fg_idx >= TILEP_MCACHE_START)
        {
            if (fg_changed)
            {
                mcache_entry *entry = mcache.get(fg_idx);
                if (entry)
                    send_mcache(entry, in_water);
                else
                {
                    json_write_comma();
                    write_message("\"doll\":[[%d,%d]]", TILEP_MONS_UNKNOWN, TILE_Y);
                    json_write_null("mcache");
                }
            }
        }
        else if (fg_idx == TILEP_PLAYER)
        {
            bool player_doll_changed = false;
            dolls_data result = player_doll;
            fill_doll_equipment(result);
            if (result != last_player_doll)
            {
                player_doll_changed = true;
                last_player_doll = result;
            }
            if (fg_changed || player_doll_changed)
            {
                send_doll(last_player_doll, in_water, false);
                if (player_uses_monster_tile())
                {
                    monster_info minfo(MONS_PLAYER, MONS_PLAYER);
                    minfo.props[MONSTER_TILE_KEY] =
                        int(last_player_doll.parts[TILEP_PART_BASE]);
                    item_def *item;
                    if (item = you.equipment.get_first_slot_item(SLOT_WEAPON))
                    {
                        item = new item_def(*item);
                        minfo.inv[MSLOT_WEAPON].reset(item);
                    }
                    if (item = you.equipment.get_first_slot_item(SLOT_OFFHAND))
                    {
                        item = new item_def(*item);
                        minfo.inv[MSLOT_SHIELD].reset(item);
                    }
                    tileidx_t mcache_idx = mcache.register_monster(minfo);
                    mcache_entry *entry = mcache.get(mcache_idx);
                    if (entry)
                        send_mcache(entry, in_water, false);
                    else
                        json_write_null("mcache");
                }
                else
                    json_write_null("mcache");
            }
        }
        else if (get_tile_texture(fg_idx) == TEX_PLAYER)
        {
            if (fg_changed)
            {
                json_write_comma();
                write_message("\"doll\":[[%u,%d]]", (unsigned int) fg_idx, TILE_Y);
                json_write_null("mcache");
            }
        }
        else
        {
            if (fg_changed)
            {
                json_write_comma();
                json_write_null("doll");
                json_write_null("mcache");
            }
        }

        bool overlays_changed = false;

        if (next_pc.num_dngn_overlay != current_pc.num_dngn_overlay)
            overlays_changed = true;
        else
        {
            for (int i = 0; i < next_pc.num_dngn_overlay; i++)
            {
                if (next_pc.dngn_overlay[i] != current_pc.dngn_overlay[i])
                {
                    overlays_changed = true;
                    break;
                }
            }
        }

        if (overlays_changed)
        {
            json_open_array("ov");
            for (int i = 0; i < next_pc.num_dngn_overlay; ++i)
                json_write_int(next_pc.dngn_overlay[i]);
            json_close_array();
        }
    }
    json_close_object(true);
}

void TilesFramework::_send_cursor(cursor_type type)
{
    if (m_cursor[type] == NO_CURSOR)
        send_message("{\"msg\":\"cursor\",\"id\":%d}", type);
    else
    {
        if (m_origin.equals(-1, -1))
            m_origin = m_cursor[type];
        send_message("{\"msg\":\"cursor\",\"id\":%d,\"loc\":{\"x\":%d,\"y\":%d}}",
                     type, m_cursor[type].x - m_origin.x,
                     m_cursor[type].y - m_origin.y);
    }
}

void TilesFramework::_mcache_ref(bool inc)
{
    for (int y = 0; y < GYM; y++)
        for (int x = 0; x < GXM; x++)
        {
            coord_def gc(x, y);

            int fg_idx = m_current_view(gc).tile.fg & TILE_FLAG_MASK;
            if (fg_idx >= TILEP_MCACHE_START)
            {
                mcache_entry *entry = mcache.get(fg_idx);
                if (entry)
                {
                    if (inc)
                        entry->inc_ref();
                    else
                        entry->dec_ref();
                }
            }
        }
}

void TilesFramework::_send_map(bool force_full)
{
    // TODO: prevent in some other / better way?
    if (_send_lock)
        return;

    unwind_bool no_rentry(_send_lock, true);

    map<uint32_t, coord_def> new_monster_locs;

    force_full = force_full || m_need_full_map;
    m_need_full_map = false;

    json_open_object();
    json_write_string("msg", "map");
    json_treat_as_empty();

    // cautionary note: this is used in heuristic ways in process_handler.py,
    // see `_is_full_map_msg`
    if (force_full)
        json_write_bool("clear", true);

    if (force_full || you.on_current_level != m_player_on_level)
    {
        json_write_bool("player_on_level", you.on_current_level);
        m_player_on_level = you.on_current_level;
    }

    if (force_full || m_current_gc != m_next_gc)
    {
        if (m_origin.equals(-1, -1))
            m_origin = m_next_gc;
        json_open_object("vgrdc");
        json_write_int("x", m_next_gc.x - m_origin.x);
        json_write_int("y", m_next_gc.y - m_origin.y);
        json_close_object();
        m_current_gc = m_next_gc;
    }

    screen_cell_t default_cell;
    default_cell.tile.bg = TILE_FLAG_UNSEEN;
    default_cell.glyph = ' ';
    default_cell.colour = 7;
    map_cell default_map_cell;

    coord_def last_gc(0, 0);
    bool send_gc = true;

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = viewmap_flash_colour();

    json_open_array("cells");
    for (int y = 0; y < GYM; y++)
        for (int x = 0; x < GXM; x++)
        {
            coord_def gc(x, y);

            if (!is_dirty(gc) && !force_full)
                continue;

            if (cell_needs_redraw(gc))
            {
                screen_cell_t *cell = &m_next_view(gc);

                if (you.flash_where && you.flash_where->is_affected(gc) <= 0)
                    draw_cell(cell, gc, false, 0);
                else
                    draw_cell(cell, gc, false, flash_colour);

                pack_cell_overlays(gc, m_next_view);
            }

            mark_clean(gc);

            if (m_origin.equals(-1, -1))
                m_origin = gc;

            json_open_object();
            if (send_gc
                || last_gc.x + 1 != gc.x
                || last_gc.y != gc.y)
            {
                json_write_int("x", x - m_origin.x);
                json_write_int("y", y - m_origin.y);
                json_treat_as_empty();
            }

            const screen_cell_t& sc = force_full ? default_cell
                : m_current_view(gc);
            const map_cell& mc = force_full ? default_map_cell
                : m_current_map_knowledge(gc);
            _send_cell(gc,
                       sc,
                       m_next_view(gc),
                       mc, env.map_knowledge(gc),
                       new_monster_locs, force_full);

            if (!json_is_empty())
            {
                send_gc = false;
                last_gc = gc;
            }
            json_close_object(true);
        }
    json_close_array(true);

    json_close_object(true);

    finish_message();

    if (force_full)
        _send_cursor(CURSOR_MAP);

    if (m_mcache_ref_done)
        _mcache_ref(false);

    m_current_map_knowledge = env.map_knowledge;
    m_current_view = m_next_view;

    _mcache_ref(true);
    m_mcache_ref_done = true;

    m_monster_locs = new_monster_locs;
}

void TilesFramework::_send_monster(const coord_def &gc, const monster_info* m,
                                   map<uint32_t, coord_def>& new_monster_locs,
                                   bool force_full)
{
    json_open_object("mon");
    if (m->client_id)
    {
        json_write_int("id", m->client_id);
        json_treat_as_empty();
        new_monster_locs[m->client_id] = gc;
    }

    const monster_info* last = nullptr;
    auto it = m_monster_locs.find(m->client_id);
    if (m->client_id == 0 || it == m_monster_locs.end())
    {
        last = m_current_map_knowledge(gc).monsterinfo();

        if (last && last->client_id != m->client_id)
            json_treat_as_nonempty(); // Force sending at least the id
    }
    else
    {
        last = m_current_map_knowledge(it->second).monsterinfo();

        if (it->second != gc)
            json_treat_as_nonempty(); // As above
    }

    if (last == nullptr)
        force_full = true;

    if (force_full || (last->full_name() != m->full_name()))
        json_write_string("name", m->full_name());

    if (force_full || (last->pluralised_name() != m->pluralised_name()))
        json_write_string("plural", m->pluralised_name());

    if (force_full || last->type != m->type)
    {
        json_write_int("type", m->type);

        // TODO: get this information to the client in another way
        json_open_object("typedata");
        json_write_int("avghp", mons_avg_hp(m->type));
        if (!mons_class_gives_xp(m->type))
            json_write_bool("no_exp", true);
        json_close_object();
    }

    if (force_full || last->attitude != m->attitude)
        json_write_int("att", m->attitude);

    if (force_full || last->base_type != m->base_type)
        json_write_int("btype", m->base_type);

    if (force_full || last->threat != m->threat)
        json_write_int("threat", m->threat);

    // tiebreakers for two monsters with the same custom name
    if (m->is_named())
        json_write_int("clientid", m->client_id);

    json_close_object(true);
}

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    if (vbuf.size().equals(0, 0))
        return;

    m_view_loaded = true;

    if (m_ui_state == UI_CRT)
        set_ui_state(UI_NORMAL);

    // First re-render the area that was covered by vbuf the last time
    for (int y = m_next_view_tl.y; y <= m_next_view_br.y; y++)
        for (int x = m_next_view_tl.x; x <= m_next_view_br.x; x++)
        {
            if (x < 0 || x >= GXM || y < 0 || y >= GYM)
                continue;

            if (!crawl_view.in_viewport_g(coord_def(x, y)))
                mark_for_redraw(coord_def(x, y));
        }

    // re-cache the map knowledge for the whole map, not just the updated portion
    // fixes render bugs for out-of-LOS when transitioning levels in shoals/slime
    for (int y = 0; y < GYM; y++)
        for (int x = 0; x < GXM; x++)
        {
            const coord_def cache_gc(x, y);
            screen_cell_t *cell = &m_next_view(cache_gc);
            cell->tile.map_knowledge = map_bounds(cache_gc) ? env.map_knowledge(cache_gc) : map_cell();
        }

    m_next_view_tl = view2grid(coord_def(1, 1));
    m_next_view_br = view2grid(crawl_view.viewsz);

    // Copy vbuf into m_next_view
    for (int y = 0; y < vbuf.size().y; y++)
        for (int x = 0; x < vbuf.size().x; x++)
        {
            coord_def pos(x+1, y+1);
            coord_def grid = view2grid(pos);

            if (grid.x < 0 || grid.x >= GXM || grid.y < 0 || grid.y >= GYM)
                continue;

            screen_cell_t *cell = &m_next_view(grid);

            *cell = ((const screen_cell_t *) vbuf)[x + vbuf.size().x * y];
            pack_cell_overlays(grid, m_next_view);

            mark_clean(grid); // Remove redraw flag
            mark_dirty(grid);
        }

    m_next_gc = gc;
}

void TilesFramework::load_dungeon(const coord_def &cen)
{
    unwind_var<coord_def> viewp(crawl_view.viewp, cen - crawl_view.viewhalfsz);
    unwind_var<coord_def> vgrdc(crawl_view.vgrdc, cen);
    unwind_var<coord_def> vlos1(crawl_view.vlos1);
    unwind_var<coord_def> vlos2(crawl_view.vlos2);

    m_next_gc = cen;

    crawl_view.calc_vlos();
    viewwindow(false, true);
    place_cursor(CURSOR_MAP, cen);
}

void TilesFramework::resize()
{
    m_text_menu.resize(crawl_view.termsz.x, crawl_view.termsz.y);
}

void TilesFramework::_send_messages()
{
    if (_send_lock)
        return;
    unwind_bool no_rentry(_send_lock, true);

    webtiles_send_messages();
}

/*
  Send everything a newly joined spectator needs
 */
void TilesFramework::_send_everything()
{
    // note: a player client will receive and process some of these messages,
    // but not all. This function is currently never called except for
    // spectators, and some of the semantics here reflect this.
    _send_version();
    send_options();
    _send_layout();

    _send_text_cursor(m_text_cursor);

    // UI State
    _send_ui_state(m_ui_state);
    m_last_ui_state = m_ui_state;

    _send_cursor(CURSOR_MOUSE);
    _send_cursor(CURSOR_TUTORIAL);

     // Player
    _send_player(true);

    // Map is sent after player, otherwise HP/MP bar can be left behind in the
    // old location if the player has moved
    _send_map(true);

    // Menus
    json_open_object();
    json_write_string("msg", "ui-stack");
    json_open_array("items");
    for (UIStackFrame &frame : m_menu_stack)
    {
        json_write_comma(); // noop immediately following open
        if (frame.type == UIStackFrame::MENU)
            frame.menu->webtiles_write_menu();
        else if (frame.type == UIStackFrame::CRT)
        {
            json_open_object();
            json_write_string("msg", "menu");
            json_write_string("type", "crt");
            json_write_string("tag", frame.crt_tag);
            json_write_bool("ui-centred", frame.centred);
            json_close_object();
        }
        else
        {
            for (const auto& json : frame.ui_json)
                if (!json.empty())
                {
                    json_write_comma();
                    m_msg_buf.append(json);
                }
            continue;
        }
    }
    json_close_array();
    json_close_object();
    finish_message();

    _send_messages();

    update_input_mode(mouse_control::current_mode(), true);

    m_text_menu.send(true);

    ui::sync_ui_state();
}

void TilesFramework::clrscr()
{
    m_text_menu.clear();

    cgotoxy(1, 1);

    set_need_redraw();
}

void TilesFramework::layout_reset()
{
    m_layout_reset = true;
}

void TilesFramework::cgotoxy(int x, int y, GotoRegion region)
{
    m_print_x = x - 1;
    m_print_y = y - 1;

    bool crt_popup = region == GOTO_CRT && !m_menu_stack.empty() &&
            m_menu_stack.back().type == UIStackFrame::CRT;
    m_print_area = crt_popup ? &m_text_menu : nullptr;
    m_cursor_region = region;
}

void TilesFramework::redraw()
{
    if (!has_receivers())
    {
        if (m_mcache_ref_done)
        {
            _mcache_ref(false);
            m_mcache_ref_done = false;
        }
        return;
    }

    if (m_layout_reset)
    {
        _send_layout();
        m_layout_reset = false;
    }

    if (m_last_text_cursor != m_text_cursor)
    {
        _send_text_cursor(m_text_cursor);
        m_last_text_cursor = m_text_cursor;
    }

    if (m_last_ui_state != m_ui_state)
    {
        _send_ui_state(m_ui_state);
        m_last_ui_state = m_ui_state;
    }

    m_text_menu.send();

    _send_player();
    _send_messages();

    if (m_need_redraw && m_view_loaded)
        _send_map(false);

    m_need_redraw = false;
    m_last_tick_redraw = get_milliseconds();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
    if (gc.x < 0 || gc.x >= GXM || gc.y < 0 || gc.y >= GYM)
        return;

    mark_for_redraw(gc);
}

void TilesFramework::clear_minimap()
{
    m_origin = coord_def(-1, -1);
    // Changing the origin invalidates coordinates on the client side
    m_current_gc = coord_def(-1, -1);
    m_need_full_map = true;
}

void TilesFramework::update_minimap_bounds()
{
}

void TilesFramework::update_tabs()
{
}

void TilesFramework::place_cursor(cursor_type type, const coord_def &gc)
{
    // This is mainly copied from DungeonRegion::place_cursor.
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != INVALID_COORD)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            m_last_clicked_grid = coord_def();

        // if map is going to be updated, send the cursor after that
        if (type == CURSOR_MAP && m_need_full_map)
            return;

        _send_cursor(type);
    }
}

void TilesFramework::clear_text_tags(text_tag_type /*type*/)
{
}

void TilesFramework::add_text_tag(text_tag_type /*type*/, const string &/*tag*/,
                                  const coord_def &/*gc*/)
{
}

void TilesFramework::add_text_tag(text_tag_type /*type*/, const monster_info& /*mon*/)
{
}

const coord_def &TilesFramework::get_cursor() const
{
    return m_cursor[CURSOR_MOUSE];
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    unsigned int ticks = (get_milliseconds() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

void TilesFramework::textcolour(int col)
{
    m_print_fg = col & 0xF;
    m_print_bg = (col >> 4) & 0xF;
}

void TilesFramework::textbackground(int col)
{
    m_print_bg = col;
}

void TilesFramework::put_ucs_string(char32_t *str)
{
    if (m_print_area == nullptr)
        return;

    while (*str)
    {
        if (*str == '\r')
            continue;

        if (*str == '\n')
        {
            m_print_x = 0;
            m_print_y++;
            // TODO: Clear end of line?
        }
        else
        {
            if (m_print_x >= m_print_area->mx)
            {
                m_print_x = 0;
                m_print_y++;
            }

            if (m_print_y < m_print_area->my)
            {
                m_print_area->put_character(*str, m_print_fg, m_print_bg,
                                            m_print_x, m_print_y);
            }

            m_print_x++;
        }

        str++;
    }
}

void TilesFramework::clear_to_end_of_line()
{
    if (m_print_area == nullptr || m_print_y >= m_print_area->my)
        return;

    for (int x = m_print_x; x < m_print_area->mx; ++x)
        m_print_area->put_character(' ', m_print_fg, m_print_bg, x, m_print_y);
}

void TilesFramework::mark_for_redraw(const coord_def& gc)
{
    mark_dirty(gc);
    m_cells_needing_redraw[gc.y * GXM + gc.x] = true;
}

void TilesFramework::mark_dirty(const coord_def& gc)
{
    m_dirty_cells[gc.y * GXM + gc.x] = true;
}

void TilesFramework::mark_clean(const coord_def& gc)
{
    m_cells_needing_redraw[gc.y * GXM + gc.x] = false;
    m_dirty_cells[gc.y * GXM + gc.x] = false;
}

bool TilesFramework::is_dirty(const coord_def& gc)
{
    return m_dirty_cells[gc.y * GXM + gc.x];
}

bool TilesFramework::cell_needs_redraw(const coord_def& gc)
{
    return m_cells_needing_redraw[gc.y * GXM + gc.x];
}

void TilesFramework::write_message_escaped(const string& s)
{
    for (unsigned char c : s)
    {
        if (c == '"')
            m_msg_buf.append("\\\"");
        else if (c == '\\')
            m_msg_buf.append("\\\\");
        else if (c < 0x20)
        {
            char buf[7];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            m_msg_buf.append(buf);
        }
        else
            m_msg_buf.push_back(c);
    }
}

void TilesFramework::json_open(const string& name, char opener, char type)
{
    m_json_stack.resize(m_json_stack.size() + 1);
    JsonFrame& fr = m_json_stack.back();
    fr.start = m_msg_buf.size();

    json_write_comma();
    if (!name.empty())
        json_write_name(name);

    m_msg_buf.append(1, opener);

    fr.prefix_end = m_msg_buf.size();
    fr.type = type;
}

void TilesFramework::json_treat_as_empty()
{
    if (m_json_stack.empty())
        die("json error: empty stack");
    m_json_stack.back().prefix_end = m_msg_buf.size();
}

void TilesFramework::json_treat_as_nonempty()
{
    if (m_json_stack.empty())
        die("json error: empty stack");
    m_json_stack.back().prefix_end = -1;
}

bool TilesFramework::json_is_empty()
{
    if (m_json_stack.empty())
        die("json error: empty stack");
    return m_json_stack.back().prefix_end == (int) m_msg_buf.size();
}

void TilesFramework::json_close(bool erase_if_empty, char type)
{
    if (m_json_stack.empty())
        die("json error: attempting to close object/array on empty stack");
    if (m_json_stack.back().type != type)
        die("json error: attempting to close wrong type");

    if (erase_if_empty && json_is_empty())
        m_msg_buf.resize(m_json_stack.back().start);
    else
        m_msg_buf.append(1, type);

    m_json_stack.pop_back();
}

void TilesFramework::json_open_object(const string& name)
{
    json_open(name, '{', '}');
}

void TilesFramework::json_close_object(bool erase_if_empty)
{
    json_close(erase_if_empty, '}');
}

void TilesFramework::json_open_array(const string& name)
{
    json_open(name, '[', ']');
}

void TilesFramework::json_close_array(bool erase_if_empty)
{
    json_close(erase_if_empty, ']');
}

void TilesFramework::json_write_comma()
{
    if (m_msg_buf.empty())
        return;
    char last = m_msg_buf[m_msg_buf.size() - 1];
    if (last == '{' || last == '[' || last == ',' || last == ':')
        return;
    write_message(",");
}

void TilesFramework::json_write_icons(const set<tileidx_t> &icons)
{
    json_open_array("icons");
    for (const tileidx_t icon : icons)
    {
        json_write_comma(); // skipped for the first one
        write_tileidx(icon);
    }
    json_close_array();
}

void TilesFramework::json_write_name(const string& name)
{
    json_write_comma();

    write_message("\"");
    write_message_escaped(name);
    write_message("\":");
}

void TilesFramework::json_write_int(int value)
{
    json_write_comma();

    write_message("%d", value);
}

void TilesFramework::json_write_int(const string& name, int value)
{
    if (!name.empty())
        json_write_name(name);

    json_write_int(value);
}

void TilesFramework::json_write_bool(bool value)
{
    json_write_comma();

    if (value)
        write_message("true");
    else
        write_message("false");
}

void TilesFramework::json_write_bool(const string& name, bool value)
{
    if (!name.empty())
        json_write_name(name);

    json_write_bool(value);
}

void TilesFramework::json_write_null()
{
    json_write_comma();

    write_message("null");
}

void TilesFramework::json_write_null(const string& name)
{
    if (!name.empty())
        json_write_name(name);

    json_write_null();
}

void TilesFramework::json_write_string(const string& value)
{
    json_write_comma();

    write_message("\"");
    write_message_escaped(value);
    write_message("\"");
}

void TilesFramework::json_write_string(const string& name, const string& value)
{
    if (!name.empty())
        json_write_name(name);

    json_write_string(value);
}

bool is_tiles()
{
    return tiles.is_controlled_from_web();
}
#endif

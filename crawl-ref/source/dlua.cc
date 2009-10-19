/*
 *  File:       dlua.cc
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 */

#include "AppHdr.h"

#include <sstream>
#include <algorithm>
#include <memory>
#include <cmath>

#include "dlua.h"
#include "l_libs.h"

#include "branch.h"
#include "chardump.h"
#include "clua.h"
#include "cloud.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "hiscores.h"
#include "initfile.h"
#include "items.h"
#include "los.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monplace.h"
#include "monstuff.h"
#include "place.h"
#include "spells3.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "view.h"

#ifdef UNIX
#include <sys/time.h>
#include <time.h>
#endif

template <typename list, typename lpush>
static int dlua_gentable(lua_State *ls, const list &strings, lpush push)
{
    lua_newtable(ls);
    for (int i = 0, size = strings.size(); i < size; ++i)
    {
        push(ls, strings[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

inline static void dlua_pushcxxstring(lua_State *ls, const std::string &s)
{
    lua_pushstring(ls, s.c_str());
}

int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s)
{
    return dlua_gentable(ls, s, dlua_pushcxxstring);
}

static int dlua_compiled_chunk_writer(lua_State *ls, const void *p,
                                      size_t sz, void *ud)
{
    std::ostringstream &out = *static_cast<std::ostringstream*>(ud);
    out.write((const char *) p, sz);
    return (0);
}

///////////////////////////////////////////////////////////////////////////
// dlua_chunk

dlua_chunk::dlua_chunk(const std::string &_context)
    : file(), chunk(), compiled(), context(_context), first(-1),
      last(-1), error()
{
    clear();
}

// Initialises a chunk from the function on the top of stack.
// This function must not be a closure, i.e. must not have any upvalues.
dlua_chunk::dlua_chunk(lua_State *ls)
    : file(), chunk(), compiled(), context(), first(-1), last(-1), error()
{
    clear();

    lua_stack_cleaner cln(ls);
    std::ostringstream out;
    const int err = lua_dump(ls, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(ls, -1);
        error = e? e : "Unknown error compiling chunk";
    }
    compiled = out.str();
}

dlua_chunk dlua_chunk::precompiled(const std::string &chunk)
{
    dlua_chunk dchunk;
    dchunk.compiled = chunk;
    return (dchunk);
}

void dlua_chunk::write(writer& outf) const
{
    if (empty())
    {
        marshallByte(outf, CT_EMPTY);
        return;
    }

    if (!compiled.empty())
    {
        marshallByte(outf, CT_COMPILED);
        marshallString4(outf, compiled);
    }
    else
    {
        marshallByte(outf, CT_SOURCE);
        marshallString4(outf, chunk);
    }

    marshallString4(outf, file);
    marshallLong(outf, first);
}

void dlua_chunk::read(reader& inf)
{
    clear();
    chunk_t type = static_cast<chunk_t>(unmarshallByte(inf));
    switch (type)
    {
    case CT_EMPTY:
        return;
    case CT_SOURCE:
        unmarshallString4(inf, chunk);
        break;
    case CT_COMPILED:
        unmarshallString4(inf, compiled);
        break;
    }
    unmarshallString4(inf, file);
    first = unmarshallLong(inf);
}

void dlua_chunk::clear()
{
    file.clear();
    chunk.clear();
    first = last = -1;
    error.clear();
    compiled.clear();
}

void dlua_chunk::set_file(const std::string &s)
{
    file = s;
}

void dlua_chunk::add(int line, const std::string &s)
{
    if (first == -1)
        first = line;

    if (line != last && last != -1)
        while (last++ < line)
            chunk += '\n';

    chunk += " ";
    chunk += s;
    last = line;
}

void dlua_chunk::set_chunk(const std::string &s)
{
    chunk = s;
}

int dlua_chunk::check_op(CLua &interp, int err)
{
    error = interp.error;
    return (err);
}

int dlua_chunk::load(CLua &interp)
{
    if (!compiled.empty())
        return check_op( interp,
                         interp.loadbuffer(compiled.c_str(), compiled.length(),
                                           context.c_str()) );

    if (empty())
    {
        chunk.clear();
        return (-1000);
    }

    int err = check_op( interp,
                        interp.loadstring(chunk.c_str(), context.c_str()) );
    if (err)
        return (err);
    std::ostringstream out;
    err = lua_dump(interp, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(interp, -1);
        error = e? e : "Unknown error compiling chunk";
        lua_pop(interp, 2);
    }
    compiled = out.str();
    chunk.clear();
    return (err);
}

int dlua_chunk::run(CLua &interp)
{
    int err = load(interp);
    if (err)
        return (err);
    // callfn returns true on success, but we want to return 0 on success.
    return (check_op(interp, !interp.callfn(NULL, 0, 0)));
}

int dlua_chunk::load_call(CLua &interp, const char *fn)
{
    int err = load(interp);
    if (err == -1000)
        return (0);
    if (err)
        return (err);

    return check_op(interp, !interp.callfn(fn, fn? 1 : 0, 0));
}

std::string dlua_chunk::orig_error() const
{
    rewrite_chunk_errors(error);
    return (error);
}

bool dlua_chunk::empty() const
{
    return compiled.empty() && trimmed_string(chunk).empty();
}

bool dlua_chunk::rewrite_chunk_errors(std::string &s) const
{
    const std::string contextm = "[string \"" + context + "\"]:";
    std::string::size_type dlwhere = s.find(contextm);

    if (dlwhere == std::string::npos)
        return (false);

    if (!dlwhere)
    {
        s = rewrite_chunk_prefix(s);
        return (true);
    }

    // Our chunk is mentioned, go back through and rewrite lines.
    std::vector<std::string> lines = split_string("\n", s);
    std::string newmsg = lines[0];
    bool wrote_prefix = false;
    for (int i = 2, size = lines.size() - 1; i < size; ++i)
    {
        const std::string &st = lines[i];
        if (st.find(context) != std::string::npos)
        {
            if (!wrote_prefix)
            {
                newmsg = get_chunk_prefix(st) + ": " + newmsg;
                wrote_prefix = true;
            }
            else
                newmsg += "\n" + rewrite_chunk_prefix(st);
        }
    }
    s = newmsg;
    return (true);
}

std::string dlua_chunk::rewrite_chunk_prefix(const std::string &line,
                                             bool skip_body) const
{
    std::string s = line;
    const std::string contextm = "[string \"" + context + "\"]:";
    const std::string::size_type ps = s.find(contextm);
    if (ps == std::string::npos)
        return (s);

    const std::string::size_type lns = ps + contextm.length();
    std::string::size_type pe = s.find(':', ps + contextm.length());
    if (pe != std::string::npos)
    {
        const std::string line_num = s.substr(lns, pe - lns);
        const int lnum = atoi(line_num.c_str());
        const std::string newlnum = make_stringf("%d", lnum + first - 1);
        s = s.substr(0, lns) + newlnum + s.substr(pe);
        pe = lns + newlnum.length();
    }

    return s.substr(0, ps) + (file.empty()? context : file) + ":"
        + (skip_body? s.substr(lns, pe - lns)
                    : s.substr(lns));
}

std::string dlua_chunk::get_chunk_prefix(const std::string &sorig) const
{
    return rewrite_chunk_prefix(sorig, true);
}

void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta)
{
    luaL_newmetatable(ls, meta);
    lua_setglobal(ls, global);

    luaL_openlib(ls, global, lua_lib, 0);

    // Do <global>.__index = <global>
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);
}

static void luaopen_dgnevent(lua_State *ls)
{
    luaopen_setmeta(ls, "dgnevent", dgnevent_lib, DEVENT_METATABLE);
}

static int mapmarker_pos(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    lua_pushnumber(ls, mark->pos.x);
    lua_pushnumber(ls, mark->pos.y);
    return (2);
}

static int mapmarker_move(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    const coord_def dest( luaL_checkint(ls, 2), luaL_checkint(ls, 3) );
    env.markers.move_marker(mark, dest);
    return (0);
}

static const struct luaL_reg mapmarker_lib[] =
{
    { "pos", mapmarker_pos },
    { "move", mapmarker_move },
    { NULL, NULL }
};

static void luaopen_mapmarker(lua_State *ls)
{
    luaopen_setmeta(ls, "mapmarker", mapmarker_lib, MAPMARK_METATABLE);
}


// Return the integer stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static int _table_int(lua_State *ls, int idx, const char *name, int defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isnumber(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not an int.", name);
    int ret = (!nil && valid ? lua_tonumber(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

// Return the character stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static char _table_char(lua_State *ls, int idx, const char *name, char defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isstring(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a string.", name);

    char ret = defval;
    if (!nil && valid)
    {
        const char *str = lua_tostring(ls, idx);
        if (str[0] && !str[1])
            ret = str[0];
        else
            luaL_error(ls, "'%s' has more than one character.", name);
    }
    lua_pop(ls, 1);
    return (ret);
}

// Return the string stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static const char* _table_str(lua_State *ls, int idx, const char *name, const char *defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isstring(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a string.", name);
    const char *ret = (!nil && valid ? lua_tostring(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

// Return the boolean stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static bool _table_bool(lua_State *ls, int idx, const char *name, bool defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isboolean(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a bool.", name);
    bool ret = (!nil && valid ? lua_toboolean(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

#define BF_INT(ls, val, def) int val = _table_int(ls, -1, #val, def);
#define BF_CHAR(ls, val, def) char val = _table_char(ls, -1, #val, def);
#define BF_STR(ls, val, def) const char *val = _table_str(ls, -1, #val, def);
#define BF_BOOL(ls, val, def) bool val = _table_bool(ls, -1, #val, def);

static void bf_octa_room(lua_State *ls, map_lines &lines)
{
    int default_oblique = std::min(lines.width(), lines.height()) / 2 - 1;
    BF_INT(ls, oblique, default_oblique);
    BF_CHAR(ls, outside, 'x');
    BF_CHAR(ls, inside, '.');
    BF_STR(ls, replace, ".");

    coord_def tl, br;
    if (!lines.find_bounds(replace, tl, br))
        return;

    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        const coord_def mc = *ri;
        char glyph = lines(mc);
        if (replace[0] && !strchr(replace, glyph))
            continue;

        int ob = 0;
        ob += std::max(oblique + tl.x - mc.x, 0);
        ob += std::max(oblique + mc.x - br.x, 0);

        bool is_inside = (mc.y >= tl.y + ob && mc.y <= br.y - ob);
        lines(mc) = is_inside ? inside : outside;
    }
}

static void bf_smear(lua_State *ls, map_lines &lines)
{
    BF_INT(ls, iterations, 1);
    BF_CHAR(ls, smear, 'x');
    BF_STR(ls, onto, ".");
    BF_BOOL(ls, boxy, false);

    const int max_test_per_iteration = 10;
    int sanity = 0;
    int max_sanity = iterations * max_test_per_iteration;

    for (int i = 0; i < iterations; i++)
    {
        bool diagonals, straights;
        coord_def mc;

        do
        {
            do
            {
                sanity++;
                mc.x = random_range(1, lines.width() - 2);
                mc.y = random_range(1, lines.height() - 2);
            }
            while (onto[0] && !strchr(onto, lines(mc)));
  
            // Prevent too many iterations. 
            if (sanity > max_sanity)
                return;
        
            // Is there a "smear" feature along the diagonal from mc?    
            diagonals = lines(coord_def(mc.x+1, mc.y+1)) == smear ||
                        lines(coord_def(mc.x-1, mc.y+1)) == smear ||
                        lines(coord_def(mc.x-1, mc.y-1)) == smear ||
                        lines(coord_def(mc.x+1, mc.y-1)) == smear;

            // Is there a "smear" feature up, down, left, or right from mc?
            straights = lines(coord_def(mc.x+1, mc.y)) == smear ||
                        lines(coord_def(mc.x-1, mc.y)) == smear ||
                        lines(coord_def(mc.x, mc.y+1)) == smear ||
                        lines(coord_def(mc.x, mc.y-1)) == smear;
        }
        while (!straights && (boxy || !diagonals));
     
        lines(mc) = smear; 
    }
}

static void bf_extend(lua_State *ls, map_lines &lines)
{
    BF_INT(ls, height, 1);
    BF_INT(ls, width, 1);
    BF_CHAR(ls, fill, 'x');

    lines.extend(width, height, fill);
}

typedef void (*bf_func)(lua_State *ls, map_lines &lines);
struct bf_entry
{
    const char* name;
    bf_func func;
};

// Create a separate list of builder funcs so that we can automatically
// generate a list of closures for them, rather than individually
// and explicitly exposing them to the dgn namespace.
static struct bf_entry bf_map[] =
{
    { "map_octa_room", &bf_octa_room },
    { "map_smear", &bf_smear },
    { "map_extend", &bf_extend }
};

static int dgn_call_builder_func(lua_State *ls)
{
    // This function gets called for all the builder functions that
    // operate on map_lines.

    MAP(ls, 1, map);
    if (!lua_istable(ls, 2) && !lua_isfunction(ls, 2))
        return luaL_argerror(ls, 2, "Expected table");

    bf_func *func = (bf_func *)lua_topointer(ls, lua_upvalueindex(1));
    if (!func)
        return luaL_error(ls, "Expected C function in closure upval");
 
    // Put the table on top.
    lua_settop(ls, 2);

    // Call the builder func itself.
    (*func)(ls, map->map);

    return (0);
}

static void _register_builder_funcs(lua_State *ls)
{
    lua_getglobal(ls, "dgn");
   
    const size_t num_entries = sizeof(bf_map) / sizeof(bf_entry); 
    for (size_t i = 0; i < num_entries; i++)
    {
        // Push a closure with the C function into the dgn table.
        lua_pushlightuserdata(ls, &bf_map[i].func);
        lua_pushcclosure(ls, &dgn_call_builder_func, 1);
        lua_setfield(ls, -2, bf_map[i].name);
    }

    lua_pop(ls, 1);
}

void init_dungeon_lua()
{
    lua_stack_cleaner clean(dlua);

    luaL_openlib(dlua, "dgn", dgn_lib, 0);
    // Add additional function to the Crawl module.
    luaL_openlib(dlua, "crawl", crawl_lib, 0);
    luaL_openlib(dlua, "file", file_lib, 0);
    luaL_openlib(dlua, "you", you_lib, 0);
    luaL_openlib(dlua, "los", los_lib, 0);

    dlua.execfile("clua/dungeon.lua", true, true);
    dlua.execfile("clua/luamark.lua", true, true);

    lua_getglobal(dlua, "dgn_run_map");
    luaopen_debug(dlua);
    luaL_newmetatable(dlua, MAP_METATABLE);

    luaopen_dgnevent(dlua);
    luaopen_mapmarker(dlua);
    luaopen_ray(dlua);

    _register_builder_funcs(dlua);

    register_mapdef_tables(dlua);
}

// Can be called from within a debugger to look at the current Lua
// call stack.  (Borrowed from ToME 3)
void print_dlua_stack(void)
{
    struct lua_Debug dbg;
    int              i = 0;
    lua_State       *L = dlua.state();

    fprintf(stderr, EOL);
    while (lua_getstack(L, i++, &dbg) == 1)
    {
        lua_getinfo(L, "lnuS", &dbg);

        char* file = strrchr(dbg.short_src, '/');
        if (file == NULL)
            file = dbg.short_src;
        else
            file++;

        fprintf(stderr, "%s, function %s, line %d" EOL, file,
                dbg.name, dbg.currentline);
    }

    fprintf(stderr, EOL);
}

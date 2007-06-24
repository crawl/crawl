/*
 *  File:       luadgn.h
 *  Summary:    Dungeon-builder Lua interface.
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-06-21T19:20:47.183838Z $
 */

#ifndef LUADGN_H
#define LUADGN_H

#include "AppHdr.h"
#include "clua.h"

extern CLua dlua;

class dlua_chunk
{
private:
    std::string file;
    std::string chunk;
    std::string context;
    int first, last;     // First and last lines of the original source.

private:
    int check_op(CLua *, int);
    std::string rewrite_chunk_prefix(const std::string &line) const;
    std::string get_chunk_prefix(const std::string &s) const;
    
public:
    std::string error;

public:
    dlua_chunk(const std::string &_context = "dlua_chunk");
    void clear();
    void add(int line, const std::string &line);
    void set_chunk(const std::string &s);
    
    int load(CLua *interp);
    int load_call(CLua *interp, const char *function);
    void set_file(const std::string &s);

    const std::string &lua_string() const { return chunk; }
    std::string orig_error() const;
    bool rewrite_chunk_errors(std::string &err) const;
    
    bool empty() const;
};

void init_dungeon_lua();
std::string dgn_set_default_depth(const std::string &s);
void dgn_reset_default_depth();

#endif

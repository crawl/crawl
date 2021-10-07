/**
 * @file
 * @brief Provide branch data as JSON
 */

#include "AppHdr.h"

#include "branch-data-json.h"

#include "json.h"
#include "json-wrapper.h"

#include "branch.h"
#include "stringutil.h" // to_string on Cygwin

static JsonNode *_branch_info_array()
{
    JsonNode *br(json_mkarray());
    for (branch_iterator it; it; ++it)
    {
        JsonNode *branch_info(json_mkobject());
        json_append_member(branch_info, "name", json_mkstring(it->shortname));
        json_append_member(branch_info, "long_name",
            json_mkstring(it->longname));
        json_append_member(branch_info, "levels", json_mknumber(it->numlevels));
        json_append_member(branch_info, "has_rune",
            json_mkbool(it->runes.size() > 0 ? true : false));
        json_append_element(br, branch_info);
    }
    return br;
}

string branch_data_json()
{
    JsonWrapper json(json_mkobject());
    json_append_member(json.node, "branches", _branch_info_array());
    return json.to_string();
}

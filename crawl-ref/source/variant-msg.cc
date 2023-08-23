/*
 * Attack messages
*/

#include <map>
#include <vector>
#include <sstream>

#include "variant-msg.h"

using namespace std;

static map<variant_msg_type, vector<string> > _messages =
{
    // elemental damage - reflexive needed?
    {VMSG_MELT,
        {"You melt %s", "%s melts you", "%s melts %s",
         "You melt yourself", "%s is melted"}},
    {VMSG_BURN,
        {"You burn %s", "%s burns you", "%s burns %s",
         "You burn yourself", "%s is burnt"}},
    {VMSG_FREEZE,
        {"You freeze %s", "%s freezes you", "%s freezes %s",
         "You freeze yourself", "%s is frozen"}},
    {VMSG_ELECTROCUTE,
        {"You electrocute %s", "%s electrocutes you", "%s electrocutes %s",
         "You electrocute yourself", "%s is electrocuted"}},

    {VMSG_CRUSH,
        {"You crush %s", "%s crushes you", "%s crushes %s"}},
    {VMSG_SHATTER,
        {"You shatter %s", "%s shatters you", "%s shatters %s"}},
    {VMSG_ENVENOM,
        {"You envenom %s", "%s envenoms you", "%s envenoms %s"}},
    {VMSG_DRAIN,
        {"You drain %s", "%s drains you", "%s drains %s"}},
    {VMSG_BLAST,
        {"You blast %s", "%s blasts you", "%s blasts %s"}},
};

static string _error;

/*
 * Return English message template
 * (Not translated and placeholders not replaced with values)
 */
const string& get_variant_template(variant_msg_type msg_id, msg_variant_type variant)
{
    auto it = _messages.find(msg_id);
    if (it != _messages.end())
    {
       const vector<string>& v = it->second;
       if (v.size() > variant && !v.at(variant).empty())
       {
           return v.at(variant);
       }
    }

    // something went wrong
    ostringstream os;
    if (msg_id == VMSG_NONE)
    {
        os << "ERROR: Attempt to use VMSG_NONE"; // @noloc
    }
    else
    {
        os << "ERROR: Undefined variant message (" // @noloc
           << msg_id << ", " << variant << ")";   // @noloc
    }

    // return value is reference, so object referred to must be persistent
    _error = os.str();
    return _error;
}

/*
 * Variant messages
 * Messages that change based on the grammatical person of the subject and object
 * (e.g. "You hit <the monster>", "<The monster> hits you", "<The monster> hits <the monster>")
 */
#pragma once

#include <string>
using namespace std;

#include "actor.h"
#include "variant-msg-type.h"

const string& get_variant_template(variant_msg_type msg_id, msg_variant_type variant);

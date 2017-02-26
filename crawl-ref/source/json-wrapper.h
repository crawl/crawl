#pragma once

#include "json.h"

// Helper for json.h
struct JsonWrapper
{
    JsonWrapper(JsonNode* n) : node(n)
    { }

    ~JsonWrapper()
    {
        if (node)
            json_delete(node);
    }

    JsonNode* operator->()
    {
        return node;
    }

    void check(JsonTag tag)
    {
        if (!node || node->tag != tag)
            throw malformed;
    }

    std::string to_string() const
    {
        char *s(json_stringify(this->node, nullptr));
        std::string result(s? s : "");
        free(s);
        return result;
    }

    JsonNode* node;

    static class MalformedException { } malformed;
};


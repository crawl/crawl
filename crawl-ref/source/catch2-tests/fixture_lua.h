#pragma once

namespace details_fixture_lua
{
    void _setup_fixture_lua();

    void _teardown_fixture_lua();
}

class FixtureLua
{
public:
    FixtureLua();

    ~FixtureLua();
};

void require_execstring(const std::string& cmd, int nresults = 0,
                        const std::string& context = "base");

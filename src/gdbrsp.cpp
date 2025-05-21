#include "common.hpp"

#include <regex>

void gdbrsp_respond(std::string const& resp)
{

}

void gdbrsp_process(std::string const& msg)
{
    static std::regex reg(
        R"(^(\?)|(D)|(g))"
        R"(|(c)([0-9]*))"
        R"(|(G)([0-9A-Fa-f]+))"
        R"(|(M)([0-9A-Fa-f]+),([0-9A-Fa-f]+):([0-9A-Fa-f]+))"
        R"(|(m)([0-9A-Fa-f]+),([0-9A-Fa-f]+))"
        R"(|([zZ])([0-1]),([0-9A-Fa-f]+),([0-9]))"
        R"(|(qAttached)$)"
        R"(|(qSupported):((?:[a-zA-Z-]+\+?;?)+))"
    );
    std::vector<std::string> parts;
    std::smatch sm;
    std::regex_match(msg, sm, reg);
    for(size_t i = 1; i < sm.size(); ++i) {
        if(sm[i].str() != "") {
            parts.push_back(sm[i].str());
        }
    }

    if(parts.empty())
    {
        gdbrsp_respond("");
        return;
    }

    std::string const& cmd = parts[0];
    if(cmd == "qSupported")
    {

    }
}

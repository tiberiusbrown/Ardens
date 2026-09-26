#pragma once

#include <cctype>
#include <string>
#include <vector>

struct cli_arg_t
{
    // An empty key denotes a positional input file.
    std::string key;
    std::string value;
};

inline bool parse_cli_bool(std::string value, bool& result)
{
    for(char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(value == "1" || value == "true" || value == "yes" || value == "on")
        result = true;
    else if(value == "0" || value == "false" || value == "no" || value == "off")
        result = false;
    else
        return false;
    return true;
}

// Shell quoting has already been removed from argv; preserve each value exactly.
inline bool parse_cli_args(int argc, char** argv,
    std::vector<cli_arg_t>& args, std::string& error)
{
    args.clear();
    error.clear();
    for(int i = 1; i < argc; ++i)
    {
        std::string argument = argv[i];
        bool const dashed = argument.compare(0, 2, "--") == 0;
        if(dashed) argument.erase(0, 2);
        auto const equals = argument.find('=');
        if(equals != std::string::npos)
        {
            args.push_back({argument.substr(0, equals), argument.substr(equals + 1)});
        }
        else if(!dashed)
        {
            args.push_back({{}, argument});
        }
        else if(argument == "screen-hash")
        {
            bool value = true;
            if(i + 1 < argc && parse_cli_bool(argv[i + 1], value))
                args.push_back({argument, argv[++i]});
            else
                args.push_back({argument, "true"});
        }
        else
        {
            if(argument.empty() || i + 1 >= argc ||
                std::string(argv[i + 1]).compare(0, 2, "--") == 0)
            {
                error = "Missing value for --" + argument + "; expected --" +
                    argument + " <value>.";
                return false;
            }
            args.push_back({argument, argv[++i]});
        }
    }
    return true;
}

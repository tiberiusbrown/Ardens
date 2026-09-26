#include "cli_args.hpp"

#include <cstdio>
#include <initializer_list>

static bool check(std::initializer_list<char const*> input,
    std::initializer_list<cli_arg_t> expected)
{
    std::vector<std::string> storage{"Ardens"};
    for(auto argument : input) storage.emplace_back(argument);
    std::vector<char*> argv;
    for(auto& argument : storage) argv.push_back(argument.data());
    std::vector<cli_arg_t> args;
    std::string error;
    if(!parse_cli_args(static_cast<int>(argv.size()), argv.data(), args, error) ||
        args.size() != expected.size())
        return false;
    size_t index = 0;
    for(auto const& argument : expected)
    {
        if(args[index].key != argument.key || args[index].value != argument.value)
            return false;
        ++index;
    }
    return true;
}

static bool missing_value(std::initializer_list<char const*> input)
{
    std::vector<std::string> storage{"Ardens"};
    for(auto argument : input) storage.emplace_back(argument);
    std::vector<char*> argv;
    for(auto& argument : storage) argv.push_back(argument.data());
    std::vector<cli_arg_t> args;
    std::string error;
    return !parse_cli_args(static_cast<int>(argv.size()), argv.data(), args, error) &&
        !error.empty();
}

int main()
{
    bool pass = true;
    pass &= check({"palette=highcontrast", "grid=normal", "current=true"},
        {{"palette", "highcontrast"}, {"grid", "normal"}, {"current", "true"}});
    pass &= check({"--palette", "highcontrast", "--grid", "normal", "--current", "true"},
        {{"palette", "highcontrast"}, {"grid", "normal"}, {"current", "true"}});
    pass &= check({"--palette=highcontrast", "--grid=normal", "--current=true"},
        {{"palette", "highcontrast"}, {"grid", "normal"}, {"current", "true"}});
    pass &= check({"--file", "C:\\games\\my game=1.hex", "file=fx data.bin",
        "game.hex", "--save", "save data.bin", "--volume", "-1", "--p", "retro",
        "--size", "800x400"},
        {{"file", "C:\\games\\my game=1.hex"}, {"file", "fx data.bin"},
         {{}, "game.hex"}, {"save", "save data.bin"}, {"volume", "-1"},
         {"p", "retro"}, {"size", "800x400"}});
    pass &= check({"--headless", "5000", "--profile-json", "profile file.json",
        "--screen-hash", "game.hex"},
        {{"headless", "5000"}, {"profile-json", "profile file.json"},
         {"screen-hash", "true"}, {{}, "game.hex"}});
    pass &= check({"headless=5000", "profile-json=profile file.json", "screen-hash=false"},
        {{"headless", "5000"}, {"profile-json", "profile file.json"},
         {"screen-hash", "false"}});
    pass &= check({"--screen-hash", "false", "--screen-hash=on", "--screen-hash", "YES"},
        {{"screen-hash", "false"}, {"screen-hash", "on"}, {"screen-hash", "YES"}});
    pass &= check({"--file=--headless", "--grid", ""},
        {{"file", "--headless"}, {"grid", ""}});
    pass &= check({"--profile-json", "file=profile.json", "profile-json=file=profile.json"},
        {{"profile-json", "file=profile.json"}, {"profile-json", "file=profile.json"}});
    pass &= missing_value({"--file"});
    pass &= missing_value({"--palette", "--file", "game.hex"});
    pass &= missing_value({"--profile-json", "--screen-hash"});
    bool value = false;
    pass &= parse_cli_bool("TRUE", value) && value;
    pass &= parse_cli_bool("off", value) && !value;
    pass &= !parse_cli_bool("invalid", value);
    std::printf("CLI argument parsing: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

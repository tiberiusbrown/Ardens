#include "headless.hpp"

#include "absim.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

namespace
{

char const* headless_value(int argc, char** argv, int& value_index)
{
    constexpr char const* PREFIX = "headless=";
    constexpr char const* DASHED_PREFIX = "--headless=";

    for(int i = 1; i < argc; ++i)
    {
        if(!std::strncmp(argv[i], PREFIX, std::strlen(PREFIX)))
        {
            value_index = i;
            return argv[i] + std::strlen(PREFIX);
        }
        if(!std::strncmp(argv[i], DASHED_PREFIX, std::strlen(DASHED_PREFIX)))
        {
            value_index = i;
            return argv[i] + std::strlen(DASHED_PREFIX);
        }
        if(!std::strcmp(argv[i], "--headless"))
        {
            value_index = i;
            return i + 1 < argc ? argv[i + 1] : nullptr;
        }
    }
    return nullptr;
}

bool parse_milliseconds(char const* value, uint64_t& milliseconds)
{
    if(!value || !*value || *value == '-') return false;
    errno = 0;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(value, &end, 10);
    if(errno == ERANGE || !end || *end != '\0') return false;
    if(parsed > std::numeric_limits<uint64_t>::max() / 1000000000ULL)
        return false;
    milliseconds = static_cast<uint64_t>(parsed);
    return true;
}

void write_serial(absim::arduboy_t& emulator)
{
    auto& bytes = emulator.core_state.cpu.serial_bytes;
    if(!bytes.empty())
    {
        std::fwrite(bytes.data(), 1, bytes.size(), stdout);
        bytes.clear();
    }
}

bool load_inputs(absim::arduboy_t& emulator, int argc, char** argv, int headless_index)
{
    for(int i = 1; i < argc; ++i)
    {
        if(i == headless_index) continue;
        if(headless_index >= 0 && !std::strcmp(argv[headless_index], "--headless") &&
            i == headless_index + 1)
            continue;

        char const* argument = argv[i];
        char const* filename = argument;
        bool save = false;
        if(!std::strncmp(argument, "file=", 5)) filename = argument + 5;
        else if(!std::strncmp(argument, "save=", 5))
        {
            filename = argument + 5;
            save = true;
        }
        else if(std::strchr(argument, '='))
            continue; // Other GUI/runtime parameters do not name input files.

        std::ifstream file(filename, std::ios::in | std::ios::binary);
        if(!file)
        {
            std::fprintf(stderr, "Could not open file: \"%s\"\n", filename);
            return false;
        }
        std::string error = emulator.load_file(filename, file, save);
        if(!error.empty())
        {
            std::fprintf(stderr, "%s: %s\n", filename, error.c_str());
            return false;
        }
    }
    return true;
}

} // namespace

bool run_headless_if_requested(int argc, char** argv, int& exit_code)
{
    int headless_index = -1;
    char const* value = headless_value(argc, argv, headless_index);
    if(headless_index < 0) return false;

    uint64_t milliseconds = 0;
    if(!parse_milliseconds(value, milliseconds))
    {
        std::fprintf(stderr, "Invalid headless duration; expected non-negative milliseconds.\n");
        exit_code = 2;
        return true;
    }

    // arduboy_t contains the full emulated memories and is too large for the
    // default Windows thread stack.
    auto emulator = std::make_unique<absim::arduboy_t>();
    if(!load_inputs(*emulator, argc, argv, headless_index))
    {
        exit_code = 1;
        return true;
    }
    if(!emulator->core_state.cpu.decoded)
    {
        std::fprintf(stderr, "No program was loaded.\n");
        exit_code = 1;
        return true;
    }

    emulator->core_state.cpu.enabled_autobreaks.reset();
    emulator->core_state.cpu.enabled_autobreaks.set(absim::AB_BREAK);
    emulator->debugger_state.paused = false;

    uint64_t const cycles_to_run = milliseconds * 16000ULL;
    uint64_t const first_cycle = emulator->core_state.cpu.cycle_count;
    uint64_t next_serial_flush = first_cycle;
    while(emulator->core_state.cpu.cycle_count - first_cycle < cycles_to_run)
    {
        emulator->cycle();
        if(emulator->core_state.cpu.autobreaks.test(absim::AB_BREAK))
            break;
        if(emulator->core_state.cpu.cycle_count >= next_serial_flush)
        {
            write_serial(*emulator);
            next_serial_flush = emulator->core_state.cpu.cycle_count + 16000;
        }
    }
    emulator->core_state.cpu.update_all();
    write_serial(*emulator);
    std::fflush(stdout);
    exit_code = 0;
    return true;
}

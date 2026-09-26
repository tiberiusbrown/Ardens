#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "headless.hpp"

#include "absim.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

namespace
{

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

void write_serial(absim::arduboy_t& emulator, bool& at_line_start)
{
    auto& bytes = emulator.core_state.cpu.serial_bytes;
    if(!bytes.empty())
    {
        std::fwrite(bytes.data(), 1, bytes.size(), stdout);
        at_line_start = bytes.back() == '\n';
        bytes.clear();
    }
}

bool load_inputs(absim::arduboy_t& emulator, std::vector<cli_arg_t> const& args)
{
    for(auto const& argument : args)
    {
        if(!argument.key.empty() && argument.key != "file" && argument.key != "save")
            continue;
        char const* filename = argument.value.c_str();
        bool const save = argument.key == "save";

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

void write_json_string(std::FILE* file, std::string const& value)
{
    std::fputc('"', file);
    for(unsigned char c : value)
    {
        switch(c)
        {
        case '"': std::fputs("\\\"", file); break;
        case '\\': std::fputs("\\\\", file); break;
        case '\n': std::fputs("\\n", file); break;
        case '\r': std::fputs("\\r", file); break;
        case '\t': std::fputs("\\t", file); break;
        default:
            if(c < 0x20)
                std::fprintf(file, "\\u%04x", unsigned(c));
            else
                std::fputc(c, file);
            break;
        }
    }
    std::fputc('"', file);
}

bool write_profile_json(absim::arduboy_t& emulator, char const* filename)
{
    std::FILE* file = std::fopen(filename, "wb");
    if(!file)
    {
        std::fprintf(stderr, "Could not open profile JSON file: \"%s\": %s\n",
            filename, std::strerror(errno));
        return false;
    }
    auto const& profile = emulator.profiler_state;
    double const cpu_usage = profile.total_with_sleep != 0 ?
        double(profile.total) / double(profile.total_with_sleep) : 0.0;
    std::fprintf(file, "{\"active_cycles\":%" PRIu64 ",\"total_cycles\":%" PRIu64
        ",\"cpu_usage\":%.17g,\"symbols\":[",
        profile.total, profile.total_with_sleep, cpu_usage);
    bool first = true;
    for(auto const& h : profile.hotspots_symbol)
    {
        uint16_t const address = emulator.core_state.cpu.disassembled_prog[h.begin].addr;
        auto const* sym = emulator.symbol_for_prog_addr(address);
        if(!sym) continue;
        if(!first) std::fputc(',', file);
        first = false;
        std::fputs("{\"name\":", file);
        write_json_string(file, sym->name);
        std::fprintf(file, ",\"address\":%u,\"cycles\":%" PRIu64 "}",
            unsigned(address), h.count);
    }
    std::fputs("]}\n", file);
    bool const write_failed = std::ferror(file) != 0;
    int const close_result = std::fclose(file);
    if(write_failed || close_result != 0)
    {
        std::fprintf(stderr, "Could not write profile JSON file: \"%s\"\n", filename);
        return false;
    }
    return true;
}

void write_screen_hash(absim::arduboy_t const& emulator)
{
    uint64_t hash = 14695981039346656037ULL;
    for(uint8_t b : emulator.peripherals.display.ram)
    {
        hash ^= b;
        hash *= 1099511628211ULL;
    }
    std::printf("screen-hash=%016" PRIx64 "\n", hash);
}

} // namespace

bool run_headless_if_requested(std::vector<cli_arg_t> const& args, int& exit_code)
{
    char const* value = nullptr;
    for(auto const& argument : args)
    {
        if(argument.key == "headless")
        {
            value = argument.value.c_str();
            break;
        }
    }
    if(!value) return false;

    char const* profile_filename = nullptr;
    bool screen_hash = false;
    for(auto const& argument : args)
    {
        if(argument.key == "profile-json")
        {
            if(argument.value.empty())
            {
                std::fprintf(stderr, "Missing profile JSON output path; expected --profile-json <path>.\n");
                exit_code = 2;
                return true;
            }
            profile_filename = argument.value.c_str();
        }
        if(argument.key == "screen-hash" && !parse_cli_bool(argument.value, screen_hash))
        {
            std::fprintf(stderr, "Invalid screen-hash value; expected a boolean.\n");
            exit_code = 2;
            return true;
        }
    }

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
    if(!load_inputs(*emulator, args))
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
    bool at_line_start = true;
    if(profile_filename)
    {
        emulator->profiler_reset();
        emulator->profiler_state.enabled = true;
    }
    while(emulator->core_state.cpu.cycle_count - first_cycle < cycles_to_run)
    {
        emulator->cycle();
        if(emulator->core_state.cpu.autobreaks.test(absim::AB_BREAK))
            break;
        if(emulator->core_state.cpu.cycle_count >= next_serial_flush)
        {
            write_serial(*emulator, at_line_start);
            next_serial_flush = emulator->core_state.cpu.cycle_count + 16000;
        }
    }
    emulator->core_state.cpu.update_all();
    write_serial(*emulator, at_line_start);
    bool profile_written = true;
    if(profile_filename)
    {
        emulator->profiler_state.enabled = false;
        emulator->profiler_state.cached_total = emulator->profiler_state.total;
        emulator->profiler_state.cached_total_with_sleep = emulator->profiler_state.total_with_sleep;
        emulator->profiler_build_hotspots();
        profile_written = write_profile_json(*emulator, profile_filename);
    }
    if(screen_hash)
    {
        if(!at_line_start) std::putchar('\n');
        write_screen_hash(*emulator);
    }
    std::fflush(stdout);
    exit_code = profile_written ? 0 : 1;
    return true;
}

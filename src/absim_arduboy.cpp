#include "absim.hpp"

#include <algorithm>

#include <sstream>
#include <tuple>

#include "absim_atmega32u4.hpp"
#include "absim_display.hpp"
#include "absim_strstream.hpp"

extern "C"
{
#include "boot/boot_game_d1.h"
#include "boot/boot_menu_d1.h"
#include "boot/boot_game_d2.h"
#include "boot/boot_menu_d2.h"
#include "boot/boot_game_e2.h"
#include "boot/boot_menu_e2.h"
#include "boot/boot_flashcart.h"
}

#define COMPRESS_TIME_TRAVEL_STATES 1

namespace absim
{

void arduboy_t::reload_fx()
{
    fx.erase_all_data();

    if(flashcart_loaded)
    {
        fx.min_page = 0;
        fx.max_page = uint32_t((fxdata.size() + 255) / 256 - 1);
        fx.write_bytes(0, fxdata.data(), fxdata.size());
        fxsave.clear();
        update_game_hash();
    }
    else
    {
        size_t fxsave_bytes = (fxsave.size() + 4095) & ~4095;
        size_t fxdata_bytes = (fxdata.size() + 255) & ~255;
        size_t fxsave_offset = w25q128_t::DATA_BYTES - fxsave_bytes;
        size_t fxdata_offset = fxsave_offset - fxdata_bytes;

        fx.min_page = uint32_t(fxdata_offset / 256);
        fx.max_page = 0xffff;

        fx.write_bytes(fxdata_offset, fxdata.data(), fxdata.size());
        update_game_hash();
        fx.write_bytes(fxsave_offset, fxsave.data(), fxsave.size());
    }

    for(size_t i = 0; i < fx.NUM_SECTORS; ++i)
    {
        auto const& s = fx.sectors_modified_data[i];
        if(!s) continue;
        fx.write_bytes(i * 4096, s->data(), s->size());
    }
}

void arduboy_t::update_game_hash()
{
    // FNV-1a 64-bit
    constexpr uint64_t OFFSET = 0xcbf29ce484222325;
    constexpr uint64_t PRIME = 0x100000001b3;
    uint64_t h = OFFSET;
    if(!flashcart_loaded)
    {
        for(size_t i = 0; i < 29 * 1024; ++i)
        {
            uint8_t byte = cpu.prog[i];
            h ^= byte;
            h *= PRIME;
        }
    }

    for(size_t i = 0; i < 7; ++i)
    {
        h ^= "ARDUBOY"[i];
        h *= PRIME;
    }
    for(size_t i = 7; i < sizeof(ARDENS_BOOT_FLASHCART); ++i)
    {
        h ^= 0xff;
        h *= PRIME;
    }
    for(size_t i = sizeof(ARDENS_BOOT_FLASHCART); i < fx.DATA_BYTES; ++i)
    {
        h ^= fx.read_byte(i); // TODO: optimize?
        h *= PRIME;
    }

    game_hash = h;
}

void arduboy_t::reset()
{
    input_history.clear();
    state_history.clear();
    present_state.clear();
    present_cycle = 0;

    profiler_reset();
    frame_cpu_usage.clear();
    total_frames = 0;
    total_ms = 0;

    cpu.lock = 0xff;
    cpu.fuse_lo = 0xff;
    cpu.fuse_hi = 0xd3;
    cpu.fuse_ext = 0xcb;
    if(flashcart_loaded || cfg.bootloader)
        cpu.fuse_hi &= ~(1 << 0);
    cpu.reset();

    display.reset();
    display.type = cfg.display_type;
    fx.reset();
    paused = false;
    break_step = 0xffffffff;

    prev_display_reset = true;

    prev_profiler_total = 0;
    prev_profiler_total_with_sleep = 0;
    prev_ms_cycles = 0;
    ms_cpu_usage.clear();

    if(breakpoints.test(0))
        paused = true;

    fxport_reg = cfg.fxport_reg;
    fxport_mask = cfg.fxport_mask;

    if(flashcart_loaded || cfg.bootloader)
    {
        unsigned char const* ptr = nullptr;
        size_t size = 0;
        if(flashcart_loaded || cfg.boot_to_menu)
        {
            if(cfg.fxport_reg == 0x2b && cfg.fxport_mask == 1 << 1)
                ptr = ARDENS_BOOT_MENU_D1, size = sizeof(ARDENS_BOOT_MENU_D1);
            if(cfg.fxport_reg == 0x2b && cfg.fxport_mask == 1 << 2)
                ptr = ARDENS_BOOT_MENU_D2, size = sizeof(ARDENS_BOOT_MENU_D2);
            if(cfg.fxport_reg == 0x2e && cfg.fxport_mask == 1 << 2)
                ptr = ARDENS_BOOT_MENU_E2, size = sizeof(ARDENS_BOOT_MENU_E2);
        }
        else
        {
            if(cfg.fxport_reg == 0x2b && cfg.fxport_mask == 1 << 1)
                ptr = ARDENS_BOOT_GAME_D1, size = sizeof(ARDENS_BOOT_GAME_D1);
            if(cfg.fxport_reg == 0x2b && cfg.fxport_mask == 1 << 2)
                ptr = ARDENS_BOOT_GAME_D2, size = sizeof(ARDENS_BOOT_GAME_D2);
            if(cfg.fxport_reg == 0x2e && cfg.fxport_mask == 1 << 2)
                ptr = ARDENS_BOOT_GAME_E2, size = sizeof(ARDENS_BOOT_GAME_E2);
        }
        if(ptr != nullptr && size != 0)
            (void)load_bootloader_hex(ptr, size);
    }

    if(cpu.program_loaded)
        cpu.decode();
}

twi_wire_state_t arduboy_t::twi_wire_state() const
{
    return cpu.twi_wire_state();
}

twi_wire_state_t arduboy_t::twi_drive_state() const
{
    return cpu.twi_drive_state();
}

void arduboy_t::set_twi_external_lines(bool scl_low, bool sda_low)
{
    cpu.set_twi_external_lines(scl_low, sda_low);
}

bool i2c_link_cable_t::endpoint_active(uint8_t endpoint) const
{
    return endpoint < num_endpoints && endpoints[endpoint].active;
}

void i2c_link_cable_t::reset_endpoint(endpoint_t& endpoint)
{
    endpoint = {};
    endpoint.id = BROADCAST_ENDPOINT;
}

uint8_t i2c_link_cable_t::attach_endpoint()
{
    if(num_endpoints >= MAX_ENDPOINTS)
        return BROADCAST_ENDPOINT;

    uint8_t id = num_endpoints++;
    auto& endpoint = endpoints[id];
    reset_endpoint(endpoint);
    endpoint.active = true;
    endpoint.id = id;
    return id;
}

void i2c_link_cable_t::disconnect()
{
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        detach_endpoint(i);
        reset_endpoint(endpoints[i]);
    }
    num_endpoints = 0;
    updating_lines = false;
    next_request_id = 1;
    reset_transport();
}

void i2c_link_cable_t::sync_lines()
{
    refresh_lines();
    poll_packets();
}

void i2c_link_cable_t::refresh_lines()
{
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(!endpoint_active(i))
            continue;

        uint64_t sample_cycle = 0;
        if(!endpoint_sample_cycle(i, sample_cycle))
            continue;

        bool external_scl_low = false;
        bool external_sda_low = false;
        for(uint8_t j = 0; j < num_endpoints; ++j)
        {
            if(i == j || !endpoint_active(j))
                continue;
            twi_wire_state_t drive{};
            if(!endpoint_drive_state(j, sample_cycle, drive))
                continue;
            external_scl_low = external_scl_low || drive.scl_low;
            external_sda_low = external_sda_low || drive.sda_low;
        }
        endpoint_set_external_lines(i, external_scl_low, external_sda_low);
    }
}

void i2c_link_cable_t::update_lines()
{
    if(updating_lines)
        return;
    updating_lines = true;
    refresh_lines();
    poll_packets();
    refresh_lines();
    updating_lines = false;
}

void i2c_link_cable_t::master_start(uint8_t endpoint, bool repeated)
{
    (void)repeated;
    if(!endpoint_active(endpoint))
        return;
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(i != endpoint && endpoint_active(i))
            pump_endpoint(i);
    }
    endpoints[endpoint].targets.clear();
    poll_packets();
}

std::vector<uint8_t> i2c_link_cable_t::route_address_targets(
    uint8_t endpoint, uint8_t address, bool& address_busy)
{
    std::vector<uint8_t> targets;
    bool general_call = address == 0;
    uint8_t nonzero_claims = 0;
    address_busy = false;
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(!endpoint_active(i))
            continue;
        if(i != endpoint)
            pump_endpoint(i);

        bool claims = endpoint_claims_address(i, address, general_call);
        if(!general_call && claims)
            nonzero_claims++;

        if(i == endpoint)
            continue;

        if(endpoint_can_address(i, address, general_call))
            targets.push_back(i);
        else if(claims)
            address_busy = true;
    }

    if(!general_call && nonzero_claims != 1)
    {
        address_busy = false;
        targets.clear();
    }
    return targets;
}

void i2c_link_cable_t::begin_request(
    endpoint_t& endpoint,
    pending_op_t op,
    std::vector<uint8_t> const& targets,
    packet_t const& packet)
{
    endpoint.pending_op = op;
    endpoint.pending_request_id = next_request_id++;
    endpoint.pending_targets = targets;
    endpoint.pending_responses.clear();
    endpoint.pending_address = packet.address;
    endpoint.pending_data = packet.data;
    endpoint.pending_read = packet.read;
    endpoint.pending_master_ack = packet.master_ack;

    packet_t p = packet;
    p.request_id = endpoint.pending_request_id;
    for(uint8_t target : targets)
    {
        p.to = target;
        send_packet(p);
    }
}

i2c_link_cable_t::result_t i2c_link_cable_t::finish_request(endpoint_t& endpoint)
{
    result_t result;
    if(endpoint.pending_responses.size() < endpoint.pending_targets.size())
    {
        result.pending = true;
        return result;
    }

    for(auto const& response : endpoint.pending_responses)
    {
        result.ack = result.ack || response.ack;
        if(response.ack)
            result.data = response.data;
    }

    endpoint.pending_op = I2C_PENDING_NONE;
    endpoint.pending_request_id = 0;
    endpoint.pending_targets.clear();
    endpoint.pending_responses.clear();
    return result;
}

i2c_link_cable_t::result_t i2c_link_cable_t::master_address(
    uint8_t endpoint, uint8_t address, bool read)
{
    if(!endpoint_active(endpoint))
        return {};

    auto& e = endpoints[endpoint];
    if(e.pending_op == I2C_PENDING_NONE)
    {
        bool address_busy = false;
        auto targets = route_address_targets(endpoint, address, address_busy);
        e.targets.clear();
        if(targets.empty())
        {
            if(address_busy)
            {
                result_t result;
                result.pending = true;
                return result;
            }
            return {};
        }

        packet_t packet;
        packet.type = packet_t::ADDRESS;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.read = read;
        packet.ack = false;
        begin_request(e, I2C_PENDING_ADDRESS, targets, packet);
    }

    poll_packets();
    auto targets = e.pending_targets;
    auto result = finish_request(e);
    if(!result.pending)
    {
        if(result.ack)
            e.targets = targets;
        else
            e.targets.clear();
    }
    return result;
}

i2c_link_cable_t::result_t i2c_link_cable_t::master_write(
    uint8_t endpoint, uint8_t address, uint8_t data)
{
    (void)address;
    if(!endpoint_active(endpoint))
        return {};

    auto& e = endpoints[endpoint];
    if(e.pending_op == I2C_PENDING_NONE)
    {
        if(e.targets.empty())
            return {};
        packet_t packet;
        packet.type = packet_t::WRITE;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.data = data;
        begin_request(e, I2C_PENDING_WRITE, e.targets, packet);
    }

    poll_packets();
    return finish_request(e);
}

i2c_link_cable_t::result_t i2c_link_cable_t::master_read(
    uint8_t endpoint, uint8_t address, bool master_ack)
{
    (void)address;
    if(!endpoint_active(endpoint))
        return {};

    auto& e = endpoints[endpoint];
    if(e.pending_op == I2C_PENDING_NONE)
    {
        if(e.targets.empty())
            return {};
        std::vector<uint8_t> targets;
        targets.push_back(e.targets.front());
        packet_t packet;
        packet.type = packet_t::READ;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.master_ack = master_ack;
        begin_request(e, I2C_PENDING_READ, targets, packet);
    }

    poll_packets();
    return finish_request(e);
}

void i2c_link_cable_t::master_stop(uint8_t endpoint)
{
    if(!endpoint_active(endpoint))
        return;

    auto& e = endpoints[endpoint];
    packet_t packet;
    packet.type = packet_t::STOP;
    packet.from = endpoint;
    packet.request_id = next_request_id++;
    for(uint8_t target : e.targets)
    {
        if(endpoint_active(target))
        {
            packet.to = target;
            send_packet(packet);
        }
    }
    e.targets.clear();
    e.pending_op = I2C_PENDING_NONE;
    e.pending_targets.clear();
    e.pending_responses.clear();
    poll_packets();
}

bool i2c_link_cable_t::drain_packets_once()
{
    bool progressed = false;
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(!endpoint_active(i))
            continue;
        packet_t packet;
        while(receive_packet(i, packet))
        {
            handle_packet(i, packet);
            progressed = true;
        }
    }
    return progressed;
}

void i2c_link_cable_t::poll_packets()
{
    for(int i = 0; i < 20000 && drain_packets_once(); ++i)
        refresh_lines();
}

void i2c_link_cable_t::pump_endpoint(uint8_t endpoint)
{
    if(!endpoint_active(endpoint))
        return;
    auto& endpoint_state = endpoints[endpoint];
    if(endpoint_state.pumping)
        return;

    endpoint_state.pumping = true;
    for(int i = 0; i < 20000 && endpoint_needs_pump(endpoint); ++i)
    {
        refresh_lines();
        endpoint_pump_cycle(endpoint);
        drain_packets_once();
    }
    refresh_lines();
    endpoint_state.pumping = false;
}

void i2c_link_cable_t::handle_packet(uint8_t endpoint, packet_t const& packet)
{
    if(packet.type == packet_t::RESPONSE)
    {
        auto& e = endpoints[endpoint];
        if(e.pending_op != I2C_PENDING_NONE &&
            e.pending_request_id == packet.request_id)
            e.pending_responses.push_back(packet);
        return;
    }

    if(!endpoint_active(endpoint))
        return;

    packet_t response;
    response.type = packet_t::RESPONSE;
    response.from = endpoint;
    response.to = packet.from;
    response.address = packet.address;
    response.request_id = packet.request_id;

    switch(packet.type)
    {
    case packet_t::ADDRESS:
        response.ack = endpoint_address(
            endpoint, packet.address, packet.read, packet.address == 0);
        if(response.ack)
            pump_endpoint(endpoint);
        send_packet(response);
        break;

    case packet_t::WRITE:
        response.ack = endpoint_write(endpoint, packet.data);
        if(response.ack)
            pump_endpoint(endpoint);
        send_packet(response);
        break;

    case packet_t::READ:
        response.ack = true;
        response.data = endpoint_read(endpoint, packet.master_ack);
        pump_endpoint(endpoint);
        send_packet(response);
        break;

    case packet_t::STOP:
        endpoint_stop(endpoint);
        pump_endpoint(endpoint);
        break;

    default:
        break;
    }
}

void i2c_local_link_cable_t::connect(std::vector<arduboy_t*> const& devices)
{
    disconnect();
    for(auto* device : devices)
    {
        if(!device)
            continue;

        uint8_t endpoint = attach_endpoint();
        if(endpoint == BROADCAST_ENDPOINT)
            continue;

        local_cpus[endpoint] = &device->cpu;
        device->cpu.attach_i2c_link(this, endpoint);
    }
    refresh_lines();
}

void i2c_local_link_cable_t::reset_transport()
{
    for(auto& queue : queues)
        queue.clear();
}

void i2c_local_link_cable_t::send_packet(packet_t const& packet)
{
    if(packet.to == BROADCAST_ENDPOINT)
    {
        for(uint8_t i = 0; i < num_endpoints; ++i)
        {
            if(i == packet.from || !endpoint_active(i))
                continue;
            packet_t p = packet;
            p.to = i;
            queues[i].push_back(p);
        }
    }
    else if(packet.to < MAX_ENDPOINTS)
    {
        queues[packet.to].push_back(packet);
    }
}

bool i2c_local_link_cable_t::receive_packet(uint8_t endpoint, packet_t& packet)
{
    if(endpoint >= MAX_ENDPOINTS || queues[endpoint].empty())
        return false;
    packet = queues[endpoint].front();
    queues[endpoint].pop_front();
    return true;
}

void i2c_local_link_cable_t::detach_endpoint(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return;

    cpu->attach_i2c_link(nullptr, BROADCAST_ENDPOINT);
    cpu->set_twi_external_lines(false, false);
    local_cpus[endpoint] = nullptr;
}

bool i2c_local_link_cable_t::endpoint_sample_cycle(uint8_t endpoint, uint64_t& cycle)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return false;
    cycle = cpu->cycle_count;
    return true;
}

bool i2c_local_link_cable_t::endpoint_drive_state(
    uint8_t endpoint, uint64_t sample_cycle, twi_wire_state_t& state)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return false;
    state = cpu->twi_drive_state_at(sample_cycle);
    return true;
}

void i2c_local_link_cable_t::endpoint_set_external_lines(
    uint8_t endpoint, bool scl_low, bool sda_low)
{
    if(auto* cpu = local_cpu(endpoint))
        cpu->set_twi_external_lines(scl_low, sda_low);
}

bool i2c_local_link_cable_t::endpoint_needs_pump(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && (cpu->twi_busy || (cpu->TWCR() & (1 << 7)));
}

void i2c_local_link_cable_t::endpoint_pump_cycle(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return;
    cpu->advance_cycle();
    cpu->update_all();
    cpu->sound_buffer.clear();
}

bool i2c_local_link_cable_t::endpoint_claims_address(
    uint8_t endpoint, uint8_t address, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_claims_address(address, general_call);
}

bool i2c_local_link_cable_t::endpoint_can_address(
    uint8_t endpoint, uint8_t address, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_can_address(address, general_call);
}

bool i2c_local_link_cable_t::endpoint_address(
    uint8_t endpoint, uint8_t address, bool read, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_address(address, read, general_call);
}

bool i2c_local_link_cable_t::endpoint_write(uint8_t endpoint, uint8_t data)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_write(data);
}

uint8_t i2c_local_link_cable_t::endpoint_read(uint8_t endpoint, bool master_ack)
{
    auto* cpu = local_cpu(endpoint);
    return cpu ? cpu->twi_slave_read(master_ack) : 0xff;
}

void i2c_local_link_cable_t::endpoint_stop(uint8_t endpoint)
{
    if(auto* cpu = local_cpu(endpoint))
        cpu->twi_slave_stop();
}

atmega32u4_t* i2c_local_link_cable_t::local_cpu(uint8_t endpoint) const
{
    if(endpoint >= MAX_ENDPOINTS)
        return nullptr;
    return local_cpus[endpoint];
}

static elf_data_symbol_t const* symbol_for_addr_helper(
    elf_data_t::map_type const& syms, uint16_t addr)
{
    for(auto const& kv : syms)
    {
        auto const& sym = kv.second;
        if(addr >= sym.addr && addr < sym.addr + sym.size)
            return &sym;
    }
    auto it = syms.find(addr);
    if(it != syms.end()) return &it->second;
    return nullptr;
}

elf_data_symbol_t const* arduboy_t::symbol_for_prog_addr(uint16_t addr)
{
    if(!elf) return nullptr;
    return symbol_for_addr_helper(elf->text_symbols, addr);
}

elf_data_symbol_t const* arduboy_t::symbol_for_data_addr(uint16_t addr)
{
    if(!elf) return nullptr;
    return symbol_for_addr_helper(elf->data_symbols, addr);
}

void arduboy_t::profiler_reset()
{
    memset(&profiler_counts, 0, sizeof(profiler_counts));
    memset(&profiler_hotspots, 0, sizeof(profiler_hotspots));
    profiler_hotspots_symbol.clear();
    num_hotspots = 0;
    profiler_total = 0;
    profiler_total_with_sleep = 0;
    prev_profiler_total = 0;
    prev_profiler_total_with_sleep = 0;
    profiler_enabled = false;
    frame_bytes = 0;
}

void arduboy_t::profiler_build_hotspots()
{
    if(!cpu.decoded) return;
    if(cpu.num_instrs <= 0) return;

    // group symbol hotspots
    profiler_hotspots_symbol.clear();
    if(elf)
    {
        for(auto const& kv : elf->text_symbols)
        {
            auto const& sym = kv.second;
            if(sym.size == 0) continue;
            if(sym.weak) continue;
            if(sym.notype) continue;
            if(sym.object) continue;
            hotspot_t h;
            h.begin = (uint16_t)cpu.addr_to_disassembled_index(sym.addr);
            h.end = (uint16_t)cpu.addr_to_disassembled_index(sym.addr + sym.size - 1);
            h.count = 0;
            for(uint32_t i = sym.addr / 2; i < (sym.addr + sym.size) / 2u; ++i)
            {
                if(i >= profiler_counts.size()) break;
                h.count += profiler_counts[i];
            }
            if(h.count == 0) continue;
            profiler_hotspots_symbol.push_back(h);
        }
    }
    std::sort(
        profiler_hotspots_symbol.begin(),
        profiler_hotspots_symbol.end(),
        [](auto const& a, auto const& b) { return a.count > b.count; }
    );

    //
    // WARNING: extremely messy hacky heuristics here
    //

    std::bitset<NUM_INSTRS> starts;
    starts.set(cpu.num_instrs - 1);

    // set starts at beginning of each func symbol
    if(elf)
    {
        for(auto const& kv : elf->text_symbols)
        {
            auto const& sym = kv.second;
            if(sym.object) continue;
            auto i = cpu.addr_to_disassembled_index(sym.addr);
            if(i < starts.size()) starts.set(i);
        }
    }

    num_hotspots = 0;

    // identify hotspot starts
    uint32_t index = 0;
    for(uint32_t index = 0; index < cpu.num_instrs; ++index)
    {
        auto const& d = cpu.disassembled_prog[index];
        auto const& i = cpu.decoded_prog[d.addr / 2];
        
        // don't split on jumps/branches that are never taken
        if(profiler_counts[d.addr / 2] == 0)
            continue;

        bool call = (
            i.func == INSTR_CALL ||
            i.func == INSTR_RCALL ||
            i.func == INSTR_ICALL);
        bool conditional = false;

        if(index > 0)
        {
            auto const& dprev = cpu.disassembled_prog[index - 1];
            auto const& iprev = cpu.decoded_prog[dprev.addr / 2];
            switch(iprev.func)
            {
            case INSTR_SBRS:
            case INSTR_SBRC:
            case INSTR_SBIS:
            case INSTR_SBIC:
            case INSTR_CPSE:
                conditional = true;
                break;
            case INSTR_BRBC:
            case INSTR_BRBS:
                // previous instruction is a branch .+2 or .+4 (skip)
                conditional = (iprev.word == 1 || iprev.word == 2);
                break;
            default:
                break;
            }
        }

        if(i.func == INSTR_BRBS || i.func == INSTR_BRBC)
            conditional = true;

        if((i.func == INSTR_RJMP || i.func == INSTR_JMP) && i.word <= 4)
            conditional = true;

        int size = 1;
        int32_t target = -1;

        switch(i.func)
        {
        case INSTR_JMP:
        case INSTR_CALL:
            size = 2;
            target = i.word;
            break;
        case INSTR_RJMP:
        case INSTR_RCALL:
        case INSTR_BRBS:
        case INSTR_BRBC:
            if(i.func == INSTR_RCALL)
                target = d.addr / 2 + 1 + (int16_t)i.word;
            else
                target = 0;
            break;
        case INSTR_IJMP:
        case INSTR_RET:
        case INSTR_RETI:
            target = 0;
            break;
        default:
            break;
        }

        if(target < 0)
            continue;

        if(conditional)
            continue;

        if(!call)
            starts.set(index + size);
        if(target > 0)
            starts.set(target);
    }

    // now collect hotspots
    for(uint32_t start = 0, index = 1; index < cpu.num_instrs; ++index)
    {
        if(starts.test(index))
        {
            auto& h = profiler_hotspots[num_hotspots++];
            h.begin = start;
            h.end = index - 1;
            h.count = 0;
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                h.count += profiler_counts[addr / 2];
            }
            if(h.count == 0) --num_hotspots;

            constexpr int LOW_COUNT_NUM = 1;
            constexpr int LOW_COUNT_DENOM = 256;
            
            // trim low-counts from beginning
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                uint64_t c = h.count * LOW_COUNT_NUM / LOW_COUNT_DENOM;
                if(profiler_counts[addr / 2] <= c)
                {
                    ++h.begin;
                    h.count -= profiler_counts[addr / 2];
                }
                else
                    break;
            }

            // trim low-counts from end
            for(int32_t i = h.end; i >= h.begin; --i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                uint64_t c = h.count * LOW_COUNT_NUM / LOW_COUNT_DENOM;
                if(profiler_counts[addr / 2] <= c)
                {
                    --h.end;
                    h.count -= profiler_counts[addr / 2];
                }
                else
                    break;
            }

            // trim from middle: N+ consecutive zero-counts
            constexpr int N = 4;
            for(int32_t ns, n = 0, i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = cpu.disassembled_prog[i].addr;
                if(profiler_counts[addr / 2] == 0)
                    ++n;
                else if(n >= N)
                {
                    auto& hn = profiler_hotspots[num_hotspots++];
                    hn.begin = h.begin;
                    hn.end = ns - 1;
                    hn.count = 0;
                    for(int32_t j = hn.begin; j <= hn.end; ++j)
                    {
                        uint16_t hnaddr = cpu.disassembled_prog[j].addr;
                        hn.count += profiler_counts[hnaddr / 2];
                    }
                    h.count -= hn.count;
                    h.begin = i;
                    n = 0;
                }
                else
                    n = 0;
                if(n == 1) ns = i;
            }

            start = index;
        }
    }

    std::sort(
        profiler_hotspots.begin(),
        profiler_hotspots.begin() + num_hotspots,
        [](auto const& a, auto const& b) { return a.count > b.count; }
    );
}

ARDENS_FORCEINLINE uint32_t arduboy_t::cycle()
{
    assert(cpu.decoded);

    bool vsync = false;
    uint8_t displayport = cpu.data[0x2b];
    uint8_t fxport = cpu.data[fxport_reg];

    uint32_t cycles = cpu.advance_cycle();

    // TODO: model SPI connection more precisely?
    // send SPI commands and data to display
    fx.set_enabled((fxport & fxport_mask) == 0);

    if(cpu.cycle_count >= cpu.spi_done_cycle)
    {
        uint8_t byte = cpu.spi_data_byte;

        // display enabled?
        if(!(displayport & (1 << 6)))
        {
            if(displayport & (1 << 4))
            {
                if(frame_bytes_total != 0 && ++frame_bytes >= frame_bytes_total)
                {
                    frame_bytes = 0;
                    vsync = true;
                }
                display.send_data(byte);
            }
            else
                display.send_command(byte);
        }

        bool was_erasing = (fx.erasing_sector != 0);
        cpu.spi_datain_byte = fx.spi_transceive(byte);
        if(fx.busy_error)
            cpu.autobreak(AB_FX_BUSY);
        cpu.spi_done_cycle = UINT64_MAX;
    }

#ifndef ARDENS_NO_DEBUGGER
    if(is_present_state())
    {
        profiler_total_with_sleep += cycles;
        if((cpu.active || cpu.wakeup_cycles != 0) &&
            cpu.executing_instr_pc < cpu.decoded_prog.size() &&
            cpu.decoded_prog[cpu.executing_instr_pc].func != INSTR_SLEEP)
        {
            profiler_total += cycles;
            if(profiler_enabled && cpu.executing_instr_pc < profiler_counts.size())
            {
                profiler_counts[cpu.executing_instr_pc] += cycles;
            }
        }
    }
#endif

    {
        auto cycles_ps = cycles * CYCLE_PS;
        bool actual_vsync = false;
        if((cpu.PORTD() & (1 << 7)) != 0)
        {
            actual_vsync = display.advance(cycles_ps);
            prev_display_reset = false;
        }
        else
        {
            if(!prev_display_reset)
                display.reset();
            prev_display_reset = true;
        }
        fx.advance(cycles_ps);
#ifndef ARDENS_NO_DEBUGGER
        if(frame_bytes_total == 0)
            vsync |= actual_vsync;
#endif
    }

#ifndef ARDENS_NO_DEBUGGER
    if(vsync && is_present_state())
    {
        // vsync occurred and we are profiling: store frame cpu usage
        uint64_t frame_total = profiler_total - prev_profiler_total;
        uint64_t frame_sleep = profiler_total_with_sleep - prev_profiler_total_with_sleep;
        prev_profiler_total = profiler_total;
        prev_profiler_total_with_sleep = profiler_total_with_sleep;
        double f = frame_sleep ? double(frame_total) / double(frame_sleep) : 0.0;
        frame_cpu_usage.push_back((float)f);
        prev_frame_cycles = frame_sleep;
        ++total_frames;

        // limit memory usage
        if(frame_cpu_usage.size() >= 65536)
        {
            frame_cpu_usage.erase(
                frame_cpu_usage.begin(),
                frame_cpu_usage.begin() + 32768);
        }
    }
#endif

#ifndef ARDENS_NO_DEBUGGER
    // time-based cpu usage
    if(cpu.cycle_count >= prev_ms_cycles && is_present_state())
    {
        constexpr size_t MS_PROF_FILT_NUM = 5;
        constexpr uint64_t PROF_MS = 1000000000ull * 20 / CYCLE_PS;
        prev_ms_cycles += PROF_MS;

        // one millisecond has passed: store cpu usage
        uint64_t ms_total = profiler_total - prev_profiler_total_ms;
        uint64_t ms_sleep = profiler_total_with_sleep - prev_profiler_total_with_sleep_ms;
        prev_profiler_total_ms = profiler_total;
        prev_profiler_total_with_sleep_ms = profiler_total_with_sleep;
        double f = ms_sleep ? double(ms_total) / double(ms_sleep) : 0.0;
        ms_cpu_usage_raw.push_back((float)f);
        if(ms_cpu_usage_raw.size() >= MS_PROF_FILT_NUM)
        {
            float t = 0.f;
            for(size_t i = 0; i < MS_PROF_FILT_NUM; ++i)
                t += ms_cpu_usage_raw[ms_cpu_usage_raw.size() - MS_PROF_FILT_NUM + i];
            ms_cpu_usage.push_back(t * (1.f / MS_PROF_FILT_NUM));
        }
        ++total_ms;

        // limit memory usage
        if(ms_cpu_usage.size() >= 65536)
        {
            ms_cpu_usage.erase(
                ms_cpu_usage.begin(),
                ms_cpu_usage.begin() + 32768);
        }
    }
#endif

    return cycles;
}

void arduboy_t::save_state_to_vector(std::vector<uint8_t>& v)
{
    std::ostringstream ss;
    save_savestate(ss);
    auto s = ss.str();
#if COMPRESS_TIME_TRAVEL_STATES
    if(!compress_zlib(v, s.data(), s.size()))
        v.clear();
#else
    v.resize(s.size());
    memcpy(v.data(), s.data(), v.size());
#endif
}

void arduboy_t::load_state_from_vector(std::vector<uint8_t> const& v)
{
    if(v.empty())
        return;
#if COMPRESS_TIME_TRAVEL_STATES
    std::vector<uint8_t> v_uncomp;
    if(!uncompress_zlib(v_uncomp, v.data(), v.size()))
        return;
    absim::istrstream ss((char const*)v_uncomp.data(), v_uncomp.size());
#else
    absim::istrstream ss((char const*)v.data(), v.size());
#endif
    load_savestate(ss);
}

void arduboy_t::update_history()
{
#ifndef ARDENS_NO_DEBUGGER
    if(cpu.cycle_count >= present_cycle)
        present_state.clear();
    if(!is_present_state())
        return;

    {
        inputs_t state;
        state.cycle = cpu.cycle_count;
        state.pinb = cpu.PINB();
        state.pine = cpu.PINE();
        state.pinf = cpu.PINF();
        if(input_history.empty() ||
            input_history.back().pinb != state.pinb ||
            input_history.back().pine != state.pine ||
            input_history.back().pinf != state.pinf)
        {
            input_history.push_back(state);
        }
    }
    if(state_history.empty() ||
        cpu.cycle_count >= state_history.back().cycle + STATE_HISTORY_CYCLES)
    {
        tt_state_t state;
        state.cycle = cpu.cycle_count;
        save_state_to_vector(state.state);
        state_history.emplace_back(std::move(state));
    }
    while(input_history.size() >= 2 &&
        input_history[1].cycle + STATE_HISTORY_TOTAL_CYCLES < cpu.cycle_count)
    {
        input_history.erase(input_history.begin());
    }
    while(state_history.size() >= 2 &&
        state_history[1].cycle + STATE_HISTORY_TOTAL_CYCLES < cpu.cycle_count)
    {
        state_history.erase(state_history.begin());
    }
#endif
}

struct pc_hist_t
{
    uint64_t cycle;
    uint16_t pc;
    uint16_t stack_depth;
    uint8_t pinb;
    uint8_t pine;
    uint8_t pinf;
};

static void travel_back_advance_instr(arduboy_t& a)
{
    int n = 0;
    auto oldpc = a.cpu.pc;
    a.cpu.no_merged = true;
    a.ps_rem = 0;
    do
    {
        a.paused = false;
        a.cycle();
        a.paused = true;
    } while(++n < 65536 && a.cpu.pc == oldpc);
}

template<class F>
static void travel_back_cond(arduboy_t& a, F&& f, uint64_t max_cycle = UINT64_MAX)
{
    if(a.state_history.empty()) return;
    if(a.input_history.empty()) return;
    if(a.present_state.empty())
    {
        a.save_state_to_vector(a.present_state);
        a.present_cycle = a.cpu.cycle_count;
    }
    size_t si = a.state_history.size();
    std::vector<pc_hist_t> pcs;
    std::vector<uint8_t> temp_state;
    a.save_state_to_vector(temp_state);
    uint64_t curr_cycle = a.cpu.cycle_count;
    max_cycle = std::min(max_cycle, curr_cycle);
    while(si >= 2 && a.state_history[si - 1].cycle >= max_cycle)
        si -= 1;
    while(si-- > 0)
    {
        auto const& state = a.state_history[si];
        uint64_t end_cycle = (si + 1 < a.state_history.size() ?
            a.state_history[si + 1].cycle : a.present_cycle);
        end_cycle = std::min(end_cycle, curr_cycle);
        size_t ii = a.input_history.size();
        while(ii-- > 0)
        {
            if(a.input_history[ii].cycle <= state.cycle)
                break;
        }
        if(ii >= a.input_history.size())
            break;
        a.load_state_from_vector(state.state);
        pcs.clear();
        while(a.cpu.cycle_count < end_cycle)
        {
            while(ii + 1 < a.input_history.size() && a.input_history[ii + 1].cycle <= a.cpu.cycle_count)
                ++ii;
            auto const& input = a.input_history[ii];
            pc_hist_t p{};
            p.cycle = a.cpu.cycle_count;
            p.pc = a.cpu.pc;
            p.stack_depth = (uint16_t)a.cpu.num_stack_frames;
            a.cpu.PINB() = p.pinb = input.pinb;
            a.cpu.PINE() = p.pine = input.pine;
            a.cpu.PINF() = p.pinf = input.pinf;
            pcs.push_back(p);
            travel_back_advance_instr(a);
        }
        size_t pi = pcs.size();
        while(pi-- > 0)
        {
            if(f(pcs[pi]))
            {
                // success
                a.load_state_from_vector(state.state);
                for(size_t i = 0; i < pi; ++i)
                {
                    a.cpu.PINB() = pcs[i].pinb;
                    a.cpu.PINE() = pcs[i].pine;
                    a.cpu.PINF() = pcs[i].pinf;
                    travel_back_advance_instr(a);
                }
                a.cpu.update_all();
                return;
            }
        }
    }
    // failed to travel back: reload previous state
    a.load_state_from_vector(temp_state);
    if(a.cpu.cycle_count >= a.present_cycle)
    {
        a.load_state_from_vector(a.present_state);
        a.present_state.clear();
    }
}

void arduboy_t::travel_back_to_cycle(uint64_t cycle)
{
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.cycle <= cycle;
    }, cycle);
}

void arduboy_t::travel_back_single_instr()
{
    uint16_t tpc = cpu.pc;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.pc != tpc;
    });
}

void arduboy_t::travel_back_single_instr_over()
{
    uint16_t tpc = cpu.pc;
    uint16_t tsd = cpu.num_stack_frames;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.pc != tpc && p.stack_depth == tsd;
    });
}

void arduboy_t::travel_back_single_instr_out()
{
    uint16_t tsd = cpu.num_stack_frames;
    if(tsd == 0) return;
    uint64_t cycle = cpu.stack_frames[tsd - 1].cycle;
    assert(cycle < cpu.cycle_count);
    if(cycle >= cpu.cycle_count) return;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.stack_depth < tsd;
    }, cycle);
}

void arduboy_t::travel_to_present()
{
    if(present_state.empty()) return;
    load_state_from_vector(present_state);
    present_state.clear();
}

void arduboy_t::travel_continue()
{
    if(present_state.empty()) return;
    present_state.clear();
    size_t i;
    i = 0;
    while(i < state_history.size() && state_history[i].cycle < cpu.cycle_count)
        ++i;
    if(i < state_history.size())
        state_history.resize(i);
    i = 0;
    while(i < input_history.size() && input_history[i].cycle < cpu.cycle_count)
        ++i;
    if(i < input_history.size())
        input_history.resize(i);
}

bool arduboy_t::is_present_state()
{
    return present_state.empty();
}

static void set_button_pins_from_history(arduboy_t& a)
{
    uint64_t cycle = a.cpu.cycle_count;
    auto const& inputs = a.input_history;
    size_t n = inputs.size();
    uint8_t pinb = 0x10;
    uint8_t pine = 0x40;
    uint8_t pinf = 0xf0;
    while(n-- > 0)
    {
        auto const& i = inputs[n];
        if(i.cycle <= cycle)
        {
            pinb = i.pinb;
            pine = i.pine;
            pinf = i.pinf;
            break;
        }
    }
    a.cpu.PINB() = pinb;
    a.cpu.PINE() = pine;
    a.cpu.PINF() = pinf;
}

void arduboy_t::advance_instr()
{
    if(!cpu.decoded) return;
    update_history();
    set_button_pins_from_history(*this);
    int n = 0;
    auto oldpc = cpu.pc;
    cpu.no_merged = true;
    ps_rem = 0;
    do
    {
        paused = false;
        cycle();
        cpu.update_all();
        paused = true;
    } while(++n < 65536 && cpu.pc == oldpc);
}

void arduboy_t::advance(uint64_t ps)
{
    update_history();

    ps += ps_rem;
    ps_rem = 0;

    if(!cpu.decoded) return;
    if(paused) return;

    cpu.autobreaks = 0;

#ifndef ARDENS_NO_DEBUGGER
    bool any_breakpoints =
        allow_nonstep_breakpoints && (
        breakpoints.any() ||
        breakpoints_rd.any() ||
        breakpoints_wr.any()) ||
        break_step != 0xffffffff;

    cpu.no_merged = profiler_enabled || any_breakpoints;
#endif

    if(!is_present_state())
        cpu.no_merged = true;

    while(ps >= PS_BUFFER)
    {
        if(!is_present_state())
            set_button_pins_from_history(*this);

        uint32_t cycles = cycle();

        ps -= cycles * CYCLE_PS;

#ifndef ARDENS_NO_DEBUGGER
        if(any_breakpoints)
        {
            if(cpu.pc == break_step || allow_nonstep_breakpoints && (
                cpu.pc < breakpoints.size() && breakpoints.test(cpu.pc) ||
                cpu.just_read < breakpoints_rd.size() && breakpoints_rd.test(cpu.just_read) ||
                cpu.just_written < breakpoints_wr.size() && breakpoints_wr.test(cpu.just_written)))
            {
                paused = true;
                break;
            }
        }
#endif

#ifndef ARDENS_NO_DEBUGGER
        if(cpu.should_autobreak())
        {
            paused = true;
            break;
        }
#endif

    }

    cpu.update_all();

    // track remainder
    if(!paused)
        ps_rem = ps;

    if(!display.enable_filter)
    {
        memcpy(
            display.filtered_pixels.data(),
            display.pixels[0].data(),
            array_bytes(display.filtered_pixels));
    }

    // update savedata
    if(cpu.eeprom_dirty)
    {
        savedata.eeprom.resize(cpu.eeprom.size());
        savedata.eeprom_modified_bytes = cpu.eeprom_modified_bytes;
        memcpy(savedata.eeprom.data(), cpu.eeprom.data(), array_bytes(savedata.eeprom));
        cpu.eeprom_dirty = false;
        if(is_present_state())
            savedata_dirty = true;
    }
    if(fx.sectors_dirty)
    {
        for(size_t i = 0; i < fx.sectors_modified.size(); ++i)
        {
            if(!fx.sectors_modified.test(i)) continue;
            auto& s = savedata.fx_sectors[(uint32_t)i];
            auto const& fxs = fx.sectors[i];
            if(!fxs)
                memset(s.data(), 0xff, 4096);
            else
                memcpy(s.data(), fxs->data(), 4096);
            auto& fxsm = fx.sectors_modified_data[i];
            if(!fxsm)
                fxsm = std::make_unique<w25q128_t::sector_t>();
            if(!fxs)
                memset(fxsm->data(), 0xff, 4096);
            else
                memcpy(fxsm->data(), fxs->data(), 4096);
        }
        fx.sectors_dirty = false;
        if(is_present_state())
            savedata_dirty = true;
    }

#ifndef ARDENS_NO_DEBUGGER
    if(cpu.cycle_count >= present_cycle)
    {
        load_state_from_vector(present_state);
        present_state.clear();
    }
#endif
}

}

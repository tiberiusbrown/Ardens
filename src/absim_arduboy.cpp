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

arduboy_t::arduboy_t() {}

void arduboy_t::reload_fx()
{
    peripherals.fx.erase_all_data();

    if(program_state.flashcart_loaded)
    {
        peripherals.fx.min_page = 0;
        peripherals.fx.max_page =
            uint32_t((program_state.fxdata.size() + w25q128_t::PAGE_BYTES - 1) /
                w25q128_t::PAGE_BYTES - 1);
        peripherals.fx.write_bytes(0, program_state.fxdata.data(), program_state.fxdata.size());
        program_state.fxsave.clear();
        update_game_hash();
    }
    else
    {
        w25q128_t::fx_data_save_layout_t layout{};
        bool layout_valid = w25q128_t::make_data_save_layout(
            program_state.fxdata.size(), program_state.fxsave.size(), layout);
        assert(layout_valid);

        if(layout.has_payload())
        {
            peripherals.fx.min_page = layout.min_page();
            peripherals.fx.max_page = w25q128_t::LAST_PAGE;
        }
        else
        {
            peripherals.fx.set_empty_page_range();
        }

        peripherals.fx.write_bytes(layout.data_offset, program_state.fxdata.data(), program_state.fxdata.size());
        update_game_hash();
        peripherals.fx.write_bytes(layout.save_offset, program_state.fxsave.data(), program_state.fxsave.size());
    }

    for(size_t i = 0; i < peripherals.fx.NUM_SECTORS; ++i)
    {
        auto const& s = peripherals.fx.sectors_modified_data[i];
        if(!s) continue;
        peripherals.fx.write_bytes(i * w25q128_t::SECTOR_BYTES, s->data(), s->size());
    }
}

void arduboy_t::update_game_hash()
{
    // FNV-1a 64-bit
    constexpr uint64_t OFFSET = 0xcbf29ce484222325;
    constexpr uint64_t PRIME = 0x100000001b3;
    uint64_t h = OFFSET;
    if(!program_state.flashcart_loaded)
    {
        for(size_t i = 0; i < atmega32u4_t::PROGRAM_FLASH_BYTES; ++i)
        {
            uint8_t byte = core_state.cpu.prog[i];
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
    for(size_t i = sizeof(ARDENS_BOOT_FLASHCART); i < peripherals.fx.DATA_BYTES; ++i)
    {
        h ^= peripherals.fx.read_byte(i); // TODO: optimize?
        h *= PRIME;
    }

    program_state.game_hash = h;
}

void arduboy_t::reset()
{
    debugger_state.input_history.clear();
    debugger_state.state_history.clear();
    debugger_state.present_state.clear();
    debugger_state.present_cycle = 0;

    profiler_reset();
    profiler_state.frame_cpu_usage.clear();
    profiler_state.total_frames = 0;
    profiler_state.total_ms = 0;

    core_state.cpu.lock = 0xff;
    core_state.cpu.fuse_lo = 0xff;
    core_state.cpu.fuse_hi = 0xd3;
    core_state.cpu.fuse_ext = 0xcb;
    if(program_state.flashcart_loaded || program_state.cfg.bootloader)
        core_state.cpu.fuse_hi &= ~(1 << 0);
    core_state.cpu.reset();

    // Arduboy bootloaders leave sketches with the USB PLL frequency setup visible.
    if(program_state.flashcart_loaded || program_state.cfg.bootloader)
        core_state.cpu.data[reg::addr::PLLFRQ] = 0x4a;

    peripherals.display.reset();
    peripherals.display.type = program_state.cfg.display_type;
    peripherals.fx.reset();
    debugger_state.paused = false;
    debugger_state.break_step = 0xffffffff;

    peripherals.prev_display_reset = true;

    profiler_state.prev_total = 0;
    profiler_state.prev_total_with_sleep = 0;
    profiler_state.prev_ms_cycles = 0;
    profiler_state.ms_cpu_usage.clear();

    if(debugger_state.breakpoints.test(0))
        debugger_state.paused = true;

    peripherals.fxport_reg = program_state.cfg.fxport_reg;
    peripherals.fxport_mask = program_state.cfg.fxport_mask;

    if(program_state.flashcart_loaded || program_state.cfg.bootloader)
    {
        unsigned char const* ptr = nullptr;
        size_t size = 0;
        if(program_state.flashcart_loaded || program_state.cfg.boot_to_menu)
        {
            if(program_state.cfg.fxport_reg == reg::addr::PORTD &&
                program_state.cfg.fxport_mask == reg::bit::PORTD::PORTD1)
                ptr = ARDENS_BOOT_MENU_D1, size = sizeof(ARDENS_BOOT_MENU_D1);
            if(program_state.cfg.fxport_reg == reg::addr::PORTD &&
                program_state.cfg.fxport_mask == reg::bit::PORTD::PORTD2)
                ptr = ARDENS_BOOT_MENU_D2, size = sizeof(ARDENS_BOOT_MENU_D2);
            if(program_state.cfg.fxport_reg == reg::addr::PORTE &&
                program_state.cfg.fxport_mask == reg::bit::PORTE::PORTE2)
                ptr = ARDENS_BOOT_MENU_E2, size = sizeof(ARDENS_BOOT_MENU_E2);
        }
        else
        {
            if(program_state.cfg.fxport_reg == reg::addr::PORTD &&
                program_state.cfg.fxport_mask == reg::bit::PORTD::PORTD1)
                ptr = ARDENS_BOOT_GAME_D1, size = sizeof(ARDENS_BOOT_GAME_D1);
            if(program_state.cfg.fxport_reg == reg::addr::PORTD &&
                program_state.cfg.fxport_mask == reg::bit::PORTD::PORTD2)
                ptr = ARDENS_BOOT_GAME_D2, size = sizeof(ARDENS_BOOT_GAME_D2);
            if(program_state.cfg.fxport_reg == reg::addr::PORTE &&
                program_state.cfg.fxport_mask == reg::bit::PORTE::PORTE2)
                ptr = ARDENS_BOOT_GAME_E2, size = sizeof(ARDENS_BOOT_GAME_E2);
        }
        if(ptr != nullptr && size != 0)
            (void)load_bootloader_hex(ptr, size);
    }

    if(core_state.cpu.program_loaded)
        core_state.cpu.decode();
}

twi_wire_state_t arduboy_t::twi_wire_state() const
{
    return core_state.cpu.twi_wire_state();
}

twi_wire_state_t arduboy_t::twi_drive_state() const
{
    return core_state.cpu.twi_drive_state();
}

void arduboy_t::set_twi_external_lines(bool scl_low, bool sda_low)
{
    core_state.cpu.set_twi_external_lines(scl_low, sda_low);
}

void i2c_bus_t::clear()
{
    participants.clear();
}

void i2c_bus_t::attach(i2c_bus_participant_t const* participant)
{
    if(participant)
        participants.push_back(participant);
}

twi_wire_state_t i2c_bus_t::resolve() const
{
    bool scl_low = false;
    bool sda_low = false;
    for(auto* participant : participants)
    {
        if(!participant)
            continue;
        scl_low = scl_low || participant->drive_scl_low;
        sda_low = sda_low || participant->drive_sda_low;
    }
    return { scl_low, sda_low, !scl_low, !sda_low };
}

bool i2c_link_adapter_t::endpoint_active(uint8_t endpoint) const
{
    return endpoint < num_endpoints && endpoints[endpoint].active;
}

void i2c_link_adapter_t::reset_endpoint(endpoint_t& endpoint)
{
    endpoint = {};
    endpoint.id = BROADCAST_ENDPOINT;
}

uint8_t i2c_link_adapter_t::attach_endpoint()
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

void i2c_link_adapter_t::disconnect()
{
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        detach_endpoint(i);
        reset_endpoint(endpoints[i]);
    }
    num_endpoints = 0;
    updating_lines = false;
    next_transaction_id = 1;
    reset_transport();
}

void i2c_link_adapter_t::sync_bus_lines()
{
    refresh_bus_lines();
    poll_messages();
}

void i2c_link_adapter_t::refresh_bus_lines()
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

void i2c_link_adapter_t::update_bus_lines()
{
    if(updating_lines)
        return;
    updating_lines = true;
    refresh_bus_lines();
    poll_messages();
    refresh_bus_lines();
    updating_lines = false;
}

void i2c_link_adapter_t::on_local_start(uint8_t endpoint, bool repeated)
{
    if(!endpoint_active(endpoint))
        return;
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(i != endpoint && endpoint_active(i))
            pump_endpoint(i);
    }
    auto& e = endpoints[endpoint];
    if(repeated)
    {
        i2c_message_t message;
        message.kind = i2c_message_t::kind_t::REPEATED_START;
        message.from = endpoint;
        message.transaction_id = next_transaction_id++;
        for(uint8_t target : e.targets)
        {
            if(endpoint_active(target))
            {
                message.to = target;
                send_message(message);
            }
        }
    }
    else
    {
        i2c_message_t message;
        message.kind = i2c_message_t::kind_t::START;
        message.from = endpoint;
        message.to = BROADCAST_ENDPOINT;
        message.transaction_id = next_transaction_id++;
        send_message(message);
    }
    e.targets.clear();
    poll_messages();
}

std::vector<uint8_t> i2c_link_adapter_t::route_address_targets(
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

void i2c_link_adapter_t::begin_request(
    endpoint_t& endpoint,
    pending_op_t op,
    std::vector<uint8_t> const& targets,
    i2c_message_t const& packet)
{
    endpoint.pending_op = op;
    endpoint.pending_transaction_id = next_transaction_id++;
    endpoint.pending_targets = targets;
    endpoint.pending_responses.clear();
    endpoint.pending_address = packet.address;
    endpoint.pending_data = packet.data;
    endpoint.pending_read = packet.read;
    endpoint.pending_master_ack = packet.master_ack;

    i2c_message_t p = packet;
    p.transaction_id = endpoint.pending_transaction_id;
    for(uint8_t target : targets)
    {
        p.to = target;
        send_message(p);
    }
}

i2c_link_adapter_t::result_t i2c_link_adapter_t::finish_request(endpoint_t& endpoint)
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
    endpoint.pending_transaction_id = 0;
    endpoint.pending_targets.clear();
    endpoint.pending_responses.clear();
    return result;
}

i2c_link_adapter_t::result_t i2c_link_adapter_t::request_address_ack(
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

        i2c_message_t packet;
        packet.kind = i2c_message_t::kind_t::ADDRESS;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.read = read;
        packet.ack = false;
        begin_request(e, I2C_PENDING_ADDRESS, targets, packet);
    }

    poll_messages();
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

i2c_link_adapter_t::result_t i2c_link_adapter_t::request_write_ack(
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
        i2c_message_t packet;
        packet.kind = i2c_message_t::kind_t::WRITE_BYTE;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.data = data;
        begin_request(e, I2C_PENDING_WRITE, e.targets, packet);
    }

    poll_messages();
    return finish_request(e);
}

i2c_link_adapter_t::result_t i2c_link_adapter_t::request_read_byte(
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
        i2c_message_t packet;
        packet.kind = i2c_message_t::kind_t::READ_REQUEST;
        packet.from = endpoint;
        packet.address = address & 0x7f;
        packet.master_ack = master_ack;
        begin_request(e, I2C_PENDING_READ, targets, packet);
    }

    poll_messages();
    return finish_request(e);
}

void i2c_link_adapter_t::on_local_stop(uint8_t endpoint)
{
    if(!endpoint_active(endpoint))
        return;

    auto& e = endpoints[endpoint];
    i2c_message_t packet;
    packet.kind = i2c_message_t::kind_t::STOP;
    packet.from = endpoint;
    packet.transaction_id = next_transaction_id++;
    for(uint8_t target : e.targets)
    {
        if(endpoint_active(target))
        {
            packet.to = target;
            send_message(packet);
        }
    }
    e.targets.clear();
    e.pending_op = I2C_PENDING_NONE;
    e.pending_targets.clear();
    e.pending_responses.clear();
    poll_messages();
}

bool i2c_link_adapter_t::drain_messages_once()
{
    bool progressed = false;
    for(uint8_t i = 0; i < num_endpoints; ++i)
    {
        if(!endpoint_active(i))
            continue;
        i2c_message_t packet;
        while(receive_message(i, packet))
        {
            handle_message(i, packet);
            progressed = true;
        }
    }
    return progressed;
}

void i2c_link_adapter_t::poll_messages()
{
    for(int i = 0; i < 20000 && drain_messages_once(); ++i)
        refresh_bus_lines();
}

void i2c_link_adapter_t::pump_endpoint(uint8_t endpoint)
{
    if(!endpoint_active(endpoint))
        return;
    auto& endpoint_state = endpoints[endpoint];
    if(endpoint_state.pumping)
        return;

    endpoint_state.pumping = true;
    for(int i = 0; i < 20000 && endpoint_needs_pump(endpoint); ++i)
    {
        refresh_bus_lines();
        endpoint_pump_cycle(endpoint);
        drain_messages_once();
    }
    refresh_bus_lines();
    endpoint_state.pumping = false;
}

void i2c_link_adapter_t::handle_message(uint8_t endpoint, i2c_message_t const& packet)
{
    if(packet.kind == i2c_message_t::kind_t::RESPONSE ||
        packet.kind == i2c_message_t::kind_t::READ_RESPONSE)
    {
        auto& e = endpoints[endpoint];
        if(e.pending_op != I2C_PENDING_NONE &&
            e.pending_transaction_id == packet.transaction_id)
            e.pending_responses.push_back(packet);
        return;
    }

    if(!endpoint_active(endpoint))
        return;

    i2c_message_t response;
    response.kind = i2c_message_t::kind_t::RESPONSE;
    response.from = endpoint;
    response.to = packet.from;
    response.address = packet.address;
    response.transaction_id = packet.transaction_id;

    switch(packet.kind)
    {
    case i2c_message_t::kind_t::START:
        break;

    case i2c_message_t::kind_t::REPEATED_START:
        endpoint_stop(endpoint);
        pump_endpoint(endpoint);
        break;

    case i2c_message_t::kind_t::ADDRESS:
        response.ack = endpoint_address(
            endpoint, packet.address, packet.read, packet.address == 0);
        if(response.ack)
            pump_endpoint(endpoint);
        send_message(response);
        break;

    case i2c_message_t::kind_t::WRITE_BYTE:
        response.ack = endpoint_write(endpoint, packet.data);
        if(response.ack)
            pump_endpoint(endpoint);
        send_message(response);
        break;

    case i2c_message_t::kind_t::READ_REQUEST:
        response.kind = i2c_message_t::kind_t::READ_RESPONSE;
        response.ack = true;
        response.data = endpoint_read(endpoint, packet.master_ack);
        pump_endpoint(endpoint);
        send_message(response);
        break;

    case i2c_message_t::kind_t::MASTER_ACK:
        break;

    case i2c_message_t::kind_t::STOP:
        endpoint_stop(endpoint);
        pump_endpoint(endpoint);
        break;

    case i2c_message_t::kind_t::ABORT:
    case i2c_message_t::kind_t::BUS_RESET:
        endpoint_stop(endpoint);
        break;

    default:
        break;
    }
}

void local_i2c_transaction_bridge_t::connect(std::vector<arduboy_t*> const& devices)
{
    disconnect();
    for(auto* device : devices)
    {
        if(!device)
            continue;

        uint8_t endpoint = attach_endpoint();
        if(endpoint == BROADCAST_ENDPOINT)
            continue;

        local_cpus[endpoint] = &device->core_state.cpu;
        device->core_state.cpu.attach_i2c_adapter(this, endpoint);
    }
    refresh_bus_lines();
}

void local_i2c_transaction_bridge_t::reset_transport()
{
    for(auto& queue : queues)
        queue.clear();
}

void local_i2c_transaction_bridge_t::send_message(i2c_message_t const& packet)
{
    if(packet.to == BROADCAST_ENDPOINT)
    {
        for(uint8_t i = 0; i < num_endpoints; ++i)
        {
            if(i == packet.from || !endpoint_active(i))
                continue;
            i2c_message_t p = packet;
            p.to = i;
            queues[i].push_back(p);
        }
    }
    else if(packet.to < MAX_ENDPOINTS)
    {
        queues[packet.to].push_back(packet);
    }
}

bool local_i2c_transaction_bridge_t::receive_message(uint8_t endpoint, i2c_message_t& packet)
{
    if(endpoint >= MAX_ENDPOINTS || queues[endpoint].empty())
        return false;
    packet = queues[endpoint].front();
    queues[endpoint].pop_front();
    return true;
}

void local_i2c_transaction_bridge_t::detach_endpoint(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return;

    cpu->attach_i2c_adapter(nullptr, BROADCAST_ENDPOINT);
    cpu->set_twi_external_lines(false, false);
    local_cpus[endpoint] = nullptr;
}

bool local_i2c_transaction_bridge_t::endpoint_sample_cycle(uint8_t endpoint, uint64_t& cycle)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return false;
    cycle = cpu->cycle_count;
    return true;
}

bool local_i2c_transaction_bridge_t::endpoint_drive_state(
    uint8_t endpoint, uint64_t sample_cycle, twi_wire_state_t& state)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return false;
    state = cpu->twi_drive_state_at(sample_cycle);
    return true;
}

void local_i2c_transaction_bridge_t::endpoint_set_external_lines(
    uint8_t endpoint, bool scl_low, bool sda_low)
{
    if(auto* cpu = local_cpu(endpoint))
        cpu->set_twi_external_lines(scl_low, sda_low);
}

bool local_i2c_transaction_bridge_t::endpoint_needs_pump(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && (
        cpu->twi_busy ||
        (cpu->data[reg::addr::TWCR] & reg::bit::TWCR::TWINT));
}

void local_i2c_transaction_bridge_t::endpoint_pump_cycle(uint8_t endpoint)
{
    auto* cpu = local_cpu(endpoint);
    if(!cpu)
        return;
    cpu->advance_cycle();
    cpu->update_all();
    cpu->sound_buffer.clear();
}

bool local_i2c_transaction_bridge_t::endpoint_claims_address(
    uint8_t endpoint, uint8_t address, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_claims_address(address, general_call);
}

bool local_i2c_transaction_bridge_t::endpoint_can_address(
    uint8_t endpoint, uint8_t address, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_can_address(address, general_call);
}

bool local_i2c_transaction_bridge_t::endpoint_address(
    uint8_t endpoint, uint8_t address, bool read, bool general_call)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_address(address, read, general_call);
}

bool local_i2c_transaction_bridge_t::endpoint_write(uint8_t endpoint, uint8_t data)
{
    auto* cpu = local_cpu(endpoint);
    return cpu && cpu->twi_slave_write(data);
}

uint8_t local_i2c_transaction_bridge_t::endpoint_read(uint8_t endpoint, bool master_ack)
{
    auto* cpu = local_cpu(endpoint);
    return cpu ? cpu->twi_slave_read(master_ack) : 0xff;
}

void local_i2c_transaction_bridge_t::endpoint_stop(uint8_t endpoint)
{
    if(auto* cpu = local_cpu(endpoint))
        cpu->twi_slave_stop();
}

atmega32u4_t* local_i2c_transaction_bridge_t::local_cpu(uint8_t endpoint) const
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
    if(!program_state.elf) return nullptr;
    return symbol_for_addr_helper(program_state.elf->text_symbols, addr);
}

elf_data_symbol_t const* arduboy_t::symbol_for_data_addr(uint16_t addr)
{
    if(!program_state.elf) return nullptr;
    return symbol_for_addr_helper(program_state.elf->data_symbols, addr);
}

void arduboy_t::profiler_reset()
{
    memset(&profiler_state.counts, 0, sizeof(profiler_state.counts));
    memset(&profiler_state.hotspots, 0, sizeof(profiler_state.hotspots));
    profiler_state.hotspots_symbol.clear();
    profiler_state.num_hotspots = 0;
    profiler_state.total = 0;
    profiler_state.total_with_sleep = 0;
    profiler_state.prev_total = 0;
    profiler_state.prev_total_with_sleep = 0;
    profiler_state.enabled = false;
    profiler_state.frame_bytes = 0;
}

void arduboy_t::profiler_build_hotspots()
{
    if(!core_state.cpu.decoded) return;
    if(core_state.cpu.num_instrs <= 0) return;

    // group symbol hotspots
    profiler_state.hotspots_symbol.clear();
    if(program_state.elf)
    {
        for(auto const& kv : program_state.elf->text_symbols)
        {
            auto const& sym = kv.second;
            if(sym.size == 0) continue;
            if(sym.weak) continue;
            if(sym.notype) continue;
            if(sym.object) continue;
            hotspot_t h;
            h.begin = (uint16_t)core_state.cpu.addr_to_disassembled_index(sym.addr);
            h.end = (uint16_t)core_state.cpu.addr_to_disassembled_index(sym.addr + sym.size - 1);
            h.count = 0;
            for(uint32_t i = sym.addr / 2; i < (sym.addr + sym.size) / 2u; ++i)
            {
                if(i >= profiler_state.counts.size()) break;
                h.count += profiler_state.counts[i];
            }
            if(h.count == 0) continue;
            profiler_state.hotspots_symbol.push_back(h);
        }
    }
    std::sort(
        profiler_state.hotspots_symbol.begin(),
        profiler_state.hotspots_symbol.end(),
        [](auto const& a, auto const& b) { return a.count > b.count; }
    );

    //
    // WARNING: extremely messy hacky heuristics here
    //

    std::bitset<NUM_INSTRS> starts;
    starts.set(core_state.cpu.num_instrs - 1);

    // set starts at beginning of each func symbol
    if(program_state.elf)
    {
        for(auto const& kv : program_state.elf->text_symbols)
        {
            auto const& sym = kv.second;
            if(sym.object) continue;
            auto i = core_state.cpu.addr_to_disassembled_index(sym.addr);
            if(i < starts.size()) starts.set(i);
        }
    }

    profiler_state.num_hotspots = 0;

    // identify hotspot starts
    uint32_t index = 0;
    for(uint32_t index = 0; index < core_state.cpu.num_instrs; ++index)
    {
        auto const& d = core_state.cpu.disassembled_prog[index];
        auto const& i = core_state.cpu.decoded_prog[d.addr / 2];
        
        // don't split on jumps/branches that are never taken
        if(profiler_state.counts[d.addr / 2] == 0)
            continue;

        bool call = (
            i.func == INSTR_CALL ||
            i.func == INSTR_RCALL ||
            i.func == INSTR_ICALL);
        bool conditional = false;

        if(index > 0)
        {
            auto const& dprev = core_state.cpu.disassembled_prog[index - 1];
            auto const& iprev = core_state.cpu.decoded_prog[dprev.addr / 2];
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
    for(uint32_t start = 0, index = 1; index < core_state.cpu.num_instrs; ++index)
    {
        if(starts.test(index))
        {
            auto& h = profiler_state.hotspots[profiler_state.num_hotspots++];
            h.begin = start;
            h.end = index - 1;
            h.count = 0;
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = core_state.cpu.disassembled_prog[i].addr;
                h.count += profiler_state.counts[addr / 2];
            }
            if(h.count == 0) --profiler_state.num_hotspots;

            constexpr int LOW_COUNT_NUM = 1;
            constexpr int LOW_COUNT_DENOM = 256;
            
            // trim low-counts from beginning
            for(int32_t i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = core_state.cpu.disassembled_prog[i].addr;
                uint64_t c = h.count * LOW_COUNT_NUM / LOW_COUNT_DENOM;
                if(profiler_state.counts[addr / 2] <= c)
                {
                    ++h.begin;
                    h.count -= profiler_state.counts[addr / 2];
                }
                else
                    break;
            }

            // trim low-counts from end
            for(int32_t i = h.end; i >= h.begin; --i)
            {
                uint16_t addr = core_state.cpu.disassembled_prog[i].addr;
                uint64_t c = h.count * LOW_COUNT_NUM / LOW_COUNT_DENOM;
                if(profiler_state.counts[addr / 2] <= c)
                {
                    --h.end;
                    h.count -= profiler_state.counts[addr / 2];
                }
                else
                    break;
            }

            // trim from middle: N+ consecutive zero-counts
            constexpr int N = 4;
            for(int32_t ns, n = 0, i = h.begin; i <= h.end; ++i)
            {
                uint16_t addr = core_state.cpu.disassembled_prog[i].addr;
                if(profiler_state.counts[addr / 2] == 0)
                    ++n;
                else if(n >= N)
                {
                    auto& hn = profiler_state.hotspots[profiler_state.num_hotspots++];
                    hn.begin = h.begin;
                    hn.end = ns - 1;
                    hn.count = 0;
                    for(int32_t j = hn.begin; j <= hn.end; ++j)
                    {
                        uint16_t hnaddr = core_state.cpu.disassembled_prog[j].addr;
                        hn.count += profiler_state.counts[hnaddr / 2];
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
        profiler_state.hotspots.begin(),
        profiler_state.hotspots.begin() + profiler_state.num_hotspots,
        [](auto const& a, auto const& b) { return a.count > b.count; }
    );
}

ARDENS_FORCEINLINE uint32_t arduboy_t::cycle()
{
    assert(core_state.cpu.decoded);

    bool vsync = false;
    uint8_t displayport = core_state.cpu.data[reg::addr::PORTD];
    uint8_t fxport = core_state.cpu.data[peripherals.fxport_reg];

    uint32_t cycles = core_state.cpu.advance_cycle();

    // TODO: model SPI connection more precisely?
    // send SPI commands and data to peripherals.display
    peripherals.fx.set_enabled((fxport & peripherals.fxport_mask) == 0);

    if(core_state.cpu.cycle_count >= core_state.cpu.spi_done_cycle)
    {
        uint8_t byte = core_state.cpu.spi_data_byte;

        // peripherals.display enabled?
        if(!(displayport & (1 << 6)))
        {
            if(displayport & (1 << 4))
            {
                if(profiler_state.frame_bytes_total != 0 && ++profiler_state.frame_bytes >= profiler_state.frame_bytes_total)
                {
                    profiler_state.frame_bytes = 0;
                    vsync = true;
                }
                peripherals.display.send_data(byte);
            }
            else
                peripherals.display.send_command(byte);
        }

        bool was_erasing = (peripherals.fx.erasing_sector != 0);
        core_state.cpu.spi_datain_byte = peripherals.fx.spi_transceive(byte);
        if(peripherals.fx.busy_error)
            core_state.cpu.autobreak(AB_FX_BUSY);
        core_state.cpu.spi_done_cycle = UINT64_MAX;
    }

#ifndef ARDENS_NO_DEBUGGER
    if(is_present_state())
    {
        profiler_state.total_with_sleep += cycles;
        if((core_state.cpu.active || core_state.cpu.wakeup_cycles != 0) &&
            core_state.cpu.executing_instr_pc < core_state.cpu.decoded_prog.size() &&
            core_state.cpu.decoded_prog[core_state.cpu.executing_instr_pc].func != INSTR_SLEEP)
        {
            profiler_state.total += cycles;
            if(profiler_state.enabled && core_state.cpu.executing_instr_pc < profiler_state.counts.size())
            {
                profiler_state.counts[core_state.cpu.executing_instr_pc] += cycles;
            }
        }
    }
#endif

    {
        auto cycles_ps = cycles * CYCLE_PS;
        bool actual_vsync = false;
        if((core_state.cpu.data[reg::addr::PORTD] & reg::bit::PORTD::PORTD7) != 0)
        {
            actual_vsync = peripherals.display.advance(cycles_ps);
            peripherals.prev_display_reset = false;
        }
        else
        {
            if(!peripherals.prev_display_reset)
                peripherals.display.reset();
            peripherals.prev_display_reset = true;
        }
        peripherals.fx.advance(cycles_ps);
#ifndef ARDENS_NO_DEBUGGER
        if(profiler_state.frame_bytes_total == 0)
            vsync |= actual_vsync;
#endif
    }

#ifndef ARDENS_NO_DEBUGGER
    if(vsync && is_present_state())
    {
        // vsync occurred and we are profiling: store frame core_state.cpu usage
        uint64_t frame_total = profiler_state.total - profiler_state.prev_total;
        uint64_t frame_sleep = profiler_state.total_with_sleep - profiler_state.prev_total_with_sleep;
        profiler_state.prev_total = profiler_state.total;
        profiler_state.prev_total_with_sleep = profiler_state.total_with_sleep;
        double f = frame_sleep ? double(frame_total) / double(frame_sleep) : 0.0;
        profiler_state.frame_cpu_usage.push_back((float)f);
        profiler_state.prev_frame_cycles = frame_sleep;
        ++profiler_state.total_frames;

        // limit memory usage
        if(profiler_state.frame_cpu_usage.size() >= 65536)
        {
            profiler_state.frame_cpu_usage.erase(
                profiler_state.frame_cpu_usage.begin(),
                profiler_state.frame_cpu_usage.begin() + 32768);
        }
    }
#endif

#ifndef ARDENS_NO_DEBUGGER
    // time-based core_state.cpu usage
    if(core_state.cpu.cycle_count >= profiler_state.prev_ms_cycles && is_present_state())
    {
        constexpr size_t MS_PROF_FILT_NUM = 5;
        constexpr uint64_t PROF_MS = 1000000000ull * 20 / CYCLE_PS;
        profiler_state.prev_ms_cycles += PROF_MS;

        // one millisecond has passed: store core_state.cpu usage
        uint64_t ms_total = profiler_state.total - profiler_state.prev_total_ms;
        uint64_t ms_sleep = profiler_state.total_with_sleep - profiler_state.prev_total_with_sleep_ms;
        profiler_state.prev_total_ms = profiler_state.total;
        profiler_state.prev_total_with_sleep_ms = profiler_state.total_with_sleep;
        double f = ms_sleep ? double(ms_total) / double(ms_sleep) : 0.0;
        profiler_state.ms_cpu_usage_raw.push_back((float)f);
        if(profiler_state.ms_cpu_usage_raw.size() >= MS_PROF_FILT_NUM)
        {
            float t = 0.f;
            for(size_t i = 0; i < MS_PROF_FILT_NUM; ++i)
                t += profiler_state.ms_cpu_usage_raw[profiler_state.ms_cpu_usage_raw.size() - MS_PROF_FILT_NUM + i];
            profiler_state.ms_cpu_usage.push_back(t * (1.f / MS_PROF_FILT_NUM));
        }
        ++profiler_state.total_ms;

        // limit memory usage
        if(profiler_state.ms_cpu_usage.size() >= 65536)
        {
            profiler_state.ms_cpu_usage.erase(
                profiler_state.ms_cpu_usage.begin(),
                profiler_state.ms_cpu_usage.begin() + 32768);
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
    if(core_state.cpu.cycle_count >= debugger_state.present_cycle)
        debugger_state.present_state.clear();
    if(!is_present_state())
        return;

    {
        inputs_t state;
        state.cycle = core_state.cpu.cycle_count;
        state.pinb = core_state.cpu.data[reg::addr::PINB];
        state.pine = core_state.cpu.data[reg::addr::PINE];
        state.pinf = core_state.cpu.data[reg::addr::PINF];
        if(debugger_state.input_history.empty() ||
            debugger_state.input_history.back().pinb != state.pinb ||
            debugger_state.input_history.back().pine != state.pine ||
            debugger_state.input_history.back().pinf != state.pinf)
        {
            debugger_state.input_history.push_back(state);
        }
    }
    if(debugger_state.state_history.empty() ||
        core_state.cpu.cycle_count >= debugger_state.state_history.back().cycle + STATE_HISTORY_CYCLES)
    {
        tt_state_t state;
        state.cycle = core_state.cpu.cycle_count;
        save_state_to_vector(state.state);
        debugger_state.state_history.emplace_back(std::move(state));
    }
    while(debugger_state.input_history.size() >= 2 &&
        debugger_state.input_history[1].cycle + STATE_HISTORY_TOTAL_CYCLES < core_state.cpu.cycle_count)
    {
        debugger_state.input_history.erase(debugger_state.input_history.begin());
    }
    while(debugger_state.state_history.size() >= 2 &&
        debugger_state.state_history[1].cycle + STATE_HISTORY_TOTAL_CYCLES < core_state.cpu.cycle_count)
    {
        debugger_state.state_history.erase(debugger_state.state_history.begin());
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
    auto oldpc = a.core_state.cpu.pc;
    a.core_state.cpu.no_merged = true;
    a.core_state.ps_rem = 0;
    do
    {
        a.debugger_state.paused = false;
        a.cycle();
        a.debugger_state.paused = true;
    } while(++n < 65536 && a.core_state.cpu.pc == oldpc);
}

template<class F>
static void travel_back_cond(arduboy_t& a, F&& f, uint64_t max_cycle = UINT64_MAX)
{
    if(a.debugger_state.state_history.empty()) return;
    if(a.debugger_state.input_history.empty()) return;
    if(a.debugger_state.present_state.empty())
    {
        a.save_state_to_vector(a.debugger_state.present_state);
        a.debugger_state.present_cycle = a.core_state.cpu.cycle_count;
    }
    size_t si = a.debugger_state.state_history.size();
    std::vector<pc_hist_t> pcs;
    std::vector<uint8_t> temp_state;
    a.save_state_to_vector(temp_state);
    uint64_t curr_cycle = a.core_state.cpu.cycle_count;
    max_cycle = std::min(max_cycle, curr_cycle);
    while(si >= 2 && a.debugger_state.state_history[si - 1].cycle >= max_cycle)
        si -= 1;
    while(si-- > 0)
    {
        auto const& state = a.debugger_state.state_history[si];
        uint64_t end_cycle = (si + 1 < a.debugger_state.state_history.size() ?
            a.debugger_state.state_history[si + 1].cycle : a.debugger_state.present_cycle);
        end_cycle = std::min(end_cycle, curr_cycle);
        size_t ii = a.debugger_state.input_history.size();
        while(ii-- > 0)
        {
            if(a.debugger_state.input_history[ii].cycle <= state.cycle)
                break;
        }
        if(ii >= a.debugger_state.input_history.size())
            break;
        a.load_state_from_vector(state.state);
        pcs.clear();
        while(a.core_state.cpu.cycle_count < end_cycle)
        {
            while(ii + 1 < a.debugger_state.input_history.size() && a.debugger_state.input_history[ii + 1].cycle <= a.core_state.cpu.cycle_count)
                ++ii;
            auto const& input = a.debugger_state.input_history[ii];
            pc_hist_t p{};
            p.cycle = a.core_state.cpu.cycle_count;
            p.pc = a.core_state.cpu.pc;
            p.stack_depth = (uint16_t)a.core_state.cpu.num_stack_frames;
            a.core_state.cpu.data[reg::addr::PINB] = p.pinb = input.pinb;
            a.core_state.cpu.data[reg::addr::PINE] = p.pine = input.pine;
            a.core_state.cpu.data[reg::addr::PINF] = p.pinf = input.pinf;
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
                    a.core_state.cpu.data[reg::addr::PINB] = pcs[i].pinb;
                    a.core_state.cpu.data[reg::addr::PINE] = pcs[i].pine;
                    a.core_state.cpu.data[reg::addr::PINF] = pcs[i].pinf;
                    travel_back_advance_instr(a);
                }
                a.core_state.cpu.update_all();
                return;
            }
        }
    }
    // failed to travel back: reload previous state
    a.load_state_from_vector(temp_state);
    if(a.core_state.cpu.cycle_count >= a.debugger_state.present_cycle)
    {
        a.load_state_from_vector(a.debugger_state.present_state);
        a.debugger_state.present_state.clear();
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
    uint16_t tpc = core_state.cpu.pc;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.pc != tpc;
    });
}

void arduboy_t::travel_back_single_instr_over()
{
    uint16_t tpc = core_state.cpu.pc;
    uint16_t tsd = core_state.cpu.num_stack_frames;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.pc != tpc && p.stack_depth == tsd;
    });
}

void arduboy_t::travel_back_single_instr_out()
{
    uint16_t tsd = core_state.cpu.num_stack_frames;
    if(tsd == 0) return;
    uint64_t cycle = core_state.cpu.stack_frames[tsd - 1].cycle;
    assert(cycle < core_state.cpu.cycle_count);
    if(cycle >= core_state.cpu.cycle_count) return;
    travel_back_cond(*this, [=](pc_hist_t const& p) {
        return p.stack_depth < tsd;
    }, cycle);
}

void arduboy_t::travel_to_present()
{
    if(debugger_state.present_state.empty()) return;
    load_state_from_vector(debugger_state.present_state);
    debugger_state.present_state.clear();
}

void arduboy_t::travel_continue()
{
    if(debugger_state.present_state.empty()) return;
    debugger_state.present_state.clear();
    size_t i;
    i = 0;
    while(i < debugger_state.state_history.size() && debugger_state.state_history[i].cycle < core_state.cpu.cycle_count)
        ++i;
    if(i < debugger_state.state_history.size())
        debugger_state.state_history.resize(i);
    i = 0;
    while(i < debugger_state.input_history.size() && debugger_state.input_history[i].cycle < core_state.cpu.cycle_count)
        ++i;
    if(i < debugger_state.input_history.size())
        debugger_state.input_history.resize(i);
}

bool arduboy_t::is_present_state()
{
    return debugger_state.present_state.empty();
}

static void set_button_pins_from_history(arduboy_t& a)
{
    uint64_t cycle = a.core_state.cpu.cycle_count;
    auto const& inputs = a.debugger_state.input_history;
    size_t n = inputs.size();
    uint8_t pinb = reg::bit::PINB::PINB4;
    uint8_t pine = reg::bit::PINE::PINE6;
    uint8_t pinf =
        reg::bit::PINF::PINF7 |
        reg::bit::PINF::PINF6 |
        reg::bit::PINF::PINF5 |
        reg::bit::PINF::PINF4;
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
    a.core_state.cpu.data[reg::addr::PINB] = pinb;
    a.core_state.cpu.data[reg::addr::PINE] = pine;
    a.core_state.cpu.data[reg::addr::PINF] = pinf;
}

void arduboy_t::advance_instr()
{
    if(!core_state.cpu.decoded) return;
    update_history();
    set_button_pins_from_history(*this);
    int n = 0;
    auto oldpc = core_state.cpu.pc;
    core_state.cpu.no_merged = true;
    core_state.ps_rem = 0;
    do
    {
        debugger_state.paused = false;
        cycle();
        core_state.cpu.update_all();
        debugger_state.paused = true;
    } while(++n < 65536 && core_state.cpu.pc == oldpc);
}

void arduboy_t::advance(uint64_t ps)
{
    update_history();

    ps += core_state.ps_rem;
    core_state.ps_rem = 0;

    if(!core_state.cpu.decoded) return;
    if(debugger_state.paused) return;

    core_state.cpu.autobreaks = 0;

#ifndef ARDENS_NO_DEBUGGER
    bool any_breakpoints =
        debugger_state.allow_nonstep_breakpoints && (
        debugger_state.breakpoints.any() ||
        debugger_state.breakpoints_rd.any() ||
        debugger_state.breakpoints_wr.any()) ||
        debugger_state.break_step != 0xffffffff;

    core_state.cpu.no_merged = profiler_state.enabled || any_breakpoints;
#endif

    if(!is_present_state())
        core_state.cpu.no_merged = true;

    while(ps >= PS_BUFFER)
    {
        if(!is_present_state())
            set_button_pins_from_history(*this);

        uint32_t cycles = cycle();

        ps -= cycles * CYCLE_PS;

#ifndef ARDENS_NO_DEBUGGER
        if(any_breakpoints)
        {
            if(core_state.cpu.pc == debugger_state.break_step || debugger_state.allow_nonstep_breakpoints && (
                core_state.cpu.pc < debugger_state.breakpoints.size() && debugger_state.breakpoints.test(core_state.cpu.pc) ||
                core_state.cpu.just_read < debugger_state.breakpoints_rd.size() && debugger_state.breakpoints_rd.test(core_state.cpu.just_read) ||
                core_state.cpu.just_written < debugger_state.breakpoints_wr.size() && debugger_state.breakpoints_wr.test(core_state.cpu.just_written)))
            {
                debugger_state.paused = true;
                break;
            }
        }
#endif

#ifndef ARDENS_NO_DEBUGGER
        if(core_state.cpu.should_autobreak())
        {
            debugger_state.paused = true;
            break;
        }
#endif

    }

    core_state.cpu.update_all();

    // track remainder
    if(!debugger_state.paused)
        core_state.ps_rem = ps;

    if(!peripherals.display.enable_filter)
    {
        memcpy(
            peripherals.display.filtered_pixels.data(),
            peripherals.display.pixels[0].data(),
            array_bytes(peripherals.display.filtered_pixels));
    }

    // update save_data_state.savedata
    if(core_state.cpu.eeprom_dirty)
    {
        save_data_state.savedata.eeprom.resize(core_state.cpu.eeprom.size());
        save_data_state.savedata.eeprom_modified_bytes = core_state.cpu.eeprom_modified_bytes;
        memcpy(save_data_state.savedata.eeprom.data(), core_state.cpu.eeprom.data(), array_bytes(save_data_state.savedata.eeprom));
        core_state.cpu.eeprom_dirty = false;
        if(is_present_state())
            save_data_state.dirty = true;
    }
    if(peripherals.fx.sectors_dirty)
    {
        for(size_t i = 0; i < peripherals.fx.sectors_modified.size(); ++i)
        {
            if(!peripherals.fx.sectors_modified.test(i)) continue;
            auto& s = save_data_state.savedata.fx_sectors[(uint32_t)i];
            auto const& fxs = peripherals.fx.sectors[i];
            if(!fxs)
                memset(s.data(), 0xff, w25q128_t::SECTOR_BYTES);
            else
                memcpy(s.data(), fxs->data(), w25q128_t::SECTOR_BYTES);
            auto& fxsm = peripherals.fx.sectors_modified_data[i];
            if(!fxsm)
                fxsm = std::make_unique<w25q128_t::sector_t>();
            if(!fxs)
                memset(fxsm->data(), 0xff, w25q128_t::SECTOR_BYTES);
            else
                memcpy(fxsm->data(), fxs->data(), w25q128_t::SECTOR_BYTES);
        }
        peripherals.fx.sectors_dirty = false;
        if(is_present_state())
            save_data_state.dirty = true;
    }

#ifndef ARDENS_NO_DEBUGGER
    if(core_state.cpu.cycle_count >= debugger_state.present_cycle)
    {
        load_state_from_vector(debugger_state.present_state);
        debugger_state.present_state.clear();
    }
#endif
}

}

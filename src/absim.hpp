#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <vector>
#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <istream>
#include <ostream>
#include <algorithm>
#include <limits>

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <bitsery/brief_syntax.h>

#include "absim_config.hpp"

#include "absim_instructions.hpp"
#include "absim_regs.hpp"
#include "absim_pqueue.hpp"

#ifdef ARDENS_LLVM
namespace llvm
{
class DWARFContext;
namespace object { class ObjectFile; }
}
#endif

namespace absim
{

enum usb_bus_state_t
{
    USB_BUS_DISCONNECTED,
    USB_BUS_CONNECTED,
};

enum autobreak_t
{
    // special autobreak for the "break" instruction
    AB_BREAK,

    AB_STACK_OVERFLOW,
    AB_NULL_DEREF,
    AB_NULL_REL_DEREF,
    AB_OOB_DEREF,
    AB_OOB_EEPROM,
    AB_OOB_IJMP,
    AB_OOB_PC,
    AB_UNKNOWN_INSTR,
    AB_SPI_WCOL,
    AB_FX_BUSY,

    AB_NUM
};

struct int_vector_info_t
{
    char const* name;
    char const* desc;
};
extern std::array<int_vector_info_t, 43> const INT_VECTOR_INFO;

struct twi_wire_state_t
{
    bool scl_low;
    bool sda_low;
    bool scl;
    bool sda;
};

struct arduboy_t;
struct atmega32u4_t;

struct i2c_bus_participant_t
{
    bool drive_scl_low = false;
    bool drive_sda_low = false;
};

struct i2c_bus_t
{
    std::vector<i2c_bus_participant_t const*> participants;

    void clear();
    void attach(i2c_bus_participant_t const* participant);
    twi_wire_state_t resolve() const;
};

struct i2c_message_t
{
    enum class kind_t : uint8_t
    {
        START,
        ADDRESS,
        WRITE_BYTE,
        READ_REQUEST,
        READ_RESPONSE,
        MASTER_ACK,
        REPEATED_START,
        STOP,
        ABORT,
        BUS_RESET,
        RESPONSE,
    };

    kind_t kind = kind_t::RESPONSE;
    uint8_t from = 0xff;
    uint8_t to = 0xff;
    uint8_t address = 0;
    uint8_t data = 0xff;
    uint32_t transaction_id = 0;
    bool read = false;
    bool master_ack = false;
    bool ack = false;
};

struct i2c_transport_t
{
    virtual ~i2c_transport_t() = default;
    virtual void send_i2c_message(i2c_message_t const& message) = 0;
    virtual bool receive_i2c_message(uint8_t endpoint, i2c_message_t& message) = 0;
};

struct i2c_remote_endpoint_t
{
    virtual ~i2c_remote_endpoint_t() = default;
    virtual bool claims_address(uint8_t address, bool general_call) const = 0;
    virtual bool can_address(uint8_t address, bool general_call) const = 0;
    virtual bool address(uint8_t address, bool read, bool general_call) = 0;
    virtual bool write_byte(uint8_t data) = 0;
    virtual uint8_t read_byte(bool master_ack) = 0;
    virtual void stop() = 0;
};

// Transaction-level link adapter. Local TWI code sees an open-drain bus and
// clock stretching; this adapter turns local byte phases into ordered I2C
// messages that can be backed by an in-process or network transport.
struct i2c_link_adapter_t
{
    static constexpr uint8_t MAX_ENDPOINTS = 128;
    static constexpr uint8_t BROADCAST_ENDPOINT = 0xff;

    struct result_t
    {
        bool pending = false;
        bool ack = false;
        uint8_t data = 0xff;
    };

    virtual ~i2c_link_adapter_t() = default;

    void disconnect();
    void sync_bus_lines();
    void refresh_bus_lines();
    void update_bus_lines();

    void on_local_start(uint8_t endpoint, bool repeated);
    result_t request_address_ack(uint8_t endpoint, uint8_t address, bool read);
    result_t request_write_ack(uint8_t endpoint, uint8_t address, uint8_t data);
    result_t request_read_byte(uint8_t endpoint, uint8_t address, bool master_ack);
    void on_local_stop(uint8_t endpoint);

protected:
    enum pending_op_t : uint8_t
    {
        I2C_PENDING_NONE,
        I2C_PENDING_ADDRESS,
        I2C_PENDING_WRITE,
        I2C_PENDING_READ,
    };

    struct endpoint_t
    {
        bool active = false;
        uint8_t id = BROADCAST_ENDPOINT;
        std::vector<uint8_t> targets;
        std::vector<i2c_message_t> pending_responses;
        std::vector<uint8_t> pending_targets;
        pending_op_t pending_op = I2C_PENDING_NONE;
        uint32_t pending_transaction_id = 0;
        uint8_t pending_address = 0;
        uint8_t pending_data = 0xff;
        bool pending_read = false;
        bool pending_master_ack = false;
        bool pumping = false;
    };

    std::array<endpoint_t, MAX_ENDPOINTS> endpoints;
    uint8_t num_endpoints = 0;
    bool updating_lines = false;
    uint32_t next_transaction_id = 1;

    uint8_t attach_endpoint();
    bool endpoint_active(uint8_t endpoint) const;

    // Called by disconnect() after all endpoints have been detached and the
    // base protocol state has been cleared. A subclass should use this to drop
    // transport-only state such as in-process packet queues, socket buffers, or
    // mirrored remote endpoint metadata. It must not call back into the base
    // cable or assume any endpoint ids are still active.
    virtual void reset_transport() {}

    // Called once for each active endpoint during disconnect(), before that
    // endpoint's base state is cleared. A process-local implementation should
    // detach any local CPU/TWI adapter and release external line levels. A
    // network implementation should unregister the endpoint from its local
    // adapter table and may notify the remote transport, but should tolerate
    // being called while packets are still buffered.
    virtual void detach_endpoint(uint8_t endpoint) { (void)endpoint; }

    // Return the local cycle count to use when this endpoint samples the cable
    // wires. The base uses the returned cycle to ask every other endpoint what
    // it is driving at that observer-relative moment. Return false when the
    // endpoint has no local wire observer in this process; the base will skip
    // setting external line levels for that endpoint during refresh_bus_lines().
    virtual bool endpoint_sample_cycle(uint8_t endpoint, uint64_t& cycle) = 0;

    // Report the open-drain SCL/SDA drive state asserted by this endpoint at
    // sample_cycle, where sample_cycle is the observing endpoint's local cycle
    // count. Implementations should set state.scl_low/state.sda_low to the
    // actively-driven-low levels and state.scl/state.sda to their inverse wire
    // levels. Return false if the endpoint's drive state is unknown or not
    // represented in this process; the base will treat it as not driving.
    virtual bool endpoint_drive_state(
        uint8_t endpoint, uint64_t sample_cycle, twi_wire_state_t& state) = 0;

    // Apply the aggregate external cable levels visible to this endpoint. The
    // arguments mean another endpoint on the cable is pulling that line low.
    // A local CPU adapter should store these as external SCL/SDA inputs. A
    // remote-only endpoint may ignore this hook, because its process will
    // compute its own local wire inputs.
    virtual void endpoint_set_external_lines(
        uint8_t endpoint, bool scl_low, bool sda_low) = 0;

    // Return true while this endpoint has pending local TWI work that must be
    // advanced for an in-flight transaction to make progress, such as TWINT
    // being set for an interrupt handler or a scheduled TWI byte completing.
    // The base calls endpoint_pump_cycle() repeatedly while this is true.
    // Remote endpoints should normally return false and make progress by
    // receiving/sending packets through the transport instead.
    virtual bool endpoint_needs_pump(uint8_t endpoint) = 0;

    // Advance one small unit of local endpoint work. For a local Arduboy this
    // usually means advancing the CPU enough for pending TWI interrupts and
    // peripheral updates to run. This hook may cause more packets to be queued
    // or consumed, but it should not block on network I/O.
    virtual void endpoint_pump_cycle(uint8_t endpoint) = 0;

    // Return true if this endpoint currently claims the given 7-bit I2C address,
    // even if it is not ready to ACK right now. The base uses this to distinguish
    // an unowned address from a busy owned address, and to detect contested
    // nonzero addresses. For address 0, general_call tells the implementation
    // whether the transaction is the standard general-call path.
    virtual bool endpoint_claims_address(
        uint8_t endpoint, uint8_t address, bool general_call) = 0;

    // Return true if this endpoint is ready to ACK an address phase for the
    // given 7-bit address. This should imply endpoint_claims_address() is true,
    // but also account for local readiness such as TWI enable, ACK enable, power
    // state, and any emulated clock-stretch/busy condition.
    virtual bool endpoint_can_address(
        uint8_t endpoint, uint8_t address, bool general_call) = 0;

    // Deliver an addressed slave transaction to this endpoint. read is true for
    // SLA+R and false for SLA+W. Return true to ACK the address phase or false
    // to NACK it. If this returns true, the endpoint should update its local TWI
    // state as though hardware had accepted the address and should be prepared
    // for subsequent endpoint_write(), endpoint_read(), or endpoint_stop() calls.
    virtual bool endpoint_address(
        uint8_t endpoint, uint8_t address, bool read, bool general_call) = 0;

    // Deliver one data byte from the current master to an endpoint that ACKed a
    // write address phase. Return true if the endpoint ACKs this byte. A false
    // return is a data NACK and ends useful writes for the current transaction
    // from the master's perspective.
    virtual bool endpoint_write(uint8_t endpoint, uint8_t data) = 0;

    // Fetch one data byte from an endpoint that ACKed a read address phase.
    // master_ack is the ACK bit the master will send after receiving the byte;
    // false means the master is NACKing the byte and generally ending the read.
    // Return 0xff if no local endpoint can provide data.
    virtual uint8_t endpoint_read(uint8_t endpoint, bool master_ack) = 0;

    // Notify this endpoint that the current addressed transaction ended with a
    // STOP. Implementations should update slave receive/transmit state and run
    // any stop-condition behavior, but should not send a RESPONSE packet because
    // STOP packets are one-way notifications in the cable protocol.
    virtual void endpoint_stop(uint8_t endpoint) = 0;

    // Queue message for delivery to message.to. BROADCAST_ENDPOINT means the
    // transport should enqueue or transmit a copy to every active endpoint other
    // than message.from. This function must be non-blocking with respect to
    // remote peers; if the underlying transport is asynchronous, buffer the
    // message and let receive_message() surface responses later.
    virtual void send_message(i2c_message_t const& message) = 0;

    // Try to receive the next message addressed to endpoint. Return true and
    // fill message when one is available; return false immediately when none is
    // ready. FIFO ordering per sender/receiver pair is required.
    virtual bool receive_message(uint8_t endpoint, i2c_message_t& message) = 0;

private:
    void reset_endpoint(endpoint_t& endpoint);
    void poll_messages();
    bool drain_messages_once();
    void handle_message(uint8_t endpoint, i2c_message_t const& message);
    void pump_endpoint(uint8_t endpoint);
    std::vector<uint8_t> route_address_targets(
        uint8_t endpoint, uint8_t address, bool& address_busy);
    void begin_request(
        endpoint_t& endpoint,
        pending_op_t op,
        std::vector<uint8_t> const& targets,
        i2c_message_t const& message);
    result_t finish_request(endpoint_t& endpoint);
};

struct local_i2c_transaction_bridge_t : i2c_link_adapter_t
{
    void connect(std::vector<arduboy_t*> const& devices);
    uint64_t take_pumped_cycles(arduboy_t const* device);

protected:
    void reset_transport() override;
    void send_message(i2c_message_t const& message) override;
    bool receive_message(uint8_t endpoint, i2c_message_t& message) override;

private:
    std::array<arduboy_t*, MAX_ENDPOINTS> local_devices{};
    std::array<uint64_t, MAX_ENDPOINTS> local_pumped_cycles{};
    std::array<std::deque<i2c_message_t>, MAX_ENDPOINTS> queues;

    void detach_endpoint(uint8_t endpoint) override;
    bool endpoint_sample_cycle(uint8_t endpoint, uint64_t& cycle) override;
    bool endpoint_drive_state(
        uint8_t endpoint, uint64_t sample_cycle, twi_wire_state_t& state) override;
    void endpoint_set_external_lines(
        uint8_t endpoint, bool scl_low, bool sda_low) override;

protected:
    void endpoint_pump_cycle(uint8_t endpoint) override;

private:
    bool endpoint_needs_pump(uint8_t endpoint) override;
    bool endpoint_claims_address(
        uint8_t endpoint, uint8_t address, bool general_call) override;
    bool endpoint_can_address(
        uint8_t endpoint, uint8_t address, bool general_call) override;
    bool endpoint_address(
        uint8_t endpoint, uint8_t address, bool read, bool general_call) override;
    bool endpoint_write(uint8_t endpoint, uint8_t data) override;
    uint8_t endpoint_read(uint8_t endpoint, bool master_ack) override;
    void endpoint_stop(uint8_t endpoint) override;
    arduboy_t* local_device(uint8_t endpoint) const;
    atmega32u4_t* local_cpu(uint8_t endpoint) const;
};

struct avr_instr_t
{
    uint16_t word;
    uint8_t src;
    uint8_t dst;
    uint8_t func;

    // extra data for merged instructions
    uint8_t m0;
    uint8_t m1;
    uint8_t m2;
};

struct atmega32u4_t
{
    static constexpr size_t PROG_SIZE_BYTES = 32 * 1024;
    static constexpr size_t BOOTLOADER_FLASH_BYTES = 3 * 1024;
    static constexpr size_t PROGRAM_FLASH_BYTES =
        PROG_SIZE_BYTES - BOOTLOADER_FLASH_BYTES;
    static constexpr size_t DATA_SIZE_BYTES = 2560 + 256;

    std::array<uint8_t, DATA_SIZE_BYTES> data;

    inline uint8_t& gpr(uint8_t n)
    {
        assert(n < 32);
        return data[n];
    }

    uint32_t just_read;
    uint32_t just_written;
    bool io_reg_accessed;

    using ld_handler_t = uint8_t(*)(atmega32u4_t& cpu, uint16_t ptr);
    using st_handler_t = void(*)(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    std::array<ld_handler_t, 256> ld_handlers;
    std::array<st_handler_t, 256> st_handlers;

    template<bool merged>
    ARDENS_FORCEINLINE uint8_t ld(uint16_t ptr)
    {
        check_deref(ptr);
        if(!merged)
            just_read = ptr;
        if(ptr < ld_handlers.size())
        {
            if(merged)
                io_reg_accessed = true;
            if(ld_handlers[ptr])
                return ld_handlers[ptr](*this, ptr);
        }
        return ptr < data.size() ? data[ptr] : 0x00;
    }
    ARDENS_FORCEINLINE uint8_t ld(uint16_t ptr) { return ld<false>(ptr); }
    template<bool merged>
    ARDENS_FORCEINLINE void st(uint16_t ptr, uint8_t x)
    {
        check_deref(ptr);
        if(!merged)
            just_written = ptr;
        if(ptr < st_handlers.size())
        {
            if(merged)
                io_reg_accessed = true;
            if(st_handlers[ptr])
                return st_handlers[ptr](*this, ptr, x);
        }
        if(ptr < data.size()) data[ptr] = x;
    }
    ARDENS_FORCEINLINE void st(uint16_t ptr, uint8_t x) { st<false>(ptr, x); }

    template<bool merged>
    ARDENS_FORCEINLINE uint8_t ld_ior(uint8_t n)
    {
        return ld<merged>(n + 32);
    }
    ARDENS_FORCEINLINE uint8_t ld_ior(uint8_t n) { return ld_ior<false>(n); }
    template<bool merged>
    ARDENS_FORCEINLINE void st_ior(uint8_t n, uint8_t x)
    {
        st<merged>(n + 32, x);
    }
    ARDENS_FORCEINLINE void st_ior(uint8_t n, uint8_t x) { st_ior<false>(n, x); }

    ARDENS_FORCEINLINE uint16_t gpr_word(uint8_t n)
    {
#if defined(ARDENS_LE)
        assert(n % 2 == 0);
        return *reinterpret_cast<uint16_t const*>(&data[n]);
#else
        uint16_t lo = gpr(n + 0);
        uint16_t hi = gpr(n + 1);
        return lo + hi * 256;
#endif
    }

    ARDENS_FORCEINLINE uint16_t w_word() { return gpr_word(24); }
    ARDENS_FORCEINLINE uint16_t x_word() { return gpr_word(26); }
    ARDENS_FORCEINLINE uint16_t y_word() { return gpr_word(28); }
    ARDENS_FORCEINLINE uint16_t z_word() { return gpr_word(30); }

    // false if the cpu is sleeping
    bool active;
    uint8_t wakeup_cycles; // for tracking interrupt wakeup delay
    bool just_interrupted;

    ARDENS_FORCEINLINE uint16_t sp()
    {
        return (uint16_t)data[reg::addr::SPL] |
            ((uint16_t)data[reg::addr::SPH] << 8);
    }

    ARDENS_FORCEINLINE uint16_t sp() const
    {
        return (uint16_t)data[reg::addr::SPL] |
            ((uint16_t)data[reg::addr::SPH] << 8);
    }

    uint32_t min_stack; // lowest value for SP
    uint32_t stack_check; // max allowable value for SP
    bool pushed_at_least_once;

    std::bitset<AB_NUM> autobreaks;
    std::bitset<AB_NUM> enabled_autobreaks;
    //autobreak_t autobreak;
    //std::array<bool, AB_NUM> enable_autobreaks;
    ARDENS_FORCEINLINE void autobreak(autobreak_t t)
    {
        (void)t;
#ifndef ARDENS_NO_DEBUGGER
        autobreaks.set(t);
#endif
    }
    ARDENS_FORCEINLINE bool should_autobreak() const
    {
#ifndef ARDENS_NO_DEBUGGER
        return autobreaks.any() && (autobreaks & enabled_autobreaks).any();
#else
        return false;
#endif
    }
    ARDENS_FORCEINLINE bool should_autobreak_gui() const
    {
#ifndef ARDENS_NO_DEBUGGER
        auto ab = autobreaks;
        ab.reset(AB_BREAK);
        return (ab & enabled_autobreaks).any();
#else
        return false;
#endif
    }

    ARDENS_FORCEINLINE void check_deref(uint16_t addr)
    {
        (void)addr;
#ifndef ARDENS_NO_DEBUGGER
        if(addr == 0)
            autobreak(AB_NULL_DEREF);
        else if(addr >= data.size())
            autobreak(AB_OOB_DEREF);
#endif
    }

    ARDENS_FORCEINLINE void check_stack_overflow(uint16_t tsp)
    {
        (void)tsp;
#ifndef ARDENS_NO_DEBUGGER
        if(!pushed_at_least_once) return;
        // check min stack
        min_stack = std::min<uint32_t>(min_stack, tsp);
        if(tsp < stack_check)
            autobreak(AB_STACK_OVERFLOW);
#endif
    }

    ARDENS_FORCEINLINE void check_stack_overflow()
    {
#ifndef ARDENS_NO_DEBUGGER
        check_stack_overflow(sp());
#endif
    }

    ARDENS_FORCEINLINE void push(uint8_t x)
    {
        uint16_t tsp = sp();
        st(tsp, x);
        --tsp;
        data[reg::addr::SPL] = uint8_t(tsp >> 0);
        data[reg::addr::SPH] = uint8_t(tsp >> 8);
        pushed_at_least_once = true;
        check_stack_overflow(tsp);
    }

    ARDENS_FORCEINLINE uint8_t pop()
    {
        uint16_t tsp = sp();
        ++tsp;
        uint8_t x = ld(tsp);
        data[reg::addr::SPL] = uint8_t(tsp >> 0);
        data[reg::addr::SPH] = uint8_t(tsp >> 8);
        return x;
    }

    std::array<uint8_t, PROG_SIZE_BYTES> prog; // program flash memory
    std::array<uint8_t, 1024>  eeprom; // EEPROM
    std::bitset<1024> eeprom_modified_bytes;
    bool eeprom_modified;
    bool eeprom_dirty;

    // SREG after previous instruction (used for interrupt bit)
    uint8_t prev_sreg;

    uint16_t pc;                       // program counter

    uint16_t executing_instr_pc;

    static constexpr size_t MAX_STACK_FRAMES = 1280;
    struct stack_frame_t 
    {
        uint64_t cycle;
        uint16_t pc;
        uint16_t sp;
    };
    std::array<stack_frame_t, MAX_STACK_FRAMES> stack_frames;
    uint32_t num_stack_frames;
    ARDENS_FORCEINLINE void push_stack_frame(uint16_t ret_addr)
    {
        (void)ret_addr;
#ifndef ARDENS_NO_DEBUGGER
        if(num_stack_frames < stack_frames.size())
            stack_frames[num_stack_frames++] = { cycle_count, ret_addr, sp() };
#endif
    }
    ARDENS_FORCEINLINE void pop_stack_frame()
    {
#ifndef ARDENS_NO_DEBUGGER
        auto tsp = sp();
        while(num_stack_frames > 0)
        {
            auto f = stack_frames[num_stack_frames - 1];
            if(f.sp > tsp) break;
            --num_stack_frames;
        }
#endif
    }

    static constexpr int MAX_INSTR_CYCLES = 4;

    uint16_t last_addr;
    uint16_t num_instrs;
    uint16_t num_instrs_total;
    bool no_merged;
    std::array<avr_instr_t, PROG_SIZE_BYTES / 2> decoded_prog;
    std::array<avr_instr_t, PROG_SIZE_BYTES / 2> merged_prog; // decoded and merged instrs
    std::array<disassembled_instr_t, PROG_SIZE_BYTES / 2> disassembled_prog;
    bool program_loaded;
    bool decoded;
    void decode();
    void merge_instrs();
    size_t addr_to_disassembled_index(uint16_t addr);

    static void st_handle_pin(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_ddrd(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_port(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    static void st_handle_prr0(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_prr1(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_gtccr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    pqueue peripheral_queue;

    static void st_handler_timsk(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // timer0/1/3 shared synchronous timer domain
    struct timer_sync_t
    {
        uint64_t prev_update_cycle;
        uint64_t next_update_cycle;
        uint32_t prescaler_cycle;
        template<class A> void serialize(A& a)
        {
            a(prev_update_cycle, next_update_cycle, prescaler_cycle);
        }
    };
    timer_sync_t timer_sync;

    // timer0
    struct timer8_t
    {
        uint64_t next_update_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa;
        uint32_t ocrNb;
        uint32_t ocrNa_buffer;
        uint32_t ocrNb_buffer;
        bool phase_correct;
        bool fast_pwm;
        bool count_down;
        bool update_ocrN_at_top;
        bool compare_block_next_tick;
        template<class A> void serialize(A& a)
        {
            a(next_update_cycle);
            a(divider);
            a(top, tov, tcnt, ocrNa, ocrNb);
            a(ocrNa_buffer, ocrNb_buffer);
            a(phase_correct, fast_pwm, count_down);
            a(update_ocrN_at_top, compare_block_next_tick);
        }
    };
    timer8_t timer0;
    static void timer0_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer0_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer0_handle_st_tcnt(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer0_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr);
    void update_sync_timers();
    void update_timer0();

    // timer 1 or 3
    struct timer16_t
    {
        uint64_t next_update_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa;
        uint32_t ocrNb;
        uint32_t ocrNc;
        uint32_t ocrNa_buffer;
        uint32_t ocrNb_buffer;
        uint32_t ocrNc_buffer;
        uint32_t icrN;
        uint32_t tifrN_addr;
        uint32_t timskN_addr;
        uint32_t prr_addr;
        uint32_t prr_mask;
        uint32_t base_addr;
        uint32_t com3a;
        uint8_t temp; // reg for 16-bit access
        uint8_t delayed_tifr_flags;
        bool phase_correct;
        bool count_down;
        bool update_ocrN_at_top;
        bool update_ocrN_at_bottom;
        bool fast_pwm;
        bool top_source_icr;
        bool compare_block_next_tick;
        template<class A> void serialize(A& a)
        {
            a(next_update_cycle);
            a(divider);
            a(top, tov, tcnt, ocrNa, ocrNb, ocrNc);
            a(ocrNa_buffer, ocrNb_buffer, ocrNc_buffer, icrN);
            a(tifrN_addr, timskN_addr, prr_addr, prr_mask, base_addr);
            a(com3a, temp, delayed_tifr_flags);
            a(phase_correct, count_down);
            a(update_ocrN_at_top, update_ocrN_at_bottom);
            a(fast_pwm, top_source_icr, compare_block_next_tick);
        }
    };
    static void timer1_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer3_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer1_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer3_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer1_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr);
    static uint8_t timer3_handle_ld_regs(atmega32u4_t& cpu, uint16_t ptr);
    void update_timer1();
    void update_timer3();

    timer16_t timer1;
    timer16_t timer3;

    // timer 4
    struct timer10_t
    {
        uint64_t prev_update_cycle;
        uint64_t next_update_cycle;
        uint32_t source_cycle;
        uint32_t divider_cycle;
        uint32_t divider;
        uint32_t top;
        uint32_t tov;
        uint32_t tcnt;
        uint32_t ocrNa_next;
        uint32_t ocrNb_next;
        uint32_t ocrNc_next;
        uint32_t ocrNd_next;
        uint32_t ocrNa;
        uint32_t ocrNb;
        uint32_t ocrNc;
        uint32_t ocrNd;
        uint32_t com4a;
        uint8_t tc4h_latch;
        bool tlock;
        bool enhc;
        bool phase_correct;
        bool count_down;
        bool update_ocrN_at_top;
        bool update_ocrN_at_bottom;
        bool compare_block_next_tick;
        bool tcnt_write_pending;
        bool tcnt_write_pending_seen;
        uint32_t start_delay_cycles;
        uint32_t tcnt_write_value;
        template<class A> void serialize(A& a)
        {
            a(prev_update_cycle, next_update_cycle);
            a(source_cycle, divider_cycle, divider);
            a(top, tov, tcnt);
            a(ocrNa, ocrNa_next);
            a(ocrNb, ocrNb_next);
            a(ocrNc, ocrNc_next);
            a(ocrNd, ocrNd_next);
            a(com4a, tc4h_latch);
            a(tlock, enhc, phase_correct, count_down);
            a(update_ocrN_at_top, update_ocrN_at_bottom);
            a(compare_block_next_tick, tcnt_write_pending, tcnt_write_pending_seen);
            a(start_delay_cycles);
            a(tcnt_write_value);
        }
    };
    static void timer4_handle_st_ocrN(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer4_handle_st_regs(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void timer4_handle_st_tifr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t timer4_handle_ld_tcnt(atmega32u4_t& cpu, uint16_t ptr);
    void update_timer4();
    timer10_t timer4;

    // LEDs
    uint8_t led_tx() const;
    uint8_t led_rx() const;
    void led_rgb(uint8_t& r, uint8_t& g, uint8_t& b) const;

    // PLL
    uint64_t pll_prev_cycle;
    uint64_t pll_lock_cycle;
    uint32_t pll_num12; // numerator /12 of pll cycles per main cycle
    bool pll_busy;
    void update_pll();
    void pll_schedule();
    static void pll_handle_st_pllcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // SPI
    bool spsr_read_after_transmit;
    bool spi_busy;
    bool spi_busy_clear; // TODO: unused
    bool spi_latch_read; // TODO: unused
    bool spi_data_latched; // TODO: unused
    uint8_t spi_data_byte;
    uint8_t spi_datain_byte;
    uint64_t spi_done_cycle;
    uint64_t spi_transmit_zero_cycle;
    uint32_t spi_clock_cycles;
    void update_spi();
    static void spi_handle_st_spcr_or_spsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void spi_handle_st_spdr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t spi_handle_ld_spsr(atmega32u4_t& cpu, uint16_t ptr);
    static uint8_t spi_handle_ld_spdr(atmega32u4_t& cpu, uint16_t ptr);

    // TWI
    enum
    {
        TWI_MODE_IDLE,
        TWI_MODE_MASTER_TX,
        TWI_MODE_MASTER_RX,
        TWI_MODE_SLAVE_RX,
        TWI_MODE_SLAVE_TX,
    };
    enum
    {
        TWI_PENDING_NONE,
        TWI_PENDING_START,
        TWI_PENDING_STOP,
        TWI_PENDING_ADDRESS,
        TWI_PENDING_WRITE,
        TWI_PENDING_READ,
    };
    i2c_link_adapter_t* twi_adapter;
    uint8_t twi_adapter_endpoint;
    uint64_t twi_prev_cycle;
    uint64_t twi_done_cycle;
    uint8_t twi_mode;
    uint8_t twi_pending;
    uint8_t twi_status;
    uint8_t twi_address;
    bool twi_busy;
    bool twi_started;
    bool twi_repeated_start;
    bool twi_reading;
    bool twi_general_call;
    bool twi_pull_scl_low;
    bool twi_pull_sda_low;
    bool twi_external_scl_low;
    bool twi_external_sda_low;
    void update_twi();
    void twi_schedule(uint8_t pending, uint32_t cycles);
    void twi_finish_status(uint8_t status);
    uint32_t twi_byte_cycles() const;
    void twi_handle_prr0(uint8_t x);
    twi_wire_state_t twi_wire_state() const;
    twi_wire_state_t twi_drive_state() const;
    twi_wire_state_t twi_drive_state_at(uint64_t sample_cycle) const;
    void set_twi_external_lines(bool scl_low, bool sda_low);
    void attach_i2c_adapter(i2c_link_adapter_t* link, uint8_t endpoint);
    static uint8_t twi_handle_ld_pind(atmega32u4_t& cpu, uint16_t ptr);
    bool twi_slave_claims_address(uint8_t address, bool general_call) const;
    bool twi_slave_can_address(uint8_t address, bool general_call) const;
    bool twi_slave_address(uint8_t address, bool read, bool general_call);
    bool twi_slave_write(uint8_t data);
    uint8_t twi_slave_read(bool master_ack);
    void twi_slave_stop();
    static void twi_handle_st_twcr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void twi_handle_st_twsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void twi_handle_st_twdr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void twi_handle_st_twar_or_twamr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // EEPROM
    uint64_t eeprom_prev_cycle;
    uint32_t eeprom_clear_eempe_cycles;
    uint32_t eeprom_write_addr;
    uint32_t eeprom_write_data;
    uint32_t eeprom_program_cycles;
    bool eeprom_busy;
    void update_eeprom();
    static void eeprom_handle_st_eecr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // ADC
    uint64_t adc_prev_cycle;
    uint32_t adc_prescaler_cycle;
    uint32_t adc_cycle;
    uint32_t adc_ref;
    uint32_t adc_result;
    uint32_t adc_seed;
    bool adc_busy;
    bool adc_nondeterminism;
    void update_adc();
    void adc_schedule();
    void adc_handle_prr0(uint8_t x);
    static void adc_st_handle_adcsra(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);

    // sound
    static constexpr int SOUND_CYCLES = 320;
    static constexpr int16_t SOUND_GAIN = 2000;
    uint64_t sound_prev_cycle;
    uint32_t sound_cycle;
    uint32_t sound_enabled; // bitmask of pins 1 and 2
    bool sound_pwm;
    int16_t sound_pwm_val;
    std::vector<int16_t> sound_buffer;
    static void sound_st_handler_ddrc(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void update_sound();

    // serial / USB
    std::vector<uint8_t> serial_bytes;
    struct usb_bank_t
    {
        std::array<uint8_t, 512> data;
        uint16_t length;
        uint16_t offset;
        bool ready;

        template<class A> void serialize(A& a)
        {
            a(data, length, offset, ready);
        }
    };
    struct usb_endpoint_t
    {
        uint8_t ueintx;
        uint8_t ueconx;
        uint8_t uecfg0x;
        uint8_t uecfg1x;
        uint8_t uesta0x;
        uint8_t uesta1x;
        uint8_t ueienx;
        uint8_t uebclx;
        uint8_t uebchx;
        std::array<usb_bank_t, 2> banks;
        uint16_t size;
        uint16_t dpram_offset;
        uint8_t bank_count;
        uint8_t cpu_bank;
        bool allocated;

        template<class A> void serialize(A& a)
        {
            a(ueintx, ueconx);
            a(uesta0x, uesta1x);
            a(uecfg0x, uecfg1x);
            a(ueienx, uebclx, uebchx);
            a(banks, size, dpram_offset, bank_count, cpu_bank, allocated);
        }
    };
    struct usb_control_transfer_t
    {
        enum stage_t : uint8_t
        {
            STAGE_IDLE,
            STAGE_SETUP,
            STAGE_DATA_IN,
            STAGE_DATA_OUT,
            STAGE_STATUS_IN,
            STAGE_STATUS_OUT,
        };

        std::array<uint8_t, 8> setup;
        std::array<uint8_t, 64> out_data;
        uint16_t out_length;
        uint16_t out_offset;
        uint16_t in_expected;
        uint16_t in_drained;
        uint8_t request;
        uint8_t stage;
        bool active;

        template<class A> void serialize(A& a)
        {
            a(setup, out_data, out_length, out_offset);
            a(in_expected, in_drained, request, stage, active);
        }
    };
    struct usb_fake_host_t
    {
        enum phase_t : uint8_t
        {
            PHASE_IDLE,
            PHASE_SET_ADDRESS,
            PHASE_DEVICE_DESCRIPTOR,
            PHASE_CONFIGURATION_HEADER,
            PHASE_CONFIGURATION_DESCRIPTOR,
            PHASE_SET_CONFIGURATION,
            PHASE_SET_LINE_ENCODING,
            PHASE_SET_CONTROL_LINE_STATE,
            PHASE_ENUMERATED,
        };

        uint64_t next_cycle;
        uint8_t phase;
        uint8_t address;
        bool attached;
        bool reset_sent;
        bool configured;
        bool line_encoding_set;
        bool control_line_state_set;

        template<class A> void serialize(A& a)
        {
            a(next_cycle, phase, address, attached, reset_sent);
            a(configured, line_encoding_set, control_line_state_set);
        }
    };
    struct usb_state_t
    {
        usb_bus_state_t bus_state = USB_BUS_CONNECTED;
        std::array<usb_endpoint_t, 8> endpoints;
        usb_control_transfer_t control;
        usb_fake_host_t host;
        std::array<uint8_t, 832> dpram;
        uint64_t next_sof_cycle;
        uint64_t next_reset_cycle;
        uint64_t next_update_cycle;
        uint16_t frame_number;
        uint8_t selected_endpoint;
        uint8_t uerst;

        template<class A> void serialize(A& a)
        {
            a(bus_state, endpoints, control, host, dpram);
            a(next_sof_cycle, next_reset_cycle, next_update_cycle);
            a(frame_number, selected_endpoint, uerst);
        }
    };
    usb_state_t usb;
    static void usb_st_handler(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static uint8_t usb_ld_handler(atmega32u4_t& cpu, uint16_t ptr);
    void update_usb();
    void reset_usb();

    // SPM
    uint64_t spm_prev_cycle;
    bool spm_busy;
    uint8_t spm_en_cycles;
    enum
    {
        SPM_OP_NONE,
        SPM_OP_PAGE_LOAD,
        SPM_OP_PAGE_ERASE,
        SPM_OP_PAGE_WRITE,
        SPM_OP_BLB_SET,
        SPM_OP_RWW_EN,
        SPM_OP_SIG_READ,
    };
    uint8_t spm_op;
    uint32_t spm_cycles;
    std::array<uint8_t, 128> spm_buffer;
    static void st_handle_spmcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void execute_spm();
    void update_spm();
    void erase_spm_buffer() { memset(spm_buffer.data(), 0xff, spm_buffer.size()); }

    // watchdog
    uint32_t watchdog_divider;
    uint32_t watchdog_divider_cycle;
    uint64_t watchdog_prev_cycle;
    uint64_t watchdog_next_cycle;
    static void st_handle_mcucr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_mcusr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    static void st_handle_wdtcsr(atmega32u4_t& cpu, uint16_t ptr, uint8_t x);
    void update_watchdog_prescaler();
    void update_watchdog();

    void schedule_interrupt_check();
    void check_all_interrupts();
    bool check_interrupt(uint8_t vector, uint8_t flag, uint8_t& tifr);

    uint64_t cycle_count;

    // lock bits
    uint8_t lock;

    // fuses
    uint8_t fuse_lo;
    uint8_t fuse_hi;
    uint8_t fuse_ext;

    inline bool HWBE   () const { return (fuse_ext & (1 << 3)) == 0; }

    inline bool OCDEN  () const { return (fuse_hi  & (1 << 7)) == 0; }
    inline bool JTAGEN () const { return (fuse_hi  & (1 << 6)) == 0; }
    inline bool SPIEN  () const { return (fuse_hi  & (1 << 5)) == 0; }
    inline bool WDTON  () const { return (fuse_hi  & (1 << 4)) == 0; }
    inline bool EESAVE () const { return (fuse_hi  & (1 << 3)) == 0; }
    inline bool BOOTRST() const { return (fuse_hi  & (1 << 0)) == 0; }

    inline bool CKDIV8 () const { return (fuse_lo  & (1 << 7)) == 0; }
    inline bool CKOUT  () const { return (fuse_lo  & (1 << 6)) == 0; }

    inline uint8_t BOOTSZ() const { return (fuse_hi >> 1) & 0x3; }
    inline uint8_t SUT   () const { return (fuse_lo >> 4) & 0x3; }
    inline uint8_t CLKSEL() const { return (fuse_lo >> 0) & 0xf; }
    
    inline uint16_t bootloader_address() const
    {
        static uint16_t const ADDRS[4] = { 0x3800, 0x3c00, 0x3e00, 0x3f00 };
        return ADDRS[BOOTSZ() & 3];
    }

    void reset();
    void soft_reset(); // for WDT

    // execute at least one cycle (return how many cycles were executed)
    uint32_t advance_cycle();

    // update delayed peripheral states
    void update_all();
};

struct display_t
{
    std::array<uint8_t, 8192> filtered_pixels;
    std::array<uint16_t, 8192> filtered_pixel_counts;

    enum type_t
    {
        SSD1306,
        SSD1309,
        SH1106,
    };
    type_t type;

    // moving average
    static constexpr int MAX_PIXEL_HISTORY = 4;
    std::array<std::array<uint8_t, 8192>, MAX_PIXEL_HISTORY> pixels;
    int pixel_history_index;
    bool enable_filter;

    // physical display RAM
    std::array<uint8_t, 1024> ram;

    // segment driver current at 0xff contrast in mA
    // Arduboy: 195uA (0.195)
    float ref_segment_current;
    float current_limit_slope;
    static constexpr float MAX_DRIVER_CURRENT = 15.f;
    bool enable_current_limiting;
    float prev_row_drive;

    uint8_t contrast;
    bool entire_display_on;
    bool inverse_display;
    bool display_on;
    bool enable_charge_pump;

    // memory addressing mode
    enum class addr_mode
    {
        HORIZONTAL,
        VERTICAL,
        PAGE,
        INVALID
    } addressing_mode;

    // for horizontal or vertical addressing mode
    uint8_t col_start;
    uint8_t col_end;
    uint8_t page_start;
    uint8_t page_end;

    uint8_t mux_ratio;

    uint8_t display_offset;
    uint8_t display_start;

    bool com_scan_direction; // true: right-to-left
    bool alternative_com;
    bool com_remap;
    bool segment_remap;

    // Fosc values for each of the 16 command settings
    uint8_t fosc_index;
    uint8_t divide_ratio;
    uint8_t phase_1;
    uint8_t phase_2;
    uint8_t vcomh_deselect;

    double fosc() const;
    double refresh_rate() const;

    void reset();

    void update_clocking();

    // display refresh state
    uint8_t row;
    uint8_t row_cycle;
    uint8_t cycles_per_row;
    uint64_t ps_per_clk;

    void update_pixels_row();
    void filter_pixels();

    uint64_t ps_rem;

    bool processing_command;
    uint8_t current_command;
    uint8_t command_byte_index;

    // data address state
    uint8_t data_page;
    uint8_t data_col;

    // whether a vsync has just occurred
    bool vsync;

    void send_data(uint8_t byte);
    void send_command(uint8_t byte);

    // advance controller state by a given time
    // returns true if vsync occurred
    bool advance(uint64_t ps);
};

struct w25q128_t
{
    static constexpr size_t NUM_SECTORS = 4096;
    static constexpr size_t SECTOR_BYTES = 4096;
    static constexpr size_t DATA_BYTES = NUM_SECTORS * SECTOR_BYTES;
    static constexpr size_t PAGE_BYTES = 256;
    static constexpr uint32_t NUM_PAGES = uint32_t(DATA_BYTES / PAGE_BYTES);
    static constexpr uint32_t LAST_PAGE = NUM_PAGES - 1;
    static constexpr uint32_t EMPTY_PAGE_SENTINEL = LAST_PAGE;

    struct fx_data_save_layout_t
    {
        size_t data_bytes;
        size_t save_bytes;
        size_t data_offset;
        size_t save_offset;

        bool has_payload() const
        {
            return data_bytes != 0 || save_bytes != 0;
        }

        uint32_t min_page() const
        {
            return has_payload() ?
                uint32_t(data_offset / PAGE_BYTES) :
                EMPTY_PAGE_SENTINEL;
        }
    };

    static bool round_up_bytes(size_t size, size_t align, size_t& rounded)
    {
        if(size > std::numeric_limits<size_t>::max() - (align - 1))
            return true;
        rounded = (size + align - 1) & ~(align - 1);
        return false;
    }

    static bool make_data_save_layout(
        size_t data_size,
        size_t save_size,
        fx_data_save_layout_t& layout)
    {
        size_t data_bytes = 0;
        size_t save_bytes = 0;
        if(round_up_bytes(data_size, PAGE_BYTES, data_bytes) ||
            round_up_bytes(save_size, SECTOR_BYTES, save_bytes) ||
            data_bytes > DATA_BYTES ||
            save_bytes > DATA_BYTES - data_bytes)
        {
            return false;
        }

        layout.data_bytes = data_bytes;
        layout.save_bytes = save_bytes;
        layout.save_offset = DATA_BYTES - save_bytes;
        layout.data_offset = layout.save_offset - data_bytes;
        return true;
    }

    using sector_t = std::array<uint8_t, SECTOR_BYTES>;
    std::array<std::unique_ptr<sector_t>, NUM_SECTORS> sectors;
    std::array<std::unique_ptr<sector_t>, NUM_SECTORS> sectors_modified_data;

    std::bitset<NUM_SECTORS> sectors_modified;
    bool sectors_dirty;

    bool enabled;
    bool woken_up;
    bool write_enabled;
    bool reading_status;
    bool processing_command;
    uint8_t reading;
    uint8_t programming;
    uint8_t erasing_sector;
    uint8_t releasing;
    uint8_t reading_jedec_id;
    uint64_t busy_ps_rem;
    uint32_t current_addr;

    enum
    {
        CMD_NONE,
        CMD_RELEASE_POWER_DOWN,
        CMD_PAGE_PROGRAM,
        CMD_READ_DATA,
        CMD_WRITE_DISABLE,
        CMD_READ_STATUS_REGISTER_1,
        CMD_WRITE_ENABLE,
        CMD_SECTOR_ERASE,
        CMD_JEDEC_ID,
        CMD_UNKNOWN,
        NUM_CMDS,
    };

    uint32_t command;

    uint32_t min_page;
    uint32_t max_page;

    bool busy_error;

    void reset();
    void erase_all_data();
    void set_empty_page_range();
    bool has_empty_page_range() const;

    uint8_t read_byte(size_t addr);
    void write_byte(size_t addr, uint8_t data);
    void program_byte(size_t addr, uint8_t data);
    void write_bytes(size_t addr, uint8_t const* data, size_t bytes);

    void advance(uint64_t ps);

    void set_enabled(bool e);
    uint8_t spi_transceive(uint8_t byte);
    void track_page();
};

struct elf_data_symbol_t
{
    std::string name;
    uint16_t addr;
    uint16_t size;
    uint16_t color_index;
    bool weak;
    bool global;
    bool notype;
    bool object;
};

struct icompare
{
    static char lower(char c)
    {
        if(c >= 'A' && c <= 'Z') return c + 'a' - 'A';
        return c;
    }
    int compare(std::string const& a, std::string const& b) const
    {
        auto sa = a.size();
        auto sb = b.size();
        auto s = (sa < sb ? sa : sb);
        for(size_t i = 0; i < s; ++i)
        {
            char ca = lower(a[i]);
            char cb = lower(b[i]);
            if(ca < cb) return -1;
            if(ca > cb) return 1;
        }
        if(sa < sb) return -1;
        if(sa > sb) return 1;
        return 0;
    }
    bool operator()(std::string const& a, std::string const& b) const
    {
        return compare(a, b) < 0;
    }
};

struct elf_data_t
{

    uint16_t data_begin;
    uint16_t data_end;
    uint16_t bss_begin;
    uint16_t bss_end;
    using map_type = std::map<uint16_t, elf_data_symbol_t>;
    map_type text_symbols;
    map_type data_symbols;
    std::vector<uint16_t> text_symbols_sorted;
    std::vector<uint16_t> data_symbols_sorted;
    std::vector<uint16_t> data_symbols_sorted_size;

    struct source_file_t
    {
        std::string filename;
        std::vector<std::string> lines;
    };

    std::vector<source_file_t> source_files;
    std::unordered_map<std::string, int> source_file_names;
    using source_line = std::pair<int, int>; // file index, line
    std::unordered_map<uint16_t, source_line> source_lines;

    std::vector<disassembled_instr_t> asm_with_source;
    size_t addr_to_disassembled_index(uint16_t addr);

    std::vector<char> fdata;
#ifdef ARDENS_LLVM
    std::unique_ptr<llvm::object::ObjectFile> obj;
    std::unique_ptr<llvm::DWARFContext> dwarf_ctx;
#endif

    struct global_t
    {
        uint64_t cu_offset; // compile unit
        int file, line;
        uint32_t addr;
        uint32_t type; // DIE offset
        bool text;
    };
    std::map<std::string, global_t, icompare> globals;

    ~elf_data_t(); // only exists for unique_ptr's above

};

// game save data
struct savedata_t
{
    static constexpr size_t MAX_EEPROM_BYTES = 1024;
    static constexpr size_t MAX_FX_SECTORS = w25q128_t::NUM_SECTORS;

    uint64_t game_hash;
    std::vector<uint8_t> eeprom;
    std::map<uint32_t, std::array<uint8_t, 4096>> fx_sectors;
    std::bitset<1024> eeprom_modified_bytes;

    template<class A> void serialize(A& a)
    {
        a(game_hash);
        a(bitsery::maxSize(eeprom, MAX_EEPROM_BYTES));
        a(bitsery::maxSize(fx_sectors, MAX_FX_SECTORS));
        a(eeprom_modified_bytes);
    }
    void clear()
    {
        game_hash = 0;
        eeprom.clear();
        fx_sectors.clear();
        eeprom_modified_bytes.reset();
    }
};

struct arduboy_config_t
{
    display_t::type_t display_type = display_t::type_t::SSD1306;
    uint8_t fxport_reg = reg::addr::PORTD;
    uint8_t fxport_mask = reg::bit::PORTD::PORTD1;
    bool bootloader = true;
    bool boot_to_menu = false;
    usb_bus_state_t usb_bus_state = USB_BUS_CONNECTED;
};

static constexpr size_t ARDUBOY_NUM_INSTRS =
    atmega32u4_t::PROG_SIZE_BYTES / 2;

struct arduboy_profiler_hotspot_t
{
    uint64_t count{};
    uint16_t begin{}, end{};
    template <class A> void serialize(A& a) { a(count, begin, end); }
};

struct arduboy_debugger_input_t
{
    uint64_t cycle{};
    uint8_t pinb{}, pine{}, pinf{};
    bool operator<(arduboy_debugger_input_t const& other) const
    {
        return cycle < other.cycle;
    }
    template <class A> void serialize(A& a) { a(cycle, pinb, pine, pinf); }
};

struct arduboy_debugger_tt_state_t
{
    uint64_t cycle{};
    size_t uncompressed_size{};
    std::vector<uint8_t> state;
    template <class A> void serialize(A& a) { a(cycle, uncompressed_size, state); }
};

struct arduboy_core_state_t
{
    atmega32u4_t cpu{};
    uint64_t ps_rem{};
};

struct arduboy_peripherals_t
{
    display_t display{};
    w25q128_t fx{};
    bool prev_display_reset{};
    uint8_t fxport_reg = reg::addr::PORTD;
    uint8_t fxport_mask = reg::bit::PORTD::PORTD1;
};

struct arduboy_program_state_t
{
    uint64_t game_hash{};
    std::string title;
    std::string device_type;
    std::string prog_filename;
    std::vector<uint8_t> prog_filedata;
    std::vector<uint8_t> fxdata;
    std::vector<uint8_t> fxsave;
    std::unique_ptr<elf_data_t> elf;
    arduboy_config_t cfg;
    bool flashcart_loaded{};
};

struct arduboy_profiler_state_t
{
    std::array<uint64_t, ARDUBOY_NUM_INSTRS> counts{};
    uint64_t total{};
    uint64_t total_with_sleep{};

    // counts for previous frame
    uint64_t prev_total{};
    uint64_t prev_total_with_sleep{};
    uint64_t prev_frame_cycles{};
    uint32_t total_frames{};
    uint32_t total_ms{};
    uint32_t frame_bytes_total{};
    uint32_t frame_bytes{};
    std::vector<float> frame_cpu_usage;

    // time-based cpu usage
    std::vector<float> ms_cpu_usage_raw;
    std::vector<float> ms_cpu_usage;
    uint64_t prev_total_ms{};
    uint64_t prev_total_with_sleep_ms{};
    uint64_t prev_ms_cycles{};

    bool enabled{};
    
    uint64_t cached_total{};
    uint64_t cached_total_with_sleep{};
    std::array<arduboy_profiler_hotspot_t, ARDUBOY_NUM_INSTRS> hotspots{};
    uint32_t num_hotspots{};
    std::vector<arduboy_profiler_hotspot_t> hotspots_symbol;
};

struct arduboy_debugger_state_t
{
    // breakpoints
    std::bitset<ARDUBOY_NUM_INSTRS> breakpoints;
    std::bitset<atmega32u4_t::DATA_SIZE_BYTES> breakpoints_rd;
    std::bitset<atmega32u4_t::DATA_SIZE_BYTES> breakpoints_wr;
    bool allow_nonstep_breakpoints{};
    uint32_t break_step = 0xffffffff;

    // paused at breakpoint
    bool paused{};

    // time-travel debugging info
    std::vector<arduboy_debugger_input_t> input_history;
    std::vector<arduboy_debugger_tt_state_t> state_history;
    arduboy_debugger_tt_state_t present_state;
};

struct arduboy_save_data_state_t
{
    savedata_t savedata;
    bool dirty{};
};

struct arduboy_t
{
    static constexpr size_t NUM_INSTRS = ARDUBOY_NUM_INSTRS;
    using hotspot_t = arduboy_profiler_hotspot_t;
    using inputs_t = arduboy_debugger_input_t;
    using tt_state_t = arduboy_debugger_tt_state_t;

    arduboy_core_state_t core_state;
    arduboy_peripherals_t peripherals;
    arduboy_program_state_t program_state;
    arduboy_profiler_state_t profiler_state;
    arduboy_debugger_state_t debugger_state;
    arduboy_save_data_state_t save_data_state;

    arduboy_t();
    arduboy_t(arduboy_t const&) = delete;
    arduboy_t& operator=(arduboy_t const&) = delete;
    arduboy_t(arduboy_t&&) = delete;
    arduboy_t& operator=(arduboy_t&&) = delete;

    void update_game_hash();
    void reload_fx();
    elf_data_symbol_t const* symbol_for_prog_addr(uint16_t addr);
    elf_data_symbol_t const* symbol_for_data_addr(uint16_t addr);

    void profiler_build_hotspots();
    void profiler_reset();

    bool load_savedata(std::istream& f);
    void save_savedata(std::ostream& f);

    static constexpr uint64_t STATE_HISTORY_CYCLES = 0x100000;
    static constexpr uint64_t STATE_HISTORY_TOTAL_MS = 60000;
    static constexpr uint64_t STATE_HISTORY_TOTAL_CYCLES =
        STATE_HISTORY_TOTAL_MS * 16000;
    static constexpr uint64_t STATE_HISTORY_TOTAL_RECORDS =
        STATE_HISTORY_TOTAL_CYCLES / STATE_HISTORY_CYCLES;

    // time-travel debugging
    void save_state_to_vector(tt_state_t& state);
    void load_state_from_vector(tt_state_t const& state);
    void update_history();
    void travel_back_to_cycle(uint64_t cycle);
    void travel_back_single_instr(uint64_t min_cycle = 0);
    void travel_back_single_instr_over(uint64_t min_cycle = 0);
    void travel_back_single_instr_out(uint64_t min_cycle = 0);
    void travel_to_present();
    void travel_continue();
    bool is_present_state();

    void reset();

    // advance at least one cycle (returns how many cycles were advanced)
    uint32_t cycle();

    void advance_instr();

    // each cycle is 62.5 ns
    static constexpr uint64_t CYCLE_PS = 62500;
    static constexpr uint64_t PS_BUFFER = CYCLE_PS * 256 * 64;

    // advance by specified number of picoseconds
    // ratio is for display filtering: 1.0 means real time,
    // 0.1 means 10x slower
    void advance(uint64_t ps);

    // returns an error string on error or empty string on success
    std::string load_file(char const* filename, std::istream& f, bool save = false);

    std::string load_bootloader_hex(std::istream& f);
    std::string load_bootloader_hex(uint8_t const* data, size_t size);
    std::string load_flashcart_zip(uint8_t const* data, size_t size);

    // snapshots contain full debugger and device state and are compressed
    bool save_snapshot(std::ostream& f);
    std::string load_snapshot(std::istream& f);

    // savestates only contain device state and are not compressed (e.g., for RetroArch)
    size_t max_savestate_size() const;
    std::string save_savestate(std::ostream& f);
    std::string load_savestate(std::istream& f);

    twi_wire_state_t twi_wire_state() const;
    twi_wire_state_t twi_drive_state() const;
    void set_twi_external_lines(bool scl_low, bool sda_low);
};


ARDENS_FORCEINLINE static uint32_t increase_counter(uint32_t& counter, uint32_t inc, uint32_t top)
{
    uint32_t c = counter + inc;
    uint32_t n = c / top;
    counter = c % top;
    return n;
}

template<class T> size_t array_bytes(T const& a)
{
    return a.size() * sizeof(*a.data());
}

bool compress_zlib(std::vector<uint8_t>& dst, void const* src, size_t src_bytes);
bool uncompress_zlib(std::vector<uint8_t>& dst, void const* src, size_t src_bytes);

}

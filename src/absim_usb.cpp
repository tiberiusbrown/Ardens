#include "absim.hpp"

namespace absim
{

constexpr uint16_t UHWCON  = 0xd7;
constexpr uint16_t USBCON  = 0xd8;
constexpr uint16_t USBSTA  = 0xd9;
constexpr uint16_t USBINT  = 0xda;
constexpr uint16_t UDCON   = 0xe0;
constexpr uint16_t UDINT   = 0xe1;
constexpr uint16_t UDIEN   = 0xe2;

constexpr uint16_t UEINTX  = 0xe8;
constexpr uint16_t UENUM   = 0xe9;
constexpr uint16_t UERST   = 0xea;
constexpr uint16_t UECONX  = 0xeb;
constexpr uint16_t UECFG0X = 0xec;
constexpr uint16_t UECFG1X = 0xed;
constexpr uint16_t UESTA0X = 0xee;
constexpr uint16_t UESTA1X = 0xef;
constexpr uint16_t UEIENX  = 0xf0;
constexpr uint16_t UEDATX  = 0xf1;
constexpr uint16_t UEBCLX  = 0xf2;
constexpr uint16_t UEBCHX  = 0xf3;
constexpr uint16_t UEINT   = 0xf4;

static void usb_set_next_update_cycle(atmega32u4_t& cpu)
{
    cpu.usb_next_update_cycle = UINT64_MAX;
    cpu.usb_next_update_cycle = std::min(cpu.usb_next_update_cycle, cpu.usb_next_eorsti_cycle);
    cpu.usb_next_update_cycle = std::min(cpu.usb_next_update_cycle, cpu.usb_next_sofi_cycle);
    cpu.usb_next_update_cycle = std::min(cpu.usb_next_update_cycle, cpu.usb_next_setconf_cycle);
    cpu.usb_next_update_cycle = std::min(cpu.usb_next_update_cycle, cpu.usb_next_setlength_cycle);
}

void atmega32u4_t::reset_usb()
{
    memset(&usb_ep, 0, sizeof(usb_ep));
    data[0xe0] = 0x1; // UDCON
    usb_attached = false;
    usb_next_update_cycle = UINT64_MAX;
    usb_next_eorsti_cycle = UINT64_MAX;
    usb_next_sofi_cycle = UINT64_MAX;
    usb_next_setconf_cycle = UINT64_MAX;
    usb_next_setlength_cycle = UINT64_MAX;
    data[USBCON] = 1 << 5;
}

void atmega32u4_t::usb_endpoint_t::configure()
{
    uint32_t size = 8 << ((uecfg1x >> 4) & 0x7);
    bool alloc = ((uecfg1x & 0x2) != 0);
    if(alloc)
    {
        start = length = 0;
        buffer.resize(size);
    }
}

static void update_rwal(atmega32u4_t& cpu)
{
    auto& ep = cpu.usb_ep[cpu.data[UENUM] & 7];
    bool rwal = (ep.length < ep.buffer.size());
    uint8_t ueintx = cpu.data[UEINTX];
    if(rwal)
        ueintx |= (1 << 5);
    else
        ueintx &= ~(1 << 5);
    cpu.data[UEINTX] = ueintx;
}

uint8_t atmega32u4_t::usb_endpoint_t::read(atmega32u4_t& cpu)
{
    if(buffer.empty() || length == 0)
    {
        cpu.data[UESTA0X] = 1 << 5;
        return 0;
    }
    uint8_t r = buffer[start];
    start = (start + 1) % buffer.size();
    --length;
    uebclx = ((length >> 0) & 0xff);
    uebchx = ((length >> 8) & 0xff);
    copy_regs(cpu);
    return r;
}

void atmega32u4_t::usb_endpoint_t::write(atmega32u4_t& cpu, uint8_t x)
{
    if(length >= buffer.size())
    {
        cpu.data[UESTA0X] = 1 << 6;
        return;
    }
    uint32_t i = (start + length) % buffer.size();
    buffer[i] = x;
    ++length;
    uebclx = ((length >> 0) & 0xff);
    uebchx = ((length >> 8) & 0xff);
    copy_regs(cpu);
}

void atmega32u4_t::usb_endpoint_t::copy_regs(atmega32u4_t& cpu)
{
    cpu.data[UEINTX]  = ueintx;
    cpu.data[UERST]   = uerst;
    cpu.data[UECONX]  = ueconx;
    cpu.data[UECFG0X] = uecfg0x;
    cpu.data[UECFG1X] = uecfg1x;
    cpu.data[UESTA0X] = uesta0x;
    cpu.data[UESTA1X] = uesta1x;
    cpu.data[UEIENX]  = ueienx;
    cpu.data[UEBCLX]  = uebclx;
    cpu.data[UEBCHX]  = uebchx;
    update_rwal(cpu);
}

uint8_t atmega32u4_t::usb_ld_handler_uedatx(atmega32u4_t& cpu, uint16_t ptr)
{
    assert(ptr = UEDATX);
    return cpu.data[UEDATX] = cpu.usb_ep[cpu.data[UENUM] & 0x7].read(cpu);
}

void atmega32u4_t::usb_st_handler(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    auto& ep = cpu.usb_ep[cpu.data[UENUM] & 7];
    switch(ptr)
    {
    case UENUM:
    {
        ep.ueintx  = cpu.data[UEINTX];
        ep.uerst   = cpu.data[UERST];
        ep.ueconx  = cpu.data[UECONX];
        ep.uecfg0x = cpu.data[UECFG0X];
        ep.uecfg1x = cpu.data[UECFG1X];
        ep.uesta0x = cpu.data[UESTA0X];
        ep.uesta1x = cpu.data[UESTA1X];
        ep.ueienx  = cpu.data[UEIENX];
        ep.uebclx  = cpu.data[UEBCLX];
        ep.uebchx  = cpu.data[UEBCHX];
        x &= 0x7;
        cpu.usb_ep[x].copy_regs(cpu);
        break;
    }
    case UEDATX:
        //ep.write(cpu, x);
        cpu.serial_bytes.push_back(x);
        break;
    case UEBCLX:
    case UEBCHX:
        // read only
        x = cpu.data[ptr];
        break;
    case UEIENX:  ep.ueienx  = x; break;
    case UESTA0X: ep.uesta0x = x; break;
    case UESTA1X: ep.uesta1x = x; break;
    case UECONX:  ep.ueconx  = x; break;
    case UERST:   ep.uerst   = x; break;
    case UECFG0X: ep.uecfg0x = x; break;
    case UECFG1X: ep.uecfg1x = x; ep.configure(); break;
    case UEINTX:  x = (ep.ueintx &= x); break;
    case USBCON:
        if((x & 0x80) && !(cpu.data[ptr] & 0x80))
        {
            cpu.usb_next_eorsti_cycle = cpu.cycle_count + 48000;
            cpu.data[USBINT] = 0x1;
        }
        if(x & 0x80)
            cpu.usb_next_setconf_cycle = cpu.cycle_count + 16000;
        else
            cpu.reset_usb();
        usb_set_next_update_cycle(cpu);
        break;
    case UDCON:
        if((cpu.data[ptr] & 1) && !(x & 1))
            cpu.usb_next_sofi_cycle = cpu.cycle_count + 16000;
        if(x & 1)
            cpu.usb_next_sofi_cycle = UINT64_MAX;
        usb_set_next_update_cycle(cpu);
        cpu.usb_attached = ((x & 1) != 0);
        break;
    default:
        break;
    }

    cpu.data[ptr] = x;
}

static void usb_endpoint_ints(atmega32u4_t& cpu)
{
    uint8_t i = 0;
    for(int n = 0; n < 7; ++n)
        if(cpu.usb_ep[n].ueintx & cpu.usb_ep[n].ueienx)
            i |= (1 << n);
    cpu.data[UEINT] = i;
}

void atmega32u4_t::update_usb()
{
    while(cycle_count >= usb_next_sofi_cycle)
    {
        // generate sofi (start of frame) interrupt
        data[UDINT] |= (1 << 2);
        usb_next_sofi_cycle += 16000;
    }
    if(cycle_count >= usb_next_eorsti_cycle)
    {
        // generate eorsti (end of reset) interrupt
        data[UDINT] |= (1 << 3);
        usb_next_eorsti_cycle = UINT64_MAX;
    }
    if(cycle_count >= usb_next_setconf_cycle)
    {
        static uint8_t const CMD[] = { 0, 9, 1, 0, 0, 0, 0, 0, };
        for(uint8_t x : CMD)
            usb_ep[0].write(*this, x);
        usb_next_setconf_cycle = UINT64_MAX;
        usb_next_setlength_cycle = cycle_count + 16000;
        usb_ep[0].ueintx |= (1 << 3);
        if(data[UENUM] == 0) usb_ep[0].copy_regs(*this);
    }
    if(cycle_count >= usb_next_setlength_cycle)
    {
        static uint8_t const CMD[] = { 0x21, 0x22, 3, 0, 0, 0, 0, 64, };
        for(uint8_t x : CMD)
            usb_ep[0].write(*this, x);
        usb_next_setlength_cycle = UINT64_MAX;
        usb_ep[0].ueintx |= (1 << 3);
        if(data[UENUM] == 0) usb_ep[0].copy_regs(*this);
    }
    usb_endpoint_ints(*this);
    usb_set_next_update_cycle(*this);
}

}

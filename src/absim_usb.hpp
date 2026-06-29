#include "absim.hpp"

namespace absim
{

static void usb_set_next_update_cycle(atmega32u4_t& cpu)
{
    uint64_t c = UINT64_MAX;
    c = std::min(c, cpu.usb_next_eorsti_cycle);
    c = std::min(c, cpu.usb_next_sofi_cycle);
    c = std::min(c, cpu.usb_next_setconf_cycle);
    c = std::min(c, cpu.usb_next_setlength_cycle);
    cpu.usb_next_update_cycle = c;
    if(c != UINT64_MAX)
        cpu.peripheral_queue.schedule(c, PQ_USB);
}

void atmega32u4_t::reset_usb()
{
    usb_ep = {};
    data[reg::addr::UDCON] = reg::bit::UDCON::DETACH;
    usb_attached = false;
    usb_next_update_cycle = UINT64_MAX;
    usb_next_eorsti_cycle = UINT64_MAX;
    usb_next_sofi_cycle = UINT64_MAX;
    usb_next_setconf_cycle = UINT64_MAX;
    usb_next_setlength_cycle = UINT64_MAX;
    data[reg::addr::USBCON] = reg::bit::USBCON::FRZCLK;
}

void atmega32u4_t::usb_endpoint_t::configure()
{
    uint32_t size = 0;
    uint8_t const size_mask =
        reg::bit::UECFG1X::EPSIZE0 |
        reg::bit::UECFG1X::EPSIZE1 |
        reg::bit::UECFG1X::EPSIZE2;
    switch((uecfg1x & size_mask) >> 4)
    {
    case 0: size = 128; break;
    case 1: size = 256; break;
    case 2: size = 512; break;
    default: size = 512; break;
    }
    bool alloc = ((uecfg1x & reg::bit::UECFG1X::ALLOC) != 0);
    if(alloc)
    {
        start = length = 0;
        buffer_size = size;
    }
}

static void update_rwal(atmega32u4_t& cpu)
{
    constexpr uint8_t EPNUM_MASK =
        reg::bit::UENUM::EPNUM0 |
        reg::bit::UENUM::EPNUM1 |
        reg::bit::UENUM::EPNUM2;
    auto& ep = cpu.usb_ep[cpu.data[reg::addr::UENUM] & EPNUM_MASK];
    bool rwal = (ep.length < ep.buffer_size);
    uint8_t ueintx = cpu.data[reg::addr::UEINTX];
    if(rwal)
        ueintx |= reg::bit::UEINTX::RWAL;
    else
        ueintx &= ~reg::bit::UEINTX::RWAL;
    cpu.data[reg::addr::UEINTX] = ueintx;
}

uint8_t atmega32u4_t::usb_endpoint_t::read(atmega32u4_t& cpu)
{
    if(buffer.empty() || length == 0)
    {
        cpu.data[reg::addr::UESTA0X] = reg::bit::UESTA0X::UNDERFI;
        return 0;
    }
    uint8_t r = buffer[start];
    start = (start + 1) % buffer_size;
    --length;
    uebclx = ((length >> 0) & 0xff);
    uebchx = ((length >> 8) & 0xff);
    copy_regs(cpu);
    return r;
}

void atmega32u4_t::usb_endpoint_t::write(atmega32u4_t& cpu, uint8_t x)
{
    if(length >= buffer_size)
    {
        cpu.data[reg::addr::UESTA0X] = reg::bit::UESTA0X::OVERFI;
        return;
    }
    if(buffer_size != 0)
    {
        uint32_t i = (start + length) % buffer_size;
        buffer[i] = x;
        ++length;
    }
    uebclx = ((length >> 0) & 0xff);
    uebchx = ((length >> 8) & 0xff);
    copy_regs(cpu);
}

void atmega32u4_t::usb_endpoint_t::copy_regs(atmega32u4_t& cpu)
{
    cpu.data[reg::addr::UEINTX] = ueintx;
    cpu.data[reg::addr::UERST] = uerst;
    cpu.data[reg::addr::UECONX] = ueconx;
    cpu.data[reg::addr::UECFG0X] = uecfg0x;
    cpu.data[reg::addr::UECFG1X] = uecfg1x;
    cpu.data[reg::addr::UESTA0X] = uesta0x;
    cpu.data[reg::addr::UESTA1X] = uesta1x;
    cpu.data[reg::addr::UEIENX] = ueienx;
    cpu.data[reg::addr::UEBCLX] = uebclx;
    cpu.data[reg::addr::UEBCHX] = uebchx;
    update_rwal(cpu);
}

uint8_t atmega32u4_t::usb_ld_handler_uedatx(atmega32u4_t& cpu, uint16_t ptr)
{
    constexpr uint8_t EPNUM_MASK =
        reg::bit::UENUM::EPNUM0 |
        reg::bit::UENUM::EPNUM1 |
        reg::bit::UENUM::EPNUM2;
    return cpu.data[reg::addr::UEDATX] =
        cpu.usb_ep[cpu.data[reg::addr::UENUM] & EPNUM_MASK].read(cpu);
}

void atmega32u4_t::usb_st_handler(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    constexpr uint8_t EPNUM_MASK =
        reg::bit::UENUM::EPNUM0 |
        reg::bit::UENUM::EPNUM1 |
        reg::bit::UENUM::EPNUM2;
    auto& ep = cpu.usb_ep[cpu.data[reg::addr::UENUM] & EPNUM_MASK];
    switch(ptr)
    {
    case reg::addr::UENUM:
    {
        ep.ueintx  = cpu.data[reg::addr::UEINTX];
        ep.uerst   = cpu.data[reg::addr::UERST];
        ep.ueconx  = cpu.data[reg::addr::UECONX];
        ep.uecfg0x = cpu.data[reg::addr::UECFG0X];
        ep.uecfg1x = cpu.data[reg::addr::UECFG1X];
        ep.uesta0x = cpu.data[reg::addr::UESTA0X];
        ep.uesta1x = cpu.data[reg::addr::UESTA1X];
        ep.ueienx  = cpu.data[reg::addr::UEIENX];
        ep.uebclx  = cpu.data[reg::addr::UEBCLX];
        ep.uebchx  = cpu.data[reg::addr::UEBCHX];
        x &= EPNUM_MASK;
        cpu.usb_ep[x].copy_regs(cpu);
        break;
    }
    case reg::addr::UEDATX:
        //ep.write(cpu, x);
        if(x != 0)
        {
            cpu.serial_bytes.push_back(x);
        }
        break;
    case reg::addr::UEBCLX:
    case reg::addr::UEBCHX:
        // read only
        x = cpu.data[ptr];
        break;
    case reg::addr::UEIENX:  ep.ueienx  = x; break;
    case reg::addr::UESTA0X: ep.uesta0x = x; break;
    case reg::addr::UESTA1X: ep.uesta1x = x; break;
    case reg::addr::UECONX:  ep.ueconx  = x; break;
    case reg::addr::UERST:   ep.uerst   = x; break;
    case reg::addr::UECFG0X: ep.uecfg0x = x; break;
    case reg::addr::UECFG1X: ep.uecfg1x = x; ep.configure(); break;
    case reg::addr::UEINTX:  x = (ep.ueintx &= x); break;
    case reg::addr::USBCON:
        if((x & reg::bit::USBCON::USBE) && !(cpu.data[ptr] & reg::bit::USBCON::USBE))
        {
            cpu.usb_next_eorsti_cycle = cpu.cycle_count + 48000;
            cpu.data[reg::addr::USBINT] = reg::bit::USBINT::VBUSTI;
        }
        if(x & reg::bit::USBCON::USBE)
            cpu.usb_next_setconf_cycle = cpu.cycle_count + 16000;
        else
            cpu.reset_usb();
        usb_set_next_update_cycle(cpu);
        cpu.schedule_interrupt_check();
        break;
    case reg::addr::UDCON:
        if((cpu.data[ptr] & reg::bit::UDCON::DETACH) && !(x & reg::bit::UDCON::DETACH))
            cpu.usb_next_sofi_cycle = cpu.cycle_count + 16000;
        if(x & reg::bit::UDCON::DETACH)
            cpu.usb_next_sofi_cycle = UINT64_MAX;
        usb_set_next_update_cycle(cpu);
        cpu.usb_attached = ((x & reg::bit::UDCON::DETACH) != 0);
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
    cpu.data[reg::addr::UEINT] = i;
    if(i != 0)
        cpu.schedule_interrupt_check();
}

void atmega32u4_t::update_usb()
{
    while(cycle_count >= usb_next_sofi_cycle)
    {
        // generate sofi (start of frame) interrupt
        data[reg::addr::UDINT] |= reg::bit::UDINT::SOFI;
        usb_next_sofi_cycle += 16000;
        schedule_interrupt_check();
    }
    if(cycle_count >= usb_next_eorsti_cycle)
    {
        // generate eorsti (end of reset) interrupt
        data[reg::addr::UDINT] |= reg::bit::UDINT::EORSTI;
        usb_next_eorsti_cycle = UINT64_MAX;
        schedule_interrupt_check();
    }
    if(cycle_count >= usb_next_setconf_cycle)
    {
        static uint8_t const CMD[] = { 0, 9, 1, 0, 0, 0, 0, 0, };
        for(uint8_t x : CMD)
            usb_ep[0].write(*this, x);
        usb_next_setconf_cycle = UINT64_MAX;
        usb_next_setlength_cycle = cycle_count + 16000;
        usb_ep[0].ueintx |= reg::bit::UEINTX::RXSTPI;
        if(data[reg::addr::UENUM] == 0) usb_ep[0].copy_regs(*this);
    }
    if(cycle_count >= usb_next_setlength_cycle)
    {
        static uint8_t const CMD[] = { 0x21, 0x22, 3, 0, 0, 0, 0, 64, };
        for(uint8_t x : CMD)
            usb_ep[0].write(*this, x);
        usb_next_setlength_cycle = UINT64_MAX;
        usb_ep[0].ueintx |= reg::bit::UEINTX::RXSTPI;
        if(data[reg::addr::UENUM] == 0) usb_ep[0].copy_regs(*this);
    }
    usb_endpoint_ints(*this);
    usb_set_next_update_cycle(*this);
}

}

#include "absim.hpp"

namespace absim
{

static constexpr uint64_t USB_FRAME_CYCLES = 16000;
static constexpr uint64_t USB_HOST_STEP_CYCLES = 8000;
static constexpr uint8_t USB_EP_MASK =
    reg::bit::UENUM::EPNUM0 |
    reg::bit::UENUM::EPNUM1 |
    reg::bit::UENUM::EPNUM2;

static constexpr uint8_t USB_EPTYPE_CONTROL = 0;
static constexpr uint8_t USB_EPTYPE_BULK = 2;
static constexpr uint8_t USB_EPTYPE_INTERRUPT = 3;

static uint16_t usb_ep_size(uint8_t uecfg1x)
{
    uint8_t code = (uecfg1x &
        (reg::bit::UECFG1X::EPSIZE0 |
         reg::bit::UECFG1X::EPSIZE1 |
         reg::bit::UECFG1X::EPSIZE2)) >> 4;
    return code <= 6 ? uint16_t(8u << code) : 0;
}

static uint8_t usb_ep_bank_count(uint8_t uecfg1x)
{
    uint8_t code = (uecfg1x &
        (reg::bit::UECFG1X::EPBK0 |
         reg::bit::UECFG1X::EPBK1)) >> 2;
    if(code == 0) return 1;
    if(code == 1) return 2;
    return 0;
}

static uint8_t usb_ep_type(atmega32u4_t::usb_endpoint_t const& ep)
{
    return (ep.uecfg0x &
        (reg::bit::UECFG0X::EPTYPE0 |
         reg::bit::UECFG0X::EPTYPE1)) >> 6;
}

static bool usb_ep_in(atmega32u4_t::usb_endpoint_t const& ep)
{
    return usb_ep_type(ep) != USB_EPTYPE_CONTROL &&
        (ep.uecfg0x & reg::bit::UECFG0X::EPDIR) != 0;
}

static bool usb_ep_enabled(atmega32u4_t::usb_endpoint_t const& ep)
{
    return (ep.ueconx & reg::bit::UECONX::EPEN) != 0;
}

static bool usb_ep_configured(atmega32u4_t::usb_endpoint_t const& ep)
{
    return usb_ep_enabled(ep) &&
        ep.allocated &&
        (ep.uesta0x & reg::bit::UESTA0X::CFGOK) != 0;
}

static bool usb_bus_connected(atmega32u4_t const& cpu)
{
    return cpu.usb.bus_state == USB_BUS_CONNECTED;
}

static bool usb_device_enabled(atmega32u4_t const& cpu)
{
    return (cpu.data[reg::addr::USBCON] & reg::bit::USBCON::USBE) != 0;
}

static bool usb_device_attached(atmega32u4_t const& cpu)
{
    return usb_bus_connected(cpu) &&
        usb_device_enabled(cpu) &&
        (cpu.data[reg::addr::UDCON] & reg::bit::UDCON::DETACH) == 0;
}

static bool usb_clock_active(atmega32u4_t const& cpu)
{
    return usb_device_attached(cpu) &&
        (cpu.data[reg::addr::USBCON] & reg::bit::USBCON::FRZCLK) == 0;
}

static atmega32u4_t::usb_endpoint_t& usb_selected_ep(atmega32u4_t& cpu)
{
    return cpu.usb.endpoints[cpu.usb.selected_endpoint & USB_EP_MASK];
}

static atmega32u4_t::usb_bank_t& usb_current_bank(
    atmega32u4_t::usb_endpoint_t& ep)
{
    return ep.banks[ep.cpu_bank & 1];
}

static void usb_clear_bank(atmega32u4_t::usb_bank_t& bank)
{
    bank.length = 0;
    bank.offset = 0;
    bank.ready = false;
}

static void usb_clear_fifo(atmega32u4_t::usb_endpoint_t& ep)
{
    for(auto& bank : ep.banks)
        usb_clear_bank(bank);
    ep.cpu_bank = 0;
}

static uint16_t usb_bank_count_bytes(atmega32u4_t::usb_endpoint_t const& ep)
{
    auto const& bank = ep.banks[ep.cpu_bank & 1];
    if(usb_ep_in(ep))
        return bank.length;
    return bank.length >= bank.offset ? uint16_t(bank.length - bank.offset) : 0;
}

static void usb_schedule(atmega32u4_t& cpu);

static void usb_update_vbus(atmega32u4_t& cpu)
{
    uint8_t usbsta = reg::bit::USBSTA::ID;
    if(usb_bus_connected(cpu))
        usbsta |= reg::bit::USBSTA::VBUS;
    cpu.data[reg::addr::USBSTA] = usbsta;
}

static void usb_update_endpoint(atmega32u4_t& cpu, uint8_t n)
{
    auto& ep = cpu.usb.endpoints[n & USB_EP_MASK];
    uint8_t uesta0x = ep.uesta0x &
        (reg::bit::UESTA0X::OVERFI |
         reg::bit::UESTA0X::UNDERFI |
         reg::bit::UESTA0X::DTSEQ0 |
         reg::bit::UESTA0X::DTSEQ1);

    if(ep.allocated)
        uesta0x |= reg::bit::UESTA0X::CFGOK;

    uint8_t busy = 0;
    for(uint8_t i = 0; i < ep.bank_count && i < ep.banks.size(); ++i)
        if(ep.banks[i].ready)
            ++busy;
    uesta0x |= std::min<uint8_t>(busy, 3) &
        (reg::bit::UESTA0X::NBUSYBK0 | reg::bit::UESTA0X::NBUSYBK1);

    ep.uesta0x = uesta0x;

    if(usb_ep_type(ep) == USB_EPTYPE_CONTROL)
    {
        ep.ueintx &= ~reg::bit::UEINTX::RWAL;
    }
    else if(usb_ep_in(ep))
    {
        auto& bank = ep.banks[ep.cpu_bank & 1];
        if(usb_ep_configured(ep) &&
            !bank.ready &&
            bank.length == 0 &&
            (ep.ueintx & reg::bit::UEINTX::TXINI) == 0)
        {
            ep.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
        }
        bool writable = usb_ep_configured(ep) &&
            !bank.ready &&
            bank.length < ep.size &&
            (ep.ueintx & reg::bit::UEINTX::TXINI) != 0;
        if(writable)
            ep.ueintx |= reg::bit::UEINTX::RWAL;
        else
            ep.ueintx &= ~reg::bit::UEINTX::RWAL;
    }
    else
    {
        auto const& bank = ep.banks[ep.cpu_bank & 1];
        bool readable = usb_ep_configured(ep) &&
            bank.ready &&
            bank.offset < bank.length;
        if(readable)
            ep.ueintx |= reg::bit::UEINTX::RWAL;
        else
            ep.ueintx &= ~reg::bit::UEINTX::RWAL;
    }

    uint16_t bytes = usb_bank_count_bytes(ep);
    ep.uebclx = uint8_t(bytes);
    ep.uebchx = uint8_t((bytes >> 8) & 0x07);
}

static void usb_copy_selected_endpoint(atmega32u4_t& cpu)
{
    uint8_t n = cpu.usb.selected_endpoint & USB_EP_MASK;
    usb_update_endpoint(cpu, n);
    auto const& ep = cpu.usb.endpoints[n];
    cpu.data[reg::addr::UENUM] = n;
    cpu.data[reg::addr::UERST] = cpu.usb.uerst;
    cpu.data[reg::addr::UEINTX] = ep.ueintx;
    cpu.data[reg::addr::UECONX] = ep.ueconx;
    cpu.data[reg::addr::UECFG0X] = ep.uecfg0x;
    cpu.data[reg::addr::UECFG1X] = ep.uecfg1x;
    cpu.data[reg::addr::UESTA0X] = ep.uesta0x;
    cpu.data[reg::addr::UESTA1X] = ep.uesta1x;
    cpu.data[reg::addr::UEIENX] = ep.ueienx;
    cpu.data[reg::addr::UEBCLX] = ep.uebclx;
    cpu.data[reg::addr::UEBCHX] = ep.uebchx;
}

static void usb_update_endpoint_interrupts(atmega32u4_t& cpu)
{
    uint8_t summary = 0;
    for(uint8_t i = 0; i < 7; ++i)
    {
        usb_update_endpoint(cpu, i);
        auto const& ep = cpu.usb.endpoints[i];
        if((ep.ueintx & ep.ueienx) != 0)
            summary |= uint8_t(1u << i);
    }
    cpu.data[reg::addr::UEINT] = summary;
    usb_copy_selected_endpoint(cpu);
    if(summary != 0)
        cpu.schedule_interrupt_check();
}

static void usb_schedule(atmega32u4_t& cpu)
{
    uint64_t c = UINT64_MAX;
    c = std::min(c, cpu.usb.next_sof_cycle);
    c = std::min(c, cpu.usb.next_reset_cycle);
    c = std::min(c, cpu.usb.host.next_cycle);
    cpu.usb.next_update_cycle = c;
    cpu.peripheral_queue.reschedule(c, PQ_USB);
}

static void usb_schedule_host(atmega32u4_t& cpu, uint64_t delay = 1)
{
    if(!usb_device_attached(cpu))
        return;
    uint64_t c = cpu.cycle_count + delay;
    if(cpu.usb.host.next_cycle > c)
        cpu.usb.host.next_cycle = c;
    usb_schedule(cpu);
}

static bool usb_endpoint_zero_ready(atmega32u4_t& cpu)
{
    auto const& ep0 = cpu.usb.endpoints[0];
    return usb_ep_configured(ep0) &&
        ep0.size != 0 &&
        (ep0.ueconx & reg::bit::UECONX::STALLRQ) == 0;
}

static uint16_t usb_setup_w_length(std::array<uint8_t, 8> const& setup)
{
    return uint16_t(setup[6]) | (uint16_t(setup[7]) << 8);
}

static bool usb_setup_device_to_host(std::array<uint8_t, 8> const& setup)
{
    return (setup[0] & 0x80) != 0;
}

static void usb_start_control_transfer(
    atmega32u4_t& cpu,
    std::array<uint8_t, 8> const& setup,
    uint8_t const* out_data,
    uint16_t out_length)
{
    auto& control = cpu.usb.control;
    auto& ep0 = cpu.usb.endpoints[0];
    control = {};
    control.setup = setup;
    control.out_length = std::min<uint16_t>(
        out_length, uint16_t(control.out_data.size()));
    for(uint16_t i = 0; i < control.out_length; ++i)
        control.out_data[i] = out_data ? out_data[i] : 0;
    control.in_expected = usb_setup_w_length(setup);
    control.request = setup[1];
    control.stage = atmega32u4_t::usb_control_transfer_t::STAGE_SETUP;
    control.active = true;

    usb_clear_fifo(ep0);
    auto& bank = ep0.banks[0];
    for(uint8_t i = 0; i < setup.size(); ++i)
        bank.data[i] = setup[i];
    bank.length = uint16_t(setup.size());
    bank.offset = 0;
    bank.ready = true;

    ep0.uesta1x = usb_setup_device_to_host(setup) ?
        reg::bit::UESTA1X::CTRLDIR : 0;
    ep0.ueconx &= ~reg::bit::UECONX::STALLRQ;
    ep0.ueintx &= ~(reg::bit::UEINTX::RXOUTI |
        reg::bit::UEINTX::STALLEDI |
        reg::bit::UEINTX::NAKINI |
        reg::bit::UEINTX::NAKOUTI);
    ep0.ueintx |= reg::bit::UEINTX::RXSTPI;
    if(usb_setup_device_to_host(setup) || control.in_expected == 0)
        ep0.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
    else
        ep0.ueintx &= ~(reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON);

    usb_update_endpoint_interrupts(cpu);
}

static void usb_finish_control_transfer(atmega32u4_t& cpu)
{
    auto& host = cpu.usb.host;
    auto const request = cpu.usb.control.request;

    if(host.phase == atmega32u4_t::usb_fake_host_t::PHASE_SET_ADDRESS)
        host.address = 1;
    else if(host.phase == atmega32u4_t::usb_fake_host_t::PHASE_SET_CONFIGURATION)
        host.configured = true;
    else if(host.phase == atmega32u4_t::usb_fake_host_t::PHASE_SET_LINE_ENCODING)
        host.line_encoding_set = true;
    else if(host.phase == atmega32u4_t::usb_fake_host_t::PHASE_SET_CONTROL_LINE_STATE)
        host.control_line_state_set = true;

    (void)request;
    cpu.usb.control = {};

    switch(host.phase)
    {
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_ADDRESS:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_DEVICE_DESCRIPTOR;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_DEVICE_DESCRIPTOR:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_CONFIGURATION_HEADER;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_CONFIGURATION_HEADER:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_SET_CONFIGURATION;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_CONFIGURATION_DESCRIPTOR:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_SET_CONFIGURATION;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_CONFIGURATION:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_SET_LINE_ENCODING;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_LINE_ENCODING:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_SET_CONTROL_LINE_STATE;
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_CONTROL_LINE_STATE:
        host.phase = atmega32u4_t::usb_fake_host_t::PHASE_ENUMERATED;
        break;
    default:
        break;
    }

    usb_schedule_host(cpu, USB_HOST_STEP_CYCLES);
}

static void usb_control_status_out(atmega32u4_t& cpu)
{
    auto& ep0 = cpu.usb.endpoints[0];
    auto& bank = ep0.banks[0];
    usb_clear_bank(bank);
    bank.ready = true;
    ep0.ueintx |= reg::bit::UEINTX::RXOUTI | reg::bit::UEINTX::FIFOCON;
    cpu.usb.control.stage =
        atmega32u4_t::usb_control_transfer_t::STAGE_STATUS_OUT;
    usb_update_endpoint_interrupts(cpu);
    usb_finish_control_transfer(cpu);
}

static void usb_deliver_control_out_data(atmega32u4_t& cpu)
{
    auto& ep0 = cpu.usb.endpoints[0];
    auto& control = cpu.usb.control;
    auto& bank = ep0.banks[0];
    usb_clear_bank(bank);
    uint16_t n = std::min<uint16_t>(control.out_length, ep0.size);
    for(uint16_t i = 0; i < n; ++i)
        bank.data[i] = control.out_data[control.out_offset + i];
    bank.length = n;
    bank.offset = 0;
    bank.ready = true;
    control.out_offset += n;
    ep0.ueintx |= reg::bit::UEINTX::RXOUTI | reg::bit::UEINTX::FIFOCON;
    usb_update_endpoint_interrupts(cpu);
}

static void usb_control_rxstpi_cleared(atmega32u4_t& cpu)
{
    auto& control = cpu.usb.control;
    auto& ep0 = cpu.usb.endpoints[0];
    usb_clear_fifo(ep0);

    if(!control.active ||
        control.stage != atmega32u4_t::usb_control_transfer_t::STAGE_SETUP)
        return;

    if(usb_setup_device_to_host(control.setup))
    {
        control.stage = atmega32u4_t::usb_control_transfer_t::STAGE_DATA_IN;
        ep0.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
    }
    else if(control.out_length != 0)
    {
        control.stage = atmega32u4_t::usb_control_transfer_t::STAGE_DATA_OUT;
        ep0.ueintx &= ~(reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON);
        usb_deliver_control_out_data(cpu);
        return;
    }
    else
    {
        control.stage = atmega32u4_t::usb_control_transfer_t::STAGE_STATUS_IN;
        ep0.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
    }
    usb_update_endpoint_interrupts(cpu);
}

static void usb_control_rxouti_cleared(atmega32u4_t& cpu)
{
    auto& control = cpu.usb.control;
    auto& ep0 = cpu.usb.endpoints[0];
    usb_clear_fifo(ep0);

    if(!control.active)
        return;

    if(control.stage == atmega32u4_t::usb_control_transfer_t::STAGE_DATA_OUT)
    {
        if(control.out_offset < control.out_length)
        {
            usb_deliver_control_out_data(cpu);
            return;
        }
        control.stage = atmega32u4_t::usb_control_transfer_t::STAGE_STATUS_IN;
        ep0.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
        usb_update_endpoint_interrupts(cpu);
    }
    else if(control.stage == atmega32u4_t::usb_control_transfer_t::STAGE_STATUS_OUT)
    {
        usb_finish_control_transfer(cpu);
    }
}

static void usb_control_txini_cleared(atmega32u4_t& cpu)
{
    auto& control = cpu.usb.control;
    auto& ep0 = cpu.usb.endpoints[0];
    auto& bank = ep0.banks[0];

    if(!control.active)
        return;

    if(control.stage == atmega32u4_t::usb_control_transfer_t::STAGE_STATUS_IN)
    {
        usb_clear_fifo(ep0);
        usb_finish_control_transfer(cpu);
        return;
    }

    if(control.stage != atmega32u4_t::usb_control_transfer_t::STAGE_DATA_IN)
        return;

    uint16_t n = bank.length;
    control.in_drained = uint16_t(
        std::min<uint32_t>(0xffffu, uint32_t(control.in_drained) + n));
    usb_clear_fifo(ep0);

    bool short_packet = n < ep0.size;
    bool enough = control.in_drained >= control.in_expected;
    if(short_packet || enough)
    {
        usb_control_status_out(cpu);
    }
    else
    {
        ep0.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
        usb_update_endpoint_interrupts(cpu);
    }
}

static void usb_drain_cdc_in(atmega32u4_t& cpu)
{
    auto const& host = cpu.usb.host;
    if(!usb_device_attached(cpu) || !host.control_line_state_set)
        return;

    for(uint8_t i = 1; i < 7; ++i)
    {
        auto& ep = cpu.usb.endpoints[i];
        if(!usb_ep_configured(ep) ||
            !usb_ep_in(ep) ||
            usb_ep_type(ep) != USB_EPTYPE_BULK)
            continue;

        auto& bank = ep.banks[0];
        if(!bank.ready && bank.length == 0)
            continue;

        for(uint16_t j = 0; j < bank.length; ++j)
            cpu.serial_bytes.push_back(bank.data[j]);
        usb_clear_bank(bank);
        ep.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
    }
    usb_update_endpoint_interrupts(cpu);
}

static void usb_start_next_host_request(atmega32u4_t& cpu)
{
    auto& host = cpu.usb.host;
    if(!usb_endpoint_zero_ready(cpu))
    {
        usb_schedule_host(cpu, USB_HOST_STEP_CYCLES);
        return;
    }

    static constexpr uint8_t LINE_9600_8N1[7] =
    {
        0x80, 0x25, 0x00, 0x00, 0x00, 0x00, 0x08,
    };

    switch(host.phase)
    {
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_ADDRESS:
        usb_start_control_transfer(
            cpu, { 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
            nullptr, 0);
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_DEVICE_DESCRIPTOR:
        usb_start_control_transfer(
            cpu, { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 18, 0x00 },
            nullptr, 0);
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_CONFIGURATION_HEADER:
        usb_start_control_transfer(
            cpu, { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 9, 0x00 },
            nullptr, 0);
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_CONFIGURATION_DESCRIPTOR:
        usb_start_control_transfer(
            cpu, { 0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 0xff, 0x00 },
            nullptr, 0);
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_CONFIGURATION:
        usb_start_control_transfer(
            cpu, { 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
            nullptr, 0);
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_LINE_ENCODING:
        usb_start_control_transfer(
            cpu, { 0x21, 0x20, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 },
            LINE_9600_8N1, sizeof(LINE_9600_8N1));
        break;
    case atmega32u4_t::usb_fake_host_t::PHASE_SET_CONTROL_LINE_STATE:
        usb_start_control_transfer(
            cpu, { 0x21, 0x22, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },
            nullptr, 0);
        break;
    default:
        host.next_cycle = UINT64_MAX;
        usb_schedule(cpu);
        break;
    }
}

static void usb_configure_endpoint(atmega32u4_t& cpu, uint8_t n)
{
    auto& ep = cpu.usb.endpoints[n & USB_EP_MASK];
    ep.size = usb_ep_size(ep.uecfg1x);
    ep.bank_count = usb_ep_bank_count(ep.uecfg1x);
    ep.allocated = false;

    bool alloc = (ep.uecfg1x & reg::bit::UECFG1X::ALLOC) != 0;
    bool valid = alloc &&
        ep.size != 0 &&
        ep.size <= 256 &&
        ep.bank_count != 0;

    uint16_t offset = 0;
    if(valid)
    {
        for(uint8_t i = 0; i < n; ++i)
        {
            auto const& prev = cpu.usb.endpoints[i];
            if(prev.allocated)
                offset = uint16_t(offset + prev.size * prev.bank_count);
        }
        if(uint32_t(offset) + ep.size * ep.bank_count <= cpu.usb.dpram.size())
        {
            ep.dpram_offset = offset;
            ep.allocated = true;
        }
    }

    if(!ep.allocated)
        ep.uesta0x &= ~reg::bit::UESTA0X::CFGOK;

    usb_clear_fifo(ep);
    if(usb_ep_in(ep) && usb_ep_enabled(ep) && ep.allocated)
        ep.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;

    usb_update_endpoint_interrupts(cpu);
}

static void usb_reset_endpoint(atmega32u4_t& cpu, uint8_t n, bool keep_config)
{
    auto& ep = cpu.usb.endpoints[n & USB_EP_MASK];
    uint8_t ueconx = keep_config ? ep.ueconx : 0;
    uint8_t uecfg0x = keep_config ? ep.uecfg0x : 0;
    uint8_t uecfg1x = keep_config ? ep.uecfg1x : 0;
    uint8_t ueienx = keep_config ? ep.ueienx : 0;
    uint16_t size = keep_config ? ep.size : 0;
    uint16_t dpram_offset = keep_config ? ep.dpram_offset : 0;
    uint8_t bank_count = keep_config ? ep.bank_count : 0;
    bool allocated = keep_config ? ep.allocated : false;

    ep = {};
    ep.ueconx = ueconx;
    ep.uecfg0x = uecfg0x;
    ep.uecfg1x = uecfg1x;
    ep.ueienx = ueienx;
    ep.size = size;
    ep.dpram_offset = dpram_offset;
    ep.bank_count = bank_count;
    ep.allocated = allocated;

    if(usb_ep_in(ep) && usb_ep_configured(ep))
        ep.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
}

static void usb_bus_reset(atmega32u4_t& cpu)
{
    for(uint8_t i = 0; i < cpu.usb.endpoints.size(); ++i)
        usb_reset_endpoint(cpu, i, false);

    cpu.data[reg::addr::UDADDR] = 0;
    cpu.data[reg::addr::UDINT] |= reg::bit::UDINT::EORSTI;
    cpu.usb.host.reset_sent = true;
    cpu.usb.host.phase = atmega32u4_t::usb_fake_host_t::PHASE_SET_ADDRESS;
    cpu.usb.host.next_cycle = cpu.cycle_count + USB_HOST_STEP_CYCLES;
    cpu.usb.next_reset_cycle = UINT64_MAX;
    cpu.usb.next_sof_cycle = cpu.cycle_count + USB_FRAME_CYCLES;
    cpu.schedule_interrupt_check();
    usb_update_endpoint_interrupts(cpu);
    usb_schedule(cpu);
}

static void usb_attach(atmega32u4_t& cpu)
{
    if(!usb_device_attached(cpu))
        return;
    if(cpu.usb.host.attached)
        return;
    cpu.usb.host.attached = true;
    cpu.usb.host.reset_sent = false;
    cpu.usb.host.configured = false;
    cpu.usb.host.line_encoding_set = false;
    cpu.usb.host.control_line_state_set = false;
    cpu.usb.host.phase = atmega32u4_t::usb_fake_host_t::PHASE_IDLE;
    cpu.usb.control = {};
    cpu.usb.next_reset_cycle = cpu.cycle_count + USB_FRAME_CYCLES;
    cpu.usb.next_sof_cycle = cpu.cycle_count + USB_FRAME_CYCLES;
    usb_schedule(cpu);
}

static void usb_detach(atmega32u4_t& cpu)
{
    cpu.usb.host.attached = false;
    cpu.usb.host.next_cycle = UINT64_MAX;
    cpu.usb.next_sof_cycle = UINT64_MAX;
    cpu.usb.next_reset_cycle = UINT64_MAX;
    cpu.usb.control = {};
    usb_schedule(cpu);
}

void atmega32u4_t::reset_usb()
{
    usb_bus_state_t bus_state = usb.bus_state;
    usb = {};
    usb.bus_state = bus_state;
    usb.next_sof_cycle = UINT64_MAX;
    usb.next_reset_cycle = UINT64_MAX;
    usb.next_update_cycle = UINT64_MAX;
    usb.host.next_cycle = UINT64_MAX;
    data[reg::addr::UDCON] = reg::bit::UDCON::DETACH;
    data[reg::addr::USBCON] = reg::bit::USBCON::FRZCLK;
    usb_update_vbus(*this);
    usb_copy_selected_endpoint(*this);
}

uint8_t atmega32u4_t::usb_ld_handler(atmega32u4_t& cpu, uint16_t ptr)
{
    usb_update_vbus(cpu);
    usb_update_endpoint_interrupts(cpu);

    if(ptr != reg::addr::UEDATX)
        return cpu.data[ptr];

    auto& ep = usb_selected_ep(cpu);
    auto& bank = usb_current_bank(ep);
    uint8_t r = 0;
    if(bank.offset < bank.length)
    {
        r = bank.data[bank.offset++];
        if(bank.offset >= bank.length && !usb_ep_in(ep))
            ep.ueintx &= ~reg::bit::UEINTX::RWAL;
    }
    else
    {
        ep.uesta0x |= reg::bit::UESTA0X::UNDERFI;
    }

    cpu.data[reg::addr::UEDATX] = r;
    usb_update_endpoint_interrupts(cpu);
    return r;
}

static void usb_write_uedatx(atmega32u4_t& cpu, uint8_t x)
{
    auto& ep = usb_selected_ep(cpu);
    auto& bank = usb_current_bank(ep);
    bool control_active = cpu.usb.control.active &&
        (cpu.usb.selected_endpoint & USB_EP_MASK) == 0;
    bool host_cdc_in = cpu.usb.host.control_line_state_set &&
        usb_ep_configured(ep) &&
        usb_ep_in(ep) &&
        usb_ep_type(ep) == USB_EPTYPE_BULK;

    if(ep.size == 0)
        ep.size = 512;

    if(bank.length < ep.size && bank.length < bank.data.size())
    {
        bank.data[bank.length++] = x;
    }
    else
    {
        ep.uesta0x |= reg::bit::UESTA0X::OVERFI;
    }

    if(usb_bus_connected(cpu) &&
        !control_active &&
        !host_cdc_in &&
        x != 0)
    {
        cpu.serial_bytes.push_back(x);
    }
    if(host_cdc_in)
        usb_schedule_host(cpu, USB_HOST_STEP_CYCLES);

    usb_update_endpoint_interrupts(cpu);
}

static void usb_commit_in_packet(atmega32u4_t& cpu, uint8_t n)
{
    auto& ep = cpu.usb.endpoints[n & USB_EP_MASK];
    auto& bank = usb_current_bank(ep);
    if(usb_ep_type(ep) == USB_EPTYPE_CONTROL)
    {
        usb_control_txini_cleared(cpu);
        return;
    }

    if(!usb_ep_in(ep))
        return;

    bank.ready = true;
    ep.ueintx &= ~(reg::bit::UEINTX::RWAL | reg::bit::UEINTX::FIFOCON);
    usb_schedule_host(cpu, USB_HOST_STEP_CYCLES);
}

void atmega32u4_t::usb_st_handler(atmega32u4_t& cpu, uint16_t ptr, uint8_t x)
{
    usb_update_vbus(cpu);

    switch(ptr)
    {
    case reg::addr::UHWCON:
        cpu.data[ptr] = x & reg::bit::UHWCON::UVREGE;
        break;

    case reg::addr::USBCON:
    {
        uint8_t old = cpu.data[ptr];
        x &= reg::bit::USBCON::USBE |
            reg::bit::USBCON::FRZCLK |
            reg::bit::USBCON::OTGPADE |
            reg::bit::USBCON::VBUSTE;
        cpu.data[ptr] = x;
        if((x & reg::bit::USBCON::USBE) == 0)
        {
            cpu.reset_usb();
        }
        else
        {
            if(usb_bus_connected(cpu) &&
                (old & reg::bit::USBCON::USBE) == 0)
                cpu.data[reg::addr::USBINT] |= reg::bit::USBINT::VBUSTI;
            usb_update_vbus(cpu);
            if(usb_device_attached(cpu))
                usb_attach(cpu);
        }
        cpu.schedule_interrupt_check();
        break;
    }

    case reg::addr::USBSTA:
        usb_update_vbus(cpu);
        break;

    case reg::addr::USBINT:
        cpu.data[ptr] &= x & reg::bit::USBINT::VBUSTI;
        cpu.schedule_interrupt_check();
        break;

    case reg::addr::UDCON:
    {
        uint8_t old = cpu.data[ptr];
        x &= reg::bit::UDCON::DETACH |
            reg::bit::UDCON::RMWKUP |
            reg::bit::UDCON::LSM |
            reg::bit::UDCON::RSTCPU;
        cpu.data[ptr] = x;
        bool was_attached = (old & reg::bit::UDCON::DETACH) == 0;
        bool now_attached = (x & reg::bit::UDCON::DETACH) == 0;
        if(was_attached && !now_attached)
            usb_detach(cpu);
        else if(!was_attached && now_attached)
            usb_attach(cpu);
        break;
    }

    case reg::addr::UDINT:
        cpu.data[ptr] &= x & 0x7f;
        cpu.schedule_interrupt_check();
        break;

    case reg::addr::UDIEN:
        cpu.data[ptr] = x & 0x7f;
        cpu.schedule_interrupt_check();
        break;

    case reg::addr::UDADDR:
        cpu.data[ptr] = x;
        break;

    case reg::addr::UDFNUML:
    case reg::addr::UDFNUMH:
        break;

    case reg::addr::UDMFN:
        cpu.data[ptr] &= x & reg::bit::UDMFN::FNCERR;
        break;

    case reg::addr::UENUM:
        cpu.usb.selected_endpoint = x & USB_EP_MASK;
        usb_copy_selected_endpoint(cpu);
        break;

    case reg::addr::UERST:
        cpu.usb.uerst = x & 0x7f;
        for(uint8_t i = 0; i < 7; ++i)
            if(cpu.usb.uerst & (1u << i))
                usb_reset_endpoint(cpu, i, true);
        usb_update_endpoint_interrupts(cpu);
        break;

    case reg::addr::UECONX:
    {
        auto& ep = usb_selected_ep(cpu);
        uint8_t old = ep.ueconx;
        x &= reg::bit::UECONX::STALLRQ |
            reg::bit::UECONX::STALLRQC |
            reg::bit::UECONX::RSTDT |
            reg::bit::UECONX::EPEN;
        if(x & reg::bit::UECONX::STALLRQC)
            x &= ~(reg::bit::UECONX::STALLRQ | reg::bit::UECONX::STALLRQC);
        x &= ~reg::bit::UECONX::RSTDT;
        ep.ueconx = x;
        if((old & reg::bit::UECONX::EPEN) &&
            !(x & reg::bit::UECONX::EPEN))
            usb_clear_fifo(ep);
        if(usb_ep_in(ep) && usb_ep_configured(ep))
            ep.ueintx |= reg::bit::UEINTX::TXINI | reg::bit::UEINTX::FIFOCON;
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UECFG0X:
    {
        auto& ep = usb_selected_ep(cpu);
        ep.uecfg0x = x & (reg::bit::UECFG0X::EPDIR |
            reg::bit::UECFG0X::EPTYPE0 |
            reg::bit::UECFG0X::EPTYPE1);
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UECFG1X:
    {
        auto n = cpu.usb.selected_endpoint & USB_EP_MASK;
        auto& ep = cpu.usb.endpoints[n];
        ep.uecfg1x = x & (reg::bit::UECFG1X::ALLOC |
            reg::bit::UECFG1X::EPBK0 |
            reg::bit::UECFG1X::EPBK1 |
            reg::bit::UECFG1X::EPSIZE0 |
            reg::bit::UECFG1X::EPSIZE1 |
            reg::bit::UECFG1X::EPSIZE2);
        usb_configure_endpoint(cpu, n);
        break;
    }

    case reg::addr::UESTA0X:
    {
        auto& ep = usb_selected_ep(cpu);
        ep.uesta0x = (ep.uesta0x & reg::bit::UESTA0X::CFGOK) |
            (x & (reg::bit::UESTA0X::OVERFI |
                  reg::bit::UESTA0X::UNDERFI |
                  reg::bit::UESTA0X::DTSEQ0 |
                  reg::bit::UESTA0X::DTSEQ1));
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UESTA1X:
    {
        auto& ep = usb_selected_ep(cpu);
        ep.uesta1x = x & reg::bit::UESTA1X::CTRLDIR;
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UEIENX:
    {
        auto& ep = usb_selected_ep(cpu);
        ep.ueienx = x & 0xdf;
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UEINTX:
    {
        auto n = cpu.usb.selected_endpoint & USB_EP_MASK;
        auto& ep = cpu.usb.endpoints[n];
        uint8_t old = ep.ueintx;
        ep.ueintx &= x;
        bool cleared_rxstpi =
            (old & reg::bit::UEINTX::RXSTPI) &&
            !(ep.ueintx & reg::bit::UEINTX::RXSTPI);
        bool cleared_rxouti =
            (old & reg::bit::UEINTX::RXOUTI) &&
            !(ep.ueintx & reg::bit::UEINTX::RXOUTI);
        bool cleared_txini =
            (old & reg::bit::UEINTX::TXINI) &&
            !(ep.ueintx & reg::bit::UEINTX::TXINI);
        if(cleared_rxouti || cleared_txini)
            ep.ueintx &= ~reg::bit::UEINTX::FIFOCON;
        if(n == 0)
        {
            if(cleared_rxstpi)
                usb_control_rxstpi_cleared(cpu);
            if(cleared_rxouti)
                usb_control_rxouti_cleared(cpu);
            if(cleared_txini)
                usb_control_txini_cleared(cpu);
        }
        else if(cleared_rxouti && !usb_ep_in(ep))
        {
            usb_clear_fifo(ep);
        }
        else if(cleared_txini && usb_ep_in(ep))
        {
            usb_commit_in_packet(cpu, n);
        }
        usb_update_endpoint_interrupts(cpu);
        break;
    }

    case reg::addr::UEDATX:
        cpu.data[ptr] = x;
        usb_write_uedatx(cpu, x);
        break;

    case reg::addr::UEBCLX:
    case reg::addr::UEBCHX:
    case reg::addr::UEINT:
        break;

    default:
        cpu.data[ptr] = x;
        break;
    }
}

void atmega32u4_t::update_usb()
{
    usb_update_vbus(*this);

    if(!usb_device_attached(*this))
    {
        usb_detach(*this);
        usb_update_endpoint_interrupts(*this);
        return;
    }

    while(cycle_count >= usb.next_sof_cycle)
    {
        usb.frame_number = uint16_t((usb.frame_number + 1) & 0x07ff);
        data[reg::addr::UDFNUML] = uint8_t(usb.frame_number);
        data[reg::addr::UDFNUMH] = uint8_t((usb.frame_number >> 8) & 0x07);
        usb.next_sof_cycle += USB_FRAME_CYCLES;
    }

    if(cycle_count >= usb.next_reset_cycle)
        usb_bus_reset(*this);

    if(cycle_count >= usb.host.next_cycle)
    {
        usb.host.next_cycle = UINT64_MAX;
        if(usb_clock_active(*this))
        {
            if(usb.host.phase == usb_fake_host_t::PHASE_IDLE)
                usb.host.phase = usb_fake_host_t::PHASE_SET_ADDRESS;
            if(usb.host.phase == usb_fake_host_t::PHASE_ENUMERATED)
                usb_drain_cdc_in(*this);
            else if(!usb.control.active)
                usb_start_next_host_request(*this);
        }
    }

    usb_update_endpoint_interrupts(*this);
    usb_schedule(*this);
}

}
